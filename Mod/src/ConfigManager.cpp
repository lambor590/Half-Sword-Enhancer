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

ConfigManager::ConfigManager() {
    configPath = GetAppDataPath() / "config.ini";
    
    ini.SetUnicode();
    LoadConfig();
}

void ConfigManager::SaveConfig() {
    bool result = ini.SaveFile(configPath.string().c_str());

    if (!result) {
        std::filesystem::create_directories(configPath.parent_path());
        result = ini.SaveFile(configPath.string().c_str());
    }
    needsSave.store(false, std::memory_order_release);
    lastSaveTime = std::chrono::steady_clock::now();
}

void ConfigManager::SaveConfigDeferred() {
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

void ConfigManager::LoadConfig() {
    if (std::filesystem::exists(configPath)) {
        ini.LoadFile(configPath.string().c_str());
    } else {
        SaveConfig();
    }
}

int ConfigManager::GetInt(std::string_view function, std::string_view param, int defaultValue) {
    return ini.GetLongValue(function.data(), param.data(), defaultValue);
}

bool ConfigManager::GetBool(std::string_view function, std::string_view param, bool defaultValue) {
    return ini.GetBoolValue(function.data(), param.data(), defaultValue);
}

float ConfigManager::GetFloat(std::string_view function, std::string_view param, float defaultValue) {
    return static_cast<float>(ini.GetDoubleValue(function.data(), param.data(), defaultValue));
}

std::string ConfigManager::GetString(std::string_view function, std::string_view param,
                                    std::string_view defaultValue) {
    return ini.GetValue(function.data(), param.data(), defaultValue.data());
}

void ConfigManager::SetInt(std::string_view function, std::string_view param, int value) {
    ini.SetLongValue(function.data(), param.data(), value);
    SaveConfigDeferred();
}

void ConfigManager::SetBool(std::string_view function, std::string_view param, bool value) {
    ini.SetBoolValue(function.data(), param.data(), value);
    SaveConfigDeferred();
}

void ConfigManager::SetFloat(std::string_view function, std::string_view param, float value) {
    ini.SetDoubleValue(function.data(), param.data(), value);
    SaveConfigDeferred();
}

void ConfigManager::SetString(std::string_view function, std::string_view param,
                             std::string_view value) {
    ini.SetValue(function.data(), param.data(), value.data());
    SaveConfigDeferred();
}
