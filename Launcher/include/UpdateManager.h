#pragma once

#include <string>
#include <expected>
#include <optional>
#include <compare>
#include <mutex>

namespace hse {

    enum class UpdateError : std::uint8_t {
        VersionParsingFailed = 1,
        NetworkError = 2,
        FileSystemError = 3,
        InvalidResponse = 4,
        UpdateFailed = 5
    };

    class Version {
    public:
        constexpr Version() noexcept = default;
        explicit Version(std::string_view versionString) noexcept;

        constexpr auto operator<=>(const Version&) const noexcept = default;
        constexpr bool operator==(const Version&) const noexcept = default;

        [[nodiscard]] std::string ToString() const;
        [[nodiscard]] constexpr bool IsValid() const noexcept {
            return major_ > 0 || minor_ > 0 || patch_ > 0;
        }

        [[nodiscard]] constexpr std::uint16_t major() const noexcept { return major_; }
        [[nodiscard]] constexpr std::uint16_t minor() const noexcept { return minor_; }
        [[nodiscard]] constexpr std::uint16_t patch() const noexcept { return patch_; }

    private:
        std::uint16_t major_ = 0;
        std::uint16_t minor_ = 0;
        std::uint16_t patch_ = 0;
    };

    struct UpdateInfo {
        bool available = false;
        Version currentVersion;
        Version remoteVersion;
        std::string downloadUrlLauncher;
        std::string downloadUrlMod;
    };

#ifdef DEV_VERSION
    struct DevUpdateInfo {
        std::optional<UpdateInfo> stableRelease;

        bool modUpdateAvailable = false;
        bool launcherUpdateAvailable = false;
        std::string modTimestamp;
        std::string launcherTimestamp;
        std::string downloadUrlMod;
        std::string downloadUrlLauncher;
    };
#endif

    class UpdateManager {
    public:
        static UpdateManager& Instance() noexcept {
            static UpdateManager instance;
            return instance;
        }

        [[nodiscard]] std::expected<Version, UpdateError> GetLocalVersion() noexcept;
        [[nodiscard]] std::expected<UpdateInfo, UpdateError> CheckForUpdates() noexcept;
        [[nodiscard]] std::expected<void, UpdateError> UpdateMod(const Version& version) noexcept;
        [[nodiscard]] std::expected<void, UpdateError> UpdateLauncher(
            std::string_view downloadUrl,
            std::string_view timestamp = {}
        ) noexcept;

#ifdef DEV_VERSION
        [[nodiscard]] std::expected<DevUpdateInfo, UpdateError> CheckForDevUpdates() noexcept;
        [[nodiscard]] std::expected<void, UpdateError> UpdateDevMod(
            std::string_view downloadUrl,
            std::string_view timestamp
        ) noexcept;
#endif

    private:
        static constexpr std::string_view GITHUB_API_URL =
            "https://api.github.com/repos/lambor590/Half-Sword-Enhancer/releases/latest";

#ifdef DEV_VERSION
        static constexpr std::string_view GITHUB_DEV_API_URL =
            "https://api.github.com/repos/lambor590/Half-Sword-Enhancer/releases/tags/dev-latest";
#endif

        mutable std::mutex mutex_;
        std::optional<Version> cachedLocalVersion_;

        UpdateManager() = default;
        ~UpdateManager() = default;
        UpdateManager(const UpdateManager&) = delete;
        UpdateManager& operator=(const UpdateManager&) = delete;
        UpdateManager(UpdateManager&&) = delete;
        UpdateManager& operator=(UpdateManager&&) = delete;

        [[nodiscard]] std::expected<Version, UpdateError> ExtractVersionFromExecutable() const noexcept;
        [[nodiscard]] std::expected<std::string, UpdateError> FetchGitHubReleaseInfo() const noexcept;
        [[nodiscard]] std::expected<Version, UpdateError> ParseVersionFromJson(std::string_view json) const noexcept;

#ifdef DEV_VERSION
        [[nodiscard]] std::expected<std::string_view, UpdateError> ExtractAssetObject(
            std::string_view json,
            std::string_view assetName
        ) const noexcept;

        [[nodiscard]] std::expected<std::string, UpdateError> ParseAssetField(
            std::string_view assetObject,
            std::string_view fieldName
        ) const noexcept;
#endif
    };

}