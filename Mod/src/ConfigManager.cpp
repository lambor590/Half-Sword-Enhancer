#include <iostream>
#include <Windows.h>

#include "ConfigManager.h"

ConfigManager::ConfigManager() {
    configPath = std::filesystem::current_path() / "HS-Enhancer_config.ini";
    ini.SetUnicode();
    LoadConfig();
}

void ConfigManager::SaveConfig() {
    bool result = ini.SaveFile(configPath.string().c_str());
    
    if (!result) {
        std::filesystem::create_directories(configPath.parent_path());
        result = ini.SaveFile(configPath.string().c_str());
    }
}

void ConfigManager::LoadConfig() {
    if (std::filesystem::exists(configPath)) {
        ini.LoadFile(configPath.string().c_str());
    } else {
        ini.SetUnicode();
        SaveConfig();
    }
}

void ConfigManager::EnsureSectionExists(const std::string& function) {
    if (!ini.SectionExists(function.c_str())) {
        ini.SetValue(function.c_str(), nullptr, nullptr);
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
    EnsureSectionExists(function);
    ini.SetLongValue(function.c_str(), param.c_str(), value);
}

void ConfigManager::SetBool(const std::string& function, const std::string& param, bool value) {
    EnsureSectionExists(function);
    ini.SetBoolValue(function.c_str(), param.c_str(), value);
}

void ConfigManager::SetFloat(const std::string& function, const std::string& param, float value) {
    EnsureSectionExists(function);
    ini.SetDoubleValue(function.c_str(), param.c_str(), value);
}

void ConfigManager::SetString(const std::string& function, const std::string& param, 
                             const std::string& value) {
    EnsureSectionExists(function);
    ini.SetValue(function.c_str(), param.c_str(), value.c_str());
}
