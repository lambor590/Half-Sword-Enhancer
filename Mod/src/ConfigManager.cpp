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

void ConfigManager::FlushPendingSave() {
    if (needsSave.load(std::memory_order_acquire)) {
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
    return ini.GetLongValue(section.data(), key.data(), defaultValue);
}

bool ConfigManager::GetBool(std::string_view section, std::string_view key, bool defaultValue) {
    return ini.GetBoolValue(section.data(), key.data(), defaultValue);
}

float ConfigManager::GetFloat(std::string_view section, std::string_view key, float defaultValue) {
    return static_cast<float>(ini.GetDoubleValue(section.data(), key.data(), defaultValue));
}

std::string ConfigManager::GetString(std::string_view section, std::string_view key, std::string_view defaultValue) {
    return ini.GetValue(section.data(), key.data(), defaultValue.data());
}

void ConfigManager::SetInt(std::string_view section, std::string_view key, int value) {
    ini.SetLongValue(section.data(), key.data(), value);
    SaveConfigDeferred();
}

void ConfigManager::SetBool(std::string_view section, std::string_view key, bool value) {
    ini.SetBoolValue(section.data(), key.data(), value);
    SaveConfigDeferred();
}

void ConfigManager::SetFloat(std::string_view section, std::string_view key, float value) {
    ini.SetDoubleValue(section.data(), key.data(), value);
    SaveConfigDeferred();
}

void ConfigManager::SetString(std::string_view section, std::string_view key, std::string_view value) {
    ini.SetValue(section.data(), key.data(), value.data());
    SaveConfigDeferred();
}
