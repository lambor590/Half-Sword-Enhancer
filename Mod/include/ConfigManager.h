#pragma once

#include <string>
#include <filesystem>
#include <chrono>

#include "../../ext/SimpleIni.h"

class ConfigManager {
private:
    CSimpleIni ini;
    std::filesystem::path configPath;
    mutable bool needsSave = false;
    mutable std::chrono::steady_clock::time_point lastSaveTime;
    static constexpr std::chrono::milliseconds SAVE_DELAY{500};

    ConfigManager();

public:
    static std::filesystem::path GetAppDataPath();
    
    static ConfigManager& Get() {
        static ConfigManager manager;
        return manager;
    }

    void SaveConfig();
    void SaveConfigDeferred();
    void LoadConfig();
    void FlushPendingSave();

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