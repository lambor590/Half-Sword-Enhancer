#pragma once

#include <string>
#include <filesystem>

#include "../../ext/SimpleIni.h"
#include "Util.h"

class LauncherConfig {
private:
    CSimpleIni ini;
    std::filesystem::path configPath;
    bool isFirstRun;
    
    static constexpr const char* VERSION_MANUAL_INSTALL = "manual-install";
    static constexpr const char* VERSION_UNKNOWN = "unknown-version";

    LauncherConfig() {
        configPath = std::filesystem::path(Util::getAppDataPath()) / "launcher_config.ini";
        isFirstRun = !std::filesystem::exists(configPath);
        ini.SetUnicode();
        if (isFirstRun) {
            SaveConfig();
        } else {
            LoadConfig();
        }
    }

public:
    static LauncherConfig& Get() {
        static LauncherConfig config;
        return config;
    }

    void SaveConfig() {
        bool result = ini.SaveFile(configPath.string().c_str());
        
        if (!result) {
            std::filesystem::create_directories(configPath.parent_path());
            result = ini.SaveFile(configPath.string().c_str());
        }
    }

    void LoadConfig() {
        ini.LoadFile(configPath.string().c_str());
    }

    bool GetCheckForUpdates() {
        return ini.GetBoolValue("Launcher", "check_for_updates", true);
    }

    bool HasCheckForUpdatesSetting() {
        return ini.KeyExists("Launcher", "check_for_updates");
    }

    void SetCheckForUpdates(bool enabled) {
        ini.SetBoolValue("Launcher", "check_for_updates", enabled);
        SaveConfig();
    }

    std::string GetDownloadedModVersion() {
        return ini.GetValue("Launcher", "downloaded_mod_version", "");
    }

    void SetDownloadedModVersion(const std::string& version) {
        ini.SetValue("Launcher", "downloaded_mod_version", version.c_str());
        SaveConfig();
    }

    bool IsModDownloaded() {
        return std::filesystem::exists(GetModFilePath());
    }

    static std::string GetModFilePath() {
        return std::string(Util::getAppDataPath()) + "\\" + Util::DLL_FILENAME;
    }

    bool HasVersionInfo() {
        return !GetDownloadedModVersion().empty();
    }

    bool IsFirstRun() {
        return isFirstRun;
    }

    void SetDownloadedModVersionAsManualInstall() {
        SetDownloadedModVersion(VERSION_MANUAL_INSTALL);
    }

    void SetDownloadedModVersionAsUnknown() {
        SetDownloadedModVersion(VERSION_UNKNOWN);
    }

    bool IsManualInstall() {
        return GetDownloadedModVersion() == VERSION_MANUAL_INSTALL;
    }

    int GetInt(const std::string& section, const std::string& key, int defaultValue) {
        return ini.GetLongValue(section.c_str(), key.c_str(), defaultValue);
    }

    bool GetBool(const std::string& section, const std::string& key, bool defaultValue) {
        return ini.GetBoolValue(section.c_str(), key.c_str(), defaultValue);
    }

    std::string GetString(const std::string& section, const std::string& key, const std::string& defaultValue) {
        return ini.GetValue(section.c_str(), key.c_str(), defaultValue.c_str());
    }

    void SetInt(const std::string& section, const std::string& key, int value) {
        ini.SetLongValue(section.c_str(), key.c_str(), value);
        SaveConfig();
    }

    void SetBool(const std::string& section, const std::string& key, bool value) {
        ini.SetBoolValue(section.c_str(), key.c_str(), value);
        SaveConfig();
    }

    void SetString(const std::string& section, const std::string& key, const std::string& value) {
        ini.SetValue(section.c_str(), key.c_str(), value.c_str());
        SaveConfig();
    }
};