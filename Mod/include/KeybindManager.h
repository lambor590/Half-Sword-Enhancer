#pragma once

#include <Windows.h>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string_view>
#include <atomic>
#include <array>

class ConfigManager;
class IMenuFunction;
extern ConfigManager& g_ConfigManager;

class KeybindManager {
public:
    using Callback = std::function<void()>;

private:
    struct Binding {
        Callback callback;
        int* keyPtr = nullptr;
        IMenuFunction* function = nullptr;
        int currentKey = -1;
    };

    struct alignas(64) HotData {
        std::unordered_map<int, std::vector<Binding*>> keyToBindings;
        int toggleGuiKey = VK_INSERT;
        std::atomic<bool> processingKeyEvent{false};
    };

    struct ColdData {
        int unbindKey = VK_DELETE;
        bool waitingForRebind = false;
        int capturedKey = -1;
        bool keyWasCaptured = false;
    };

    static std::unordered_map<int*, Binding> s_bindings;
    static bool s_initialized;

    static HotData s_hotData;
    static ColdData s_coldData;

    static inline const std::array<bool, 256> s_validKeys = []() constexpr {
        std::array<bool, 256> valid{};
        for (size_t i = 0; i < 256; ++i) {
            valid[i] = (i != 0 && i != VK_LWIN && i != VK_RWIN && i != VK_APPS);
        }
        return valid;
    }();

public:
    static void Initialize() noexcept;
    static void RegisterKeybind(int* keyPtr, Callback callback, IMenuFunction* function) noexcept;
    static void UnregisterKeybind(int* keyPtr) noexcept;
    static bool HandleKeyPress(bool& waitingForKey, int& key) noexcept;
    static bool ProcessRebindEvent(UINT msg, WPARAM wParam) noexcept;
    static void StartWaitingForRebind() noexcept;
    static void CancelRebind() noexcept;
    static bool IsValidKey(int key) noexcept;

    static constexpr std::string_view GetKeyNameConstexpr(unsigned char index) noexcept {
        switch (index) {
            case 255: return "Unbound";
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
            case VK_NUMPAD0: return "Numpad 0";
            case VK_NUMPAD1: return "Numpad 1";
            case VK_NUMPAD2: return "Numpad 2";
            case VK_NUMPAD3: return "Numpad 3";
            case VK_NUMPAD4: return "Numpad 4";
            case VK_NUMPAD5: return "Numpad 5";
            case VK_NUMPAD6: return "Numpad 6";
            case VK_NUMPAD7: return "Numpad 7";
            case VK_NUMPAD8: return "Numpad 8";
            case VK_NUMPAD9: return "Numpad 9";
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
            case VK_F1: return "F1";
            case VK_F2: return "F2";
            case VK_F3: return "F3";
            case VK_F4: return "F4";
            case VK_F5: return "F5";
            case VK_F6: return "F6";
            case VK_F7: return "F7";
            case VK_F8: return "F8";
            case VK_F9: return "F9";
            case VK_F10: return "F10";
            case VK_F11: return "F11";
            case VK_F12: return "F12";
            case '0': return "0";
            case '1': return "1";
            case '2': return "2";
            case '3': return "3";
            case '4': return "4";
            case '5': return "5";
            case '6': return "6";
            case '7': return "7";
            case '8': return "8";
            case '9': return "9";
            case 'A': return "A";
            case 'B': return "B";
            case 'C': return "C";
            case 'D': return "D";
            case 'E': return "E";
            case 'F': return "F";
            case 'G': return "G";
            case 'H': return "H";
            case 'I': return "I";
            case 'J': return "J";
            case 'K': return "K";
            case 'L': return "L";
            case 'M': return "M";
            case 'N': return "N";
            case 'O': return "O";
            case 'P': return "P";
            case 'Q': return "Q";
            case 'R': return "R";
            case 'S': return "S";
            case 'T': return "T";
            case 'U': return "U";
            case 'V': return "V";
            case 'W': return "W";
            case 'X': return "X";
            case 'Y': return "Y";
            case 'Z': return "Z";
            default: return "Unknown";
        }
    }

    static inline const char* GetKeyName(int vKey) noexcept {
        return GetKeyNameConstexpr(static_cast<unsigned char>(vKey)).data();
    }

    static bool ProcessKeyEvent(UINT msg, WPARAM wParam) noexcept;

    static int& GetToggleGuiKey() noexcept { return s_hotData.toggleGuiKey; }
    static int& GetUnbindKey() noexcept { return s_coldData.unbindKey; }
    static void SaveKeybinds() noexcept;

    static bool IsKeyBound(int key, int* excludeKeyPtr = nullptr) noexcept;
    static void RemoveBinding(int key, int* excludeKeyPtr = nullptr) noexcept;
    static IMenuFunction* GetBoundFunction(int key, int* excludeKeyPtr = nullptr) noexcept;
    static std::vector<IMenuFunction*> GetAllBoundFunctions(int key, int* excludeKeyPtr = nullptr) noexcept;
    static int GetBindingCount(int key, int* excludeKeyPtr = nullptr) noexcept;
    static void UpdateBinding(int* keyPtr) noexcept;

private:
    static int ExtractKeyCode(UINT msg, WPARAM wParam) noexcept;
    static constexpr bool IsRelevantMessage(UINT msg) noexcept;
};
