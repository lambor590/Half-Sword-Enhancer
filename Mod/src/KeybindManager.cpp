#include <algorithm>

#include "KeybindManager.h"
#include "ConfigManager.h"
#include "GlobalDefinitions.h"
#include "Menu/IMenuFunction.h"

KeybindManager::Callback KeybindManager::s_callbacks[256] = {};
std::vector<KeybindManager::Binding> KeybindManager::s_bindingList;
bool KeybindManager::s_initialized = false;
int KeybindManager::s_toggleGuiKey = VK_INSERT;
int KeybindManager::s_unbindKey = VK_DELETE;
std::vector<int> KeybindManager::s_boundCodes;

const char* KeybindManager::s_keyNameTable[256];
static void InitKeyNameTable() noexcept {
    for (int i = 0; i < 256; ++i) KeybindManager::s_keyNameTable[i] = "Unknown";
    KeybindManager::s_keyNameTable[255] = "Unbound";
    KeybindManager::s_keyNameTable[VK_LSHIFT]      = "Left Shift";
    KeybindManager::s_keyNameTable[VK_RSHIFT]      = "Right Shift";
    KeybindManager::s_keyNameTable[VK_SHIFT]       = "Shift";
    KeybindManager::s_keyNameTable[VK_CONTROL]     = "Control";
    KeybindManager::s_keyNameTable[VK_LCONTROL]    = "Left Control";
    KeybindManager::s_keyNameTable[VK_RCONTROL]    = "Right Control";
    KeybindManager::s_keyNameTable[VK_MENU]        = "Alt";
    KeybindManager::s_keyNameTable[VK_LMENU]       = "Left Alt";
    KeybindManager::s_keyNameTable[VK_RMENU]       = "Right Alt";
    KeybindManager::s_keyNameTable[VK_BACK]        = "Backspace";
    KeybindManager::s_keyNameTable[VK_TAB]         = "Tab";
    KeybindManager::s_keyNameTable[VK_RETURN]      = "Enter";
    KeybindManager::s_keyNameTable[VK_SPACE]       = "Space";
    KeybindManager::s_keyNameTable[VK_CAPITAL]     = "Caps Lock";
    KeybindManager::s_keyNameTable[VK_ESCAPE]      = "Escape";
    KeybindManager::s_keyNameTable[VK_LEFT]        = "Left";
    KeybindManager::s_keyNameTable[VK_UP]          = "Up";
    KeybindManager::s_keyNameTable[VK_RIGHT]       = "Right";
    KeybindManager::s_keyNameTable[VK_DOWN]        = "Down";
    KeybindManager::s_keyNameTable[VK_DELETE]      = "Delete";
    KeybindManager::s_keyNameTable[VK_INSERT]      = "Insert";
    KeybindManager::s_keyNameTable[VK_HOME]        = "Home";
    KeybindManager::s_keyNameTable[VK_END]         = "End";
    KeybindManager::s_keyNameTable[VK_PRIOR]       = "Page Up";
    KeybindManager::s_keyNameTable[VK_NEXT]        = "Page Down";
    KeybindManager::s_keyNameTable[VK_SNAPSHOT]    = "Print Screen";
    KeybindManager::s_keyNameTable[VK_SCROLL]      = "Scroll Lock";
    KeybindManager::s_keyNameTable[VK_PAUSE]       = "Pause";
    KeybindManager::s_keyNameTable[VK_NUMLOCK]     = "Num Lock";
    KeybindManager::s_keyNameTable[VK_NUMPAD0]     = "Numpad 0";
    KeybindManager::s_keyNameTable[VK_NUMPAD1]     = "Numpad 1";
    KeybindManager::s_keyNameTable[VK_NUMPAD2]     = "Numpad 2";
    KeybindManager::s_keyNameTable[VK_NUMPAD3]     = "Numpad 3";
    KeybindManager::s_keyNameTable[VK_NUMPAD4]     = "Numpad 4";
    KeybindManager::s_keyNameTable[VK_NUMPAD5]     = "Numpad 5";
    KeybindManager::s_keyNameTable[VK_NUMPAD6]     = "Numpad 6";
    KeybindManager::s_keyNameTable[VK_NUMPAD7]     = "Numpad 7";
    KeybindManager::s_keyNameTable[VK_NUMPAD8]     = "Numpad 8";
    KeybindManager::s_keyNameTable[VK_NUMPAD9]     = "Numpad 9";
    KeybindManager::s_keyNameTable[VK_MULTIPLY]    = "Numpad *";
    KeybindManager::s_keyNameTable[VK_ADD]         = "Numpad +";
    KeybindManager::s_keyNameTable[VK_SUBTRACT]    = "Numpad -";
    KeybindManager::s_keyNameTable[VK_DECIMAL]     = "Numpad .";
    KeybindManager::s_keyNameTable[VK_DIVIDE]      = "Numpad /";
    KeybindManager::s_keyNameTable[VK_OEM_1]       = ";";
    KeybindManager::s_keyNameTable[VK_OEM_PLUS]    = "=";
    KeybindManager::s_keyNameTable[VK_OEM_COMMA]   = ",";
    KeybindManager::s_keyNameTable[VK_OEM_MINUS]   = "-";
    KeybindManager::s_keyNameTable[VK_OEM_PERIOD]  = ".";
    KeybindManager::s_keyNameTable[VK_OEM_2]       = "/";
    KeybindManager::s_keyNameTable[VK_OEM_3]       = "`";
    KeybindManager::s_keyNameTable[VK_OEM_4]       = "[";
    KeybindManager::s_keyNameTable[VK_OEM_5]       = "\\";
    KeybindManager::s_keyNameTable[VK_OEM_6]       = "]";
    KeybindManager::s_keyNameTable[VK_OEM_7]       = "'";
    KeybindManager::s_keyNameTable[VK_MBUTTON]     = "MMB";
    KeybindManager::s_keyNameTable[VK_XBUTTON1]    = "Mouse 4";
    KeybindManager::s_keyNameTable[VK_XBUTTON2]    = "Mouse 5";
    static const char* digitNames[10] = {"0","1","2","3","4","5","6","7","8","9"};
    for (int i = 0; i < 10; ++i) KeybindManager::s_keyNameTable['0' + i] = digitNames[i];
    static const char* letterNames[26] = {"A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z"};
    for (int i = 0; i < 26; ++i) KeybindManager::s_keyNameTable['A' + i] = letterNames[i];
    static const char* fKeyNames[12] = {"F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12"};
    for (int i = 0; i < 12; ++i) KeybindManager::s_keyNameTable[VK_F1 + i] = fKeyNames[i];
}

static const struct KeyNameTableInitializer {
    KeyNameTableInitializer() { InitKeyNameTable(); }
} s_keyNameTableInitializer;

void KeybindManager::Initialize() noexcept {
    if (!s_initialized) {
        s_bindingList.reserve(16);
        s_toggleGuiKey = g_ConfigManager.GetInt("Keybinds", "toggle_gui_key", VK_INSERT);
        s_unbindKey = g_ConfigManager.GetInt("Keybinds", "unbind_key", VK_DELETE);
        s_initialized = true;
    }
}

void KeybindManager::RegisterKeybind(int* keyPtr, Callback callback, IMenuFunction* function) noexcept {
    auto it = std::find_if(s_bindingList.begin(), s_bindingList.end(),
        [keyPtr](const auto& b) { return b.keyPtr == keyPtr; });
    if (it != s_bindingList.end()) {
        int oldCode = *it->keyPtr;
        if (oldCode >= 0 && oldCode < 256)
            s_callbacks[oldCode] = nullptr;
        *it = Binding{keyPtr, callback, function};
    } else {
        s_bindingList.emplace_back(Binding{keyPtr, callback, function});
    }
    int code = *keyPtr;
    if (code >= 0 && code < 256)
        s_callbacks[code] = callback;
}

void KeybindManager::UnregisterKeybind(int* keyPtr) noexcept {
    auto it = std::find_if(s_bindingList.begin(), s_bindingList.end(),
        [keyPtr](const auto& b) { return b.keyPtr == keyPtr; });
    if (it != s_bindingList.end()) {
        int code = *it->keyPtr;
        if (code >= 0 && code < 256)
            s_callbacks[code] = nullptr;
        s_bindingList.erase(it);
    }
}

void KeybindManager::UpdateBindings() noexcept {
    for (int code : s_boundCodes) {
        if (code >= 0 && code < 256)
            s_callbacks[code] = nullptr;
    }
    s_boundCodes.clear();
    for (auto& binding : s_bindingList) {
        int code = *binding.keyPtr;
        if (code >= 0 && code < 256) {
            s_callbacks[code] = binding.callback;
            s_boundCodes.push_back(code);
        }
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

bool KeybindManager::IsKeyBound(int code, int* excludeKeyPtr) noexcept {
    for (auto& b : s_bindingList) {
        if (b.keyPtr != excludeKeyPtr && *b.keyPtr == code)
            return true;
    }
    return false;
}

void KeybindManager::RemoveBinding(int code, int* excludeKeyPtr) noexcept {
    for (auto& b : s_bindingList) {
        if (b.keyPtr != excludeKeyPtr && *b.keyPtr == code) {
            *b.keyPtr = -1;
            if (b.function) {
                if (auto kf = dynamic_cast<KeyFunction*>(b.function)) {
                    kf->ResetPrevKey();
                    kf->SaveConfig("key", *b.keyPtr);
                    g_ConfigManager.SaveConfig();
                }
            }
            UnregisterKeybind(b.keyPtr);
            return;
        }
    }
}

IMenuFunction* KeybindManager::GetBoundFunction(int code, int* excludeKeyPtr) noexcept {
    for (const auto& b : s_bindingList) {
        if (b.keyPtr != excludeKeyPtr && *b.keyPtr == code)
            return b.function;
    }
    return nullptr;
} 