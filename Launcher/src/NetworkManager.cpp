#include <vector>
#include <filesystem>
#include <array>
#include <algorithm>
#include <cstdint>

#include "../include/NetworkManager.h"
#include "../include/Logger.h"

namespace hse {

    std::expected<NetworkManager::HttpConnection, NetworkError> NetworkManager::ParseUrl(const std::string& url
    ) const noexcept {
        const std::wstring wUrl(url.begin(), url.end());

        std::vector<wchar_t> hostBuffer(256);
        std::vector<wchar_t> pathBuffer(1024);

        URL_COMPONENTS urlComp{sizeof(URL_COMPONENTS)};
        urlComp.lpszHostName = hostBuffer.data();
        urlComp.dwHostNameLength = static_cast<DWORD>(hostBuffer.size());
        urlComp.lpszUrlPath = pathBuffer.data();
        urlComp.dwUrlPathLength = static_cast<DWORD>(pathBuffer.size());

        if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
            return std::unexpected(NetworkError::InvalidUrl);
        }

        return HttpConnection{
            .host = std::wstring(hostBuffer.data()), .path = std::wstring(pathBuffer.data()), .port = urlComp.nPort
        };
    }

    std::expected<WinHttpSession, NetworkError> NetworkManager::CreateSession(const HttpConnection& connection
    ) const noexcept {
        WinHttpSession session;

        session.set_session(WinHttpOpen(USER_AGENT.data(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0));

        if (!session.session()) {
            return std::unexpected(NetworkError::ConnectionFailed);
        }

        session.set_connection(WinHttpConnect(session.session(), connection.host.c_str(), connection.port, 0));

        if (!session.connection()) {
            return std::unexpected(NetworkError::ConnectionFailed);
        }

        session.set_request(WinHttpOpenRequest(
            session.connection(), L"GET", connection.path.c_str(), nullptr, nullptr, WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE
        ));

        if (!session) {
            return std::unexpected(NetworkError::RequestFailed);
        }

        return session;
    }

    std::expected<void, NetworkError> NetworkManager::SendRequest(
        HINTERNET request, int connectTimeoutMs, int receiveTimeoutMs
    ) const noexcept {
        WinHttpSetTimeouts(request, connectTimeoutMs, connectTimeoutMs, connectTimeoutMs, receiveTimeoutMs);

        if (!WinHttpSendRequest(request, nullptr, 0, nullptr, 0, 0, 0) || !WinHttpReceiveResponse(request, nullptr)) {
            return std::unexpected(NetworkError::RequestFailed);
        }

        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        if (WinHttpQueryHeaders(
                request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &statusCode, &statusCodeSize,
                nullptr
            ) &&
            statusCode != 200) {
            return std::unexpected(NetworkError::ServerError);
        }

        return {};
    }

    std::expected<DownloadResult, NetworkError> NetworkManager::DownloadFile(const DownloadConfig& config) noexcept {
        hse::Logger::info(config.description + "...");

        auto connection = ParseUrl(config.url);
        if (!connection) {
            return std::unexpected(connection.error());
        }

        auto session = CreateSession(*connection);
        if (!session) {
            return std::unexpected(session.error());
        }

        const auto timeoutMs = static_cast<int>(config.connectTimeout.count());
        const auto receiveTimeoutMs = static_cast<int>(config.receiveTimeout.count());

        auto sendResult = SendRequest(session->request(), timeoutMs, receiveTimeoutMs);
        if (!sendResult) {
            return std::unexpected(sendResult.error());
        }

        std::string actualPath = config.outputPath;
        HANDLE hFile =
            CreateFileA(actualPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (hFile == INVALID_HANDLE_VALUE && !config.fallbackPath.empty()) {
            actualPath = config.fallbackPath;
            hFile = CreateFileA(
                actualPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
            );
        }

        if (hFile == INVALID_HANDLE_VALUE) {
            return std::unexpected(NetworkError::FileCreationFailed);
        }

        struct FileGuard {
            HANDLE handle;
            ~FileGuard() {
                if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
            }
        } fileGuard{hFile};

        std::vector<char> buffer(BUFFER_SIZE);
        DWORD totalBytes = 0;
        DWORD dwSize, dwDownloaded, dwWritten;

        while (WinHttpQueryDataAvailable(session->request(), &dwSize) && dwSize > 0) {
            dwSize = (std::min)(dwSize, static_cast<DWORD>(buffer.size()));

            if (!WinHttpReadData(session->request(), buffer.data(), dwSize, &dwDownloaded)) {
                return std::unexpected(NetworkError::DownloadFailed);
            }

            if (!WriteFile(hFile, buffer.data(), dwDownloaded, &dwWritten, nullptr) || dwWritten != dwDownloaded) {
                return std::unexpected(NetworkError::DownloadFailed);
            }

            totalBytes += dwDownloaded;
        }

        if (totalBytes < config.minFileSize) {
            return std::unexpected(NetworkError::FileSizeTooSmall);
        }

        return DownloadResult{actualPath, totalBytes};
    }

    std::expected<std::string, NetworkError> NetworkManager::DownloadToString(const std::string& url) noexcept {
        auto connection = ParseUrl(url);
        if (!connection) {
            return std::unexpected(connection.error());
        }

        auto session = CreateSession(*connection);
        if (!session) {
            return std::unexpected(session.error());
        }

        auto sendResult = SendRequest(session->request());
        if (!sendResult) {
            return std::unexpected(sendResult.error());
        }

        std::string result;
        std::vector<char> buffer(BUFFER_SIZE);
        DWORD dwSize, dwDownloaded;

        while (WinHttpQueryDataAvailable(session->request(), &dwSize) && dwSize > 0) {
            dwSize = (std::min)(dwSize, static_cast<DWORD>(buffer.size()));

            if (!WinHttpReadData(session->request(), buffer.data(), dwSize, &dwDownloaded)) {
                return std::unexpected(NetworkError::DownloadFailed);
            }

            result.append(buffer.data(), dwDownloaded);
        }

        return result;
    }

}