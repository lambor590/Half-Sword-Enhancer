#include <algorithm>

#include "KeybindManager.h"
#include "ConfigManager.h"
#include "GlobalDefinitions.h"

int KeybindManager::s_toggleGuiKey = VK_INSERT;
int KeybindManager::s_unbindKey = VK_DELETE;
std::unordered_map<int*, std::function<void()>> KeybindManager::s_keybinds;
bool KeybindManager::s_initialized = false;

void KeybindManager::Initialize() {
    if (!s_initialized) {
        s_toggleGuiKey = g_ConfigManager.GetInt("Keybinds", "toggle_gui_key", VK_INSERT);
        s_unbindKey = g_ConfigManager.GetInt("Keybinds", "unbind_key", VK_DELETE);
        s_initialized = true;
    }
}

void KeybindManager::SaveKeybinds() {
    g_ConfigManager.SetInt("Keybinds", "toggle_gui_key", s_toggleGuiKey);
    g_ConfigManager.SetInt("Keybinds", "unbind_key", s_unbindKey);
    g_ConfigManager.SaveConfig();
}

bool KeybindManager::ProcessKeyEvent(UINT msg, WPARAM wParam) {
    if (msg == WM_KEYDOWN) {
        auto it = std::find_if(s_keybinds.begin(), s_keybinds.end(), 
            [wParam](const auto& pair) { return *pair.first == wParam; });
        
        if (it != s_keybinds.end()) {
            it->second();
            return true;
        }
    }
    
    return false;
} 