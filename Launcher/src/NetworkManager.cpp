#include <vector>
#include <filesystem>
#include <array>
#include <algorithm>
#include <cstdint>

#include "../include/NetworkManager.h"
#include "../include/Logger.h"

namespace hse {
    namespace {

        struct FileHandle {
            HANDLE handle = INVALID_HANDLE_VALUE;

            ~FileHandle() {
                if (handle != INVALID_HANDLE_VALUE) {
                    CloseHandle(handle);
                }
            }

            FileHandle(const FileHandle&) = delete;
            FileHandle& operator=(const FileHandle&) = delete;

            FileHandle() = default;
            explicit FileHandle(HANDLE fileHandle) noexcept : handle(fileHandle) {}

            FileHandle(FileHandle&& other) noexcept : handle(std::exchange(other.handle, INVALID_HANDLE_VALUE)) {}

            FileHandle& operator=(FileHandle&& other) noexcept {
                if (this != &other) {
                    if (handle != INVALID_HANDLE_VALUE) {
                        CloseHandle(handle);
                    }
                    handle = std::exchange(other.handle, INVALID_HANDLE_VALUE);
                }
                return *this;
            }
        };

        [[nodiscard]] std::expected<std::pair<FileHandle, std::filesystem::path>, NetworkError> OpenDownloadFile(
            const DownloadConfig& config
        ) noexcept {
            auto openFile = [](const std::filesystem::path& path) noexcept {
                return CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            };

            auto handle = openFile(config.outputPath);
            if (handle != INVALID_HANDLE_VALUE) {
                return std::pair<FileHandle, std::filesystem::path>{FileHandle(handle), config.outputPath};
            }

            if (!config.fallbackPath.empty()) {
                handle = openFile(config.fallbackPath);
                if (handle != INVALID_HANDLE_VALUE) {
                    return std::pair<FileHandle, std::filesystem::path>{FileHandle(handle), config.fallbackPath};
                }
            }

            return std::unexpected(NetworkError::FileCreationFailed);
        }

        template <typename Sink>
        [[nodiscard]] std::expected<std::uint32_t, NetworkError> StreamResponse(
            HINTERNET request, std::vector<char>& buffer, Sink&& sink
        ) noexcept {
            std::uint32_t totalBytes = 0;

            while (true) {
                DWORD availableBytes = 0;
                if (!WinHttpQueryDataAvailable(request, &availableBytes)) {
                    return std::unexpected(NetworkError::DownloadFailed);
                }

                if (availableBytes == 0) {
                    return totalBytes;
                }

                const DWORD bytesToRead = (std::min)(availableBytes, static_cast<DWORD>(buffer.size()));
                DWORD bytesRead = 0;

                if (!WinHttpReadData(request, buffer.data(), bytesToRead, &bytesRead)) {
                    return std::unexpected(NetworkError::DownloadFailed);
                }

                if (bytesRead == 0) {
                    continue;
                }

                if (auto sinkResult = sink(buffer.data(), bytesRead); !sinkResult) {
                    return std::unexpected(sinkResult.error());
                }

                totalBytes += bytesRead;
            }
        }

    }

    std::expected<NetworkManager::HttpConnection, NetworkError> NetworkManager::ParseUrl(const std::string& url
    ) const noexcept {
        const std::wstring wUrl(url.begin(), url.end());

        std::vector<wchar_t> hostBuffer(256);
        std::vector<wchar_t> pathBuffer(1024);
        std::vector<wchar_t> extraBuffer(1024);

        URL_COMPONENTS urlComp{sizeof(URL_COMPONENTS)};
        urlComp.lpszHostName = hostBuffer.data();
        urlComp.dwHostNameLength = static_cast<DWORD>(hostBuffer.size());
        urlComp.lpszUrlPath = pathBuffer.data();
        urlComp.dwUrlPathLength = static_cast<DWORD>(pathBuffer.size());
        urlComp.lpszExtraInfo = extraBuffer.data();
        urlComp.dwExtraInfoLength = static_cast<DWORD>(extraBuffer.size());

        if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
            return std::unexpected(NetworkError::InvalidUrl);
        }

        std::wstring path(pathBuffer.data(), urlComp.dwUrlPathLength);
        if (urlComp.dwExtraInfoLength > 0) {
            path.append(extraBuffer.data(), urlComp.dwExtraInfoLength);
        }

        return HttpConnection{
            .host = std::wstring(hostBuffer.data(), urlComp.dwHostNameLength),
            .path = std::move(path),
            .port = urlComp.nPort,
            .secure = (urlComp.nScheme == INTERNET_SCHEME_HTTPS)};
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
            connection.secure ? WINHTTP_FLAG_SECURE : 0
        ));

        if (!session) {
            return std::unexpected(NetworkError::RequestFailed);
        }

        return session;
    }

    std::expected<void, NetworkError> NetworkManager::SendRequest(
        HINTERNET request, std::chrono::milliseconds connectTimeout, std::chrono::milliseconds receiveTimeout
    ) const noexcept {
        const auto connectTimeoutMs = static_cast<int>(connectTimeout.count());
        const auto receiveTimeoutMs = static_cast<int>(receiveTimeout.count());
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

        auto sendResult = SendRequest(session->request(), config.connectTimeout, config.receiveTimeout);
        if (!sendResult) {
            return std::unexpected(sendResult.error());
        }

        auto fileResult = OpenDownloadFile(config);
        if (!fileResult) {
            return std::unexpected(fileResult.error());
        }

        auto [fileHandle, actualPath] = std::move(*fileResult);

        std::vector<char> buffer(BUFFER_SIZE);
        auto totalBytes = StreamResponse(
            session->request(), buffer,
            [&fileHandle](const char* data, DWORD size) noexcept -> std::expected<void, NetworkError> {
            DWORD bytesWritten = 0;
            if (!WriteFile(fileHandle.handle, data, size, &bytesWritten, nullptr) || bytesWritten != size) {
                return std::unexpected(NetworkError::DownloadFailed);
            }
            return std::expected<void, NetworkError>{};
        });
        if (!totalBytes) {
            return std::unexpected(totalBytes.error());
        }

        if (*totalBytes < config.minFileSize) {
            return std::unexpected(NetworkError::FileSizeTooSmall);
        }

        return DownloadResult{std::move(actualPath), *totalBytes};
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
        auto downloadResult = StreamResponse(session->request(), buffer, [&result](const char* data, DWORD size) noexcept {
            result.append(data, size);
            return std::expected<void, NetworkError>{};
        });
        if (!downloadResult) {
            return std::unexpected(downloadResult.error());
        }

        return result;
    }

}
