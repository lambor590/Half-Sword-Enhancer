#pragma once

#include <Windows.h>
#include <TlHelp32.h>
#include <ShlObj.h>
#include <filesystem>
#include <string>
#include <thread>
#include <iostream>
#include <libloaderapi.h>
#include <memory>
#include <functional>

#include "Logger.h"

namespace Util {
    template<typename HandleType, typename CleanupFunc>
    class ScopedResource {
    private:
        HandleType handle;
        CleanupFunc cleanup;
        bool released = false;

    public:
        ScopedResource(HandleType h, CleanupFunc cf) : handle(h), cleanup(cf) {}
        
        ~ScopedResource() { 
            if (!released && isValid()) 
                cleanup(handle); 
        }
        
        operator HandleType() const { return handle; }
        HandleType get() const { return handle; }
        
        HandleType release() { 
            released = true; 
            return handle; 
        }
        
        bool isValid() const {
            if constexpr (std::is_same_v<HandleType, HANDLE>)
                return handle != NULL && handle != INVALID_HANDLE_VALUE;
            else
                return handle != NULL;
        }

        ScopedResource(const ScopedResource&) = delete;
        ScopedResource& operator=(const ScopedResource&) = delete;

        ScopedResource(ScopedResource&& other) noexcept 
            : handle(other.handle), cleanup(other.cleanup), released(other.released) {
            other.released = true;
        }
        
        ScopedResource& operator=(ScopedResource&& other) noexcept {
            if (this != &other) {
                if (!released && isValid())
                    cleanup(handle);
                
                handle = other.handle;
                cleanup = other.cleanup;
                released = other.released;
                other.released = true;
            }
            return *this;
        }
    };

    using ScopedHandle = ScopedResource<HANDLE, std::function<void(HANDLE)>>;
    using ScopedVirtualMemory = ScopedResource<LPVOID, std::function<void(LPVOID)>>;

    inline ScopedHandle createScopedHandle(HANDLE handle) {
        return ScopedHandle(handle, [](HANDLE h) { CloseHandle(h); });
    }

    inline ScopedVirtualMemory createScopedVirtualMemory(HANDLE process, LPVOID memory) {
        return ScopedVirtualMemory(memory, [process](LPVOID mem) { 
            if (process && mem) 
                VirtualFreeEx(process, mem, 0, MEM_RELEASE); 
        });
    }

    [[noreturn]] inline void fail(const char* msg) {
        MessageBoxA(nullptr, msg, "Error", MB_ICONERROR);
        exit(1);
    }

    inline void showError(const char* msg) {
        MessageBoxA(nullptr, msg, "Error", MB_ICONERROR);
    }

    struct EnumWindowsData {
        DWORD processId;
        HWND windowHandle;
    };

    inline BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
        auto* data = reinterpret_cast<EnumWindowsData*>(lParam);
        DWORD windowProcessId;
        GetWindowThreadProcessId(hwnd, &windowProcessId);

        if (windowProcessId == data->processId && IsWindowVisible(hwnd)) {
            data->windowHandle = hwnd;
            return FALSE;
        }
        return TRUE;
    }

    inline HWND FindProcessWindow(DWORD processId) {
        EnumWindowsData data = { processId, NULL };
        EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));
        return data.windowHandle;
    }

    inline bool WaitForGameWindow(DWORD processId, int timeoutSeconds = 60) {
        Logger::info("Waiting for Half Sword window to be available...");

        for (int i = 0; i < timeoutSeconds; i++) {
            if (HWND hwnd = FindProcessWindow(processId)) {
                Logger::info("Half Sword window found!");
                return true;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        return false;
    }

    inline DWORD getProcessIdByName(const char* processName) {
        PROCESSENTRY32 processEntry{ sizeof(PROCESSENTRY32) };
        auto snapshot = createScopedHandle(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        
        if (!snapshot.isValid()) 
            return 0;

        if (Process32First(snapshot, &processEntry)) {
            do {
                if (_stricmp(processEntry.szExeFile, processName) == 0)
                    return processEntry.th32ProcessID;
            } while (Process32Next(snapshot, &processEntry));
        }
        
        return 0;
    }

    inline bool isRunningAsAdmin() {
        BOOL isAdmin = FALSE;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        PSID adminGroup = NULL;

        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            CheckTokenMembership(NULL, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }

        return isAdmin != 0;
    }

    inline const std::string& getAppDataPath() {
        static std::string fullPath;

        if (fullPath.empty()) {
            char appDataPath[MAX_PATH];

            if (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath)))
                fail("Failed to get AppData path");

            fullPath = std::string(appDataPath) + "\\Half Sword Enhancer";

            if (!std::filesystem::exists(fullPath) && 
                !std::filesystem::create_directory(fullPath))
                fail("Failed to create directory in AppData");
        }

        return fullPath;
    }

    inline DWORD findOrLaunchGame(const char* processName, int timeoutSeconds = 60) {
        Logger::info("Searching for Half Sword process...");
        DWORD processId = getProcessIdByName(processName);

        if (processId == 0) {
            Logger::info("Half Sword not found, launching it...");
            ShellExecuteA(0, 0, "steam://rungameid/2527870", 0, 0, SW_SHOW);

            for (int i = 0; i < timeoutSeconds && (processId = getProcessIdByName(processName)) == 0; i++)
                std::this_thread::sleep_for(std::chrono::seconds(1));

            if (processId == 0)
                fail("Timeout waiting for game to start");
        }

        Logger::info("Half Sword process found.");
        return processId;
    }

    inline void extractDllToTempFile(const std::string& dllPath, DWORD resourceId) {
        HRSRC hResource = FindResourceA(NULL, MAKEINTRESOURCE(resourceId), RT_RCDATA);
        if (!hResource) 
            fail("Failed to find mod resource!");

        #pragma warning(suppress : 6387)
        HGLOBAL hLoadedResource = LoadResource(NULL, hResource);
        if (!hLoadedResource) 
            fail("Failed to load mod resource!");

        #pragma warning(suppress : 6387)
        LPVOID pLockedResource = LockResource(hLoadedResource);
        if (!pLockedResource) 
            fail("Failed to lock mod resource!");
        
        #pragma warning(suppress : 6387)
        DWORD dwResourceSize = SizeofResource(NULL, hResource);
        if (dwResourceSize == 0) 
            fail("Resource size is zero!");

        auto hFile = createScopedHandle(CreateFileA(
            dllPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 
            FILE_ATTRIBUTE_NORMAL, NULL
        ));
        
        if (!hFile.isValid()) 
            fail("Failed to create temporary DLL file.");

        DWORD bytesWritten;
        if (!WriteFile(hFile, pLockedResource, dwResourceSize, &bytesWritten, NULL) || 
            bytesWritten != dwResourceSize)
            fail("Failed to write DLL to temporary file.");

        Logger::info("DLL written to temporary file successfully.");
    }

    inline bool injectDll(DWORD processId, const std::string& dllPath, int timeoutMs = 10000) {
        auto procHandle = createScopedHandle(OpenProcess(
            PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD, 
            FALSE, processId
        ));
        
        if (!procHandle.isValid()) {
            showError("Failed to open handle! Please run the launcher as administrator or check your antivirus.");
            return false;
        }
        
        Logger::info("Handle opened successfully!");

        auto remoteMem = createScopedVirtualMemory(
            procHandle, 
            VirtualAllocEx(procHandle, nullptr, dllPath.length() + 1, MEM_COMMIT, PAGE_READWRITE)
        );
        
        if (!remoteMem.isValid()) {
            showError("Failed to allocate memory in Half Sword's process.");
            return false;
        }

        if (!WriteProcessMemory(procHandle, remoteMem, dllPath.c_str(), dllPath.length() + 1, NULL)) {
            showError("Failed to write DLL path to Half Sword's process memory.");
            return false;
        }
        
        HMODULE kernel32Module = GetModuleHandleA("kernel32.dll");
        FARPROC loadLibraryAddr = kernel32Module ? GetProcAddress(kernel32Module, "LoadLibraryA") : NULL;
        
        if (!loadLibraryAddr) {
            showError("Failed to get LoadLibraryA address");
            return false;
        }

        auto threadHandle = createScopedHandle(CreateRemoteThread(
            procHandle, nullptr, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, 
            remoteMem, 0, nullptr
        ));
        
        if (!threadHandle.isValid()) {
            showError("Failed to create remote thread.");
            return false;
        }

        DWORD waitResult = WaitForSingleObject(threadHandle, timeoutMs);
        if (waitResult == WAIT_TIMEOUT) {
            Logger::warn("Thread execution timed out. The game might be unresponsive.");
            #pragma warning(suppress : 6258)
            TerminateThread(threadHandle, 1);
            return false;
        }

        Logger::info("DLL injected successfully.");
        return true;
    }
}