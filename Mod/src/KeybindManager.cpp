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

        SetToggleGuiKey(IsValidKey(loadedToggleKey) ? loadedToggleKey : VK_INSERT);
        SetUnbindKey(IsValidKey(loadedUnbindKey) ? loadedUnbindKey : VK_DELETE);

        s_initialized = true;
    }
}

void KeybindManager::RegisterKeybind(
    int* keyPtr, Callback callback, std::string name, bool isToggle, Callback onUnbound
) {
    const std::scoped_lock lock(s_hotData.bindingsMutex);
    UnregisterKeybindLocked(keyPtr);

    int currentKey = *keyPtr;
    auto& bindings = Bindings();
    auto bindingIt =
        bindings
            .emplace(
                keyPtr,
                Binding{std::move(callback), keyPtr, std::move(name), isToggle, currentKey, std::move(onUnbound)}
            )
            .first;

    if (IsIndexedKey(currentKey)) {
        s_hotData.keyToBindings[static_cast<size_t>(currentKey)].push_back(&bindingIt->second);
    }
}

void KeybindManager::UnregisterKeybind(int* keyPtr) {
    const std::scoped_lock lock(s_hotData.bindingsMutex);
    UnregisterKeybindLocked(keyPtr);
}

void KeybindManager::UnregisterKeybindLocked(int* keyPtr) {
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

    if (keyCode == GetToggleGuiKey()) [[unlikely]] {
        Gui::ToggleVisibility();
        return true;
    }

    const std::scoped_lock lock(s_hotData.bindingsMutex);
    const auto* bindings = FindBindings(keyCode);
    if (!bindings) [[likely]]
        return false;

    for (const Binding* binding : *bindings) {
        binding->callback();

        if (!binding->name.empty() && !binding->isToggle) [[likely]] {
            NotificationManager::NotifyOneTimeAction(binding->name);
        }
    }

    return true;
}

void KeybindManager::BeginRebind(const void* owner) noexcept {
    if (!owner) return;

    const std::scoped_lock lock(s_coldData.rebindMutex);
    s_coldData.rebindOwner = owner;
    s_coldData.rebindPhase = RebindPhase::Waiting;
    s_coldData.capturedKey = -1;
}

void KeybindManager::CancelRebind() noexcept {
    const std::scoped_lock lock(s_coldData.rebindMutex);
    s_coldData.rebindOwner = nullptr;
    s_coldData.rebindPhase = RebindPhase::Idle;
    s_coldData.capturedKey = -1;
}

KeybindManager::RebindResult KeybindManager::PollRebind(const void* owner, int& key) noexcept {
    const std::scoped_lock lock(s_coldData.rebindMutex);
    if (!owner || s_coldData.rebindOwner != owner) return RebindResult::None;

    RebindResult result = RebindResult::None;
    if (s_coldData.rebindPhase == RebindPhase::Assigned) {
        key = s_coldData.capturedKey;
        result = RebindResult::Assigned;
    } else if (s_coldData.rebindPhase == RebindPhase::Cancelled) {
        result = RebindResult::Cancelled;
    }

    if (result != RebindResult::None) {
        s_coldData.rebindOwner = nullptr;
        s_coldData.rebindPhase = RebindPhase::Idle;
        s_coldData.capturedKey = -1;
    }
    return result;
}

bool KeybindManager::IsRebinding(const void* owner) noexcept {
    const std::scoped_lock lock(s_coldData.rebindMutex);
    return owner && s_coldData.rebindOwner == owner && s_coldData.rebindPhase == RebindPhase::Waiting;
}

void KeybindManager::SaveKeybinds() {
    auto& config = ConfigManager::Get();
    config.BatchSave([&] {
        config.SetInt("Keybinds", "toggle_gui_key", GetToggleGuiKey());
        config.SetInt("Keybinds", "unbind_key", GetUnbindKey());
    });
}

const std::vector<KeybindManager::Binding*>* KeybindManager::FindBindings(int key) noexcept {
    if (!IsIndexedKey(key)) return nullptr;
    const auto& bindings = s_hotData.keyToBindings[static_cast<size_t>(key)];
    return bindings.empty() ? nullptr : &bindings;
}

void KeybindManager::RemoveBinding(int key, int* excludeKeyPtr) {
    if (!IsIndexedKey(key)) return;

    const std::scoped_lock lock(s_hotData.bindingsMutex);
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
        int* keyPtr = binding->keyPtr;
        keyBindings.erase(foundIt);
        Bindings().erase(keyPtr);
    }
}

std::string KeybindManager::GetBoundName(int key, int* excludeKeyPtr) {
    const std::scoped_lock lock(s_hotData.bindingsMutex);
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
    const std::scoped_lock lock(s_hotData.bindingsMutex);
    auto* bindings = FindBindings(key);
    if (!bindings) [[likely]]
        return {};

    std::vector<std::string> names;
    names.reserve(bindings->size());

    for (const Binding* binding : *bindings) {
        if (binding->keyPtr != excludeKeyPtr) {
            names.push_back(binding->name);
        }
    }
    return names;
}

int KeybindManager::GetBindingCount(int key, int* excludeKeyPtr) {
    const std::scoped_lock lock(s_hotData.bindingsMutex);
    auto* bindings = FindBindings(key);
    if (!bindings) [[likely]]
        return 0;

    if (!excludeKeyPtr) return static_cast<int>(bindings->size());

    bool hasExcluded = std::ranges::find_if(*bindings, [excludeKeyPtr](const Binding* b) {
                           return b->keyPtr == excludeKeyPtr;
                       }) != bindings->end();
    return static_cast<int>(bindings->size() - (hasExcluded ? 1 : 0));
}

void KeybindManager::UpdateBindingName(int* keyPtr, std::string name) {
    const std::scoped_lock lock(s_hotData.bindingsMutex);
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
    const std::scoped_lock lock(s_coldData.rebindMutex);
    if (s_coldData.rebindPhase != RebindPhase::Waiting) return false;

    int keyCode = ExtractKeyCode(msg, wParam);
    if (keyCode == -1) return false;

    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        if (keyCode == VK_ESCAPE) {
            s_coldData.rebindPhase = RebindPhase::Cancelled;
            return true;
        } else if (keyCode == GetUnbindKey()) {
            keyCode = -1;
        } else if (!IsValidKey(keyCode)) {
            return false;
        }
    }

    s_coldData.capturedKey = keyCode;
    s_coldData.rebindPhase = RebindPhase::Assigned;
    return true;
}

bool KeybindManager::ProcessToggleGuiEvent(UINT msg, WPARAM wParam) noexcept {
    if (!IsRelevantMessage(msg)) return false;
    if (ExtractKeyCode(msg, wParam) != GetToggleGuiKey()) return false;
    Gui::ToggleVisibility();
    return true;
}
