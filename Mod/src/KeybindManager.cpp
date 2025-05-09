#include <algorithm>

#include "KeybindManager.h"
#include "ConfigManager.h"
#include "GlobalDefinitions.h"

int KeybindManager::s_toggleGuiKey = VK_INSERT;
int KeybindManager::s_unbindKey = VK_DELETE;
std::unordered_map<int, std::function<void()>> KeybindManager::s_keybinds;
std::unordered_map<int*, int> KeybindManager::s_keyPtrMap;
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
    int keyCode;
    switch (msg) {
        case WM_KEYDOWN:
            keyCode = static_cast<int>(wParam);
            break;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
            keyCode = VK_MBUTTON;
            break;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK: {
            WORD xbutton = GET_XBUTTON_WPARAM(wParam);
            keyCode = (xbutton == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2);
            break;
        }
        default:
            return false;
    }
    auto it = s_keybinds.find(keyCode);
    if (it != s_keybinds.end()) {
        it->second();
        return true;
    }
    return false;
} 