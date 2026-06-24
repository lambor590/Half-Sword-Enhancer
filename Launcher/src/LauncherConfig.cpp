#include "../include/LauncherConfig.h"

namespace hse {
    namespace {

        constexpr const char* LAUNCHER_SECTION = "Launcher";
        constexpr const char* INSTALL_SECTION = "Install";
        constexpr const char* CHECK_FOR_UPDATES_KEY = "check_for_updates";
        constexpr const char* GAME_PATH_KEY = "game_path";
        constexpr const char* GAME_EDITION_KEY = "game_edition";
        constexpr const char* DEMO_EDITION = "demo";
        constexpr const char* FULL_EDITION = "full";

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

    std::expected<void, ConfigError> LauncherConfig::SetGamePath(const std::filesystem::path& path) {
        return SetString(INSTALL_SECTION, GAME_PATH_KEY, path.string());
    }

    bool LauncherConfig::HasGamePath() const {
        return HasKey(INSTALL_SECTION, GAME_PATH_KEY);
    }

    GameEdition LauncherConfig::GetGameEdition() const {
        auto result = GetString(INSTALL_SECTION, GAME_EDITION_KEY, FULL_EDITION);
        return (result == DEMO_EDITION) ? GameEdition::Demo : GameEdition::FullGame;
    }

    std::expected<void, ConfigError> LauncherConfig::SetGameEdition(GameEdition edition) {
        return SetString(INSTALL_SECTION, GAME_EDITION_KEY, edition == GameEdition::Demo ? DEMO_EDITION : FULL_EDITION);
    }

    std::expected<void, ConfigError> LauncherConfig::SaveConfig() {
        return SaveConfigUnlocked();
    }

    std::expected<void, ConfigError> LauncherConfig::LoadConfig() {
        return LoadConfigUnlocked();
    }

}
