#pragma once

#include <Windows.h>
#include <functional>
#include <unordered_map>
#include <string>

class Gui;
class ConfigManager;

extern ConfigManager& g_ConfigManager;

class KeybindManager {
private:
    static int s_toggleGuiKey;
    static int s_unbindKey;
    static std::unordered_map<int*, std::function<void()>> s_keybinds;
    static bool s_initialized;

public:
    static void Initialize();

    static const char* GetKeyName(int vKey) {
        static const std::unordered_map<int, const char*> keyNameMap = {
            {-1, "Unbound"}, {VK_LSHIFT, "Left Shift"}, {VK_RSHIFT, "Right Shift"}, {VK_SHIFT, "Shift"},
            {VK_CONTROL, "Control"}, {VK_LCONTROL, "Left Control"}, {VK_RCONTROL, "Right Control"},
            {VK_MENU, "Alt"}, {VK_LMENU, "Left Alt"}, {VK_RMENU, "Right Alt"},
            {VK_BACK, "Backspace"}, {VK_TAB, "Tab"}, {VK_RETURN, "Enter"},
            {VK_SPACE, "Space"}, {VK_CAPITAL, "Caps Lock"}, {VK_ESCAPE, "Escape"},
            {VK_LEFT, "Left"}, {VK_UP, "Up"}, {VK_RIGHT, "Right"}, {VK_DOWN, "Down"},
            {VK_DELETE, "Delete"}, {VK_INSERT, "Insert"}, {VK_HOME, "Home"}, {VK_END, "End"},
            {VK_PRIOR, "Page Up"}, {VK_NEXT, "Page Down"}, {VK_SNAPSHOT, "Print Screen"},
            {VK_SCROLL, "Scroll Lock"}, {VK_PAUSE, "Pause"}, {VK_NUMLOCK, "Num Lock"},
            {VK_NUMPAD0, "Numpad 0"}, {VK_NUMPAD1, "Numpad 1"}, {VK_NUMPAD2, "Numpad 2"},
            {VK_NUMPAD3, "Numpad 3"}, {VK_NUMPAD4, "Numpad 4"}, {VK_NUMPAD5, "Numpad 5"},
            {VK_NUMPAD6, "Numpad 6"}, {VK_NUMPAD7, "Numpad 7"}, {VK_NUMPAD8, "Numpad 8"},
            {VK_NUMPAD9, "Numpad 9"}, {VK_MULTIPLY, "Numpad *"}, {VK_ADD, "Numpad +"},
            {VK_SUBTRACT, "Numpad -"}, {VK_DECIMAL, "Numpad ."}, {VK_DIVIDE, "Numpad /"},
            {VK_OEM_1, ";"}, {VK_OEM_PLUS, "="}, {VK_OEM_COMMA, ","}, {VK_OEM_MINUS, "-"},
            {VK_OEM_PERIOD, "."}, {VK_OEM_2, "/"}, {VK_OEM_3, "`"}, {VK_OEM_4, "["},
            {VK_OEM_5, "\\"}, {VK_OEM_6, "]"}, {VK_OEM_7, "'"}
        };

        if (auto it = keyNameMap.find(vKey); it != keyNameMap.end())
            return it->second;

        static char singleChar[2] = { 0 };
        if ((vKey >= '0' && vKey <= '9') || (vKey >= 'A' && vKey <= 'Z')) {
            singleChar[0] = static_cast<char>(vKey);
            singleChar[1] = '\0';
            return singleChar;
        }

        static char fKeyName[4] = { 0 };
        if (vKey >= VK_F1 && vKey <= VK_F12) {
            sprintf_s(fKeyName, "F%d", vKey - VK_F1 + 1);
            return fKeyName;
        }

        static char keyName[32];
        UINT scanCode = MapVirtualKey(vKey, MAPVK_VK_TO_VSC);
        return GetKeyNameTextA(scanCode << 16, keyName, sizeof(keyName)) > 0 ? keyName : "Unknown";
    }

    static bool HandleKeyPress(bool& waitingForKey, int& key) {
        if (!waitingForKey) return false;

        for (int i = 0; i < 256; i++) {
            if (GetAsyncKeyState(i) & 0x8000) {
                key = i == s_unbindKey ? -1 : i;
                waitingForKey = false;
                return true;
            }
        }
        return false;
    }

    static void RegisterKeybind(int* key, std::function<void()> callback) {
        s_keybinds[key] = callback;
    }

    static void UnregisterKeybind(int* key) {
        s_keybinds.erase(key);
    }

    static int& GetToggleGuiKey() {
        return s_toggleGuiKey;
    }

    static int& GetUnbindKey() {
        return s_unbindKey;
    }

    static void SaveKeybinds();

    static bool ProcessKeyEvent(UINT msg, WPARAM wParam);

    static std::unordered_map<int*, std::function<void()>>& GetKeybinds() {
        return s_keybinds;
    }
};