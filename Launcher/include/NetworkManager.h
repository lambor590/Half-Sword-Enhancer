#pragma once

#include <string>
#include <memory>
#include <expected>
#include <chrono>
#include <array>
#include <Windows.h>
#include <winhttp.h>

namespace hse {

    enum class NetworkError : std::uint8_t {
        InvalidUrl = 1,
        ConnectionFailed = 2,
        RequestFailed = 3,
        ServerError = 4,
        FileCreationFailed = 5,
        FileSizeTooSmall = 6,
        DownloadFailed = 7
    };

    struct DownloadConfig {
        std::string url;
        std::string outputPath;
        std::string description;
        std::string fallbackPath;
        std::chrono::milliseconds connectTimeout{5000};
        std::chrono::milliseconds receiveTimeout{15000};
        std::uint32_t minFileSize = 0;
    };

    struct DownloadResult {
        std::string finalPath;
        std::uint32_t totalBytes = 0;
    };

    class WinHttpSession {
    public:
        explicit WinHttpSession(
            HINTERNET session = nullptr, HINTERNET connection = nullptr, HINTERNET request = nullptr
        ) noexcept
            : session_(session), connection_(connection), request_(request) {}

        ~WinHttpSession() noexcept {
            if (request_) WinHttpCloseHandle(request_);
            if (connection_) WinHttpCloseHandle(connection_);
            if (session_) WinHttpCloseHandle(session_);
        }

        WinHttpSession(const WinHttpSession&) = delete;
        WinHttpSession& operator=(const WinHttpSession&) = delete;

        WinHttpSession(WinHttpSession&& other) noexcept
            : session_(std::exchange(other.session_, nullptr)),
              connection_(std::exchange(other.connection_, nullptr)),
              request_(std::exchange(other.request_, nullptr)) {}

        WinHttpSession& operator=(WinHttpSession&& other) noexcept {
            if (this != &other) {
                if (request_) WinHttpCloseHandle(request_);
                if (connection_) WinHttpCloseHandle(connection_);
                if (session_) WinHttpCloseHandle(session_);

                session_ = std::exchange(other.session_, nullptr);
                connection_ = std::exchange(other.connection_, nullptr);
                request_ = std::exchange(other.request_, nullptr);
            }
            return *this;
        }

        [[nodiscard]] HINTERNET session() const noexcept { return session_; }
        [[nodiscard]] HINTERNET connection() const noexcept { return connection_; }
        [[nodiscard]] HINTERNET request() const noexcept { return request_; }

        void set_session(HINTERNET handle) noexcept { session_ = handle; }
        void set_connection(HINTERNET handle) noexcept { connection_ = handle; }
        void set_request(HINTERNET handle) noexcept { request_ = handle; }

        [[nodiscard]] explicit operator bool() const noexcept { return session_ && connection_ && request_; }

    private:
        HINTERNET session_;
        HINTERNET connection_;
        HINTERNET request_;
    };

    class NetworkManager {
    public:
        static NetworkManager& Instance() noexcept {
            static NetworkManager instance;
            return instance;
        }

        [[nodiscard]] std::expected<DownloadResult, NetworkError> DownloadFile(const DownloadConfig& config) noexcept;
        [[nodiscard]] std::expected<std::string, NetworkError> DownloadToString(const std::string& url) noexcept;

    private:
        static constexpr std::wstring_view USER_AGENT = L"Half Sword Enhancer Updater";
        static constexpr std::size_t BUFFER_SIZE = 16384;

        struct HttpConnection {
            std::wstring host;
            std::wstring path;
            INTERNET_PORT port;
        };

        [[nodiscard]] std::expected<HttpConnection, NetworkError> ParseUrl(const std::string& url) const noexcept;
        [[nodiscard]] std::expected<WinHttpSession, NetworkError> CreateSession(const HttpConnection& connection
        ) const noexcept;
        [[nodiscard]] std::expected<void, NetworkError> SendRequest(
            HINTERNET request, int connectTimeoutMs = 5000, int receiveTimeoutMs = 15000
        ) const noexcept;

        NetworkManager() = default;
        ~NetworkManager() = default;
        NetworkManager(const NetworkManager&) = delete;
        NetworkManager& operator=(const NetworkManager&) = delete;
        NetworkManager(NetworkManager&&) = delete;
        NetworkManager& operator=(NetworkManager&&) = delete;
    };

}
