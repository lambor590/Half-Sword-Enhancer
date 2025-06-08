#pragma once

#include <Windows.h>
#include <functional>
#include <unordered_map>

class ConfigManager;
class IMenuFunction;
extern ConfigManager& g_ConfigManager;

class KeybindManager {
public:
    using Callback = std::function<void()>;
    
    static const char* s_keyNameTable[256];

private:
    struct Binding {
        int* keyPtr;
        Callback callback;
        IMenuFunction* function;
    };
    
    static std::unordered_map<int*, Binding> s_bindings;
    static bool s_initialized;
    static int s_toggleGuiKey;
    static int s_unbindKey;

public:
    static void Initialize() noexcept;
    static void RegisterKeybind(int* keyPtr, Callback callback, IMenuFunction* function) noexcept;
    static void UnregisterKeybind(int* keyPtr) noexcept;
    static bool HandleKeyPress(bool& waitingForKey, int& key) noexcept;

    static inline const char* GetKeyName(int vKey) noexcept {
        return s_keyNameTable[static_cast<unsigned char>(vKey)];
    }

    static bool ProcessKeyEvent(UINT msg, WPARAM wParam) noexcept;

    static int& GetToggleGuiKey() noexcept { return s_toggleGuiKey; }
    static int& GetUnbindKey() noexcept { return s_unbindKey; }
    static void SaveKeybinds() noexcept;

    static bool IsKeyBound(int key, int* excludeKeyPtr = nullptr) noexcept;
    static void RemoveBinding(int key, int* excludeKeyPtr = nullptr) noexcept;
    static IMenuFunction* GetBoundFunction(int key, int* excludeKeyPtr = nullptr) noexcept;
};