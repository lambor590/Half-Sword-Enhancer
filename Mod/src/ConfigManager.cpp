#include <Windows.h>
#include <ShlObj.h>
#include <KnownFolders.h>

#include "ConfigManager.h"

std::filesystem::path ConfigManager::GetAppDataPath() {
    std::filesystem::path basePath;
    PWSTR appDataPath = nullptr;
    
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appDataPath))) {
        basePath = std::filesystem::path(appDataPath) / "Half Sword Enhancer";
        CoTaskMemFree(appDataPath);
        std::filesystem::create_directories(basePath);
    } else {
        basePath = std::filesystem::current_path();
    }
    
    return basePath;
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
    needsSave = false;
    lastSaveTime = std::chrono::steady_clock::now();
}

void ConfigManager::SaveConfigDeferred() {
    needsSave = true;
    auto now = std::chrono::steady_clock::now();
    if (now - lastSaveTime >= SAVE_DELAY) {
        SaveConfig();
    }
}

void ConfigManager::FlushPendingSave() {
    if (needsSave) {
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

int ConfigManager::GetInt(const std::string& function, const std::string& param, int defaultValue) {
    return ini.GetLongValue(function.c_str(), param.c_str(), defaultValue);
}

bool ConfigManager::GetBool(const std::string& function, const std::string& param, bool defaultValue) {
    return ini.GetBoolValue(function.c_str(), param.c_str(), defaultValue);
}

float ConfigManager::GetFloat(const std::string& function, const std::string& param, float defaultValue) {
    return static_cast<float>(ini.GetDoubleValue(function.c_str(), param.c_str(), defaultValue));
}

std::string ConfigManager::GetString(const std::string& function, const std::string& param, 
                                    const std::string& defaultValue) {
    return ini.GetValue(function.c_str(), param.c_str(), defaultValue.c_str());
}

void ConfigManager::SetInt(const std::string& function, const std::string& param, int value) {
    ini.SetLongValue(function.c_str(), param.c_str(), value);
    SaveConfigDeferred();
}

void ConfigManager::SetBool(const std::string& function, const std::string& param, bool value) {
    ini.SetBoolValue(function.c_str(), param.c_str(), value);
    SaveConfigDeferred();
}

void ConfigManager::SetFloat(const std::string& function, const std::string& param, float value) {
    ini.SetDoubleValue(function.c_str(), param.c_str(), value);
    SaveConfigDeferred();
}

void ConfigManager::SetString(const std::string& function, const std::string& param, 
                             const std::string& value) {
    ini.SetValue(function.c_str(), param.c_str(), value.c_str());
    SaveConfigDeferred();
}
