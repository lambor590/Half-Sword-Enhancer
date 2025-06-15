#pragma once

#include <Windows.h>
#include <ShlObj.h>
#include <winhttp.h>
#include <filesystem>
#include <string>
#include <thread>
#include <fstream>
#include <vector>

#include "Logger.h"

namespace Util {
    constexpr DWORD INJECTION_TIMEOUT_MS = 10000;
    constexpr SIZE_T BUFFER_SIZE = 16384;
    constexpr SIZE_T RESPONSE_BUFFER_SIZE = 8192;
    
    constexpr const char* STEAM_GAME_URL = "steam://rungameid/2642680";
    constexpr const char* APP_FOLDER_NAME = "Half Sword Enhancer";
    constexpr const char* DLL_FILENAME = "HS-Enhancer.dll";
    constexpr const char* TEMP_DLL_FILENAME = "HS-Enhancer-Temp.dll";

    [[noreturn]] inline void fail(const std::string& msg) noexcept {
        MessageBoxA(nullptr, msg.c_str(), "Error", MB_ICONERROR);
        exit(1);
    }

    inline void showError(const std::string& msg) noexcept {
        MessageBoxA(nullptr, msg.c_str(), "Error", MB_ICONERROR);
    }

    [[nodiscard]] inline DWORD findApplicationByWindow() noexcept {
        HWND gameWindow = FindWindowA("UnrealWindow", nullptr);
        
        if (gameWindow) {
            DWORD processId = 0;
            GetWindowThreadProcessId(gameWindow, &processId);
            return processId;
        }
        
        return 0;
    }

    [[nodiscard]] inline const std::string& getAppDataPath() {
        static std::string fullPath;

        if (fullPath.empty()) {
            char appDataPath[MAX_PATH];
            if (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath)))
                fail("Failed to get AppData path");

            fullPath = std::string(appDataPath) + "\\" + APP_FOLDER_NAME;
            try {
                std::filesystem::create_directories(fullPath);
            } catch (const std::filesystem::filesystem_error& e) {
                fail((std::string("Failed to create directory in AppData: ") + e.what()).c_str());
            }
        }

        return fullPath;
    }

    [[nodiscard]] inline DWORD locateOrStartApplication() {
        DWORD pid = findApplicationByWindow();
        
        if (pid == 0) {
            Logger::info("Starting application...");
            ShellExecuteA(0, 0, STEAM_GAME_URL, 0, 0, SW_SHOW);
            
            Logger::info("Waiting for application window...");
            for (int i = 0; i < 60 && (pid = findApplicationByWindow()) == 0; i++) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
        return pid;
    }


    inline bool downloadDllFromGitHub(const std::string& dllPath, const std::string& version) {
        try {
            Logger::info("Downloading mod DLL from GitHub...");

            char downloadUrl[256];
            sprintf_s(downloadUrl, "https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v%s/HS-Enhancer.dll", version.c_str());
            std::wstring wDownloadUrl(downloadUrl, downloadUrl + strlen(downloadUrl));

            URL_COMPONENTS urlComp = { sizeof(URL_COMPONENTS) };
            wchar_t hostName[256] = {}, urlPath[1024] = {};
            urlComp.lpszHostName = hostName;
            urlComp.dwHostNameLength = sizeof(hostName) / sizeof(wchar_t);
            urlComp.lpszUrlPath = urlPath;
            urlComp.dwUrlPathLength = sizeof(urlPath) / sizeof(wchar_t);

            if (!WinHttpCrackUrl(wDownloadUrl.c_str(), 0, 0, &urlComp)) {
                Logger::error("Failed to parse DLL download URL");
                return false;
            }

            HINTERNET hSession = WinHttpOpen(L"Half Sword Enhancer",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
            if (!hSession) {
                Logger::error("Failed to initialize network connection for DLL download");
                return false;
            }

            HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
            if (!hConnect) {
                Logger::error("Failed to connect to GitHub for DLL download");
                WinHttpCloseHandle(hSession);
                return false;
            }

            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath,
                nullptr, nullptr, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (!hRequest) {
                Logger::error("Failed to create DLL download request");
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return false;
            }

            WinHttpSetTimeouts(hRequest, 5000, 5000, 5000, 15000);

            if (!WinHttpSendRequest(hRequest, nullptr, 0, nullptr, 0, 0, 0) ||
                !WinHttpReceiveResponse(hRequest, nullptr)) {
                Logger::error("Failed to send/receive DLL download request");
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return false;
            }

            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                NULL, &statusCode, &statusCodeSize, NULL) && statusCode != 200) {
                Logger::error("GitHub returned error code for DLL: " + std::to_string(statusCode));
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return false;
            }

            try {
                std::filesystem::remove(dllPath);
                Logger::info("Removed existing DLL file.");
            } catch (const std::filesystem::filesystem_error& e) {
                Logger::warn(std::string("Could not remove existing DLL: ") + e.what());
            }

            HANDLE hFile = CreateFileA(dllPath.c_str(), GENERIC_WRITE, 0, NULL,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                std::string altPath = std::filesystem::current_path().string() + "\\" + TEMP_DLL_FILENAME;
                hFile = CreateFileA(altPath.c_str(), GENERIC_WRITE, 0, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    Logger::info("Using alternative DLL path: " + altPath);
                    const_cast<std::string&>(dllPath) = altPath;
                } else {
                    Logger::error("Failed to create DLL file");
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return false;
                }
            }

            std::vector<char> buffer(BUFFER_SIZE);
            DWORD dwSize, dwDownloaded, dwWritten;
            DWORD totalBytes = 0;
            bool downloadSuccess = true;

            while (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
                dwSize = min(dwSize, static_cast<DWORD>(buffer.size()));
                if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                    downloadSuccess = false;
                    break;
                }

                if (!WriteFile(hFile, buffer.data(), dwDownloaded, &dwWritten, NULL) || dwWritten != dwDownloaded) {
                    downloadSuccess = false;
                    break;
                }

                totalBytes += dwDownloaded;
            }

            CloseHandle(hFile);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);

            if (!downloadSuccess || totalBytes < 10000) {
                Logger::error("DLL download failed or file too small");
                return false;
            }

            Logger::info("DLL downloaded successfully (" + std::to_string(totalBytes) + " bytes)");
            return true;
        }
        catch (const std::exception& e) {
            Logger::error(std::string("Error downloading DLL: ") + e.what());
            return false;
        }
    }


    inline bool initializeModInjection(DWORD processId, const std::string& dllPath) {
        try {
            Logger::info("Initializing mod injection...");
            
            HANDLE hProcess = OpenProcess(
                PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | 
                PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                FALSE, processId);
                
            if (!hProcess) {
                Logger::error("Failed to open target process");
                showError("Failed to access the game process. Try running as administrator.");
                return false;
            }
            
            SIZE_T pathLength = dllPath.length() + 1;
            LPVOID remotePath = VirtualAllocEx(hProcess, NULL, pathLength, 
                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                
            if (!remotePath) {
                Logger::error("Failed to allocate memory in target process");
                CloseHandle(hProcess);
                showError("Failed to allocate memory in the game process.");
                return false;
            }
            
            SIZE_T bytesWritten;
            if (!WriteProcessMemory(hProcess, remotePath, dllPath.c_str(), pathLength, &bytesWritten) 
                || bytesWritten != pathLength) {
                Logger::error("Failed to write DLL path to target process");
                VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
                CloseHandle(hProcess);
                showError("Failed to write data in the game process.");
                return false;
            }
            
            HMODULE hKernel32 = GetModuleHandleA("kernel32");
            if (!hKernel32) {
                Logger::error("Failed to get kernel32 module handle");
                VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
                CloseHandle(hProcess);
                showError("Failed to locate required system modules.");
                return false;
            }
            
            FARPROC loadLibraryAddr = GetProcAddress(hKernel32, "LoadLibraryA");
            if (!loadLibraryAddr) {
                Logger::error("Failed to get LoadLibraryA address");
                VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
                CloseHandle(hProcess);
                showError("Failed to locate required system functions.");
                return false;
            }
            
            Logger::info("Executing mod injection...");
            
            HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
                reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibraryAddr),
                remotePath, 0, NULL);
                
            if (!hThread) {
                Logger::error("Failed to create remote thread");
                VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
                CloseHandle(hProcess);
                showError("Failed to start the mod process.");
                return false;
            }
            
            DWORD waitResult = WaitForSingleObject(hThread, INJECTION_TIMEOUT_MS);
            DWORD exitCode = 0;
            GetExitCodeThread(hThread, &exitCode);
            CloseHandle(hThread);
            
            VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            
            if (waitResult == WAIT_TIMEOUT) {
                Logger::error("Injection timeout");
                showError("Timeout expired. The game is not responding.");
                return false;
            }
            
            if (exitCode == 0) {
                Logger::error("LoadLibrary failed in remote process");
                showError("Mod could not be loaded correctly. This might be due to antivirus blocking the mod.");
                return false;
            }
            
            Logger::info("Mod injection completed successfully.");
            return true;
            
        } catch (const std::exception& e) {
            Logger::error(std::string("Error in mod injection: ") + e.what());
            showError("An error occurred during the injection of the mod.");
            return false;
        }
    }
}