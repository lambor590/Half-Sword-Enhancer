#include <thread>
#include <chrono>
#include <Windows.h>

#include "../include/ProcessManager.h"
#include "../include/Logger.h"

namespace hse {

    std::expected<ProcessId, ProcessError> ProcessManager::LocateOrStartGame() noexcept {
        auto processId = FindGameProcess();
        if (processId) {
            return *processId;
        }

        hse::Logger::info("Starting Half Sword...");
        auto startResult = StartGameViaStream();
        if (!startResult) {
            return std::unexpected(startResult.error());
        }

        hse::Logger::info("Waiting for game window...");
        const auto maxWaitTime = std::chrono::steady_clock::now() + MAX_GAME_WAIT_TIME;

        while (std::chrono::steady_clock::now() < maxWaitTime) {
            processId = FindGameProcess();
            if (processId) {
                return *processId;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        return std::unexpected(ProcessError::GameNotFound);
    }

    std::expected<InjectionResult, ProcessError> ProcessManager::InjectDLL(
        ProcessId processId,
        std::string_view dllPath
    ) noexcept {
        const auto startTime = std::chrono::steady_clock::now();

        if (dllPath.empty()) {
            return std::unexpected(ProcessError::InvalidDllPath);
        }

        auto processHandle = OpenGameProcess(processId);
        if (!processHandle) {
            return std::unexpected(processHandle.error());
        }

        const SIZE_T pathSize = dllPath.length() + 1;
        LPVOID remotePath = VirtualAllocEx(
            processHandle->get(),
            nullptr,
            pathSize,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        );

        if (!remotePath) {
            return std::unexpected(ProcessError::MemoryAllocationFailed);
        }

        struct MemoryGuard {
            HANDLE process;
            LPVOID memory;
            ~MemoryGuard() {
                if (memory && process) VirtualFreeEx(process, memory, 0, MEM_RELEASE);
            }
        } memGuard{ processHandle->get(), remotePath };

        SIZE_T bytesWritten;
        if (!WriteProcessMemory(
            processHandle->get(),
            remotePath,
            dllPath.data(),
            pathSize,
            &bytesWritten) || bytesWritten != pathSize) {
            return std::unexpected(ProcessError::DllPathWriteFailed);
        }

        HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
        if (!kernel32) {
            return std::unexpected(ProcessError::ThreadCreationFailed);
        }

        FARPROC loadLibraryAddr = GetProcAddress(kernel32, "LoadLibraryA");
        if (!loadLibraryAddr) {
            return std::unexpected(ProcessError::ThreadCreationFailed);
        }

        HANDLE hThread = CreateRemoteThread(
            processHandle->get(),
            nullptr,
            0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibraryAddr),
            remotePath,
            0,
            nullptr
        );

        if (!hThread) {
            return std::unexpected(ProcessError::ThreadCreationFailed);
        }

        struct ThreadGuard {
            HANDLE handle;
            ~ThreadGuard() { if (handle) CloseHandle(handle); }
        } threadGuard{ hThread };

        const DWORD waitResult = WaitForSingleObject(hThread, static_cast<DWORD>(INJECTION_TIMEOUT.count()));
        if (waitResult != WAIT_OBJECT_0) {
            return std::unexpected(ProcessError::InjectionTimeout);
        }

        DWORD exitCode;
        if (!GetExitCodeThread(hThread, &exitCode)) {
            hse::Logger::error("Failed to get thread exit code");
            return std::unexpected(ProcessError::ThreadCreationFailed);
        }

        if (exitCode == 0) {
            hse::Logger::error("LoadLibraryA failed - DLL was not loaded into the target process");
            hse::Logger::error("Common causes: Antivirus blocking or corrupted DLL file");
            return std::unexpected(ProcessError::DllLoadFailed);
        }

        const auto endTime = std::chrono::steady_clock::now();
        const auto injectionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        hse::Logger::info("DLL injection completed successfully");
        return InjectionResult{ processId, injectionTime };
    }

    std::expected<ProcessId, ProcessError> ProcessManager::FindGameProcess() const noexcept {
        const HWND gameWindow = FindWindowA(GAME_WINDOW_CLASS.data(), nullptr);
        if (!gameWindow) {
            return std::unexpected(ProcessError::GameNotFound);
        }

        DWORD processId = 0;
        GetWindowThreadProcessId(gameWindow, &processId);

        if (processId == 0) {
            return std::unexpected(ProcessError::GameNotFound);
        }

        return static_cast<ProcessId>(processId);
    }

    std::expected<void, ProcessError> ProcessManager::StartGameViaStream() const noexcept {
        const HINSTANCE result = ShellExecuteA(
            nullptr,
            "open",
            STEAM_GAME_URL.data(),
            nullptr,
            nullptr,
            SW_SHOW
        );

        if (reinterpret_cast<INT_PTR>(result) <= 32) {
            return std::unexpected(ProcessError::GameStartFailed);
        }

        return {};
    }

    std::expected<ProcessHandle, ProcessError> ProcessManager::OpenGameProcess(ProcessId pid) const noexcept {
        HANDLE handle = OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
            FALSE,
            static_cast<DWORD>(pid)
        );

        if (!handle || handle == INVALID_HANDLE_VALUE) {
            return std::unexpected(ProcessError::ProcessOpenFailed);
        }

        return ProcessHandle(handle);
    }

}