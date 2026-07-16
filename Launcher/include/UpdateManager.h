#pragma once

#include <string>
#include <string_view>
#include <expected>
#include <optional>
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
        std::optional<UpdateInfo> stableRelease;

        bool modUpdateAvailable = false;
        bool launcherUpdateAvailable = false;
        std::string modTimestamp;
        std::string launcherTimestamp;
        std::string downloadUrlMod;
        std::string downloadUrlProxy;
        std::string downloadUrlBridge;
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
        [[nodiscard]] static std::expected<void, UpdateError> DownloadModToPath(
            std::string_view downloadUrl, const std::filesystem::path& outputPath, std::uint32_t minFileSize = 300000
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
        [[nodiscard]] static std::expected<void, UpdateError> DownloadToTempAndInstall(
            std::string_view modUrl, std::string_view proxyUrl, std::string_view bridgeUrl,
            const std::filesystem::path& gameBinPath, InstallMode installMode, std::uint32_t modMinSize = 300000
        );
        [[nodiscard]] static std::expected<Version, UpdateError> ExtractVersionFromFile(
            const std::filesystem::path& filePath
        );
        [[nodiscard]] static std::expected<std::string, UpdateError> ParseJsonStringField(
            std::string_view json, std::string_view fieldName
        );
        [[nodiscard]] static std::expected<Version, UpdateError> ParseVersionFromJson(std::string_view json);

#ifdef EXPERIMENTAL_VERSION
        struct ExperimentalAssets {
            std::string modTimestamp;
            std::string launcherTimestamp;
            std::string modUrl;
            std::string proxyUrl;
            std::string bridgeUrl;
            std::string launcherUrl;
        };

        [[nodiscard]] static std::expected<ExperimentalAssets, UpdateError> ParseExperimentalAssets(
            std::string_view json
        );
#endif
    };

}
