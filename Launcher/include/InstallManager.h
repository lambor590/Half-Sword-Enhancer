#pragma once

#include <expected>
#include <filesystem>
#include <cstdint>

namespace hse {

    enum class InstallError : std::uint8_t { FileNotFound, CopyFailed, PermissionDenied, InvalidPath };

    struct InstallStatus {
        bool proxyInstalled = false;
        bool modInstalled = false;
    };

    [[nodiscard]] InstallStatus CheckInstallation(const std::filesystem::path& gameBinPath) noexcept;

    [[nodiscard]] std::expected<void, InstallError> InstallFiles(
        const std::filesystem::path& sourcePath, const std::filesystem::path& gameBinPath
    ) noexcept;

    [[nodiscard]] std::expected<bool, InstallError> TestWritePermissions(const std::filesystem::path& gameBinPath
    ) noexcept;

}
