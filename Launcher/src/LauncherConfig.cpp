#include "../include/LauncherConfig.h"

namespace hse {

    LauncherConfig::LauncherConfig() {
        configPath_ = std::filesystem::path(hse::getAppDataPath()) / "launcher_config.ini";
        isFirstRun_ = !std::filesystem::exists(configPath_);

        ini_.SetUnicode();

        if (isFirstRun_) {
            (void)SaveConfig();
        }
        else {
            (void)LoadConfig();
        }
    }

    bool LauncherConfig::GetCheckForUpdates() const noexcept {
        auto result = GetBool("Launcher", "check_for_updates", true);
        return result.value_or(true);
    }

    std::expected<void, ConfigError> LauncherConfig::SetCheckForUpdates(bool enabled) noexcept {
        return SetBool("Launcher", "check_for_updates", enabled);
    }

    bool LauncherConfig::HasCheckForUpdatesSetting() const noexcept {
        std::lock_guard lock(mutex_);
        return ini_.KeyExists("Launcher", "check_for_updates");
    }

    std::filesystem::path LauncherConfig::GetGamePath() const noexcept {
        auto result = GetString("Install", "game_path", "");
        return result ? std::filesystem::path(*result) : std::filesystem::path();
    }

    std::expected<void, ConfigError> LauncherConfig::SetGamePath(const std::filesystem::path& path) noexcept {
        return SetString("Install", "game_path", path.string());
    }

    bool LauncherConfig::HasGamePath() const noexcept {
        std::lock_guard lock(mutex_);
        return ini_.KeyExists("Install", "game_path");
    }

    GameEdition LauncherConfig::GetGameEdition() const noexcept {
        auto result = GetString("Install", "game_edition", "full");
        return (result && *result == "demo") ? GameEdition::Demo : GameEdition::FullGame;
    }

    std::expected<void, ConfigError> LauncherConfig::SetGameEdition(GameEdition edition) noexcept {
        return SetString("Install", "game_edition",
            edition == GameEdition::Demo ? "demo" : "full");
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