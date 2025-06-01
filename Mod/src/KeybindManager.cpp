#include <algorithm>

#include "KeybindManager.h"
#include "ConfigManager.h"
#include "Menu/IMenuFunction.h"

static constexpr const char* GetKeyNameForIndex(int index) noexcept {
    if (index == 255) return "Unbound";
    
    switch (index) {
    case VK_LSHIFT: return "Left Shift";
    case VK_RSHIFT: return "Right Shift";
    case VK_SHIFT: return "Shift";
    case VK_CONTROL: return "Control";
    case VK_LCONTROL: return "Left Control";
    case VK_RCONTROL: return "Right Control";
    case VK_MENU: return "Alt";
    case VK_LMENU: return "Left Alt";
    case VK_RMENU: return "Right Alt";
    case VK_BACK: return "Backspace";
    case VK_TAB: return "Tab";
    case VK_RETURN: return "Enter";
    case VK_SPACE: return "Space";
    case VK_CAPITAL: return "Caps Lock";
    case VK_ESCAPE: return "Escape";
    case VK_LEFT: return "Left";
    case VK_UP: return "Up";
    case VK_RIGHT: return "Right";
    case VK_DOWN: return "Down";
    case VK_DELETE: return "Delete";
    case VK_INSERT: return "Insert";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "Page Up";
    case VK_NEXT: return "Page Down";
    case VK_SNAPSHOT: return "Print Screen";
    case VK_SCROLL: return "Scroll Lock";
    case VK_PAUSE: return "Pause";
    case VK_NUMLOCK: return "Num Lock";
    case VK_NUMPAD0: return "Numpad 0";
    case VK_NUMPAD1: return "Numpad 1";
    case VK_NUMPAD2: return "Numpad 2";
    case VK_NUMPAD3: return "Numpad 3";
    case VK_NUMPAD4: return "Numpad 4";
    case VK_NUMPAD5: return "Numpad 5";
    case VK_NUMPAD6: return "Numpad 6";
    case VK_NUMPAD7: return "Numpad 7";
    case VK_NUMPAD8: return "Numpad 8";
    case VK_NUMPAD9: return "Numpad 9";
    case VK_MULTIPLY: return "Numpad *";
    case VK_ADD: return "Numpad +";
    case VK_SUBTRACT: return "Numpad -";
    case VK_DECIMAL: return "Numpad .";
    case VK_DIVIDE: return "Numpad /";
    case VK_OEM_1: return ";";
    case VK_OEM_PLUS: return "=";
    case VK_OEM_COMMA: return ",";
    case VK_OEM_MINUS: return "-";
    case VK_OEM_PERIOD: return ".";
    case VK_OEM_2: return "/";
    case VK_OEM_3: return "`";
    case VK_OEM_4: return "[";
    case VK_OEM_5: return "\\";
    case VK_OEM_6: return "]";
    case VK_OEM_7: return "'";
    case VK_MBUTTON: return "MMB";
    case VK_XBUTTON1: return "Mouse 4";
    case VK_XBUTTON2: return "Mouse 5";
    case VK_F1: return "F1";
    case VK_F2: return "F2";
    case VK_F3: return "F3";
    case VK_F4: return "F4";
    case VK_F5: return "F5";
    case VK_F6: return "F6";
    case VK_F7: return "F7";
    case VK_F8: return "F8";
    case VK_F9: return "F9";
    case VK_F10: return "F10";
    case VK_F11: return "F11";
    case VK_F12: return "F12";
    case '0': return "0";
    case '1': return "1";
    case '2': return "2";
    case '3': return "3";
    case '4': return "4";
    case '5': return "5";
    case '6': return "6";
    case '7': return "7";
    case '8': return "8";
    case '9': return "9";
    case 'A': return "A";
    case 'B': return "B";
    case 'C': return "C";
    case 'D': return "D";
    case 'E': return "E";
    case 'F': return "F";
    case 'G': return "G";
    case 'H': return "H";
    case 'I': return "I";
    case 'J': return "J";
    case 'K': return "K";
    case 'L': return "L";
    case 'M': return "M";
    case 'N': return "N";
    case 'O': return "O";
    case 'P': return "P";
    case 'Q': return "Q";
    case 'R': return "R";
    case 'S': return "S";
    case 'T': return "T";
    case 'U': return "U";
    case 'V': return "V";
    case 'W': return "W";
    case 'X': return "X";
    case 'Y': return "Y";
    case 'Z': return "Z";
    default: return "Unknown";
    }
}

const char* KeybindManager::s_keyNameTable[256];

static struct KeyNameTableInitializer {
    KeyNameTableInitializer() noexcept {
        for (int i = 0; i < 256; ++i) {
            KeybindManager::s_keyNameTable[i] = GetKeyNameForIndex(i);
        }
    }
} s_keyNameTableInit;

std::array<KeybindManager::Callback, 256> KeybindManager::s_callbacks{};
std::vector<KeybindManager::Binding> KeybindManager::s_bindingList;
bool KeybindManager::s_initialized = false;
int KeybindManager::s_toggleGuiKey = VK_INSERT;
int KeybindManager::s_unbindKey = VK_DELETE;
std::vector<int> KeybindManager::s_boundCodes;

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
        it->callback = callback;
        it->function = function;
    } else {
        s_bindingList.emplace_back(Binding{keyPtr, callback, function});
    }
    UpdateBindings();
}

void KeybindManager::UnregisterKeybind(int* keyPtr) noexcept {
    auto it = std::find_if(s_bindingList.begin(), s_bindingList.end(),
        [keyPtr](const auto& b) { return b.keyPtr == keyPtr; });
    if (it != s_bindingList.end()) {
        s_bindingList.erase(it);
    }
    UpdateBindings();
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
            if (i == VK_LBUTTON || i == VK_RBUTTON) {
                keyPressed[i] = false; 
                continue; 
            }
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
    auto it = std::find_if(s_bindingList.begin(), s_bindingList.end(),
        [&](const Binding& b) { return b.keyPtr != excludeKeyPtr && *b.keyPtr == code; });
    if (it != s_bindingList.end()) {
        *it->keyPtr = 255;
        UpdateBindings();
    }
}

IMenuFunction* KeybindManager::GetBoundFunction(int code, int* excludeKeyPtr) noexcept {
    for (auto& b : s_bindingList) {
        if (b.keyPtr != excludeKeyPtr && *b.keyPtr == code)
            return b.function;
    }
    return nullptr;
} 