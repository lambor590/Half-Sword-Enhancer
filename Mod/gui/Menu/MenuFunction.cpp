#include <Windows.h>
#include <format>

#include "IMenuFunction.h"
#include "imgui.h"

GameHook* g_GameHook = new GameHook();

void HookedFunction::Render() {
    std::string id = std::format("##Hook_{}", name);
    bool prevEnabled = isEnabled;
    
    if (ImGui::Checkbox((name + id).c_str(), &isEnabled)) {
        if (isEnabled && !prevEnabled) {
            g_GameHook->RegisterHook(hookedFunction, callback);
        }
        else if (!isEnabled && prevEnabled) {
            g_GameHook->UnregisterHook(hookedFunction);
        }
    }
    
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Hooked function: %s", hookedFunction.c_str());
        ImGui::EndTooltip();
    }
}

std::string GetKeyName(int vKey) {
    switch (vKey) {
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
        default: {
            if (vKey >= '0' && vKey <= '9') return std::string(1, (char)vKey);
            if (vKey >= 'A' && vKey <= 'Z') return std::string(1, (char)vKey);
            if (vKey >= VK_F1 && vKey <= VK_F12) return "F" + std::to_string(vKey - VK_F1 + 1);
            
            char keyName[32];
            UINT scanCode = MapVirtualKey(vKey, MAPVK_VK_TO_VSC);
            if (GetKeyNameTextA(scanCode << 16, keyName, sizeof(keyName)) > 0) {
                return keyName;
            }
            return "Unknown";
        }
    }
}

void KeybindFunction::Render() {
    std::string id = std::format("##Key_{}", name);
    ImGui::Checkbox((name + id).c_str(), &isEnabled);
    
    ImGui::SameLine();
    
    std::string keyText = waitingForKey ? "Press any key..." : GetKeyName(*key);
    std::string buttonId = std::format("{}##btn_{}", keyText, name);
    
    if (ImGui::Button(buttonId.c_str())) {
        waitingForKey = true;
    }
    
    if (waitingForKey) {
        for (int i = 0; i < 256; i++) {
            if (GetAsyncKeyState(i) & 0x8000) {
                *key = i;
                waitingForKey = false;
                break;
            }
        }
    }
    
    if (isEnabled && (*key != 0) && (GetAsyncKeyState(*key) & 0x8000)) {
        callback();
    }
} 