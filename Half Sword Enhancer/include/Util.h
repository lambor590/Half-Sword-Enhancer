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

#include "Logger.h"

namespace Util {
    struct EnumWindowsData {
        DWORD processId;
        HWND windowHandle;
    };

    class ScopedHandle {
    private:
        HANDLE handle;
    public:
        explicit ScopedHandle(HANDLE h) : handle(h) {}
        ~ScopedHandle() { if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
        operator HANDLE() const { return handle; }
        HANDLE get() const { return handle; }
        HANDLE release() { HANDLE temp = handle; handle = nullptr; return temp; }
        bool isValid() const { return handle && handle != INVALID_HANDLE_VALUE; }
    };

    class ScopedVirtualMemory {
    private:
        HANDLE process;
        LPVOID memory;
    public:
        ScopedVirtualMemory(HANDLE proc, LPVOID mem) : process(proc), memory(mem) {}
        ~ScopedVirtualMemory() { if (process && memory) VirtualFreeEx(process, memory, 0, MEM_RELEASE); }
        LPVOID get() const { return memory; }
        LPVOID release() { LPVOID temp = memory; memory = nullptr; return temp; }
    };

    inline BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
        EnumWindowsData* data = reinterpret_cast<EnumWindowsData*>(lParam);
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

    inline bool WaitForGameWindow(DWORD processId) {
        Logger::info("Waiting for Half Sword window to be available...");

        for (int i = 0; i < 60; i++) {
            HWND hwnd = FindProcessWindow(processId);
            if (hwnd != NULL) {
                char windowTitle[256];
                GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));
                Logger::info("Half Sword window found!");
                return true;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        return false;
    }

    inline static DWORD getProcessIdByName(const char* processName) {
        PROCESSENTRY32 processEntry{ sizeof(PROCESSENTRY32) };
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return 0;

        DWORD processId = 0;
        if (Process32First(snapshot, &processEntry)) {
            do {
                if (_stricmp(processEntry.szExeFile, processName) == 0) {
                    processId = processEntry.th32ProcessID;
                    break;
                }
            } while (Process32Next(snapshot, &processEntry));
        }
        CloseHandle(snapshot);
        return processId;
    }

    inline static const bool isRunningAsAdmin() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            CheckTokenMembership(NULL, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }

        return isAdmin;
    }

    inline static void fail(const char* msg) {
        MessageBox(nullptr, msg, "Error", MB_ICONERROR);
        exit(1);
    }

    inline static const std::string& getAppDataPath() {
        static std::string fullPath;

        if (fullPath.empty()) {
            char appDataPath[MAX_PATH];

            if (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
                fail("Failed to get AppData path");
            }

            fullPath = std::string(appDataPath) + "\\Half Sword Enhancer";

            if (!std::filesystem::exists(fullPath)) {
                if (!std::filesystem::create_directory(fullPath))
                    fail("Failed to create directory in AppData");
            }
        }

        return fullPath;
    }

    inline void showBanner(HANDLE hConsole) {
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::cout << R"(
            __  __      ______   _____                        __
           / / / /___ _/ / __/  / ___/      ______  _________/ /
          / /_/ / __ `/ / /_    \__ \ | /| / / __ \/ ___/ __  /
         / __  / /_/ / / __/   ___/ / |/ |/ / /_/ / /  / /_/ /
        /_/ /_/\__,_/_/_/     /____/|__/|__/\____/_/   \__,_/
        )";

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << R"(
            ______      __
           / ____/___  / /_  ____ _____  ________  _____
          / __/ / __ \/ __ \/ __ `/ __ \/ ___/ _ \/ ___/
         / /___/ / / / / / / /_/ / / / / /__/  __/ /
        /_____/_/ /_/_/ /_/\__,_/_/ /_/\___/\___/_/     
        )" << "\n";

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }

    inline DWORD findOrLaunchGame(const char* processName) {
        Logger::info("Searching for Half Sword process...");
        DWORD processId = getProcessIdByName(processName);

        if (processId == 0) {
            Logger::info("Half Sword not found, launching it...");
            ShellExecuteA(0, 0, "steam://rungameid/2527870", 0, 0, SW_SHOW);

            for (int i = 0; i < 60; i++) {
                processId = getProcessIdByName(processName);
                if (processId != 0) break;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            if (processId == 0) {
                fail("Timeout waiting for game to start");
            }
        }

        Logger::info("Half Sword process found.");
        return processId;
    }

    inline void extractDllToTempFile(const std::string& dllPath, DWORD resourceId) {
        HRSRC hResource = FindResourceA(NULL, MAKEINTRESOURCE(resourceId), RT_RCDATA);
        if (!hResource) fail("Failed to find mod resource!");

        HGLOBAL hLoadedResource = LoadResource(NULL, hResource);
        if (!hLoadedResource) fail("Failed to load mod resource!");

        LPVOID pLockedResource = LockResource(hLoadedResource);
        DWORD dwResourceSize = SizeofResource(NULL, hResource);
        if (!pLockedResource || dwResourceSize == 0) fail("Failed to lock mod resource!");

        ScopedHandle hFile(CreateFileA(dllPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
        if (!hFile.isValid()) fail("Failed to create temporary DLL file.");

        DWORD bytesWritten;
        if (!WriteFile(hFile.get(), pLockedResource, dwResourceSize, &bytesWritten, NULL) || bytesWritten != dwResourceSize) {
            fail("Failed to write DLL to temporary file.");
        }

        Logger::info("DLL written to temporary file successfully.");
    }

    inline void injectDll(HANDLE procHandle, const std::string& dllPath) {
        LPVOID remoteMem = VirtualAllocEx(procHandle, nullptr, dllPath.length() + 1, MEM_COMMIT, PAGE_READWRITE);
        if (!remoteMem) fail("Failed to allocate memory in Half Sword's process.");

        ScopedVirtualMemory scopedMem(procHandle, remoteMem);

        if (!WriteProcessMemory(procHandle, remoteMem, dllPath.c_str(), dllPath.length() + 1, NULL)) {
            fail("Failed to write DLL path to Half Sword's process memory.");
        }

        Logger::info("DLL path written to Half Sword's process memory.");

        ScopedHandle threadHandle(CreateRemoteThread(procHandle, nullptr, 0,
            (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"),
            remoteMem, 0, nullptr));

        if (!threadHandle.isValid()) fail("Failed to create remote thread.");

        DWORD waitResult = WaitForSingleObject(threadHandle.get(), 10000);
        if (waitResult == WAIT_TIMEOUT) {
            Logger::warn("Thread execution timed out. The game might be unresponsive.");
            fail("DLL injection timed out. Please try again with the game freshly launched.");
        }

        Logger::info("Remote thread created successfully.");
    }
}