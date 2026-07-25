#pragma once

#include <expected>
#include <filesystem>
#include <cstdint>
#include <span>
#include <string_view>

namespace hse {

    enum class InstallError : std::uint8_t { FileNotFound, CopyFailed, PermissionDenied, InvalidPath };
    enum class InstallMode : std::uint8_t { Standalone, Ue4ss };
    enum class InstallArtifact : std::uint8_t { Mod, Proxy, Ue4ssBridge };

    struct InstallArtifactSpec {
        InstallArtifact artifact;
        std::string_view filename;
        std::uint32_t minimumFileSize;
    };

    /// Authoritative artifact set for downloading, installing, and completeness checks.
    [[nodiscard]] std::span<const InstallArtifactSpec, 2> GetInstallPlan(InstallMode mode) noexcept;
    [[nodiscard]] std::filesystem::path GetInstallDestination(
        const std::filesystem::path& gameBinPath, InstallArtifact artifact
    );

    [[nodiscard]] InstallMode DetectInstallMode(const std::filesystem::path& gameBinPath);
    [[nodiscard]] bool IsInstallModeAvailable(const std::filesystem::path& gameBinPath, InstallMode mode);
    [[nodiscard]] bool IsInstallationComplete(const std::filesystem::path& gameBinPath, InstallMode mode);

    [[nodiscard]] std::expected<void, InstallError> InstallFiles(
        const std::filesystem::path& sourcePath, const std::filesystem::path& gameBinPath, InstallMode mode
    );

    [[nodiscard]] std::expected<bool, InstallError> TestWritePermissions(const std::filesystem::path& gameBinPath);

}
