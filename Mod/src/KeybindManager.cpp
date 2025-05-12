#include <algorithm>
#include <cstdio>

#include "KeybindManager.h"
#include "ConfigManager.h"
#include "GlobalDefinitions.h"

KeybindManager::Callback KeybindManager::s_callbacks[256] = {};
std::vector<std::pair<int*, KeybindManager::Callback>> KeybindManager::s_bindingList;
bool KeybindManager::s_initialized = false;
int KeybindManager::s_toggleGuiKey = VK_INSERT;
int KeybindManager::s_unbindKey = VK_DELETE;

// Static lookup table for key names to optimize runtime resolution
static const char* s_keyNameTable[256];
static void InitKeyNameTable() noexcept {
    for (int i = 0; i < 256; ++i) s_keyNameTable[i] = nullptr;
    s_keyNameTable[VK_LSHIFT]      = "Left Shift";
    s_keyNameTable[VK_RSHIFT]      = "Right Shift";
    s_keyNameTable[VK_SHIFT]       = "Shift";
    s_keyNameTable[VK_CONTROL]     = "Control";
    s_keyNameTable[VK_LCONTROL]    = "Left Control";
    s_keyNameTable[VK_RCONTROL]    = "Right Control";
    s_keyNameTable[VK_MENU]        = "Alt";
    s_keyNameTable[VK_LMENU]       = "Left Alt";
    s_keyNameTable[VK_RMENU]       = "Right Alt";
    s_keyNameTable[VK_BACK]        = "Backspace";
    s_keyNameTable[VK_TAB]         = "Tab";
    s_keyNameTable[VK_RETURN]      = "Enter";
    s_keyNameTable[VK_SPACE]       = "Space";
    s_keyNameTable[VK_CAPITAL]     = "Caps Lock";
    s_keyNameTable[VK_ESCAPE]      = "Escape";
    s_keyNameTable[VK_LEFT]        = "Left";
    s_keyNameTable[VK_UP]          = "Up";
    s_keyNameTable[VK_RIGHT]       = "Right";
    s_keyNameTable[VK_DOWN]        = "Down";
    s_keyNameTable[VK_DELETE]      = "Delete";
    s_keyNameTable[VK_INSERT]      = "Insert";
    s_keyNameTable[VK_HOME]        = "Home";
    s_keyNameTable[VK_END]         = "End";
    s_keyNameTable[VK_PRIOR]       = "Page Up";
    s_keyNameTable[VK_NEXT]        = "Page Down";
    s_keyNameTable[VK_SNAPSHOT]    = "Print Screen";
    s_keyNameTable[VK_SCROLL]      = "Scroll Lock";
    s_keyNameTable[VK_PAUSE]       = "Pause";
    s_keyNameTable[VK_NUMLOCK]     = "Num Lock";
    s_keyNameTable[VK_NUMPAD0]     = "Numpad 0";
    s_keyNameTable[VK_NUMPAD1]     = "Numpad 1";
    s_keyNameTable[VK_NUMPAD2]     = "Numpad 2";
    s_keyNameTable[VK_NUMPAD3]     = "Numpad 3";
    s_keyNameTable[VK_NUMPAD4]     = "Numpad 4";
    s_keyNameTable[VK_NUMPAD5]     = "Numpad 5";
    s_keyNameTable[VK_NUMPAD6]     = "Numpad 6";
    s_keyNameTable[VK_NUMPAD7]     = "Numpad 7";
    s_keyNameTable[VK_NUMPAD8]     = "Numpad 8";
    s_keyNameTable[VK_NUMPAD9]     = "Numpad 9";
    s_keyNameTable[VK_MULTIPLY]    = "Numpad *";
    s_keyNameTable[VK_ADD]         = "Numpad +";
    s_keyNameTable[VK_SUBTRACT]    = "Numpad -";
    s_keyNameTable[VK_DECIMAL]     = "Numpad .";
    s_keyNameTable[VK_DIVIDE]      = "Numpad /";
    s_keyNameTable[VK_OEM_1]       = ";";
    s_keyNameTable[VK_OEM_PLUS]    = "=";
    s_keyNameTable[VK_OEM_COMMA]   = ",";
    s_keyNameTable[VK_OEM_MINUS]   = "-";
    s_keyNameTable[VK_OEM_PERIOD]  = ".";
    s_keyNameTable[VK_OEM_2]       = "/";
    s_keyNameTable[VK_OEM_3]       = "`";
    s_keyNameTable[VK_OEM_4]       = "[";
    s_keyNameTable[VK_OEM_5]       = "\\";
    s_keyNameTable[VK_OEM_6]       = "]";
    s_keyNameTable[VK_OEM_7]       = "'";
    s_keyNameTable[VK_MBUTTON]     = "MMB";
    s_keyNameTable[VK_XBUTTON1]    = "Mouse 4";
    s_keyNameTable[VK_XBUTTON2]    = "Mouse 5";
}
static const struct KeyNameTableInitializer {
    KeyNameTableInitializer() { InitKeyNameTable(); }
} s_keyNameTableInitializer;

void KeybindManager::Initialize() noexcept {
    if (!s_initialized) {
        s_toggleGuiKey = g_ConfigManager.GetInt("Keybinds", "toggle_gui_key", VK_INSERT);
        s_unbindKey = g_ConfigManager.GetInt("Keybinds", "unbind_key", VK_DELETE);
        s_initialized = true;
    }
}

void KeybindManager::RegisterKeybind(int* keyPtr, Callback callback) noexcept {
    auto it = std::find_if(s_bindingList.begin(), s_bindingList.end(),
        [keyPtr](const auto& p) { return p.first == keyPtr; });
    if (it != s_bindingList.end()) {
        int oldCode = *it->first;
        if (oldCode >= 0 && oldCode < 256)
            s_callbacks[oldCode] = nullptr;
        *it = {keyPtr, callback};
    } else {
        s_bindingList.emplace_back(keyPtr, callback);
    }
    int code = *keyPtr;
    if (code >= 0 && code < 256)
        s_callbacks[code] = callback;
}

void KeybindManager::UnregisterKeybind(int* keyPtr) noexcept {
    auto it = std::find_if(s_bindingList.begin(), s_bindingList.end(),
        [keyPtr](const auto& p) { return p.first == keyPtr; });
    if (it != s_bindingList.end()) {
        int code = *it->first;
        if (code >= 0 && code < 256)
            s_callbacks[code] = nullptr;
        s_bindingList.erase(it);
    }
}

void KeybindManager::UpdateBindings() noexcept {
    for (auto& cb : s_callbacks)
        cb = nullptr;
    for (auto& binding : s_bindingList) {
        int code = *binding.first;
        if (code >= 0 && code < 256)
            s_callbacks[code] = binding.second;
    }
}

bool KeybindManager::ProcessKeyEvent(UINT msg, WPARAM wParam) noexcept {
    int keyCode = -1;
    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
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
    if (keyCode >= 0 && keyCode < 256) {
        auto& cb = s_callbacks[keyCode];
        if (cb) {
            cb();
            return true;
        }
    }
    return false;
}

bool KeybindManager::HandleKeyPress(bool& waitingForKey, int& key) noexcept {
    if (!waitingForKey) return false;
    static bool keyPressed[256] = { false };
    for (int i = 0; i < 256; ++i) {
        if (GetAsyncKeyState(i) & 0x8000) {
            keyPressed[i] = true;
        } else if (keyPressed[i]) {
            key = (i == s_unbindKey ? -1 : i);
            waitingForKey = false;
            keyPressed[i] = false;
            return true;
        }
    }
    return false;
}

void KeybindManager::SaveKeybinds() noexcept {
    g_ConfigManager.SetInt("Keybinds", "toggle_gui_key", s_toggleGuiKey);
    g_ConfigManager.SetInt("Keybinds", "unbind_key", s_unbindKey);
    g_ConfigManager.SaveConfig();
}

const char* KeybindManager::GetKeyName(int vKey) noexcept {
    if (vKey == -1) return "Unbound";
    unsigned uv = static_cast<unsigned>(vKey);
    if (uv < 256) {
        const char* name = s_keyNameTable[uv];
        if (name) return name;
    }
    if ((vKey >= '0' && vKey <= '9') || (vKey >= 'A' && vKey <= 'Z')) {
        static char singleChar[2] = {0};
        singleChar[0] = static_cast<char>(vKey);
        singleChar[1] = '\0';
        return singleChar;
    }
    if (vKey >= VK_F1 && vKey <= VK_F12) {
        static char fKeyName[4] = {0};
        sprintf_s(fKeyName, "F%d", vKey - VK_F1 + 1);
        return fKeyName;
    }
    static char keyName[32];
    UINT scanCode = MapVirtualKey(vKey, MAPVK_VK_TO_VSC);
    return GetKeyNameTextA(scanCode << 16, keyName, sizeof(keyName)) > 0 ? keyName : "Unknown";
} 