#include "../include/LauncherConfig.h"

#include <Windows.h>

#include <fstream>
#include <iterator>

#include "../include/Util.h"

namespace hse {
    namespace {

        constexpr const char* LAUNCHER_SECTION = "Launcher";
        constexpr const char* INSTALL_SECTION = "Install";
        constexpr const char* CHECK_FOR_UPDATES_KEY = "check_for_updates";
        constexpr const char* GAME_PATH_KEY = "game_path";
        constexpr const char* GAME_EDITION_KEY = "game_edition";
    }

    LauncherConfig::LauncherConfig() {
        configPath_ = getAppDataDirectory() / "launcher_config.ini";
        isFirstRun_ = !std::filesystem::exists(configPath_);

        ini_.SetUnicode();

        if (isFirstRun_) {
            [[maybe_unused]] const auto saveResult = SaveConfig();
        } else {
            [[maybe_unused]] const auto loadResult = LoadConfig();
        }
    }

    bool LauncherConfig::GetCheckForUpdates() const {
        return GetBool(LAUNCHER_SECTION, CHECK_FOR_UPDATES_KEY, true);
    }

    std::expected<void, ConfigError> LauncherConfig::SetCheckForUpdates(bool enabled) {
        return SetBool(LAUNCHER_SECTION, CHECK_FOR_UPDATES_KEY, enabled);
    }

    bool LauncherConfig::HasCheckForUpdatesSetting() const {
        return HasKey(LAUNCHER_SECTION, CHECK_FOR_UPDATES_KEY);
    }

    std::filesystem::path LauncherConfig::GetGamePath() const {
        return GetString(INSTALL_SECTION, GAME_PATH_KEY, "");
    }

    bool LauncherConfig::HasGamePath() const {
        return HasKey(INSTALL_SECTION, GAME_PATH_KEY);
    }

    GameEdition LauncherConfig::GetGameEdition() const {
        const std::string defaultEdition{DescribeGameEdition(GameEdition::FullGame).configValue};
        return ParseGameEdition(GetString(INSTALL_SECTION, GAME_EDITION_KEY, defaultEdition.c_str()));
    }

    std::expected<void, ConfigError> LauncherConfig::SetGameLocation(
        const std::filesystem::path& path, GameEdition edition
    ) {
        const auto pathText = path.string();
        const std::string editionText{DescribeGameEdition(edition).configValue};
        if (ini_.SetValue(INSTALL_SECTION, GAME_PATH_KEY, pathText.c_str()) < 0 ||
            ini_.SetValue(INSTALL_SECTION, GAME_EDITION_KEY, editionText.c_str()) < 0)
            return std::unexpected(ConfigError::InvalidValue);
        return SaveConfig();
    }

    std::expected<void, ConfigError> LauncherConfig::SaveConfig() {
        std::error_code error;
        std::filesystem::create_directories(configPath_.parent_path(), error);
        if (error) return std::unexpected(ConfigError::WritePermissionDenied);

        std::string content;
        if (ini_.Save(content) < 0) return std::unexpected(ConfigError::InvalidFormat);

        auto temporary = configPath_;
        temporary += L".tmp";

        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) return std::unexpected(ConfigError::WritePermissionDenied);
            output.write(content.data(), static_cast<std::streamsize>(content.size()));
            output.close();
            if (!output) {
                std::filesystem::remove(temporary, error);
                return std::unexpected(ConfigError::WritePermissionDenied);
            }
        }

        if (!MoveFileExW(temporary.c_str(), configPath_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::filesystem::remove(temporary, error);
            return std::unexpected(ConfigError::WritePermissionDenied);
        }
        return {};
    }

    std::expected<void, ConfigError> LauncherConfig::LoadConfig() {
        std::ifstream input(configPath_, std::ios::binary);
        if (!input) return std::unexpected(ConfigError::FileNotFound);

        std::string content{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>(),
        };
        if (input.bad()) return std::unexpected(ConfigError::InvalidFormat);

        ini_.Reset();
        ini_.SetUnicode();
        if (ini_.LoadData(content) < 0) return std::unexpected(ConfigError::InvalidFormat);
        return {};
    }

    bool LauncherConfig::HasKey(const char* section, const char* key) const {
        return ini_.KeyExists(section, key);
    }

    bool LauncherConfig::GetBool(const char* section, const char* key, bool defaultValue) const {
        return ini_.GetBoolValue(section, key, defaultValue);
    }

    std::string LauncherConfig::GetString(const char* section, const char* key, const char* defaultValue) const {
        return std::string(ini_.GetValue(section, key, defaultValue));
    }

    std::expected<void, ConfigError> LauncherConfig::SetBool(const char* section, const char* key, bool value) {
        if (ini_.SetBoolValue(section, key, value) < 0) return std::unexpected(ConfigError::InvalidValue);
        return SaveConfig();
    }

    std::expected<void, ConfigError> LauncherConfig::SetString(
        const char* section, const char* key, std::string_view value
    ) {
        const std::string valueStr(value);
        if (ini_.SetValue(section, key, valueStr.c_str()) < 0) return std::unexpected(ConfigError::InvalidValue);
        return SaveConfig();
    }

}
