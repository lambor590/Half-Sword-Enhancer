#include "../include/LauncherConfig.h"

namespace hse {

    LauncherConfig::LauncherConfig() {
        configPath_ = std::filesystem::path(hse::getAppDataPath()) / "launcher_config.ini";
        isFirstRun_ = !std::filesystem::exists(configPath_);

        ini_.SetUnicode();

        if (isFirstRun_) {
            [[maybe_unused]] auto save_result = SaveConfig();
        }
        else {
            [[maybe_unused]] auto load_result = LoadConfig();
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

    bool LauncherConfig::IsModDownloaded() const noexcept {
        return std::filesystem::exists(GetModFilePath());
    }

    std::filesystem::path LauncherConfig::GetModFilePath() noexcept {
        return std::filesystem::path(hse::getAppDataPath()) / hse::DLL_FILENAME;
    }

    GameMode LauncherConfig::GetGameMode() const noexcept {
        std::lock_guard lock(mutex_);
        const char* val = ini_.GetValue("Launcher", "game_mode", "full");
        return (val && std::string_view(val) == "demo") ? GameMode::Demo : GameMode::FullGame;
    }

    std::expected<void, ConfigError> LauncherConfig::SetGameMode(GameMode mode) noexcept {
        return SetString("Launcher", "game_mode", mode == GameMode::Demo ? "demo" : "full");
    }

    bool LauncherConfig::HasGameModeSetting() const noexcept {
        std::lock_guard lock(mutex_);
        return ini_.KeyExists("Launcher", "game_mode");
    }

    std::filesystem::path LauncherConfig::GetCacheDir() noexcept {
        static std::filesystem::path dir = []{
            auto d = std::filesystem::path(hse::getAppDataPath()) / hse::CACHE_FOLDER_NAME;
            try { std::filesystem::create_directories(d); }
            catch (...) {}
            return d;
        }();
        return dir;
    }

    std::filesystem::path LauncherConfig::GetCachedModPath(GameMode mode) noexcept {
        const char* filename = (mode == GameMode::Demo) ? "HSEnhancer_demo.dll" : "HSEnhancer_full.dll";
        return GetCacheDir() / filename;
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