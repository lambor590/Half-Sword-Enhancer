#pragma once

#include <string>
#include <filesystem>
#include <expected>

#include "../../ext/SimpleIni.h"
#include "GameEdition.h"

namespace hse {

    enum class ConfigError : std::uint8_t {
        FileNotFound = 1,
        InvalidFormat = 2,
        WritePermissionDenied = 3,
        InvalidValue = 4
    };

    class LauncherConfig {
    public:
        static LauncherConfig& Instance() {
            static LauncherConfig instance;
            return instance;
        }

        [[nodiscard]] bool GetCheckForUpdates() const;
        [[nodiscard]] std::expected<void, ConfigError> SetCheckForUpdates(bool enabled);

        [[nodiscard]] bool HasCheckForUpdatesSetting() const;
        [[nodiscard]] bool IsFirstRun() const noexcept { return isFirstRun_; }

        [[nodiscard]] std::filesystem::path GetGamePath() const;
        [[nodiscard]] bool HasGamePath() const;

        [[nodiscard]] GameEdition GetGameEdition() const;
        [[nodiscard]] std::expected<void, ConfigError> SetGameLocation(
            const std::filesystem::path& path, GameEdition edition
        );

        [[nodiscard]] bool GetBool(const char* section, const char* key, bool defaultValue = false) const;
        [[nodiscard]] std::string GetString(const char* section, const char* key, const char* defaultValue = "") const;

        [[nodiscard]] std::expected<void, ConfigError> SetBool(const char* section, const char* key, bool value);

        [[nodiscard]] std::expected<void, ConfigError> SetString(
            const char* section, const char* key, std::string_view value
        );

    private:
        std::filesystem::path configPath_;
        CSimpleIniA ini_;
        bool isFirstRun_;

        LauncherConfig();
        ~LauncherConfig() = default;
        LauncherConfig(const LauncherConfig&) = delete;
        LauncherConfig& operator=(const LauncherConfig&) = delete;
        LauncherConfig(LauncherConfig&&) = delete;
        LauncherConfig& operator=(LauncherConfig&&) = delete;

        [[nodiscard]] std::expected<void, ConfigError> SaveConfig();
        [[nodiscard]] std::expected<void, ConfigError> LoadConfig();

        [[nodiscard]] bool HasKey(const char* section, const char* key) const;
    };
}
