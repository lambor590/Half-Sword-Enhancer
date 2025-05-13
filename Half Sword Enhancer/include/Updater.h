#pragma once

#include <winhttp.h>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <iostream>

#include "Logger.h"
#include "Util.h"

namespace Updater {

    inline std::string getLocalVersion() {
        static std::string cachedVersion;

        if (!cachedVersion.empty()) {
            return cachedVersion;
        }

        const std::string defaultVersion = "0.0.0";
        char filePath[MAX_PATH];

        if (!GetModuleFileNameA(NULL, filePath, MAX_PATH)) {
            cachedVersion = defaultVersion;
            return cachedVersion;
        }

        DWORD verSize = GetFileVersionInfoSizeA(filePath, nullptr);

        std::vector<BYTE> verData(verSize);
        VS_FIXEDFILEINFO* fileInfo = nullptr;
        UINT size = 0;

        if (!GetFileVersionInfoA(filePath, 0, verSize, verData.data()) ||
            !VerQueryValueA(verData.data(), "\\", (void**)&fileInfo, &size)) {
            cachedVersion = defaultVersion;
            return cachedVersion;
        }

        cachedVersion = std::to_string(HIWORD(fileInfo->dwFileVersionMS)) + "." +
            std::to_string(LOWORD(fileInfo->dwFileVersionMS)) + "." +
            std::to_string(HIWORD(fileInfo->dwFileVersionLS));

        return cachedVersion;
    }

    inline std::string getRemoteVersion() {
        try {
            const wchar_t host[] = L"api.github.com";
            const wchar_t path[] = L"/repos/lambor590/Half-Sword-Enhancer/releases/latest";
            std::string version = "0.0.0";

            Logger::info("Checking for updates...");

            HINTERNET session = WinHttpOpen(L"Half Sword Enhancer", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!session) {
                Logger::error("Failed to initialize WinHTTP: " + std::to_string(GetLastError()));
                return version;
            }

            HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
            if (!connect) {
                Logger::error("Failed to connect to GitHub");
                WinHttpCloseHandle(session);
                return version;
            }

            HINTERNET request = WinHttpOpenRequest(connect, L"GET", path, NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
            if (!request) {
                Logger::error("Failed to open request");
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return version;
            }

            WinHttpSetTimeouts(request, 5000, 5000, 5000, 15000);

            if (!WinHttpSendRequest(request, NULL, 0, NULL, 0, 0, 0) ||
                !WinHttpReceiveResponse(request, NULL)) {
                Logger::error("Failed to send/receive request");
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return version;
            }

            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                NULL, &statusCode, &statusCodeSize, NULL) && statusCode != 200) {
                Logger::error("GitHub API error: " + std::to_string(statusCode));
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return version;
            }

            std::string response;
            response.reserve(8192);
            DWORD size;
            char buffer[8192];

            while (WinHttpQueryDataAvailable(request, &size) && size > 0) {
                DWORD downloaded;
                if (WinHttpReadData(request, buffer, min(size, sizeof(buffer)), &downloaded)) {
                    response.append(buffer, downloaded);
                }
            }

            if (response.empty()) {
                Logger::error("Empty response from GitHub");
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return version;
            }

            static const char* TAG_PREFIX = "\"tag_name\":\"";
            size_t tagPos = response.find(TAG_PREFIX);
            if (tagPos != std::string::npos) {
                tagPos += strlen(TAG_PREFIX);
                size_t endPos = response.find("\"", tagPos);
                if (endPos != std::string::npos) {
                    version = response.substr(tagPos, endPos - tagPos);

                    if (!version.empty() && version[0] == 'v') {
                        version = version.substr(1);
                    }

                    Logger::info("Latest version: " + version);
                    WinHttpCloseHandle(request);
                    WinHttpCloseHandle(connect);
                    WinHttpCloseHandle(session);
                    return version;
                }
            }

            static const char* ASSETS_PREFIX = "\"assets\":";
            static const char* NAME_PREFIX = "\"name\":\"HS_Enhancer_Launcher.exe\"";

            size_t assetsPos = response.find(ASSETS_PREFIX);
            if (assetsPos == std::string::npos) {
                Logger::error("Invalid GitHub response format");
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return version;
            }

            size_t namePos = response.find(NAME_PREFIX, assetsPos);
            if (namePos == std::string::npos) {
                Logger::error("Launcher executable not found in response");
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return version;
            }

            tagPos = response.find(TAG_PREFIX);
            if (tagPos == std::string::npos) {
                Logger::error("Version tag not found in response");
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return version;
            }

            tagPos += strlen(TAG_PREFIX);
            size_t endPos = response.find("\"", tagPos);
            if (endPos == std::string::npos) {
                Logger::error("Malformed version tag");
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return version;
            }

            version = response.substr(tagPos, endPos - tagPos);

            if (!version.empty() && version[0] == 'v') {
                version = version.substr(1);
            }

            Logger::info("Latest version: " + version);
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return version;
        }
        catch (const std::exception& e) {
            Logger::error("Update check failed: " + std::string(e.what()));
            return "0.0.0";
        }
    }

    inline bool isUpdateAvailable(const char* local, const char* remote) {
        std::vector<int> localVer(3, 0), remoteVer(3, 0);
        std::istringstream localStream(local), remoteStream(remote);
        std::string token;

        for (int i = 0; i < 3; i++) {
            if (std::getline(localStream, token, '.')) localVer[i] = std::stoi(token);
            if (std::getline(remoteStream, token, '.')) remoteVer[i] = std::stoi(token);
        }

        for (int i = 0; i < 3; i++) {
            if (remoteVer[i] > localVer[i]) return true;
            if (remoteVer[i] < localVer[i]) return false;
        }

        return false;
    }

    inline void createAndRunUpdateScript(const char* tempFileName, const char* currentPath) {
        try {
            const std::string& appDataPath = Util::getAppDataPath();
            std::string batPath = appDataPath + "\\update_hse.bat";

            if (std::filesystem::exists(batPath)) {
                try {
                    std::filesystem::remove(batPath);
                } catch (const std::exception&) {
                    Logger::warn("Failed to remove existing update script. Continuing anyway.");
                }
            }

            std::ofstream batFile(batPath);
            if (!batFile.is_open()) {
                Util::showError("Failed to create update script. Please try again or download the latest version yourself.");
                return;
            }

            {
                std::filesystem::path exePath(currentPath);
                std::filesystem::path dir = exePath.parent_path();
                std::filesystem::path testFile = dir / "update_test.tmp";
                std::ofstream test(testFile.string());
                if (!test.is_open()) {
                    Util::showError("Failed to update: please run the launcher as administrator or move it to a folder with write permissions.");
                    return;
                }
                test.close();
                std::filesystem::remove(testFile);
            }

            batFile << "@echo off\n"
                << "copy /Y \"" << tempFileName << "\" \"" << currentPath << "\"\n"
                << "if errorlevel 1 goto :error\n"
                << "del \"" << tempFileName << "\"\n"
                << "timeout /t 1 /nobreak > nul\n"
                << "start \"\" \"" << currentPath << "\"\n"
                << "del \"%~f0\"\n"
                << "exit\n"
                << ":error\n"
                << "del \"" << tempFileName << "\"\n"
                << "del \"%~f0\"\n";
            batFile.close();

            if (!std::filesystem::exists(batPath)) {
                Util::showError("Failed to create update script. Please try again or download the latest version yourself.");
                return;
            }

            STARTUPINFOA si = { sizeof(si) };
            PROCESS_INFORMATION pi;

            std::string cmdLine = "cmd.exe /c \"" + batPath + "\"";
            std::vector<char> cmdLineBuffer(cmdLine.length() + 1);
            strcpy_s(cmdLineBuffer.data(), cmdLineBuffer.size(), cmdLine.c_str());

            BOOL processCreated = CreateProcessA(NULL, cmdLineBuffer.data(), NULL, NULL, FALSE,
                                                CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);

            if (!processCreated) {
                Util::showError("Failed to launch update script. Please try again or download the latest version yourself.");
                return;
            }

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            ExitProcess(0);
        }
        catch (const std::exception&) {
            Util::showError("Error creating update script. Please try again or download the latest version yourself.");
        }
    }

    inline bool downloadUpdate(const std::string& downloadUrl) {
        try {
            Logger::info("Downloading update...");

            std::wstring wDownloadUrl(downloadUrl.begin(), downloadUrl.end());
            URL_COMPONENTS urlComp = { sizeof(URL_COMPONENTS) };
            wchar_t hostName[256] = {}, urlPath[1024] = {};
            urlComp.lpszHostName = hostName;
            urlComp.dwHostNameLength = sizeof(hostName) / sizeof(wchar_t);
            urlComp.lpszUrlPath = urlPath;
            urlComp.dwUrlPathLength = sizeof(urlPath) / sizeof(wchar_t);

            if (!WinHttpCrackUrl(wDownloadUrl.c_str(), 0, 0, &urlComp)) {
                Logger::error("Failed to parse download URL");
                Util::showError("Failed to parse download URL. Please try again or download manually from GitHub.");
                return false;
            }

            HINTERNET hSession = WinHttpOpen(L"Half Sword Enhancer Updater",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
            if (!hSession) {
                Logger::error("Failed to initialize network connection");
                Util::showError("Failed to initialize network connection. Please check your internet connection.");
                return false;
            }

            HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
            if (!hConnect) {
                Logger::error("Failed to connect to update server");
                WinHttpCloseHandle(hSession);
                Util::showError("Failed to connect to update server. Please check your internet connection.");
                return false;
            }

            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath,
                nullptr, nullptr, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (!hRequest) {
                Logger::error("Failed to create download request");
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                Util::showError("Failed to create download request. Please try again later.");
                return false;
            }

            WinHttpSetTimeouts(hRequest, 5000, 5000, 5000, 60000);

            if (!WinHttpSendRequest(hRequest, nullptr, 0, nullptr, 0, 0, 0) ||
                !WinHttpReceiveResponse(hRequest, nullptr)) {
                Logger::error("Failed to download update");
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                Util::showError("Failed to download update. Please check your internet connection and try again.");
                return false;
            }

            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                NULL, &statusCode, &statusCodeSize, NULL) && statusCode != 200) {
                Logger::error("Server returned error code: " + std::to_string(statusCode));
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                Util::showError("Server returned error code: " + std::to_string(statusCode) + ". Please try again later.");
                return false;
            }

            char currentPath[MAX_PATH];
            GetModuleFileNameA(NULL, currentPath, MAX_PATH);
            const std::string& appDataPath = Util::getAppDataPath();
            std::string tempFileName = appDataPath + "\\HS_Enhancer_Update.exe";

            if (std::filesystem::exists(tempFileName)) {
                try {
                    std::filesystem::remove(tempFileName);
                } catch (const std::exception&) {
                    Logger::warn("Failed to remove existing update file. Continuing anyway.");
                }
            }

            HANDLE hFile = CreateFileA(tempFileName.c_str(), GENERIC_WRITE, 0, NULL,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                Logger::error("Failed to create update file");
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                Util::showError("Failed to create update file. Please check your antivirus settings or try running as administrator.");
                return false;
            }

            char buffer[16384]{};
            DWORD dwSize, dwDownloaded, dwWritten;
            DWORD totalBytes = 0;
            bool downloadSuccess = true;

            while (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
                dwSize = min(dwSize, sizeof(buffer));
                if (!WinHttpReadData(hRequest, buffer, dwSize, &dwDownloaded)) {
                    downloadSuccess = false;
                    break;
                }

                if (!WriteFile(hFile, buffer, dwDownloaded, &dwWritten, NULL) || dwWritten != dwDownloaded) {
                    downloadSuccess = false;
                    break;
                }

                totalBytes += dwDownloaded;
            }

            CloseHandle(hFile);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);

            if (!downloadSuccess) {
                Logger::error("Download failed");
                Util::showError("Failed to download update completely. Please try again later.");
                return false;
            }

            if (totalBytes < 300000) { 
                Logger::error("Downloaded file is too small");
                Util::showError("Downloaded update file appears to be invalid. Please try again later.");
                return false;
            }

            Logger::info("Download completed successfully");

            MessageBoxA(NULL, "Update downloaded successfully. The application will now close and update itself.", "Update Complete", MB_OK | MB_ICONINFORMATION);

            createAndRunUpdateScript(tempFileName.c_str(), currentPath);
            return true;
        }
        catch (const std::exception& e) {
            Logger::error("Update download failed");
            Util::showError("Error during update process: " + std::string(e.what()) + ". Please try again later or download the latest version yourself.");
            return false;
        }
    }

    inline void checkForUpdates() {
        try {
            std::string remoteVersion = getRemoteVersion();

            if (strcmp(remoteVersion.c_str(), "0.0.0") == 0) {
                return;
            }

            std::string localVersion = getLocalVersion();

            if (!isUpdateAvailable(localVersion.c_str(), remoteVersion.c_str())) {
                return;
            }

            std::string message = "A new version of Half Sword Enhancer is available!\n\n";
            message += "Current version: " + localVersion + "\n";
            message += "New version: " + remoteVersion + "\n\n";
            message += "Do you want to download and install the update now?";

            int result = MessageBoxA(NULL, message.c_str(), "Update Available", MB_YESNO | MB_ICONINFORMATION);

            if (result != IDYES) {
                return;
            }

            std::string downloadUrl = "https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v" +
                remoteVersion + "/HS_Enhancer_Launcher.exe";

            downloadUpdate(downloadUrl);
        }
        catch (const std::exception&) {
            Logger::error("Error checking for updates");
            Util::showError("An error occurred while checking for updates. Please try again later.");
            return;
        }
    }
}
