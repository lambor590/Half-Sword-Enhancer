#pragma once

#include <string>
#include <filesystem>
#include <expected>
#include <mutex>

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
        static LauncherConfig& Instance() noexcept {
            static LauncherConfig instance;
            return instance;
        }

        [[nodiscard]] bool GetCheckForUpdates() const noexcept;
        [[nodiscard]] std::expected<void, ConfigError> SetCheckForUpdates(bool enabled) noexcept;

        [[nodiscard]] bool HasCheckForUpdatesSetting() const noexcept;
        [[nodiscard]] bool IsFirstRun() const noexcept { return isFirstRun_; }

        [[nodiscard]] std::filesystem::path GetGamePath() const noexcept;
        [[nodiscard]] std::expected<void, ConfigError> SetGamePath(const std::filesystem::path& path) noexcept;
        [[nodiscard]] bool HasGamePath() const noexcept;

        [[nodiscard]] GameEdition GetGameEdition() const noexcept;
        [[nodiscard]] std::expected<void, ConfigError> SetGameEdition(GameEdition edition) noexcept;

        [[nodiscard]] std::expected<bool, ConfigError> GetBool(
            std::string_view section, std::string_view key, bool defaultValue = false
        ) const noexcept;

        [[nodiscard]] std::expected<int, ConfigError> GetInt(
            std::string_view section, std::string_view key, int defaultValue = 0
        ) const noexcept;

        [[nodiscard]] std::expected<std::string, ConfigError> GetString(
            std::string_view section, std::string_view key, std::string_view defaultValue = ""
        ) const noexcept;

        [[nodiscard]] std::expected<void, ConfigError> SetBool(
            std::string_view section, std::string_view key, bool value
        ) noexcept;

        [[nodiscard]] std::expected<void, ConfigError> SetInt(
            std::string_view section, std::string_view key, int value
        ) noexcept;

        [[nodiscard]] std::expected<void, ConfigError> SetString(
            std::string_view section, std::string_view key, std::string_view value
        ) noexcept;

    private:
        mutable std::mutex mutex_;
        std::filesystem::path configPath_;
        CSimpleIni ini_;
        bool isFirstRun_;

        LauncherConfig();
        ~LauncherConfig() = default;
        LauncherConfig(const LauncherConfig&) = delete;
        LauncherConfig& operator=(const LauncherConfig&) = delete;
        LauncherConfig(LauncherConfig&&) = delete;
        LauncherConfig& operator=(LauncherConfig&&) = delete;

        [[nodiscard]] std::expected<void, ConfigError> SaveConfig() noexcept;
        [[nodiscard]] std::expected<void, ConfigError> LoadConfig() noexcept;

        [[nodiscard]] std::expected<void, ConfigError> SaveConfigUnlocked() noexcept;
        [[nodiscard]] std::expected<void, ConfigError> LoadConfigUnlocked() noexcept;
        [[nodiscard]] bool HasKey(std::string_view section, std::string_view key) const noexcept;
    };

    inline std::expected<void, ConfigError> LauncherConfig::SaveConfigUnlocked() noexcept {
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

        return std::unexpected(retryResult == SI_FILE ? ConfigError::WritePermissionDenied
                                                      : ConfigError::InvalidFormat);
    }

    inline std::expected<void, ConfigError> LauncherConfig::LoadConfigUnlocked() noexcept {
        const auto loadResult = ini_.LoadFile(configPath_.string().c_str());
        if (loadResult >= 0) {
            return {};
        }

        return std::unexpected(loadResult == SI_FILE ? ConfigError::FileNotFound : ConfigError::InvalidFormat);
    }

    inline bool LauncherConfig::HasKey(std::string_view section, std::string_view key) const noexcept {
        std::lock_guard lock(mutex_);
        return ini_.KeyExists(section.data(), key.data());
    }

    inline std::expected<bool, ConfigError> LauncherConfig::GetBool(
        std::string_view section, std::string_view key, bool defaultValue
    ) const noexcept {
        std::lock_guard lock(mutex_);
        return ini_.GetBoolValue(section.data(), key.data(), defaultValue);
    }

    inline std::expected<int, ConfigError> LauncherConfig::GetInt(
        std::string_view section, std::string_view key, int defaultValue
    ) const noexcept {
        std::lock_guard lock(mutex_);
        return static_cast<int>(ini_.GetLongValue(section.data(), key.data(), defaultValue));
    }

    inline std::expected<std::string, ConfigError> LauncherConfig::GetString(
        std::string_view section, std::string_view key, std::string_view defaultValue
    ) const noexcept {
        std::lock_guard lock(mutex_);
        std::string default_str(defaultValue);
        return std::string(ini_.GetValue(section.data(), key.data(), default_str.c_str()));
    }

    inline std::expected<void, ConfigError> LauncherConfig::SetBool(
        std::string_view section, std::string_view key, bool value
    ) noexcept {
        std::lock_guard lock(mutex_);
        if (ini_.SetBoolValue(section.data(), key.data(), value) < 0) {
            return std::unexpected(ConfigError::InvalidValue);
        }
        return SaveConfigUnlocked();
    }

    inline std::expected<void, ConfigError> LauncherConfig::SetInt(
        std::string_view section, std::string_view key, int value
    ) noexcept {
        std::lock_guard lock(mutex_);
        if (ini_.SetLongValue(section.data(), key.data(), value) < 0) {
            return std::unexpected(ConfigError::InvalidValue);
        }
        return SaveConfigUnlocked();
    }

    inline std::expected<void, ConfigError> LauncherConfig::SetString(
        std::string_view section, std::string_view key, std::string_view value
    ) noexcept {
        std::lock_guard lock(mutex_);
        std::string value_str(value);
        if (ini_.SetValue(section.data(), key.data(), value_str.c_str()) < 0) {
            return std::unexpected(ConfigError::InvalidValue);
        }
        return SaveConfigUnlocked();
    }

}
