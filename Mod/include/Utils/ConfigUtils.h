#pragma once

#include "ConfigManager.h"

namespace ConfigUtils {
    class ConfigTransaction {
    private:
        ConfigManager& config;
        bool autoSave;
        
    public:
        explicit ConfigTransaction(bool autoSave = true) 
            : config(ConfigManager::Get()), autoSave(autoSave) {}
        
        ~ConfigTransaction() { 
            if (autoSave) config.SaveConfig(); 
        }
        
        void SetBool(const std::string& section, const std::string& key, bool value) {
            config.SetBool(section, key, value);
        }
        
        void SetInt(const std::string& section, const std::string& key, int value) {
            config.SetInt(section, key, value);
        }
        
        void SetFloat(const std::string& section, const std::string& key, float value) {
            config.SetFloat(section, key, value);
        }
        
        void SetString(const std::string& section, const std::string& key, const std::string& value) {
            config.SetString(section, key, value);
        }
        
        bool GetBool(const std::string& section, const std::string& key, bool defaultValue) {
            return config.GetBool(section, key, defaultValue);
        }
        
        int GetInt(const std::string& section, const std::string& key, int defaultValue) {
            return config.GetInt(section, key, defaultValue);
        }
        
        float GetFloat(const std::string& section, const std::string& key, float defaultValue) {
            return config.GetFloat(section, key, defaultValue);
        }
        
        std::string GetString(const std::string& section, const std::string& key, const std::string& defaultValue) {
            return config.GetString(section, key, defaultValue);
        }
    };
    
    inline void BatchUpdate(std::function<void(ConfigTransaction&)> updates) {
        ConfigTransaction transaction;
        updates(transaction);
    }
}