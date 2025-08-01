#include <algorithm>
#include <array>

#include "KeybindManager.h"
#include "ConfigManager.h"
#include "Menu/IMenuFunction.h"
#include "NotificationManager.h"
#include "Gui.h"

const char* KeybindManager::s_keyNameTable[256];

std::unordered_map<int*, KeybindManager::Binding> KeybindManager::s_bindings;
std::unordered_map<int, std::unordered_set<int*>> KeybindManager::s_keyToBindings;
bool KeybindManager::s_initialized = false;
int KeybindManager::s_toggleGuiKey = VK_INSERT;
int KeybindManager::s_unbindKey = VK_DELETE;
bool KeybindManager::s_processingKeyEvent = false;

void KeybindManager::Initialize() noexcept {
    if (!s_initialized) {
        s_toggleGuiKey = g_ConfigManager.GetInt("Keybinds", "toggle_gui_key", VK_INSERT);
        s_unbindKey = g_ConfigManager.GetInt("Keybinds", "unbind_key", VK_DELETE);
        s_initialized = true;
    }
}

void KeybindManager::RegisterKeybind(int* keyPtr, Callback callback, IMenuFunction* function) noexcept {
    if (s_processingKeyEvent) [[unlikely]] return;
    
    UnregisterKeybind(keyPtr);
    
    s_bindings[keyPtr] = {keyPtr, std::move(callback), function};
    
    if (*keyPtr != -1) {
        s_keyToBindings[*keyPtr].insert(keyPtr);
    }
}

void KeybindManager::UnregisterKeybind(int* keyPtr) noexcept {
    if (s_processingKeyEvent) [[unlikely]] return;
    
    auto it = s_bindings.find(keyPtr);
    if (it == s_bindings.end()) return;
    
    int currentKey = *keyPtr;
    if (currentKey != -1) {
        auto keyIt = s_keyToBindings.find(currentKey);
        if (keyIt != s_keyToBindings.end()) {
            keyIt->second.erase(keyPtr);
            if (keyIt->second.empty()) {
                s_keyToBindings.erase(keyIt);
            }
        }
    }
    
    s_bindings.erase(it);
}

bool KeybindManager::ProcessKeyEvent(UINT msg, WPARAM wParam) noexcept {
    if (s_processingKeyEvent) [[unlikely]] return false;
    
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
    
    if (keyCode == s_toggleGuiKey) [[unlikely]] {
        Gui::ToggleVisibility();
        return true;
    }
    
    const auto it = s_keyToBindings.find(keyCode);
    if (it == s_keyToBindings.end()) [[likely]] return false;
    
    s_processingKeyEvent = true;
    
    std::vector<std::pair<Callback, IMenuFunction*>> callbacksToExecute;
    callbacksToExecute.reserve(it->second.size());
    
    for (int* keyPtr : it->second) {
        const auto bindingIt = s_bindings.find(keyPtr);
        if (bindingIt != s_bindings.end()) [[likely]] {
            callbacksToExecute.emplace_back(bindingIt->second.callback, bindingIt->second.function);
        }
    }
    
    for (const auto& [callback, function] : callbacksToExecute) {
        callback();
        
        if (function) [[likely]] {
            if (const auto name = function->GetName(); !name.empty()) [[likely]] {
                if (auto* hookedFunc = dynamic_cast<HookedFunction*>(function)) [[likely]] {
                    NotificationManager::NotifyHookToggle(std::string{name}, hookedFunc->LoadEnabledState());
                } else {
                    NotificationManager::NotifyOneTimeAction(std::string{name});
                }
            }
        }
    }
    
    s_processingKeyEvent = false;
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
    
    static constexpr size_t numRelevantKeys = sizeof(relevantKeys) / sizeof(relevantKeys[0]);
    static bool asyncKeyStates[numRelevantKeys];
    
    for (size_t i = 0; i < numRelevantKeys; ++i) {
        const int vKey = relevantKeys[i];
        const bool isCurrentlyPressed = (GetAsyncKeyState(vKey) & 0x8000) != 0;
        
        if (isCurrentlyPressed && !asyncKeyStates[i]) {
            asyncKeyStates[i] = true;
        } else if (!isCurrentlyPressed && asyncKeyStates[i]) {
            asyncKeyStates[i] = false;
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
    
    if (!excludeKeyPtr) return !it->second.empty();
    
    return it->second.size() > (it->second.contains(excludeKeyPtr) ? 1 : 0);
}

void KeybindManager::RemoveBinding(int key, int* excludeKeyPtr) noexcept {
    auto it = s_keyToBindings.find(key);
    if (it == s_keyToBindings.end()) return;
    
    auto& keyBindings = it->second;
    for (int* keyPtr : keyBindings) {
        if (keyPtr != excludeKeyPtr) {
            *keyPtr = 255;
            s_bindings.erase(keyPtr);
            keyBindings.erase(keyPtr);
            
            if (keyBindings.empty()) {
                s_keyToBindings.erase(it);
            }
            return;
        }
    }
}

IMenuFunction* KeybindManager::GetBoundFunction(int key, int* excludeKeyPtr) noexcept {
    const auto it = s_keyToBindings.find(key);
    if (it == s_keyToBindings.end()) [[likely]] return nullptr;
    
    for (int* keyPtr : it->second) {
        if (keyPtr != excludeKeyPtr) {
            const auto bindingIt = s_bindings.find(keyPtr);
            if (bindingIt != s_bindings.end()) [[likely]] {
                return bindingIt->second.function;
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
    
    for (int* keyPtr : it->second) {
        if (keyPtr != excludeKeyPtr) {
            const auto bindingIt = s_bindings.find(keyPtr);
            if (bindingIt != s_bindings.end()) [[likely]] {
                if (auto* function = bindingIt->second.function) {
                    functions.push_back(function);
                }
            }
        }
    }
    return functions;
}

int KeybindManager::GetBindingCount(int key, int* excludeKeyPtr) noexcept {
    const auto it = s_keyToBindings.find(key);
    if (it == s_keyToBindings.end()) [[likely]] return 0;
    
    if (!excludeKeyPtr) return static_cast<int>(it->second.size());
    
    return static_cast<int>(it->second.size() - (it->second.contains(excludeKeyPtr) ? 1 : 0));
}

void KeybindManager::UpdateBinding(int* keyPtr) noexcept {
    if (s_processingKeyEvent) [[unlikely]] return;
    
    auto it = s_bindings.find(keyPtr);
    if (it == s_bindings.end()) return;
    
    int newKey = *keyPtr;
    
    for (auto& [key, bindings] : s_keyToBindings) {
        if (bindings.contains(keyPtr)) {
            if (key == newKey) return;
            bindings.erase(keyPtr);
            if (bindings.empty()) {
                s_keyToBindings.erase(key);
            }
            break;
        }
    }
    
    if (newKey != -1) {
        s_keyToBindings[newKey].insert(keyPtr);
    }
} 