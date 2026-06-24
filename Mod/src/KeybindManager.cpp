#include <algorithm>
#include <array>
#include <ranges>

#include "KeybindManager.h"
#include "ConfigManager.h"
#include "NotificationManager.h"
#include "Gui.h"

bool KeybindManager::s_initialized = false;

KeybindManager::HotData KeybindManager::s_hotData;
KeybindManager::ColdData KeybindManager::s_coldData;

namespace {
    constexpr bool IsIndexedKey(int key) noexcept {
        return key >= 0 && key < 256;
    }
}

std::map<int*, KeybindManager::Binding>& KeybindManager::Bindings() {
    static std::map<int*, Binding> bindings;
    return bindings;
}

void KeybindManager::Initialize() noexcept {
    if (!s_initialized) {
        int loadedToggleKey = ConfigManager::Get().GetInt("Keybinds", "toggle_gui_key", VK_INSERT);
        int loadedUnbindKey = ConfigManager::Get().GetInt("Keybinds", "unbind_key", VK_DELETE);

        s_hotData.toggleGuiKey = IsValidKey(loadedToggleKey) ? loadedToggleKey : VK_INSERT;
        s_coldData.unbindKey = IsValidKey(loadedUnbindKey) ? loadedUnbindKey : VK_DELETE;

        s_initialized = true;
    }
}

void KeybindManager::RegisterKeybind(
    int* keyPtr, Callback callback, std::string name, bool isToggle, Callback onUnbound
) {
    bool expected = false;
    if (!s_hotData.processingKeyEvent.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
        return;
    }

    UnregisterKeybind(keyPtr);

    int currentKey = *keyPtr;
    auto& bindings = Bindings();
    bindings[keyPtr] = {std::move(callback), keyPtr, std::move(name), isToggle, currentKey, std::move(onUnbound)};

    if (IsIndexedKey(currentKey)) {
        s_hotData.keyToBindings[static_cast<size_t>(currentKey)].push_back(&bindings[keyPtr]);
    }

    s_hotData.processingKeyEvent.store(false, std::memory_order_release);
}

void KeybindManager::UnregisterKeybind(int* keyPtr) {
    auto& bindings = Bindings();
    auto it = bindings.find(keyPtr);
    if (it == bindings.end()) return;

    Binding& binding = it->second;
    if (IsIndexedKey(binding.currentKey)) {
        auto& vec = s_hotData.keyToBindings[static_cast<size_t>(binding.currentKey)];
        std::erase(vec, &binding);
    }

    bindings.erase(it);
}

constexpr bool KeybindManager::IsRelevantMessage(UINT msg) noexcept {
    return (msg == WM_KEYDOWN) || (msg == WM_SYSKEYDOWN) || (msg == WM_MBUTTONDOWN) || (msg == WM_MBUTTONDBLCLK) ||
           (msg == WM_XBUTTONDOWN) || (msg == WM_XBUTTONDBLCLK);
}

int KeybindManager::ExtractKeyCode(UINT msg, WPARAM wParam) noexcept {
    switch (msg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: return static_cast<int>(wParam);

        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK: return VK_MBUTTON;

        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK: return (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;

        default: return -1;
    }
}

bool KeybindManager::ProcessKeyEvent(UINT msg, WPARAM wParam) {
    if (!IsRelevantMessage(msg)) [[likely]] {
        return false;
    }

    int keyCode = ExtractKeyCode(msg, wParam);
    if (keyCode == -1) return false;

    if (keyCode == s_hotData.toggleGuiKey) [[unlikely]] {
        Gui::ToggleVisibility();
        return true;
    }

    const auto* bindings = FindBindings(keyCode);
    if (!bindings) [[likely]]
        return false;

    bool expected = false;
    if (!s_hotData.processingKeyEvent.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
        return false;
    }

    static thread_local std::vector<Binding*> bindingCache;
    bindingCache.clear();
    bindingCache.reserve(bindings->size());

    for (Binding* binding : *bindings) {
        bindingCache.push_back(binding);
    }

    for (const Binding* binding : bindingCache) {
        binding->callback();

        if (!binding->name.empty()) [[likely]] {
            if (binding->isToggle) {
                NotificationManager::NotifyHookToggle(binding->name, true);
            } else {
                NotificationManager::NotifyOneTimeAction(binding->name);
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
    ConfigManager::Get().SetInt("Keybinds", "toggle_gui_key", s_hotData.toggleGuiKey);
    ConfigManager::Get().SetInt("Keybinds", "unbind_key", s_coldData.unbindKey);
    ConfigManager::Get().SaveConfig();
}

const std::vector<KeybindManager::Binding*>* KeybindManager::FindBindings(int key) noexcept {
    if (!IsIndexedKey(key)) return nullptr;
    const auto& bindings = s_hotData.keyToBindings[static_cast<size_t>(key)];
    return bindings.empty() ? nullptr : &bindings;
}

bool KeybindManager::IsKeyBound(int key, int* excludeKeyPtr) noexcept {
    auto* bindings = FindBindings(key);
    if (!bindings) [[likely]]
        return false;

    if (!excludeKeyPtr) return !bindings->empty();

    bool hasExcluded = std::ranges::find_if(*bindings, [excludeKeyPtr](const Binding* b) {
                           return b->keyPtr == excludeKeyPtr;
                       }) != bindings->end();
    return bindings->size() > (hasExcluded ? 1 : 0);
}

void KeybindManager::RemoveBinding(int key, int* excludeKeyPtr) {
    if (!IsIndexedKey(key)) return;

    auto& keyBindings = s_hotData.keyToBindings[static_cast<size_t>(key)];
    if (keyBindings.empty()) return;

    auto foundIt =
        std::ranges::find_if(keyBindings, [excludeKeyPtr](const Binding* b) { return b->keyPtr != excludeKeyPtr; });

    if (foundIt != keyBindings.end()) {
        Binding* binding = *foundIt;
        *(binding->keyPtr) = -1;
        if (binding->onUnbound) {
            binding->onUnbound();
        }
        Bindings().erase(binding->keyPtr);
        keyBindings.erase(foundIt);
    }
}

std::string KeybindManager::GetBoundName(int key, int* excludeKeyPtr) {
    auto* bindings = FindBindings(key);
    if (!bindings) [[likely]]
        return {};

    for (const Binding* binding : *bindings) {
        if (binding->keyPtr != excludeKeyPtr) {
            return binding->name;
        }
    }
    return {};
}

std::vector<std::string> KeybindManager::GetAllBoundNames(int key, int* excludeKeyPtr) {
    auto* bindings = FindBindings(key);
    if (!bindings) [[likely]]
        return {};

    std::vector<std::string> names;
    names.reserve(bindings->size());

    for (const Binding* binding : *bindings) {
        if (binding->keyPtr != excludeKeyPtr && !binding->name.empty()) {
            names.push_back(binding->name);
        }
    }
    return names;
}

int KeybindManager::GetBindingCount(int key, int* excludeKeyPtr) noexcept {
    auto* bindings = FindBindings(key);
    if (!bindings) [[likely]]
        return 0;

    if (!excludeKeyPtr) return static_cast<int>(bindings->size());

    bool hasExcluded = std::ranges::find_if(*bindings, [excludeKeyPtr](const Binding* b) {
                           return b->keyPtr == excludeKeyPtr;
                       }) != bindings->end();
    return static_cast<int>(bindings->size() - (hasExcluded ? 1 : 0));
}

void KeybindManager::UpdateBinding(int* keyPtr) {
    bool expected = false;
    if (!s_hotData.processingKeyEvent.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
        return;
    }

    auto& bindings = Bindings();
    auto it = bindings.find(keyPtr);
    if (it == bindings.end()) {
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

    if (IsIndexedKey(oldKey)) {
        auto& oldBindings = s_hotData.keyToBindings[static_cast<size_t>(oldKey)];
        std::erase(oldBindings, &binding);
    }

    binding.currentKey = newKey;
    if (IsIndexedKey(newKey)) {
        s_hotData.keyToBindings[static_cast<size_t>(newKey)].push_back(&binding);
    }

    s_hotData.processingKeyEvent.store(false, std::memory_order_release);
}

void KeybindManager::UpdateBindingName(int* keyPtr, std::string name) {
    auto& bindings = Bindings();
    auto it = bindings.find(keyPtr);
    if (it != bindings.end()) {
        it->second.name = std::move(name);
    }
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
