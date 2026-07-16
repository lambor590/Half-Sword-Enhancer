#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
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
            WinHttpSession() = default;

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

            WinHttpSession& operator=(WinHttpSession&&) = delete;

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

            explicit FileHandle(HANDLE fileHandle) noexcept : handle(fileHandle) {}
        };

        template <typename Sink>
        [[nodiscard]] std::expected<std::uint32_t, NetworkError> StreamResponse(
            HINTERNET request, std::array<char, BUFFER_SIZE>& buffer, Sink&& sink
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

        [[nodiscard]] std::expected<HttpConnection, NetworkError> ParseUrl(std::string_view url) {
            const std::wstring wUrl(url.begin(), url.end());

            std::array<wchar_t, 256> hostBuffer{};
            std::array<wchar_t, 2'048> pathBuffer{};
            std::array<wchar_t, 1'024> extraBuffer{};

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
            if (!WinHttpQueryHeaders(
                    request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &statusCode,
                    &statusCodeSize, nullptr
                ) ||
                statusCode != 200) {
                return std::unexpected(NetworkError::ServerError);
            }

            return {};
        }

        [[nodiscard]] std::expected<WinHttpSession, NetworkError> OpenRequest(
            std::string_view url, std::chrono::milliseconds connectTimeout = std::chrono::milliseconds{5000},
            std::chrono::milliseconds receiveTimeout = std::chrono::milliseconds{15000}
        ) {
            auto connection = ParseUrl(url);
            if (!connection) return std::unexpected(connection.error());
            auto session = CreateSession(*connection);
            if (!session) return std::unexpected(session.error());
            if (auto sent = SendRequest(session->requestHandle, connectTimeout, receiveTimeout); !sent)
                return std::unexpected(sent.error());
            return session;
        }

    }

    std::expected<void, NetworkError> DownloadFile(const DownloadConfig& config) {
        hse::Logger::info("%s...", config.description.c_str());

        auto session = OpenRequest(config.url, config.connectTimeout, config.receiveTimeout);
        if (!session) return std::unexpected(session.error());

        FileHandle fileHandle(CreateFileW(
            config.outputPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
        ));
        if (fileHandle.handle == INVALID_HANDLE_VALUE)
            return std::unexpected(NetworkError::FileCreationFailed);

        std::array<char, BUFFER_SIZE> buffer{};
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

    std::expected<std::string, NetworkError> DownloadToString(std::string_view url) {
        auto session = OpenRequest(url);
        if (!session) return std::unexpected(session.error());

        std::string result;
        result.reserve(BUFFER_SIZE);
        std::array<char, BUFFER_SIZE> buffer{};
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
