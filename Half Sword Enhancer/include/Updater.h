#pragma once

#include <winhttp.h>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

#include "Logger.h"
#include "Util.h"

namespace Updater {
    constexpr const wchar_t* GITHUB_HOST = L"api.github.com";
    constexpr const wchar_t* GITHUB_API_PATH = L"/repos/lambor590/Half-Sword-Enhancer/releases/latest";
    constexpr const char* TAG_PREFIX = "\"tag_name\":\"";
    constexpr const char* ASSETS_PREFIX = "\"assets\":";
    constexpr const char* LAUNCHER_NAME = "\"name\":\"HS_Enhancer_Launcher.exe\"";
    constexpr const char* DEFAULT_VERSION = "0.0.0";
    constexpr const wchar_t* USER_AGENT = L"Half Sword Enhancer";

    [[nodiscard]] inline std::string getLocalVersion() noexcept {
        static std::string cachedVersion;

        if (!cachedVersion.empty()) {
            return cachedVersion;
        }

        char filePath[MAX_PATH];

        if (!GetModuleFileNameA(NULL, filePath, MAX_PATH)) {
            cachedVersion = DEFAULT_VERSION;
            return cachedVersion;
        }

        DWORD verSize = GetFileVersionInfoSizeA(filePath, nullptr);

        std::vector<BYTE> verData(verSize);
        VS_FIXEDFILEINFO* fileInfo = nullptr;
        UINT size = 0;

        if (!GetFileVersionInfoA(filePath, 0, verSize, verData.data()) ||
            !VerQueryValueA(verData.data(), "\\", (void**)&fileInfo, &size)) {
            cachedVersion = DEFAULT_VERSION;
            return cachedVersion;
        }

        char version[32];
        sprintf_s(version, "%d.%d.%d", 
            HIWORD(fileInfo->dwFileVersionMS), 
            LOWORD(fileInfo->dwFileVersionMS), 
            HIWORD(fileInfo->dwFileVersionLS));
        cachedVersion = version;

        return cachedVersion;
    }

    template<typename CleanupFunc>
    class scoped_winhttp_handle {
    private:
        HINTERNET handle;
        CleanupFunc cleanup;

    public:
        constexpr scoped_winhttp_handle(HINTERNET h, CleanupFunc cf) noexcept 
            : handle(h), cleanup(std::move(cf)) {}

        ~scoped_winhttp_handle() {
            if (handle) cleanup(handle);
        }

        constexpr operator HINTERNET() const noexcept { return handle; }
        constexpr HINTERNET get() const noexcept { return handle; }
        constexpr bool valid() const noexcept { return handle != nullptr; }

        scoped_winhttp_handle(const scoped_winhttp_handle&) = delete;
        scoped_winhttp_handle& operator=(const scoped_winhttp_handle&) = delete;
    };

    [[nodiscard]] inline auto make_winhttp_session(const wchar_t* userAgent) noexcept {
        return scoped_winhttp_handle(
            WinHttpOpen(userAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0),
            [](HINTERNET h) { WinHttpCloseHandle(h); }
        );
    }

    [[nodiscard]] inline auto make_winhttp_connection(HINTERNET session, const wchar_t* host) noexcept {
        return scoped_winhttp_handle(
            WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0),
            [](HINTERNET h) { WinHttpCloseHandle(h); }
        );
    }

    [[nodiscard]] inline auto make_winhttp_request(HINTERNET connection, const wchar_t* path) noexcept {
        return scoped_winhttp_handle(
            WinHttpOpenRequest(connection, L"GET", path, NULL, NULL, NULL, WINHTTP_FLAG_SECURE),
            [](HINTERNET h) { WinHttpCloseHandle(h); }
        );
    }

    [[nodiscard]] inline std::string getRemoteVersion() noexcept {
        try {
            std::string version = DEFAULT_VERSION;

            Logger::info("Checking for updates...");

            auto session = make_winhttp_session(USER_AGENT);
            if (!session.valid()) {
                Logger::error("Failed to initialize WinHTTP: " + std::to_string(GetLastError()));
                return version;
            }

            auto connect = make_winhttp_connection(session, GITHUB_HOST);
            if (!connect.valid()) {
                Logger::error("Failed to connect to GitHub");
                return version;
            }

            auto request = make_winhttp_request(connect, GITHUB_API_PATH);
            if (!request.valid()) {
                Logger::error("Failed to open request");
                return version;
            }

            if (!WinHttpSendRequest(request, NULL, 0, NULL, 0, 0, 0) ||
                !WinHttpReceiveResponse(request, NULL)) {
                Logger::error("Failed to send/receive request");
                return version;
            }

            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                NULL, &statusCode, &statusCodeSize, NULL) && statusCode != 200) {
                Logger::error("GitHub API error: " + std::to_string(statusCode));
                return version;
            }

            std::string response;
            response.reserve(Util::RESPONSE_BUFFER_SIZE);
            DWORD size;
            std::vector<char> buffer(Util::RESPONSE_BUFFER_SIZE);

            while (WinHttpQueryDataAvailable(request, &size) && size > 0) {
                DWORD downloaded;
                size = min(size, Util::RESPONSE_BUFFER_SIZE);
                if (WinHttpReadData(request, buffer.data(), size, &downloaded)) {
                    response.append(buffer.data(), downloaded);
                }
            }

            if (response.empty()) {
                Logger::error("Empty response from GitHub");
                return version;
            }

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
                    return version;
                }
            }

            Logger::error("Failed to parse version from GitHub response");
            return version;
        }
        catch (const std::exception& e) {
            Logger::error("Update check failed: " + std::string(e.what()));
            return DEFAULT_VERSION;
        }
    }

    [[nodiscard]] inline bool isUpdateAvailable(const char* local, const char* remote) noexcept {
        if (!remote || strcmp(remote, "0.0.0") == 0) {
            return false;
        }
        
        #ifdef DEV_VERSION
            int localMajor = 0, localMinor = 0, localPatch = 0;
            int remoteMajor = 0, remoteMinor = 0, remotePatch = 0;
            
            sscanf_s(local, "%d.%d.%d", &localMajor, &localMinor, &localPatch);
            sscanf_s(remote, "%d.%d.%d", &remoteMajor, &remoteMinor, &remotePatch);
            
            return (remoteMajor > localMajor) || 
                   (remoteMajor == localMajor && remoteMinor > localMinor) || 
                   (remoteMajor == localMajor && remoteMinor == localMinor && remotePatch >= localPatch);
        #else
            return strcmp(remote, local) != 0;
        #endif
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

            std::vector<char> buffer(16384, 0);
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

            char message[512];
            sprintf_s(message, "A new version of Half Sword Enhancer is available!\n\n"
                              "Current version: %s\n"
                              "New version: %s\n\n"
                              "Do you want to download and install the update now?",
                              localVersion.c_str(), remoteVersion.c_str());

            int result = MessageBoxA(NULL, message, "Update Available", MB_YESNO | MB_ICONINFORMATION);

            if (result != IDYES) {
                return;
            }

            Logger::info("Updating mod first...");
            const std::string& appDataPath = Util::getAppDataPath();
            std::string modPath = appDataPath + "\\" + Util::DLL_FILENAME;
            
            try {
                std::filesystem::remove(modPath);
                Logger::info("Removed existing mod DLL for update");
            } catch (const std::exception&) {
                Logger::warn("Could not remove existing mod DLL");
            }

            if (!Util::downloadDllFromGitHub(modPath, remoteVersion)) {
                Logger::error("Failed to update mod");
                Util::showError("Failed to update mod files. The launcher update will continue anyway.");
            } else {
                Logger::info("Mod updated successfully");
            }

            Logger::info("Now updating launcher...");
            char launcherDownloadUrl[256];
            sprintf_s(launcherDownloadUrl, "https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v%s/HS_Enhancer_Launcher.exe", remoteVersion.c_str());

            downloadUpdate(launcherDownloadUrl);
        }
        catch (const std::exception&) {
            Logger::error("Error checking for updates");
            Util::showError("An error occurred while checking for updates. Please try again later.");
            return;
        }
    }
}
