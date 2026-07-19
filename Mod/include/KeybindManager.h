#pragma once

#include <Windows.h>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <vector>
#include <string>
#include <string_view>
#include <atomic>
#include <array>

class KeybindManager {
public:
    using Callback = std::function<void()>;
    enum class RebindResult : uint8_t { None, Assigned, Cancelled };

private:
    enum class RebindPhase : uint8_t { Idle, Waiting, Assigned, Cancelled };

    struct Binding {
        Callback callback;
        int* keyPtr = nullptr;
        std::string name;
        int currentKey = -1;
        Callback onUnbound;
    };

    struct HotData {
        std::array<std::vector<Binding*>, 256> keyToBindings;
        std::mutex bindingsMutex;
        std::atomic<int> toggleGuiKey{VK_INSERT};
    };

    struct ColdData {
        std::atomic<int> unbindKey{VK_DELETE};
        std::mutex rebindMutex;
        const void* rebindOwner = nullptr;
        RebindPhase rebindPhase = RebindPhase::Idle;
        int capturedKey = -1;
    };

    static std::map<int*, Binding>& Bindings();
    static HotData s_hotData;
    static ColdData s_coldData;

    static constexpr std::array<bool, 256> s_validKeys = [] {
        std::array<bool, 256> valid{};
        for (size_t i = 0; i < 256; ++i) {
            valid[i] = (i != 0 && i != VK_LWIN && i != VK_RWIN && i != VK_APPS);
        }
        return valid;
    }();

    static constexpr std::array<const char*, 10> DIGIT_NAMES = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    static constexpr std::array<const char*, 26> LETTER_NAMES = {"A", "B", "C", "D", "E", "F", "G", "H", "I",
                                                                 "J", "K", "L", "M", "N", "O", "P", "Q", "R",
                                                                 "S", "T", "U", "V", "W", "X", "Y", "Z"};
    static constexpr std::array<const char*, 10> NUMPAD_NAMES = {"Numpad 0", "Numpad 1", "Numpad 2", "Numpad 3",
                                                                 "Numpad 4", "Numpad 5", "Numpad 6", "Numpad 7",
                                                                 "Numpad 8", "Numpad 9"};
    static constexpr std::array<const char*, 12> FUNCTION_KEY_NAMES = {"F1", "F2", "F3", "F4",  "F5",  "F6",
                                                                       "F7", "F8", "F9", "F10", "F11", "F12"};

public:
    static void Initialize() noexcept;
    static void RegisterKeybind(int* keyPtr, Callback callback, std::string name, Callback onUnbound);
    static void UnregisterKeybind(int* keyPtr);
    static void BeginRebind(const void* owner) noexcept;
    static void CancelRebind() noexcept;
    static RebindResult PollRebind(const void* owner, int& key) noexcept;
    static bool IsRebinding(const void* owner) noexcept;
    static bool ProcessRebindEvent(UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
    static bool IsValidKey(int key) noexcept;

    static constexpr std::string_view GetKeyNameConstexpr(unsigned char index) noexcept {
        if (index >= '0' && index <= '9') return DIGIT_NAMES[index - '0'];
        if (index >= 'A' && index <= 'Z') return LETTER_NAMES[index - 'A'];
        if (index >= VK_NUMPAD0 && index <= VK_NUMPAD9) return NUMPAD_NAMES[index - VK_NUMPAD0];
        if (index >= VK_F1 && index <= VK_F12) return FUNCTION_KEY_NAMES[index - VK_F1];

        switch (index) {
            case 255: return "No key";
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
            default: return "Unknown";
        }
    }

    static inline const char* GetKeyName(int vKey) noexcept {
        return GetKeyNameConstexpr(static_cast<unsigned char>(vKey)).data();
    }

    static bool ProcessKeyEvent(UINT msg, WPARAM wParam, LPARAM lParam);
    static bool ProcessToggleGuiEvent(UINT msg, WPARAM wParam, LPARAM lParam) noexcept;

    static int GetToggleGuiKey() noexcept { return s_hotData.toggleGuiKey.load(std::memory_order_acquire); }
    static int GetUnbindKey() noexcept { return s_coldData.unbindKey.load(std::memory_order_acquire); }
    static void SetToggleGuiKey(int key) noexcept { s_hotData.toggleGuiKey.store(key, std::memory_order_release); }
    static void SetUnbindKey(int key) noexcept { s_coldData.unbindKey.store(key, std::memory_order_release); }
    static void SaveKeybinds();

    static void RemoveBinding(int key, int* excludeKeyPtr = nullptr);
    static std::string GetBoundName(int key, int* excludeKeyPtr = nullptr);
    static std::vector<std::string> GetAllBoundNames(int key, int* excludeKeyPtr = nullptr);
    static int GetBindingCount(int key, int* excludeKeyPtr = nullptr);
    static void UpdateBindingName(int* keyPtr, std::string name);

private:
    static const std::vector<Binding*>* FindBindings(int key) noexcept;
    static void UnregisterKeybindLocked(int* keyPtr);
    static int ExtractKeyCode(UINT msg, WPARAM wParam) noexcept;
    static constexpr bool IsRelevantMessage(UINT msg) noexcept;
};
