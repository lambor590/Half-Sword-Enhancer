#include <algorithm>

#include "Menu/IMenuFunction.h"
#include "Menu/ICollapsibleSection.h"
#include "Hooks/GameHook.h"
#include "ConfigManager.h"

static std::string NormalizeSection(const std::string& name) {
    std::string result = name;
    result.erase(std::remove(result.begin(), result.end(), ' '), result.end());
    return result;
}

int IMenuFunction::GetConfig(const std::string& paramName, int defaultValue) const {
    return ConfigManager::Get().GetInt(NormalizeSection(GetName()), paramName, defaultValue);
}

bool IMenuFunction::GetConfig(const std::string& paramName, bool defaultValue) const {
    return ConfigManager::Get().GetBool(NormalizeSection(GetName()), paramName, defaultValue);
}

float IMenuFunction::GetConfig(const std::string& paramName, float defaultValue) const {
    return ConfigManager::Get().GetFloat(NormalizeSection(GetName()), paramName, defaultValue);
}

std::string IMenuFunction::GetConfig(const std::string& paramName, const std::string& defaultValue) const {
    return ConfigManager::Get().GetString(NormalizeSection(GetName()), paramName, defaultValue);
}

void IMenuFunction::SaveConfig(const std::string& paramName, int value) const {
    ConfigManager::Get().SetInt(NormalizeSection(GetName()), paramName, value);
}

void IMenuFunction::SaveConfig(const std::string& paramName, bool value) const {
    ConfigManager::Get().SetBool(NormalizeSection(GetName()), paramName, value);
}

void IMenuFunction::SaveConfig(const std::string& paramName, float value) const {
    ConfigManager::Get().SetFloat(NormalizeSection(GetName()), paramName, value);
}

void IMenuFunction::SaveConfig(const std::string& paramName, const std::string& value) const {
    ConfigManager::Get().SetString(NormalizeSection(GetName()), paramName, value);
}

void IMenuFunction::SetEnabled(bool enabled) {
    if (isEnabled != enabled) {
        isEnabled = enabled;
        SaveConfig("enabled", enabled);
    }
}

bool IMenuFunction::LoadEnabledState(bool defaultState) {
    return isEnabled = GetConfig("enabled", defaultState);
}

HookedFunction::~HookedFunction() {
    if (isEnabled) {
        g_GameHook->UnregisterHook(hookedFunction);
    }
}

void HookedFunction::SetKey(int newKey) {
    if (*key != newKey) {
        *key = prevKey = newKey;
        SaveConfig("key", *key);
        
        if (GetConfig("key", -1) != *key) {
            ConfigManager::Get().SetInt(NormalizeSection(GetName()), "key", *key);
            ConfigManager::Get().SaveConfig();
        }
    }
}

void HookedFunction::SetEnabled(bool enabled) {
    if (isEnabled != enabled) {
        isEnabled = enabled;
        SaveConfig("enabled", enabled);
        
        if (isEnabled) {
            g_GameHook->RegisterHook(hookedFunction, callback);
        } else {
            g_GameHook->UnregisterHook(hookedFunction);
        }
    }
}

void HookedFunction::LoadConfig() {
    *key = GetConfig("key", *key);
    prevKey = *key;
    
    if (LoadEnabledState(false) && isEnabled) {
        g_GameHook->RegisterHook(hookedFunction, callback);
    }
    LoadParameters();
}

void KeybindFunction::LoadConfig() {
    *key = GetConfig("key", *key);
    prevKey = *key;
    isEnabled = (*key != VK_ESCAPE);
    LoadParameters();
}

void KeybindFunction::UpdateKey(int newKey) {
    if (prevKey != newKey) {
        prevKey = *key = newKey;
        SaveConfig("key", *key);
        
        if (GetConfig("key", -1) != *key) {
            ConfigManager::Get().SetInt(NormalizeSection(GetName()), "key", *key);
            ConfigManager::Get().SaveConfig();
        }
        
        isEnabled = (*key != VK_ESCAPE);
    }
}