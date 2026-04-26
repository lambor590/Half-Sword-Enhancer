#include "../include/LauncherConfig.h"

namespace hse {
    namespace {

        constexpr std::string_view LAUNCHER_SECTION = "Launcher";
        constexpr std::string_view INSTALL_SECTION = "Install";
        constexpr std::string_view CHECK_FOR_UPDATES_KEY = "check_for_updates";
        constexpr std::string_view GAME_PATH_KEY = "game_path";
        constexpr std::string_view GAME_EDITION_KEY = "game_edition";
        constexpr std::string_view DEMO_EDITION = "demo";
        constexpr std::string_view FULL_EDITION = "full";

    }

    LauncherConfig::LauncherConfig() {
        configPath_ = getAppDataDirectory() / "launcher_config.ini";
        isFirstRun_ = !std::filesystem::exists(configPath_);

        ini_.SetUnicode();

        if (isFirstRun_) {
            (void)SaveConfig();
        } else {
            (void)LoadConfig();
        }
    }

    bool LauncherConfig::GetCheckForUpdates() const noexcept {
        auto result = GetBool(LAUNCHER_SECTION, CHECK_FOR_UPDATES_KEY, true);
        return result.value_or(true);
    }

    std::expected<void, ConfigError> LauncherConfig::SetCheckForUpdates(bool enabled) noexcept {
        return SetBool(LAUNCHER_SECTION, CHECK_FOR_UPDATES_KEY, enabled);
    }

    bool LauncherConfig::HasCheckForUpdatesSetting() const noexcept {
        return HasKey(LAUNCHER_SECTION, CHECK_FOR_UPDATES_KEY);
    }

    std::filesystem::path LauncherConfig::GetGamePath() const noexcept {
        auto result = GetString(INSTALL_SECTION, GAME_PATH_KEY, "");
        return result ? std::filesystem::path(*result) : std::filesystem::path();
    }

    std::expected<void, ConfigError> LauncherConfig::SetGamePath(const std::filesystem::path& path) noexcept {
        return SetString(INSTALL_SECTION, GAME_PATH_KEY, path.string());
    }

    bool LauncherConfig::HasGamePath() const noexcept {
        return HasKey(INSTALL_SECTION, GAME_PATH_KEY);
    }

    GameEdition LauncherConfig::GetGameEdition() const noexcept {
        auto result = GetString(INSTALL_SECTION, GAME_EDITION_KEY, FULL_EDITION);
        return (result && *result == DEMO_EDITION) ? GameEdition::Demo : GameEdition::FullGame;
    }

    std::expected<void, ConfigError> LauncherConfig::SetGameEdition(GameEdition edition) noexcept {
        return SetString(INSTALL_SECTION, GAME_EDITION_KEY, edition == GameEdition::Demo ? DEMO_EDITION : FULL_EDITION);
    }

    std::expected<void, ConfigError> LauncherConfig::SaveConfig() noexcept {
        std::lock_guard lock(mutex_);
        return SaveConfigUnlocked();
    }

    std::expected<void, ConfigError> LauncherConfig::LoadConfig() noexcept {
        std::lock_guard lock(mutex_);
        return LoadConfigUnlocked();
    }

}
