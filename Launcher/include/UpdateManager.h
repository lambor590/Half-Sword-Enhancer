#pragma once

#include <string>
#include <string_view>
#include <expected>
#include <filesystem>
#include <cstdint>

#include "Version.h"

namespace hse {

    enum class InstallMode : std::uint8_t;

    enum class UpdateError : std::uint8_t {
        VersionParsingFailed = 1,
        NetworkError = 2,
        FileSystemError = 3,
        InvalidResponse = 4,
        UpdateFailed = 5
    };

    struct UpdateInfo {
        bool available = false;
        Version currentVersion;
        Version remoteVersion;
        std::string downloadUrlLauncher;
    };

#ifdef EXPERIMENTAL_VERSION
    struct ExperimentalUpdateInfo {
        bool packageUpdateAvailable = false;
        bool launcherUpdateAvailable = false;
        std::string packageTimestamp;
        std::string launcherTimestamp;
        std::string downloadUrlPackage;
        std::string downloadUrlLauncher;
    };
#endif

    class UpdateManager {
    public:
        [[nodiscard]] static std::expected<Version, UpdateError> GetLocalVersion();
        [[nodiscard]] static std::expected<UpdateInfo, UpdateError> CheckForUpdates();
        [[nodiscard]] static std::expected<Version, UpdateError> GetInstalledModVersion(
            const std::filesystem::path& gameBinPath
        );
        [[nodiscard]] static std::expected<void, UpdateError> DownloadAndInstallMod(
            const Version& version, const std::filesystem::path& gameBinPath, InstallMode installMode
        );
        [[nodiscard]] static std::expected<void, UpdateError> UpdateLauncher(
            std::string_view downloadUrl, const Version& expectedVersion, std::string_view timestamp = {}
        );

#ifdef EXPERIMENTAL_VERSION
        [[nodiscard]] static std::expected<ExperimentalUpdateInfo, UpdateError> CheckForExperimentalUpdates();
        [[nodiscard]] static std::expected<void, UpdateError> DownloadAndInstallExperimentalMod(
            const ExperimentalUpdateInfo& info, const std::filesystem::path& gameBinPath, InstallMode installMode
        );
#endif

    private:
        static constexpr std::string_view GITHUB_API_URL =
            "https://api.github.com/repos/lambor590/Half-Sword-Enhancer/releases/latest";
#ifdef EXPERIMENTAL_VERSION
        static constexpr std::string_view GITHUB_EXPERIMENTAL_API_URL =
            "https://api.github.com/repos/lambor590/Half-Sword-Enhancer/releases/tags/experimental-latest";
#endif

        [[nodiscard]] static std::string BuildReleaseUrl(std::string_view version, std::string_view filename);
        [[nodiscard]] static std::expected<void, UpdateError> DownloadPackageAndInstall(
            std::string_view packageUrl, const std::filesystem::path& gameBinPath, InstallMode installMode
        );
        [[nodiscard]] static std::expected<Version, UpdateError> ExtractVersionFromFile(
            const std::filesystem::path& filePath
        );
        [[nodiscard]] static std::expected<std::string, UpdateError> ParseJsonStringField(
            std::string_view json, std::string_view fieldName
        );
        [[nodiscard]] static std::expected<Version, UpdateError> ParseVersionFromJson(std::string_view json);
    };

}
