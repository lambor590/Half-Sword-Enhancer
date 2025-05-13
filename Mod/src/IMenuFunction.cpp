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
    for (auto evt : eventTypes)
        g_GameHook->UnregisterEvent(evt, this);
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
        for (auto evt : eventTypes) {
            if (isEnabled)
                g_GameHook->RegisterEvent(evt, this, [this]() { callback(isEnabled); });
            else
                g_GameHook->UnregisterEvent(evt, this);
        }
        if (executeOnToggle) callback(isEnabled);
        g_ConfigManager.SaveConfig();
    }
}

void HookedFunction::LoadConfig() {
    *key = GetConfig("key", *key);
    prevKey = *key;
    if (*key != -1)
        KeybindManager::RegisterKeybind(key, [this]() { SetEnabled(!isEnabled); }, this);
    LoadEnabledState(false);
    if (isEnabled) {
        for (auto evt : eventTypes)
            g_GameHook->RegisterEvent(evt, this, [this]() { callback(isEnabled); });
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
                KeybindManager::RegisterKeybind(key, [this]() { callback(isEnabled); }, this);
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
            KeybindManager::RegisterKeybind(key, [this]() { callback(isEnabled); }, this);
    } else {
        if (*key != -1)
            KeybindManager::RegisterKeybind(key, [this]() { callback(true); }, this);
    }
    LoadParameters();
}

void KeybindFunction::UpdateKey() {
    if (toggleable) {
        if (prevKey != *key) {
            if (isEnabled) {
                KeybindManager::UnregisterKeybind(&prevKey);
                if (*key != -1)
                    KeybindManager::RegisterKeybind(key, [this]() { callback(isEnabled); }, this);
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
                KeybindManager::RegisterKeybind(key, [this]() { callback(true); }, this);
        }
    }
}