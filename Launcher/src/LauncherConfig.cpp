#include <filesystem>

#include "../include/LauncherConfig.h"
#include "../include/Util.h"

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

    std::expected<void, ConfigError> LauncherConfig::SaveConfig() noexcept {
        std::lock_guard lock(mutex_);
        return SaveConfigUnlocked();
    }

    std::expected<void, ConfigError> LauncherConfig::LoadConfig() noexcept {
        std::lock_guard lock(mutex_);
        return LoadConfigUnlocked();
    }

}