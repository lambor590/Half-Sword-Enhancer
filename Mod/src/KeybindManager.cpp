#include <algorithm>
#include <array>

#include "KeybindManager.h"
#include "ConfigManager.h"
#include "Menu/IMenuFunction.h"
#include "NotificationManager.h"
#include "Gui.h"

static constexpr std::array<const char*, 256> CreateKeyNameTable() noexcept {
    std::array<const char*, 256> table{};
    
    for (int i = 0; i < 256; ++i) {
        table[i] = "Unknown";
    }
    
    table[255] = "Unbound";
    table[VK_LSHIFT] = "Left Shift";
    table[VK_RSHIFT] = "Right Shift";
    table[VK_SHIFT] = "Shift";
    table[VK_CONTROL] = "Control";
    table[VK_LCONTROL] = "Left Control";
    table[VK_RCONTROL] = "Right Control";
    table[VK_MENU] = "Alt";
    table[VK_LMENU] = "Left Alt";
    table[VK_RMENU] = "Right Alt";
    table[VK_BACK] = "Backspace";
    table[VK_TAB] = "Tab";
    table[VK_RETURN] = "Enter";
    table[VK_SPACE] = "Space";
    table[VK_CAPITAL] = "Caps Lock";
    table[VK_ESCAPE] = "Escape";
    table[VK_LEFT] = "Left";
    table[VK_UP] = "Up";
    table[VK_RIGHT] = "Right";
    table[VK_DOWN] = "Down";
    table[VK_DELETE] = "Delete";
    table[VK_INSERT] = "Insert";
    table[VK_HOME] = "Home";
    table[VK_END] = "End";
    table[VK_PRIOR] = "Page Up";
    table[VK_NEXT] = "Page Down";
    table[VK_SNAPSHOT] = "Print Screen";
    table[VK_SCROLL] = "Scroll Lock";
    table[VK_PAUSE] = "Pause";
    table[VK_NUMLOCK] = "Num Lock";
    table[VK_NUMPAD0] = "Numpad 0";
    table[VK_NUMPAD1] = "Numpad 1";
    table[VK_NUMPAD2] = "Numpad 2";
    table[VK_NUMPAD3] = "Numpad 3";
    table[VK_NUMPAD4] = "Numpad 4";
    table[VK_NUMPAD5] = "Numpad 5";
    table[VK_NUMPAD6] = "Numpad 6";
    table[VK_NUMPAD7] = "Numpad 7";
    table[VK_NUMPAD8] = "Numpad 8";
    table[VK_NUMPAD9] = "Numpad 9";
    table[VK_MULTIPLY] = "Numpad *";
    table[VK_ADD] = "Numpad +";
    table[VK_SUBTRACT] = "Numpad -";
    table[VK_DECIMAL] = "Numpad .";
    table[VK_DIVIDE] = "Numpad /";
    table[VK_OEM_1] = ";";
    table[VK_OEM_PLUS] = "=";
    table[VK_OEM_COMMA] = ",";
    table[VK_OEM_MINUS] = "-";
    table[VK_OEM_PERIOD] = ".";
    table[VK_OEM_2] = "/";
    table[VK_OEM_3] = "`";
    table[VK_OEM_4] = "[";
    table[VK_OEM_5] = "\\";
    table[VK_OEM_6] = "]";
    table[VK_OEM_7] = "'";
    table[VK_MBUTTON] = "MMB";
    table[VK_XBUTTON1] = "Mouse 4";
    table[VK_XBUTTON2] = "Mouse 5";
    table[VK_F1] = "F1";
    table[VK_F2] = "F2";
    table[VK_F3] = "F3";
    table[VK_F4] = "F4";
    table[VK_F5] = "F5";
    table[VK_F6] = "F6";
    table[VK_F7] = "F7";
    table[VK_F8] = "F8";
    table[VK_F9] = "F9";
    table[VK_F10] = "F10";
    table[VK_F11] = "F11";
    table[VK_F12] = "F12";
    table['0'] = "0";
    table['1'] = "1";
    table['2'] = "2";
    table['3'] = "3";
    table['4'] = "4";
    table['5'] = "5";
    table['6'] = "6";
    table['7'] = "7";
    table['8'] = "8";
    table['9'] = "9";
    table['A'] = "A";
    table['B'] = "B";
    table['C'] = "C";
    table['D'] = "D";
    table['E'] = "E";
    table['F'] = "F";
    table['G'] = "G";
    table['H'] = "H";
    table['I'] = "I";
    table['J'] = "J";
    table['K'] = "K";
    table['L'] = "L";
    table['M'] = "M";
    table['N'] = "N";
    table['O'] = "O";
    table['P'] = "P";
    table['Q'] = "Q";
    table['R'] = "R";
    table['S'] = "S";
    table['T'] = "T";
    table['U'] = "U";
    table['V'] = "V";
    table['W'] = "W";
    table['X'] = "X";
    table['Y'] = "Y";
    table['Z'] = "Z";
    
    return table;
}

static constexpr auto s_keyNameLookup = CreateKeyNameTable();

static const char* GetKeyNameForIndex(int index) noexcept {
    return s_keyNameLookup[static_cast<unsigned char>(index)];
}

const char* KeybindManager::s_keyNameTable[256];

static struct KeyNameTableInitializer {
    KeyNameTableInitializer() noexcept {
        for (int i = 0; i < 256; ++i) {
            KeybindManager::s_keyNameTable[i] = s_keyNameLookup[i];
        }
    }
} s_keyNameTableInit;

std::unordered_map<int*, KeybindManager::Binding> KeybindManager::s_bindings;
std::unordered_map<int, std::vector<KeybindManager::Binding*>> KeybindManager::s_keyToBindings;
bool KeybindManager::s_initialized = false;
int KeybindManager::s_toggleGuiKey = VK_INSERT;
int KeybindManager::s_unbindKey = VK_DELETE;

void KeybindManager::Initialize() noexcept {
    if (!s_initialized) {
        s_toggleGuiKey = g_ConfigManager.GetInt("Keybinds", "toggle_gui_key", VK_INSERT);
        s_unbindKey = g_ConfigManager.GetInt("Keybinds", "unbind_key", VK_DELETE);
        s_initialized = true;
    }
}

void KeybindManager::RegisterKeybind(int* keyPtr, Callback callback, IMenuFunction* function) noexcept {
    auto& binding = s_bindings[keyPtr];
    binding = {keyPtr, callback, function};
    
    if (*keyPtr != -1) {
        s_keyToBindings[*keyPtr].push_back(&binding);
    }
}

void KeybindManager::UnregisterKeybind(int* keyPtr) noexcept {
    auto it = s_bindings.find(keyPtr);
    if (it != s_bindings.end()) {
        int key = *it->second.keyPtr;
        if (key != -1) {
            auto& keyBindings = s_keyToBindings[key];
            keyBindings.erase(std::remove_if(keyBindings.begin(), keyBindings.end(),
                [keyPtr](const Binding* b) { return b->keyPtr == keyPtr; }), keyBindings.end());
            
            if (keyBindings.empty()) {
                s_keyToBindings.erase(key);
            }
        }
        s_bindings.erase(it);
    }
}

bool KeybindManager::ProcessKeyEvent(UINT msg, WPARAM wParam) noexcept {
    int keyCode;
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
    case WM_XBUTTONDBLCLK:
        keyCode = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
        break;
    default:
        return false;
    }
    
    if (keyCode == s_toggleGuiKey) {
        Gui::ToggleVisibility();
        return true;
    }
    
    const auto it = s_keyToBindings.find(keyCode);
    if (it == s_keyToBindings.end()) [[likely]] return false;
    
    const auto& bindings = it->second;
    for (const auto* binding : bindings) {
        if (!binding) [[unlikely]] continue;
        
        binding->callback();
        
        if (binding->function) [[likely]] {
            const auto name = binding->function->GetName();
            if (!name.empty()) [[likely]] {
                const std::string functionName{name};
                
                if (auto hookedFunc = dynamic_cast<HookedFunction*>(binding->function)) [[likely]] {
                    NotificationManager::NotifyHookToggle(functionName, hookedFunc->LoadEnabledState());
                } else {
                    NotificationManager::NotifyOneTimeAction(functionName);
                }
            }
        }
    }
    return true;
}

bool KeybindManager::HandleKeyPress(bool& waitingForKey, int& key) noexcept {
    if (!waitingForKey) return false;
    
    static bool keyPressed[256] = { false };
    static constexpr int relevantKeys[] = {
        VK_ESCAPE, VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        VK_SPACE, VK_RETURN, VK_TAB, VK_SHIFT, VK_CONTROL, VK_MENU,
        VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_INSERT, VK_DELETE, VK_HOME, VK_END, VK_PRIOR, VK_NEXT,
        VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2
    };
    
    for (int vKey : relevantKeys) {
        const bool isCurrentlyPressed = (GetAsyncKeyState(vKey) & 0x8000) != 0;
        
        if (isCurrentlyPressed) {
            keyPressed[vKey] = true;
        } else if (keyPressed[vKey]) {
            keyPressed[vKey] = false;
            key = (vKey == s_unbindKey) ? -1 : vKey;
            waitingForKey = false;
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

bool KeybindManager::IsKeyBound(int key, int* excludeKeyPtr) noexcept {
    const auto it = s_keyToBindings.find(key);
    if (it == s_keyToBindings.end()) [[likely]] return false;
    
    for (const auto* binding : it->second) {
        if (binding && binding->keyPtr && binding->keyPtr != excludeKeyPtr) [[likely]] {
            return true;
        }
    }
    return false;
}

void KeybindManager::RemoveBinding(int key, int* excludeKeyPtr) noexcept {
    auto it = s_keyToBindings.find(key);
    if (it != s_keyToBindings.end()) {
        auto& keyBindings = it->second;
        for (auto bindingIt = keyBindings.begin(); bindingIt != keyBindings.end(); ++bindingIt) {
            auto* binding = *bindingIt;
            if (binding && binding->keyPtr && binding->keyPtr != excludeKeyPtr) {
                *binding->keyPtr = 255;
                keyBindings.erase(bindingIt);
                s_bindings.erase(binding->keyPtr);
                
                if (keyBindings.empty()) {
                    s_keyToBindings.erase(key);
                }
                return;
            }
        }
    }
}

IMenuFunction* KeybindManager::GetBoundFunction(int key, int* excludeKeyPtr) noexcept {
    auto it = s_keyToBindings.find(key);
    if (it != s_keyToBindings.end()) {
        for (auto* binding : it->second) {
            if (binding && binding->keyPtr && binding->keyPtr != excludeKeyPtr) {
                return binding->function;
            }
        }
    }
    return nullptr;
}

std::vector<IMenuFunction*> KeybindManager::GetAllBoundFunctions(int key, int* excludeKeyPtr) noexcept {
    const auto it = s_keyToBindings.find(key);
    if (it == s_keyToBindings.end()) [[likely]] return {};
    
    std::vector<IMenuFunction*> functions;
    functions.reserve(it->second.size());
    
    for (const auto* binding : it->second) {
        if (binding && binding->keyPtr && binding->keyPtr != excludeKeyPtr && binding->function) [[likely]] {
            functions.push_back(binding->function);
        }
    }
    return functions;
}

int KeybindManager::GetBindingCount(int key, int* excludeKeyPtr) noexcept {
    const auto it = s_keyToBindings.find(key);
    if (it == s_keyToBindings.end()) [[likely]] return 0;
    
    int count = 0;
    for (const auto* binding : it->second) {
        if (binding && binding->keyPtr && binding->keyPtr != excludeKeyPtr) [[likely]] {
            ++count;
        }
    }
    return count;
} 