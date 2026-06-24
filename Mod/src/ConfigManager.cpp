#include <Windows.h>
#include <ShlObj.h>
#include <KnownFolders.h>

#include "ConfigManager.h"

std::filesystem::path ConfigManager::GetAppDataPath() {
    static std::filesystem::path cached = [] {
        PWSTR appDataPath = nullptr;
        std::filesystem::path result;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appDataPath))) {
            result = std::filesystem::path(appDataPath) / "Half Sword Enhancer";
            CoTaskMemFree(appDataPath);
            std::filesystem::create_directories(result);
        } else {
            result = std::filesystem::current_path();
        }
        return result;
    }();
    return cached;
}

ConfigManager& ConfigManager::Get() {
    static ConfigManager manager;
    return manager;
}

ConfigManager::ConfigManager() {
    configPath = GetAppDataPath() / "config.ini";

    ini.SetUnicode();
    LoadConfig();
}

void ConfigManager::SaveConfig() {
    auto rc = ini.SaveFile(configPath.string().c_str());

    if (rc < 0) {
        std::filesystem::create_directories(configPath.parent_path());
        ini.SaveFile(configPath.string().c_str());
    }
    needsSave.store(false, std::memory_order_release);
    lastSaveTime = std::chrono::steady_clock::now();
}

void ConfigManager::SaveConfigDeferred() {
    if (suppressDeferred) return;
    needsSave.store(true, std::memory_order_relaxed);
    const auto now = std::chrono::steady_clock::now();
    if (now - lastSaveTime >= SAVE_DELAY) {
        SaveConfig();
    }
}

void ConfigManager::SuppressDeferred(bool suppress) {
    suppressDeferred = suppress;
}

void ConfigManager::BatchSave(const std::function<void()>& updates) {
    SuppressDeferred(true);
    updates();
    SuppressDeferred(false);
    SaveConfig();
}

void ConfigManager::LoadConfig() {
    if (std::filesystem::exists(configPath)) {
        ini.LoadFile(configPath.string().c_str());
    } else {
        SaveConfig();
    }
}

int ConfigManager::GetInt(std::string_view section, std::string_view key, int defaultValue) {
    const std::string sectionName(section);
    const std::string keyName(key);
    return ini.GetLongValue(sectionName.c_str(), keyName.c_str(), defaultValue);
}

bool ConfigManager::GetBool(std::string_view section, std::string_view key, bool defaultValue) {
    const std::string sectionName(section);
    const std::string keyName(key);
    return ini.GetBoolValue(sectionName.c_str(), keyName.c_str(), defaultValue);
}

float ConfigManager::GetFloat(std::string_view section, std::string_view key, float defaultValue) {
    const std::string sectionName(section);
    const std::string keyName(key);
    return static_cast<float>(ini.GetDoubleValue(sectionName.c_str(), keyName.c_str(), defaultValue));
}

std::string ConfigManager::GetString(std::string_view section, std::string_view key, std::string_view defaultValue) {
    const std::string sectionName(section);
    const std::string keyName(key);
    const std::string defaultValueStr(defaultValue);
    return ini.GetValue(sectionName.c_str(), keyName.c_str(), defaultValueStr.c_str());
}

void ConfigManager::SetInt(std::string_view section, std::string_view key, int value) {
    const std::string sectionName(section);
    const std::string keyName(key);
    ini.SetLongValue(sectionName.c_str(), keyName.c_str(), value);
    SaveConfigDeferred();
}

void ConfigManager::SetBool(std::string_view section, std::string_view key, bool value) {
    const std::string sectionName(section);
    const std::string keyName(key);
    ini.SetBoolValue(sectionName.c_str(), keyName.c_str(), value);
    SaveConfigDeferred();
}

void ConfigManager::SetFloat(std::string_view section, std::string_view key, float value) {
    const std::string sectionName(section);
    const std::string keyName(key);
    ini.SetDoubleValue(sectionName.c_str(), keyName.c_str(), value);
    SaveConfigDeferred();
}

void ConfigManager::SetString(std::string_view section, std::string_view key, std::string_view value) {
    const std::string sectionName(section);
    const std::string keyName(key);
    const std::string valueStr(value);
    ini.SetValue(sectionName.c_str(), keyName.c_str(), valueStr.c_str());
    SaveConfigDeferred();
}

void ConfigManager::DeleteSection(std::string_view section) {
    const std::string sectionName(section);
    ini.Delete(sectionName.c_str(), nullptr);
    SaveConfigDeferred();
}
