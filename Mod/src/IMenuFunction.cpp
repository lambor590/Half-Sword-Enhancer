#include <algorithm>

#include "Menu/IMenuFunction.h"
#include "Menu/ICollapsibleSection.h"
#include "Hooks/GameHook.h"
#include "ConfigManager.h"
#include "Gui.h"
#include "GlobalDefinitions.h"
#include "KeybindManager.h"

HookedFunction::~HookedFunction() {
    if (!useEvent && isEnabled) {
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
        
        if (!useEvent) {
            if (isEnabled) {
                g_GameHook->RegisterHook(hookedFunction, [this]() { callback(isEnabled); });
            } else {
                g_GameHook->UnregisterHook(hookedFunction);
            }
        }
        
        if (executeOnToggle) callback(isEnabled);
        
        g_ConfigManager.SaveConfig();
    }
}

void HookedFunction::LoadConfig() {
    *key = GetConfig("key", *key);
    prevKey = *key;
    
    if (useEvent) {
        static std::unordered_map<HookedFunction*, bool> registered;
        this->LoadEnabledState(false);
        if (!registered[this]) {
            GameHook::Get().RegisterEvent(eventType, [this]() { callback(isEnabled); });
            registered[this] = true;
        }
    } else {
        if (this->LoadEnabledState(false) && isEnabled) {
            g_GameHook->RegisterHook(hookedFunction, [this]() { callback(isEnabled); });
        }
    }
    LoadParameters();
}

void KeybindFunction::SetEnabled(bool enabled) {
    if (!toggleable) return;
    if (isEnabled != enabled) {
        isEnabled = enabled;
        SaveConfig("enabled", enabled);
        if (isEnabled) {
            if (*key != -1)
                KeybindManager::RegisterKeybind(key, [this]() { callback(isEnabled); });
        } else {
            KeybindManager::UnregisterKeybind(&prevKey);
        }
        g_ConfigManager.SaveConfig();
    }
}

void KeybindFunction::LoadConfig() {
    *key = GetConfig("key", *key);
    prevKey = *key;
    if (toggleable) {
        if (this->LoadEnabledState(false) && isEnabled && *key != -1)
            KeybindManager::RegisterKeybind(key, [this]() { callback(isEnabled); });
    } else {
        if (*key != -1)
            KeybindManager::RegisterKeybind(key, [this]() { callback(true); });
    }
    LoadParameters();
}

void KeybindFunction::UpdateKey() {
    if (toggleable) {
        if (prevKey != *key) {
            if (isEnabled) {
                KeybindManager::UnregisterKeybind(&prevKey);
                if (*key != -1)
                    KeybindManager::RegisterKeybind(key, [this]() { callback(isEnabled); });
            }
            prevKey = *key;
            SaveConfig("key", *key);
            g_ConfigManager.SaveConfig();
        }
    } else {
        KeybindManager::UnregisterKeybind(&prevKey);
        if (prevKey != *key) {
            prevKey = *key;
            SaveConfig("key", *key);
            g_ConfigManager.SaveConfig();
            if (*key != -1)
                KeybindManager::RegisterKeybind(key, [this]() { callback(true); });
        }
    }
}