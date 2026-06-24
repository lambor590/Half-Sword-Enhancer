#pragma once

#include <expected>
#include <filesystem>
#include <cstdint>

namespace hse {

    enum class InstallError : std::uint8_t { FileNotFound, CopyFailed, PermissionDenied, InvalidPath };
    enum class InstallMode : std::uint8_t { Standalone, Ue4ss };

    [[nodiscard]] InstallMode DetectInstallMode(const std::filesystem::path& gameBinPath);
    [[nodiscard]] bool IsInstallationComplete(
        const std::filesystem::path& gameBinPath, InstallMode mode
    );

    [[nodiscard]] std::expected<void, InstallError> InstallFiles(
        const std::filesystem::path& sourcePath, const std::filesystem::path& gameBinPath, InstallMode mode
    );

    [[nodiscard]] std::expected<bool, InstallError> TestWritePermissions(const std::filesystem::path& gameBinPath);

}
