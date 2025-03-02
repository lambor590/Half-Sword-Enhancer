#pragma once

#include <string>
#include <memory>
#include <filesystem>

#include "SimpleIni.h"

class ConfigManager {
private:
    inline static ConfigManager* instance = nullptr;
    CSimpleIni ini;
    std::filesystem::path configPath;

    ConfigManager();

public:
    static ConfigManager& Get() {
        if (!instance) {
            instance = new ConfigManager();
        }
        return *instance;
    }

    void SaveConfig();
    void LoadConfig();
    void EnsureSectionExists(const std::string& function);

    int GetInt(const std::string& function, const std::string& param, int defaultValue);
               
    bool GetBool(const std::string& function, const std::string& param, bool defaultValue);
                 
    float GetFloat(const std::string& function, const std::string& param, float defaultValue);
                   
    std::string GetString(const std::string& function, const std::string& param, 
                          const std::string& defaultValue);

    void SetInt(const std::string& function, const std::string& param, int value);
                
    void SetBool(const std::string& function, const std::string& param, bool value);
                 
    void SetFloat(const std::string& function, const std::string& param, float value);
                  
    void SetString(const std::string& function, const std::string& param, 
                   const std::string& value);
}; 