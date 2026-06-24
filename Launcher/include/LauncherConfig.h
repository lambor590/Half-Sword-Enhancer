#pragma once

#include <string>
#include <filesystem>
#include <expected>

#include "../../ext/SimpleIni.h"
#include "Util.h"

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
        [[nodiscard]] std::expected<void, ConfigError> SetGamePath(const std::filesystem::path& path);
        [[nodiscard]] bool HasGamePath() const;

        [[nodiscard]] GameEdition GetGameEdition() const;
        [[nodiscard]] std::expected<void, ConfigError> SetGameEdition(GameEdition edition);

        [[nodiscard]] bool GetBool(const char* section, const char* key, bool defaultValue = false) const;
        [[nodiscard]] std::string GetString(const char* section, const char* key, const char* defaultValue = "") const;

        [[nodiscard]] std::expected<void, ConfigError> SetBool(const char* section, const char* key, bool value);

        [[nodiscard]] std::expected<void, ConfigError> SetString(
            const char* section, const char* key, std::string_view value
        );

    private:
        std::filesystem::path configPath_;
        CSimpleIni ini_;
        bool isFirstRun_;

        LauncherConfig();
        ~LauncherConfig() = default;
        LauncherConfig(const LauncherConfig&) = delete;
        LauncherConfig& operator=(const LauncherConfig&) = delete;
        LauncherConfig(LauncherConfig&&) = delete;
        LauncherConfig& operator=(LauncherConfig&&) = delete;

        [[nodiscard]] std::expected<void, ConfigError> SaveConfig();
        [[nodiscard]] std::expected<void, ConfigError> LoadConfig();

        [[nodiscard]] std::expected<void, ConfigError> SaveConfigUnlocked();
        [[nodiscard]] std::expected<void, ConfigError> LoadConfigUnlocked();
        [[nodiscard]] bool HasKey(const char* section, const char* key) const;
    };

    inline std::expected<void, ConfigError> LauncherConfig::SaveConfigUnlocked() {
        const auto saveResult = ini_.SaveFile(configPath_.string().c_str());
        if (saveResult >= 0) {
            return {};
        }

        if (saveResult != SI_FILE) {
            return std::unexpected(ConfigError::InvalidFormat);
        }

        std::error_code ec;
        std::filesystem::create_directories(configPath_.parent_path(), ec);
        if (ec) {
            return std::unexpected(ConfigError::WritePermissionDenied);
        }

        const auto retryResult = ini_.SaveFile(configPath_.string().c_str());
        if (retryResult >= 0) {
            return {};
        }

        return std::unexpected(
            retryResult == SI_FILE ? ConfigError::WritePermissionDenied : ConfigError::InvalidFormat
        );
    }

    inline std::expected<void, ConfigError> LauncherConfig::LoadConfigUnlocked() {
        const auto loadResult = ini_.LoadFile(configPath_.string().c_str());
        if (loadResult >= 0) {
            return {};
        }

        return std::unexpected(loadResult == SI_FILE ? ConfigError::FileNotFound : ConfigError::InvalidFormat);
    }

    inline bool LauncherConfig::HasKey(const char* section, const char* key) const {
        return ini_.KeyExists(section, key);
    }

    inline bool LauncherConfig::GetBool(const char* section, const char* key, bool defaultValue) const {
        return ini_.GetBoolValue(section, key, defaultValue);
    }

    inline std::string LauncherConfig::GetString(const char* section, const char* key, const char* defaultValue) const {
        return std::string(ini_.GetValue(section, key, defaultValue));
    }

    inline std::expected<void, ConfigError> LauncherConfig::SetBool(const char* section, const char* key, bool value) {
        if (ini_.SetBoolValue(section, key, value) < 0) {
            return std::unexpected(ConfigError::InvalidValue);
        }
        return SaveConfigUnlocked();
    }

    inline std::expected<void, ConfigError> LauncherConfig::SetString(
        const char* section, const char* key, std::string_view value
    ) {
        const std::string valueStr(value);
        if (ini_.SetValue(section, key, valueStr.c_str()) < 0) {
            return std::unexpected(ConfigError::InvalidValue);
        }
        return SaveConfigUnlocked();
    }

}
