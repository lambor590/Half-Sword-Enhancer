#pragma once

#include <Windows.h>
#include <TlHelp32.h>
#include <ShlObj.h>
#include <filesystem>
#include <string>
#include <thread>
#include "Logger.h"

namespace Util {
    struct EnumWindowsData {
        DWORD processId;
        HWND windowHandle;
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
        Logger::info("Waiting for game window to be available...");
        
        const int checkIntervalMs = 500;
        int attempts = 60 * 1000 / checkIntervalMs;
        
        for (int i = 0; i < attempts; i++) {
            HWND hwnd = FindProcessWindow(processId);
            if (hwnd != NULL) {
                char windowTitle[256];
                GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));
                Logger::info("Game window found: " + std::string(windowTitle));
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(checkIntervalMs));
        }
        
        Logger::warn("Timeout reached. Game window not found.");
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
}