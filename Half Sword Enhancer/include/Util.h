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
            ShellExecuteA(0, 0, "steam://rungameid/2642680", 0, 0, SW_SHOW);

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
        try {
            HANDLE hProcess = OpenProcess(
                PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                FALSE, processId);

            if (!hProcess) {
                showError("Failed to access the game process.");
                return false;
            }

            Logger::info("Waiting a bit to inject the mod...");
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));

            SIZE_T pathLength = dllPath.length() + 1;
            LPVOID remotePath = VirtualAllocEx(hProcess, NULL, pathLength, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            
            if (!remotePath) {
                CloseHandle(hProcess);
                showError("Failed to allocate memory in the game process.");
                return false;
            }

            SIZE_T bytesWritten = 0;
            const char* pathData = dllPath.c_str();
            
            if (!WriteProcessMemory(hProcess, remotePath, pathData, pathLength, &bytesWritten) || bytesWritten != pathLength) {
                VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
                CloseHandle(hProcess);
                showError("Failed to write data in the game process.");
                return false;
            }
            
            FARPROC loadLibraryAddr = NULL;
            
            HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
            if (hKernel32) {
                loadLibraryAddr = (FARPROC)GetProcAddress(hKernel32, "LoadLibraryA");
            }
            
            if (!loadLibraryAddr) {
                VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
                CloseHandle(hProcess);
                showError("Failed to locate required system functions.");
                return false;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            Logger::info("Injecting mod...");
            
            HANDLE hThread = CreateRemoteThread(
                hProcess, 
                NULL, 
                0, 
                (LPTHREAD_START_ROUTINE)loadLibraryAddr, 
                remotePath, 
                0, 
                NULL
            );
            
            if (!hThread) {
                VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
                CloseHandle(hProcess);
                showError("Failed to start the mod process.");
                return false;
            }
            
            DWORD waitResult = WaitForSingleObject(hThread, timeoutMs);
            
            DWORD exitCode = 0;
            GetExitCodeThread(hThread, &exitCode);
            CloseHandle(hThread);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            
            if (waitResult == WAIT_TIMEOUT) {
                showError("Timeout expired. The game is not responding.");
                return false;
            }
            
            if (exitCode == 0) {
                showError("Mod could not be loaded correctly.");
                return false;
            }
            
            Logger::info("Mod injected successfully in the game.");
            return true;
        }
        catch (const std::exception& e) {
            Logger::error(std::string("Error in the injection process: ") + e.what());
            showError("An error occurred during the injection of the mod.");
            return false;
        }
    }
}