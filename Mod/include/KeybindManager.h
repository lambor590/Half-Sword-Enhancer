#pragma once

#include <Windows.h>
#include <functional>
#include <vector>
#include <utility>

class ConfigManager;
extern ConfigManager& g_ConfigManager;

class KeybindManager {
public:
    using Callback = std::function<void()>;

private:
    static Callback s_callbacks[256];
    static std::vector<std::pair<int*, Callback>> s_bindingList;
    static bool s_initialized;
    static int s_toggleGuiKey;
    static int s_unbindKey;

public:
    static void Initialize() noexcept;

    static void RegisterKeybind(int* keyPtr, Callback callback) noexcept;
    static void UnregisterKeybind(int* keyPtr) noexcept;
    static void UpdateBindings() noexcept;
    static bool HandleKeyPress(bool& waitingForKey, int& key) noexcept;

    static const char* GetKeyName(int vKey) noexcept;

    static bool ProcessKeyEvent(UINT msg, WPARAM wParam) noexcept;

    static int& GetToggleGuiKey() noexcept { return s_toggleGuiKey; }
    static int& GetUnbindKey() noexcept { return s_unbindKey; }
    static void SaveKeybinds() noexcept;
};