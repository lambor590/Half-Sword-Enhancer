#pragma once

#include <winhttp.h>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <iostream>

#include "Logger.h"

namespace Updater {

    inline std::string getLocalVersion() {
        const char defaultVersion[] = "0.0.0";
        char filePath[MAX_PATH];

        if (!GetModuleFileNameA(NULL, filePath, MAX_PATH)) {
            return defaultVersion;
        }

        DWORD verSize = GetFileVersionInfoSizeA(filePath, nullptr);

        std::vector<BYTE> verData(verSize);
        VS_FIXEDFILEINFO* fileInfo = nullptr;
        UINT size = 0;

        if (!GetFileVersionInfoA(filePath, 0, verSize, verData.data()) ||
            !VerQueryValueA(verData.data(), "\\", (void**)&fileInfo, &size)) {
            return defaultVersion;
        }

        return std::to_string(HIWORD(fileInfo->dwFileVersionMS)) + "." +
               std::to_string(LOWORD(fileInfo->dwFileVersionMS)) + "." +
               std::to_string(HIWORD(fileInfo->dwFileVersionLS));
    }

    inline std::string getRemoteVersion() {
        const wchar_t host[] = L"api.github.com";
        const wchar_t path[] = L"/repos/lambor590/augfohndfjgbdajfgdnfjgadbuofidgjsdnfjgisfudhngdfgjkdfgbsjgdbj/releases/latest";
        std::string version = "0.0.0";

        HINTERNET session = WinHttpOpen(L"Half Sword Enhancer", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) return version;

        HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!connect) {
            WinHttpCloseHandle(session);
            return version;
        }

        HINTERNET request = WinHttpOpenRequest(connect, L"GET", path, NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
        if (!request) {
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return version;
        }

        if (!WinHttpSendRequest(request, NULL, 0, NULL, 0, 0, 0) ||
            !WinHttpReceiveResponse(request, NULL)) {
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return version;
        }

        std::string response;
        DWORD size;
        char buffer[2048];

        while (WinHttpQueryDataAvailable(request, &size) && size > 0) {
            DWORD downloaded;
            if (WinHttpReadData(request, buffer, size, &downloaded)) {
                response.append(buffer, downloaded);
            }
        }

            size_t assetsPos = response.find("\"assets\":");
            size_t namePos = (assetsPos != std::string::npos) ?
                response.find("\"name\":\"HS_Enhancer_Launcher.exe\"", assetsPos) : std::string::npos;
            size_t start = (namePos != std::string::npos) ?
                response.find("\"tag_name\":\"") : std::string::npos;

            if (start != std::string::npos) {
                start += 12;
                size_t end = response.find("\"", start);
            if (end != std::string::npos) {
                version = response.substr(start + 1, end - start - 1);
            }
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return version;
    }

    inline bool isUpdateAvailable(const std::string& local, const std::string& remote) {
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

    inline void createAndRunUpdateScript(const std::string& tempFileName, const std::string& currentPath) {
        char batPath[MAX_PATH];
        GetTempPathA(MAX_PATH, batPath);
        strcat_s(batPath, "update_hse.bat");

        std::ofstream batFile(batPath);

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

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        CreateProcessA(NULL, (LPSTR)("cmd.exe /c " + std::string(batPath)).c_str(),
            NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        ExitProcess(0);
    }

    inline bool downloadUpdate(const std::string& downloadUrl) {
        std::wstring wDownloadUrl(downloadUrl.begin(), downloadUrl.end());
        URL_COMPONENTS urlComp = { sizeof(URL_COMPONENTS) };
        wchar_t hostName[256] = {}, urlPath[1024] = {};
        urlComp.lpszHostName = hostName;
        urlComp.dwHostNameLength = sizeof(hostName) / sizeof(wchar_t);
        urlComp.lpszUrlPath = urlPath;
        urlComp.dwUrlPathLength = sizeof(urlPath) / sizeof(wchar_t);

        if (!WinHttpCrackUrl(wDownloadUrl.c_str(), 0, 0, &urlComp)) {
            return false;
        }

        HINTERNET hSession = WinHttpOpen(L"Half Sword Enhancer Updater",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
        if (!hSession) return false;

        HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath,
            nullptr, nullptr, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        if (!WinHttpSendRequest(hRequest, nullptr, 0, nullptr, 0, 0, 0) ||
            !WinHttpReceiveResponse(hRequest, nullptr)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        char currentPath[MAX_PATH];
        GetModuleFileNameA(NULL, currentPath, MAX_PATH);
        std::string tempFileName = std::string(currentPath) + ".update";

        HANDLE hFile = CreateFileA(tempFileName.c_str(), GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        char buffer[8192]; // Aumentado el tamaño del buffer para mejorar rendimiento
        DWORD dwSize, dwDownloaded, dwWritten;
        bool downloadSuccess = true;

        while (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
            dwSize = min(dwSize, sizeof(buffer));
            if (!WinHttpReadData(hRequest, buffer, dwSize, &dwDownloaded) ||
                !WriteFile(hFile, buffer, dwDownloaded, &dwWritten, NULL)) {
                downloadSuccess = false;
                break;
            }
        }

        CloseHandle(hFile);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        if (downloadSuccess) {
            createAndRunUpdateScript(tempFileName, currentPath);
            return true;
        }

        return false;
    }

    inline void checkForUpdates() {
        std::string localVersion = getLocalVersion();
        std::string remoteVersion = getRemoteVersion();
        
        Logger::info("Mod version: " + localVersion);
        
        if (remoteVersion == "0.0.0") {
            Logger::error("Failed to check for updates.");
            std::this_thread::sleep_for(std::chrono::seconds(3));
            return;
        }

        Logger::info("Latest public version: " + remoteVersion);

        if (!isUpdateAvailable(localVersion, remoteVersion)) {
            Logger::info("No updates available.");
            return;
        }

        Logger::info("Update available!\nDownloading...");

        std::string downloadUrl = "https://github.com/lambor590/augfohndfjgbdajfgdnfjgadbuofidgjsdnfjgisfudhngdfgjkdfgbsjgdbj/releases/download/v" +
            remoteVersion + "/HS_Enhancer_Launcher.exe";

        if (!downloadUpdate(downloadUrl)) {
            Logger::error("Error downloading update.");
        }
    }
}
