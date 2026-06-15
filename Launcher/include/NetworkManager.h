#pragma once

#include <string>
#include <expected>
#include <chrono>
#include <filesystem>

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
        std::filesystem::path outputPath;
        std::string description;
        std::chrono::milliseconds connectTimeout{5000};
        std::chrono::milliseconds receiveTimeout{15000};
        std::uint32_t minFileSize = 0;
    };

    [[nodiscard]] std::expected<void, NetworkError> DownloadFile(const DownloadConfig& config) noexcept;
    [[nodiscard]] std::expected<std::string, NetworkError> DownloadToString(const std::string& url) noexcept;

}
