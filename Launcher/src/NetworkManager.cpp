#include <vector>
#include <filesystem>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <Windows.h>
#include <winhttp.h>

#include "../include/NetworkManager.h"
#include "../include/Logger.h"

namespace hse {
    namespace {
        constexpr wchar_t USER_AGENT[] = L"Half Sword Enhancer Updater";
        constexpr std::size_t BUFFER_SIZE = 16384;

        struct HttpConnection {
            std::wstring host;
            std::wstring path;
            INTERNET_PORT port;
            bool secure = true;
        };

        struct WinHttpSession {
            explicit WinHttpSession(
                HINTERNET session = nullptr, HINTERNET connection = nullptr, HINTERNET request = nullptr
            ) noexcept
                : sessionHandle(session), connectionHandle(connection), requestHandle(request) {}

            ~WinHttpSession() noexcept {
                if (requestHandle) WinHttpCloseHandle(requestHandle);
                if (connectionHandle) WinHttpCloseHandle(connectionHandle);
                if (sessionHandle) WinHttpCloseHandle(sessionHandle);
            }

            WinHttpSession(const WinHttpSession&) = delete;
            WinHttpSession& operator=(const WinHttpSession&) = delete;

            WinHttpSession(WinHttpSession&& other) noexcept
                : sessionHandle(std::exchange(other.sessionHandle, nullptr)),
                  connectionHandle(std::exchange(other.connectionHandle, nullptr)),
                  requestHandle(std::exchange(other.requestHandle, nullptr)) {}

            WinHttpSession& operator=(WinHttpSession&& other) noexcept {
                if (this != &other) {
                    if (requestHandle) WinHttpCloseHandle(requestHandle);
                    if (connectionHandle) WinHttpCloseHandle(connectionHandle);
                    if (sessionHandle) WinHttpCloseHandle(sessionHandle);

                    sessionHandle = std::exchange(other.sessionHandle, nullptr);
                    connectionHandle = std::exchange(other.connectionHandle, nullptr);
                    requestHandle = std::exchange(other.requestHandle, nullptr);
                }
                return *this;
            }

            [[nodiscard]] explicit operator bool() const noexcept {
                return sessionHandle && connectionHandle && requestHandle;
            }

            HINTERNET sessionHandle = nullptr;
            HINTERNET connectionHandle = nullptr;
            HINTERNET requestHandle = nullptr;
        };

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

        [[nodiscard]] std::expected<FileHandle, NetworkError> OpenDownloadFile(const DownloadConfig& config) noexcept {
            auto handle = CreateFileW(
                config.outputPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
            );
            if (handle != INVALID_HANDLE_VALUE) {
                return FileHandle(handle);
            }

            return std::unexpected(NetworkError::FileCreationFailed);
        }

        template <typename Sink>
        [[nodiscard]] std::expected<std::uint32_t, NetworkError> StreamResponse(
            HINTERNET request, std::vector<char>& buffer, Sink&& sink
        ) {
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

        [[nodiscard]] std::expected<HttpConnection, NetworkError> ParseUrl(const std::string& url) {
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
                .secure = (urlComp.nScheme == INTERNET_SCHEME_HTTPS)
            };
        }

        [[nodiscard]] std::expected<WinHttpSession, NetworkError> CreateSession(
            const HttpConnection& connection
        ) noexcept {
            WinHttpSession session;

            session.sessionHandle = WinHttpOpen(USER_AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);

            if (!session.sessionHandle) {
                return std::unexpected(NetworkError::ConnectionFailed);
            }

            session.connectionHandle =
                WinHttpConnect(session.sessionHandle, connection.host.c_str(), connection.port, 0);

            if (!session.connectionHandle) {
                return std::unexpected(NetworkError::ConnectionFailed);
            }

            session.requestHandle = WinHttpOpenRequest(
                session.connectionHandle, L"GET", connection.path.c_str(), nullptr, nullptr,
                WINHTTP_DEFAULT_ACCEPT_TYPES, connection.secure ? WINHTTP_FLAG_SECURE : 0
            );

            if (!session) {
                return std::unexpected(NetworkError::RequestFailed);
            }

            return session;
        }

        [[nodiscard]] std::expected<void, NetworkError> SendRequest(
            HINTERNET request, std::chrono::milliseconds connectTimeout = std::chrono::milliseconds{5000},
            std::chrono::milliseconds receiveTimeout = std::chrono::milliseconds{15000}
        ) noexcept {
            const auto connectTimeoutMs = static_cast<int>(connectTimeout.count());
            const auto receiveTimeoutMs = static_cast<int>(receiveTimeout.count());
            WinHttpSetTimeouts(request, connectTimeoutMs, connectTimeoutMs, connectTimeoutMs, receiveTimeoutMs);

            if (!WinHttpSendRequest(request, nullptr, 0, nullptr, 0, 0, 0) ||
                !WinHttpReceiveResponse(request, nullptr)) {
                return std::unexpected(NetworkError::RequestFailed);
            }

            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);
            if (WinHttpQueryHeaders(
                    request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &statusCode,
                    &statusCodeSize, nullptr
                ) &&
                statusCode != 200) {
                return std::unexpected(NetworkError::ServerError);
            }

            return {};
        }

    }

    std::expected<void, NetworkError> DownloadFile(const DownloadConfig& config) {
        hse::Logger::info(config.description + "...");

        auto connection = ParseUrl(config.url);
        if (!connection) {
            return std::unexpected(connection.error());
        }

        auto session = CreateSession(*connection);
        if (!session) {
            return std::unexpected(session.error());
        }

        auto sendResult = SendRequest(session->requestHandle, config.connectTimeout, config.receiveTimeout);
        if (!sendResult) {
            return std::unexpected(sendResult.error());
        }

        auto fileResult = OpenDownloadFile(config);
        if (!fileResult) {
            return std::unexpected(fileResult.error());
        }

        auto fileHandle = std::move(*fileResult);

        std::vector<char> buffer(BUFFER_SIZE);
        auto totalBytes = StreamResponse(
            session->requestHandle, buffer,
            [&fileHandle](const char* data, DWORD size) noexcept -> std::expected<void, NetworkError> {
                DWORD bytesWritten = 0;
                if (!WriteFile(fileHandle.handle, data, size, &bytesWritten, nullptr) || bytesWritten != size) {
                    return std::unexpected(NetworkError::DownloadFailed);
                }
                return std::expected<void, NetworkError>{};
            }
        );
        if (!totalBytes) {
            return std::unexpected(totalBytes.error());
        }

        if (*totalBytes < config.minFileSize) {
            return std::unexpected(NetworkError::FileSizeTooSmall);
        }

        return {};
    }

    std::expected<std::string, NetworkError> DownloadToString(const std::string& url) {
        auto connection = ParseUrl(url);
        if (!connection) {
            return std::unexpected(connection.error());
        }

        auto session = CreateSession(*connection);
        if (!session) {
            return std::unexpected(session.error());
        }

        auto sendResult = SendRequest(session->requestHandle);
        if (!sendResult) {
            return std::unexpected(sendResult.error());
        }

        std::string result;
        std::vector<char> buffer(BUFFER_SIZE);
        auto downloadResult = StreamResponse(session->requestHandle, buffer, [&result](const char* data, DWORD size) {
            result.append(data, size);
            return std::expected<void, NetworkError>{};
        });
        if (!downloadResult) {
            return std::unexpected(downloadResult.error());
        }

        return result;
    }

}
