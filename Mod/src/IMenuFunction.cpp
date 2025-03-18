#include <algorithm>

#include "Menu/IMenuFunction.h"
#include "Menu/ICollapsibleSection.h"
#include "Hooks/GameHook.h"
#include "ConfigManager.h"
#include "Gui.h"
#include "GlobalDefinitions.h"
#include "KeybindManager.h"

static std::string NormalizeSection(const std::string& name) {
    std::string result = name;
    result.erase(std::remove(result.begin(), result.end(), ' '), result.end());
    return result;
}

int IMenuFunction::GetConfig(const std::string& paramName, int defaultValue) const {
    return g_ConfigManager.GetInt(NormalizeSection(GetName()), paramName, defaultValue);
}

bool IMenuFunction::GetConfig(const std::string& paramName, bool defaultValue) const {
    return g_ConfigManager.GetBool(NormalizeSection(GetName()), paramName, defaultValue);
}

float IMenuFunction::GetConfig(const std::string& paramName, float defaultValue) const {
    return g_ConfigManager.GetFloat(NormalizeSection(GetName()), paramName, defaultValue);
}

std::string IMenuFunction::GetConfig(const std::string& paramName, const std::string& defaultValue) const {
    return g_ConfigManager.GetString(NormalizeSection(GetName()), paramName, defaultValue);
}

void IMenuFunction::SaveConfig(const std::string& paramName, int value) const {
    g_ConfigManager.SetInt(NormalizeSection(GetName()), paramName, value);
}

void IMenuFunction::SaveConfig(const std::string& paramName, bool value) const {
    g_ConfigManager.SetBool(NormalizeSection(GetName()), paramName, value);
}

void IMenuFunction::SaveConfig(const std::string& paramName, float value) const {
    g_ConfigManager.SetFloat(NormalizeSection(GetName()), paramName, value);
}

void IMenuFunction::SaveConfig(const std::string& paramName, const std::string& value) const {
    g_ConfigManager.SetString(NormalizeSection(GetName()), paramName, value);
}

void IMenuFunction::SetEnabled(bool enabled) {
    if (isEnabled != enabled) {
        isEnabled = enabled;
        SaveConfig("enabled", enabled);
        g_ConfigManager.SaveConfig();
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

void HookedFunction::SetKey() {
    if (prevKey != *key) {
        prevKey = *key;
        SaveConfig("key", *key);
        g_ConfigManager.SaveConfig();
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
        
        g_ConfigManager.SaveConfig();
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
    if (*key != -1)
        KeybindManager::RegisterKeybind(key, callback);
    
    LoadParameters();
}

void KeybindFunction::UpdateKey() {
    KeybindManager::UnregisterKeybind(&prevKey);
    if (prevKey != *key) {
        prevKey = *key;
        SaveConfig("key", *key);
        g_ConfigManager.SaveConfig();
    }
}