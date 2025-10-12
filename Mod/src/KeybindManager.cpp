#include <algorithm>
#include <array>

#include "KeybindManager.h"
#include "ConfigManager.h"
#include "Menu/IMenuFunction.h"
#include "NotificationManager.h"
#include "Gui.h"

std::unordered_map<int*, KeybindManager::Binding> KeybindManager::s_bindings;
std::unordered_map<int*, int> KeybindManager::s_ptrToCurrentKey;
bool KeybindManager::s_initialized = false;

KeybindManager::HotData KeybindManager::s_hotData;
KeybindManager::ColdData KeybindManager::s_coldData;

void KeybindManager::Initialize() noexcept {
    if (!s_initialized) {
        int loadedToggleKey = g_ConfigManager.GetInt("Keybinds", "toggle_gui_key", VK_INSERT);
        int loadedUnbindKey = g_ConfigManager.GetInt("Keybinds", "unbind_key", VK_DELETE);

        s_hotData.toggleGuiKey = IsValidKey(loadedToggleKey) ? loadedToggleKey : VK_INSERT;
        s_coldData.unbindKey = IsValidKey(loadedUnbindKey) ? loadedUnbindKey : VK_DELETE;

        s_initialized = true;
    }
}

void KeybindManager::RegisterKeybind(int* keyPtr, Callback callback, IMenuFunction* function) noexcept {
    bool expected = false;
    if (!s_hotData.processingKeyEvent.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
        return;
    }

    UnregisterKeybind(keyPtr);

    s_bindings[keyPtr] = {std::move(callback), keyPtr, function};

    if (*keyPtr != -1) {
        s_hotData.keyToBindings[*keyPtr].insert(keyPtr);
        s_ptrToCurrentKey[keyPtr] = *keyPtr;
    }

    s_hotData.processingKeyEvent.store(false, std::memory_order_release);
}

void KeybindManager::UnregisterKeybind(int* keyPtr) noexcept {
    auto it = s_bindings.find(keyPtr);
    if (it == s_bindings.end()) return;

    int currentKey = *keyPtr;
    if (currentKey != -1) {
        auto keyIt = s_hotData.keyToBindings.find(currentKey);
        if (keyIt != s_hotData.keyToBindings.end()) {
            keyIt->second.erase(keyPtr);
            if (keyIt->second.empty()) {
                s_hotData.keyToBindings.erase(keyIt);
            }
        }
        s_ptrToCurrentKey.erase(keyPtr);
    }

    s_bindings.erase(it);
}

constexpr bool KeybindManager::IsRelevantMessage(UINT msg) noexcept {
    return (msg == WM_KEYDOWN) ||
           (msg == WM_SYSKEYDOWN) ||
           (msg == WM_MBUTTONDOWN) ||
           (msg == WM_MBUTTONDBLCLK) ||
           (msg == WM_XBUTTONDOWN) ||
           (msg == WM_XBUTTONDBLCLK);
}

int KeybindManager::ExtractKeyCode(UINT msg, WPARAM wParam) noexcept {
    switch (msg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            return static_cast<int>(wParam);

        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
            return VK_MBUTTON;

        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
            return (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;

        default:
            return -1;
    }
}

bool KeybindManager::ProcessKeyEvent(UINT msg, WPARAM wParam) noexcept {
    if (!IsRelevantMessage(msg)) [[likely]] {
        return false;
    }

    int keyCode = ExtractKeyCode(msg, wParam);
    if (keyCode == -1) return false;

    if (keyCode == s_hotData.toggleGuiKey) [[unlikely]] {
        Gui::ToggleVisibility();
        return true;
    }

    const auto it = s_hotData.keyToBindings.find(keyCode);
    if (it == s_hotData.keyToBindings.end()) [[likely]] return false;

    bool expected = false;
    if (!s_hotData.processingKeyEvent.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
        return false;
    }

    static thread_local std::vector<std::pair<Callback, IMenuFunction*>> s_callbackPool;
    s_callbackPool.clear();
    s_callbackPool.reserve(it->second.size());

    for (int* keyPtr : it->second) {
        const auto bindingIt = s_bindings.find(keyPtr);
        if (bindingIt != s_bindings.end()) [[likely]] {
            s_callbackPool.emplace_back(bindingIt->second.callback, bindingIt->second.function);
        }
    }

    for (const auto& [callback, function] : s_callbackPool) {
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

    s_hotData.processingKeyEvent.store(false, std::memory_order_release);
    return true;
}

bool KeybindManager::HandleKeyPress(bool& waitingForKey, int& key) noexcept {
    if (!waitingForKey) {
        return false;
    }

    static bool previousWaitingState = false;

    if (waitingForKey && !previousWaitingState) {
        StartWaitingForRebind();
        previousWaitingState = true;
    }

    if (s_coldData.keyWasCaptured) {
        key = s_coldData.capturedKey;
        waitingForKey = false;
        previousWaitingState = false;

        s_coldData.keyWasCaptured = false;
        s_coldData.capturedKey = -1;

        return true;
    }

    if (!waitingForKey) {
        previousWaitingState = false;
    }

    return false;
}

void KeybindManager::SaveKeybinds() noexcept {
    g_ConfigManager.SetInt("Keybinds", "toggle_gui_key", s_hotData.toggleGuiKey);
    g_ConfigManager.SetInt("Keybinds", "unbind_key", s_coldData.unbindKey);
    g_ConfigManager.SaveConfig();
}

bool KeybindManager::IsKeyBound(int key, int* excludeKeyPtr) noexcept {
    const auto it = s_hotData.keyToBindings.find(key);
    if (it == s_hotData.keyToBindings.end()) [[likely]] return false;

    if (!excludeKeyPtr) return !it->second.empty();

    return it->second.size() > (it->second.contains(excludeKeyPtr) ? 1 : 0);
}

void KeybindManager::RemoveBinding(int key, int* excludeKeyPtr) noexcept {
    auto it = s_hotData.keyToBindings.find(key);
    if (it == s_hotData.keyToBindings.end()) return;

    auto& keyBindings = it->second;
    for (int* keyPtr : keyBindings) {
        if (keyPtr != excludeKeyPtr) {
            *keyPtr = 255;
            s_bindings.erase(keyPtr);
            s_ptrToCurrentKey.erase(keyPtr);
            keyBindings.erase(keyPtr);

            if (keyBindings.empty()) {
                s_hotData.keyToBindings.erase(it);
            }
            return;
        }
    }
}

IMenuFunction* KeybindManager::GetBoundFunction(int key, int* excludeKeyPtr) noexcept {
    const auto it = s_hotData.keyToBindings.find(key);
    if (it == s_hotData.keyToBindings.end()) [[likely]] return nullptr;

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
    const auto it = s_hotData.keyToBindings.find(key);
    if (it == s_hotData.keyToBindings.end()) [[likely]] return {};

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
    const auto it = s_hotData.keyToBindings.find(key);
    if (it == s_hotData.keyToBindings.end()) [[likely]] return 0;

    if (!excludeKeyPtr) return static_cast<int>(it->second.size());

    return static_cast<int>(it->second.size() - (it->second.contains(excludeKeyPtr) ? 1 : 0));
}

void KeybindManager::UpdateBinding(int* keyPtr) noexcept {
    bool expected = false;
    if (!s_hotData.processingKeyEvent.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
        return;
    }

    auto it = s_bindings.find(keyPtr);
    if (it == s_bindings.end()) {
        s_hotData.processingKeyEvent.store(false, std::memory_order_release);
        return;
    }

    int newKey = *keyPtr;

    auto currentKeyIt = s_ptrToCurrentKey.find(keyPtr);
    if (currentKeyIt != s_ptrToCurrentKey.end()) {
        int oldKey = currentKeyIt->second;
        if (oldKey == newKey) {
            s_hotData.processingKeyEvent.store(false, std::memory_order_release);
            return;
        }

        auto& oldBindings = s_hotData.keyToBindings[oldKey];
        oldBindings.erase(keyPtr);
        if (oldBindings.empty()) {
            s_hotData.keyToBindings.erase(oldKey);
        }
    }

    if (newKey != -1) {
        s_hotData.keyToBindings[newKey].insert(keyPtr);
        s_ptrToCurrentKey[keyPtr] = newKey;
    } else {
        s_ptrToCurrentKey.erase(keyPtr);
    }

    s_hotData.processingKeyEvent.store(false, std::memory_order_release);
}

bool KeybindManager::IsValidKey(int key) noexcept {
    if (key < 0 || key > 255) return false;
    return s_validKeys[key];
}

bool KeybindManager::ProcessRebindEvent(UINT msg, WPARAM wParam) noexcept {
    if (!s_coldData.waitingForRebind) return false;

    int keyCode = ExtractKeyCode(msg, wParam);
    if (keyCode == -1) return false;

    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        if (keyCode == VK_ESCAPE) {
            CancelRebind();
            s_coldData.capturedKey = -1;
            s_coldData.keyWasCaptured = true;
            return true;
        }

        if (keyCode == s_coldData.unbindKey) {
            s_coldData.capturedKey = -1;
            s_coldData.keyWasCaptured = true;
            s_coldData.waitingForRebind = false;
            return true;
        }

        if (IsValidKey(keyCode)) {
            s_coldData.capturedKey = keyCode;
            s_coldData.keyWasCaptured = true;
            s_coldData.waitingForRebind = false;
            return true;
        }

        return false;
    }

    s_coldData.capturedKey = keyCode;
    s_coldData.keyWasCaptured = true;
    s_coldData.waitingForRebind = false;
    return true;
}

void KeybindManager::StartWaitingForRebind() noexcept {
    s_coldData.waitingForRebind = true;
    s_coldData.capturedKey = -1;
    s_coldData.keyWasCaptured = false;
    ResetKeyStates();
}

void KeybindManager::CancelRebind() noexcept {
    s_coldData.waitingForRebind = false;
    s_coldData.capturedKey = -1;
    s_coldData.keyWasCaptured = false;
}

void KeybindManager::ResetKeyStates() noexcept {
    static constexpr int relevantKeys[] = {
        VK_ESCAPE, VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        VK_SPACE, VK_RETURN, VK_TAB, VK_SHIFT, VK_CONTROL, VK_MENU,
        VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_INSERT, VK_DELETE, VK_HOME, VK_END, VK_PRIOR, VK_NEXT,
        VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2
    };

    static constexpr size_t numRelevantKeys = sizeof(relevantKeys) / sizeof(relevantKeys[0]);
    static bool asyncKeyStates[numRelevantKeys] = { false };

    std::fill(std::begin(asyncKeyStates), std::end(asyncKeyStates), false);
}
