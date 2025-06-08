#include <algorithm>

#include "Menu/IMenuFunction.h"
#include "Menu/ICollapsibleSection.h"
#include "Hooks/GameHook.h"
#include "ConfigManager.h"
#include "Gui.h"
#include "GlobalDefinitions.h"
#include "KeybindManager.h"

HookedFunction::~HookedFunction() {
    if (!isEnabled) return;
    for (const auto evt : eventTypes) {
        g_GameHook->UnregisterEvent(evt, this);
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
    if (isEnabled == enabled) return;
    
    isEnabled = enabled;
    SaveConfig("enabled", enabled);
    
    for (const auto evt : eventTypes) {
        if (isEnabled) {
            g_GameHook->RegisterEvent(evt, this, [this]() { callback(isEnabled); });
        } else {
            g_GameHook->UnregisterEvent(evt, this);
        }
    }
    
    if (executeOnToggle) callback(isEnabled);
    g_ConfigManager.SaveConfig();
}

void HookedFunction::LoadConfig() {
    *key = GetConfig("key", *key);
    prevKey = *key;
    
    if (*key != -1) {
        KeybindManager::RegisterKeybind(key, [this]() { SetEnabled(!isEnabled); }, this);
    }
    
    LoadEnabledState(false);
    
    if (isEnabled) {
        for (const auto evt : eventTypes) {
            g_GameHook->RegisterEvent(evt, this, [this]() { callback(isEnabled); });
        }
    }
    
    LoadParameters();
}

void KeybindFunction::SetEnabled(bool enabled) {
    if (!toggleable || isEnabled == enabled) return;
    
    isEnabled = enabled;
    SaveConfig("enabled", enabled);
    
    if (isEnabled && *key != -1) {
        KeybindManager::RegisterKeybind(key, [this]() { callback(isEnabled); }, this);
    } else {
        KeybindManager::UnregisterKeybind(key);
    }
    
    g_ConfigManager.SaveConfig();
}

void KeybindFunction::LoadConfig() {
    *key = GetConfig("key", *key);
    prevKey = *key;
    
    if (toggleable) {
        if (LoadEnabledState(false) && isEnabled && *key != -1) {
            KeybindManager::RegisterKeybind(key, [this]() { callback(isEnabled); }, this);
        }
    } else if (*key != -1) {
        KeybindManager::RegisterKeybind(key, [this]() { callback(true); }, this);
    }
    
    LoadParameters();
}

void KeybindFunction::UpdateKey() {
    if (prevKey == *key) return;
    
    if (toggleable) {
        if (isEnabled) {
            KeybindManager::UnregisterKeybind(key);
            if (*key != -1) {
                KeybindManager::RegisterKeybind(key, [this]() { callback(isEnabled); }, this);
            }
        }
    } else {
        KeybindManager::UnregisterKeybind(key);
        if (*key != -1) {
            KeybindManager::RegisterKeybind(key, [this]() { callback(true); }, this);
        }
    }
    
    prevKey = *key;
    SaveConfig("key", *key);
    g_ConfigManager.SaveConfig();
}