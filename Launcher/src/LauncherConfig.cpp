#include "../include/LauncherConfig.h"
#include "../include/UpdateManager.h"

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

    std::filesystem::path LauncherConfig::GetCustomDllPath() noexcept {
        return GetCacheDir() / hse::CUSTOM_DLL_FILENAME;
    }

    std::filesystem::path LauncherConfig::GetOfficialDllPath(GameMode mode, const Version& version) noexcept {
        const auto compact = version.ToCompactString();
        const std::string_view prefix = (mode == GameMode::Demo) ? "HSEnhancer_demo_v" : "HSEnhancer_v";
        std::string filename;
        filename.reserve(prefix.size() + compact.size() + 4);
        filename.append(prefix);
        filename.append(compact);
        filename.append(".dll");
        return GetCacheDir() / filename;
    }

    std::filesystem::path LauncherConfig::GetExperimentalDllPath(std::string_view sanitizedTimestamp) noexcept {
        constexpr std::string_view prefix = "HSEnhancer_exp_";
        std::string filename;
        filename.reserve(prefix.size() + sanitizedTimestamp.size() + 4);
        filename.append(prefix);
        filename.append(sanitizedTimestamp);
        filename.append(".dll");
        return GetCacheDir() / filename;
    }

    std::filesystem::path LauncherConfig::GetLegacyModFilePath() noexcept {
        return std::filesystem::path(hse::getAppDataPath()) / hse::LEGACY_DLL_FILENAME;
    }

    std::filesystem::path LauncherConfig::ResolveModPath([[maybe_unused]] GameMode mode) const noexcept {
        try {
            if (GetDllSource() == DllSource::Custom) {
                auto customPath = GetCustomDllPath();
                if (std::filesystem::exists(customPath))
                    return customPath;
            }

#ifdef EXPERIMENTAL_VERSION
            auto timestamp = GetString("ExperimentalUpdate", "mod_timestamp", "").value_or("");
            if (!timestamp.empty()) {
                auto expPath = GetExperimentalDllPath(hse::SanitizeTimestamp(timestamp));
                if (std::filesystem::exists(expPath))
                    return expPath;
            }
#else
            auto versionKey = (mode == GameMode::Demo) ? "official_demo_version" : "official_version";
            auto versionStr = GetString("DLL", versionKey, "").value_or("");
            if (!versionStr.empty()) {
                Version version(versionStr);
                if (version.IsValid()) {
                    auto officialPath = GetOfficialDllPath(mode, version);
                    if (std::filesystem::exists(officialPath))
                        return officialPath;
                }
            }
#endif
        }
        catch (...) {}
        return {};
    }

    DllSource LauncherConfig::GetDllSource() const noexcept {
        auto result = GetBool("DLL", "use_custom", false);
        return result.value_or(false) ? DllSource::Custom : DllSource::Official;
    }

    std::expected<void, ConfigError> LauncherConfig::SetDllSource(DllSource source) noexcept {
        return SetBool("DLL", "use_custom", source == DllSource::Custom);
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