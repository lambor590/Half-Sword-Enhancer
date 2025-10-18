#pragma once

#include <string>
#include <filesystem>
#include <expected>
#include <mutex>
#include <optional>

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
        [[nodiscard]] bool IsModDownloaded() const noexcept;
        [[nodiscard]] static std::filesystem::path GetModFilePath() noexcept;
        [[nodiscard]] bool IsFirstRun() const noexcept { return isFirstRun_; }

        [[nodiscard]] std::expected<bool, ConfigError> GetBool(
            std::string_view section,
            std::string_view key,
            bool defaultValue = false
        ) const noexcept;

        [[nodiscard]] std::expected<int, ConfigError> GetInt(
            std::string_view section,
            std::string_view key,
            int defaultValue = 0
        ) const noexcept;

        [[nodiscard]] std::expected<std::string, ConfigError> GetString(
            std::string_view section,
            std::string_view key,
            std::string_view defaultValue = ""
        ) const noexcept;

        [[nodiscard]] std::expected<void, ConfigError> SetBool(
            std::string_view section,
            std::string_view key,
            bool value
        ) noexcept;

        [[nodiscard]] std::expected<void, ConfigError> SetInt(
            std::string_view section,
            std::string_view key,
            int value
        ) noexcept;

        [[nodiscard]] std::expected<void, ConfigError> SetString(
            std::string_view section,
            std::string_view key,
            std::string_view value
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
    };

    inline std::expected<void, ConfigError> LauncherConfig::SaveConfigUnlocked() noexcept {
        try {
            if (!ini_.SaveFile(configPath_.string().c_str())) {
                std::filesystem::create_directories(configPath_.parent_path());
                if (!ini_.SaveFile(configPath_.string().c_str())) {
                    return std::unexpected(ConfigError::WritePermissionDenied);
                }
            }
            return {};
        }
        catch (const std::filesystem::filesystem_error&) {
            return std::unexpected(ConfigError::WritePermissionDenied);
        }
        catch (...) {
            return std::unexpected(ConfigError::InvalidFormat);
        }
    }

    inline std::expected<void, ConfigError> LauncherConfig::LoadConfigUnlocked() noexcept {
        try {
            if (ini_.LoadFile(configPath_.string().c_str()) < 0) {
                return std::unexpected(ConfigError::FileNotFound);
            }
            return {};
        }
        catch (...) {
            return std::unexpected(ConfigError::InvalidFormat);
        }
    }

    inline std::expected<bool, ConfigError> LauncherConfig::GetBool(
        std::string_view section,
        std::string_view key,
        bool defaultValue
    ) const noexcept {
        std::lock_guard lock(mutex_);
        return ini_.GetBoolValue(section.data(), key.data(), defaultValue);
    }

    inline std::expected<int, ConfigError> LauncherConfig::GetInt(
        std::string_view section,
        std::string_view key,
        int defaultValue
    ) const noexcept {
        std::lock_guard lock(mutex_);
        return static_cast<int>(ini_.GetLongValue(section.data(), key.data(), defaultValue));
    }

    inline std::expected<std::string, ConfigError> LauncherConfig::GetString(
        std::string_view section,
        std::string_view key,
        std::string_view defaultValue
    ) const noexcept {
        std::lock_guard lock(mutex_);
        thread_local std::string temp_default;
        temp_default.assign(defaultValue);
        return std::string(ini_.GetValue(section.data(), key.data(), temp_default.c_str()));
    }

    inline std::expected<void, ConfigError> LauncherConfig::SetBool(
        std::string_view section,
        std::string_view key,
        bool value
    ) noexcept {
        try {
            std::lock_guard lock(mutex_);
            ini_.SetBoolValue(section.data(), key.data(), value);
            return SaveConfigUnlocked();
        }
        catch (...) {
            return std::unexpected(ConfigError::InvalidValue);
        }
    }

    inline std::expected<void, ConfigError> LauncherConfig::SetInt(
        std::string_view section,
        std::string_view key,
        int value
    ) noexcept {
        try {
            std::lock_guard lock(mutex_);
            ini_.SetLongValue(section.data(), key.data(), value);
            return SaveConfigUnlocked();
        }
        catch (...) {
            return std::unexpected(ConfigError::InvalidValue);
        }
    }

    inline std::expected<void, ConfigError> LauncherConfig::SetString(
        std::string_view section,
        std::string_view key,
        std::string_view value
    ) noexcept {
        try {
            std::lock_guard lock(mutex_);
            thread_local std::string temp_value;
            temp_value.assign(value);
            ini_.SetValue(section.data(), key.data(), temp_value.c_str());
            return SaveConfigUnlocked();
        }
        catch (...) {
            return std::unexpected(ConfigError::InvalidValue);
        }
    }

}