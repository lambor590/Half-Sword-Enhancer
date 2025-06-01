#pragma once

#include <Windows.h>
#include <functional>
#include <vector>
#include <array>

class ConfigManager;
class IMenuFunction;
extern ConfigManager& g_ConfigManager;

class KeybindManager {
public:
    using Callback = std::function<void()>;
    
    static const char* s_keyNameTable[256];

private:
    static std::array<Callback, 256> s_callbacks;
    struct Binding { int* keyPtr; Callback callback; IMenuFunction* function; };
    static std::vector<Binding> s_bindingList;
    static bool s_initialized;
    static int s_toggleGuiKey;
    static int s_unbindKey;
    static std::vector<int> s_boundCodes;

public:
    static void Initialize() noexcept;

    static void RegisterKeybind(int* keyPtr, Callback callback, IMenuFunction* function) noexcept;
    static void UnregisterKeybind(int* keyPtr) noexcept;
    static void UpdateBindings() noexcept;
    static bool HandleKeyPress(bool& waitingForKey, int& key) noexcept;

    static inline const char* GetKeyName(int vKey) noexcept {
        return s_keyNameTable[static_cast<unsigned char>(vKey)];
    }

    static bool ProcessKeyEvent(UINT msg, WPARAM wParam) noexcept;

    static int& GetToggleGuiKey() noexcept { return s_toggleGuiKey; }
    static int& GetUnbindKey() noexcept { return s_unbindKey; }
    static void SaveKeybinds() noexcept;

    static bool IsKeyBound(int code, int* excludeKeyPtr = nullptr) noexcept;
    static void RemoveBinding(int code, int* excludeKeyPtr = nullptr) noexcept;
    static IMenuFunction* GetBoundFunction(int code, int* excludeKeyPtr = nullptr) noexcept;
};