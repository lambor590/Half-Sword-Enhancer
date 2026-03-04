#include <algorithm>
#include <array>
#include <ranges>

#include "KeybindManager.h"
#include "ConfigManager.h"
#include "Menu/IMenuFunction.h"
#include "NotificationManager.h"
#include "Gui.h"

std::map<int*, KeybindManager::Binding> KeybindManager::s_bindings;
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

    int currentKey = *keyPtr;
    s_bindings[keyPtr] = {std::move(callback), keyPtr, function, currentKey};

    if (currentKey != -1) {
        s_hotData.keyToBindings[currentKey].push_back(&s_bindings[keyPtr]);
    }

    s_hotData.processingKeyEvent.store(false, std::memory_order_release);
}

void KeybindManager::UnregisterKeybind(int* keyPtr) noexcept {
    auto it = s_bindings.find(keyPtr);
    if (it == s_bindings.end()) return;

    Binding& binding = it->second;
    if (binding.currentKey != -1) {
        auto keyIt = s_hotData.keyToBindings.find(binding.currentKey);
        if (keyIt != s_hotData.keyToBindings.end()) {
            auto& vec = keyIt->second;
            std::erase(vec, &binding);
            if (vec.empty()) {
                s_hotData.keyToBindings.erase(keyIt);
            }
        }
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

    static thread_local std::vector<Binding*> s_bindingCache;
    s_bindingCache.clear();
    s_bindingCache.reserve(it->second.size());

    for (Binding* binding : it->second) {
        s_bindingCache.push_back(binding);
    }

    for (const Binding* binding : s_bindingCache) {
        binding->callback();

        if (IMenuFunction* function = binding->function) [[likely]] {
            if (const auto name = function->GetName(); !name.empty()) [[likely]] {
                if (auto* hookedFunc = dynamic_cast<HookedFunction*>(function)) [[likely]] {
                    NotificationManager::NotifyHookToggle(name, hookedFunc->LoadEnabledState());
                } else {
                    NotificationManager::NotifyOneTimeAction(name);
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

const std::vector<KeybindManager::Binding*>* KeybindManager::FindBindings(int key) noexcept {
    auto it = s_hotData.keyToBindings.find(key);
    return (it != s_hotData.keyToBindings.end()) ? &it->second : nullptr;
}

bool KeybindManager::IsKeyBound(int key, int* excludeKeyPtr) noexcept {
    auto* bindings = FindBindings(key);
    if (!bindings) [[likely]] return false;

    if (!excludeKeyPtr) return !bindings->empty();

    bool hasExcluded = std::ranges::find_if(*bindings,
        [excludeKeyPtr](const Binding* b) { return b->keyPtr == excludeKeyPtr; }) != bindings->end();
    return bindings->size() > (hasExcluded ? 1 : 0);
}

void KeybindManager::RemoveBinding(int key, int* excludeKeyPtr) noexcept {
    auto it = s_hotData.keyToBindings.find(key);
    if (it == s_hotData.keyToBindings.end()) return;

    auto& bindings = it->second;
    auto foundIt = std::ranges::find_if(bindings,
        [excludeKeyPtr](const Binding* b) { return b->keyPtr != excludeKeyPtr; });

    if (foundIt != bindings.end()) {
        Binding* binding = *foundIt;
        *(binding->keyPtr) = 255;
        s_bindings.erase(binding->keyPtr);
        bindings.erase(foundIt);

        if (bindings.empty()) {
            s_hotData.keyToBindings.erase(it);
        }
    }
}

IMenuFunction* KeybindManager::GetBoundFunction(int key, int* excludeKeyPtr) noexcept {
    auto* bindings = FindBindings(key);
    if (!bindings) [[likely]] return nullptr;

    for (const Binding* binding : *bindings) {
        if (binding->keyPtr != excludeKeyPtr) {
            return binding->function;
        }
    }
    return nullptr;
}

std::vector<IMenuFunction*> KeybindManager::GetAllBoundFunctions(int key, int* excludeKeyPtr) noexcept {
    auto* bindings = FindBindings(key);
    if (!bindings) [[likely]] return {};

    std::vector<IMenuFunction*> functions;
    functions.reserve(bindings->size());

    for (const Binding* binding : *bindings) {
        if (binding->keyPtr != excludeKeyPtr && binding->function) {
            functions.push_back(binding->function);
        }
    }
    return functions;
}

int KeybindManager::GetBindingCount(int key, int* excludeKeyPtr) noexcept {
    auto* bindings = FindBindings(key);
    if (!bindings) [[likely]] return 0;

    if (!excludeKeyPtr) return static_cast<int>(bindings->size());

    bool hasExcluded = std::ranges::find_if(*bindings,
        [excludeKeyPtr](const Binding* b) { return b->keyPtr == excludeKeyPtr; }) != bindings->end();
    return static_cast<int>(bindings->size() - (hasExcluded ? 1 : 0));
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

    Binding& binding = it->second;
    int newKey = *keyPtr;
    int oldKey = binding.currentKey;

    if (oldKey == newKey) {
        s_hotData.processingKeyEvent.store(false, std::memory_order_release);
        return;
    }

    if (oldKey != -1) {
        auto& oldBindings = s_hotData.keyToBindings[oldKey];
        std::erase(oldBindings, &binding);
        if (oldBindings.empty()) {
            s_hotData.keyToBindings.erase(oldKey);
        }
    }

    binding.currentKey = newKey;
    if (newKey != -1) {
        s_hotData.keyToBindings[newKey].push_back(&binding);
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
            return true;
        } else if (keyCode == s_coldData.unbindKey) {
            keyCode = -1;
        } else if (!IsValidKey(keyCode)) {
            return false;
        }
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
}

void KeybindManager::CancelRebind() noexcept {
    s_coldData.waitingForRebind = false;
    s_coldData.capturedKey = -1;
    s_coldData.keyWasCaptured = false;
}
