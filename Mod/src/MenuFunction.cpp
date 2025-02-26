#include <Windows.h>
#include <format>

#include "Menu/IMenuFunction.h"
#include "imgui/imgui.h"
#include "Gui.h"

static std::string GetKeyName(int vKey) {
    static const std::unordered_map<int, std::string> keyNames = {
        {VK_LSHIFT, "Left Shift"},
        {VK_RSHIFT, "Right Shift"},
        {VK_SHIFT, "Shift"},
        {VK_CONTROL, "Control"},
        {VK_LCONTROL, "Left Control"},
        {VK_RCONTROL, "Right Control"},
        {VK_MENU, "Alt"},
        {VK_LMENU, "Left Alt"},
        {VK_RMENU, "Right Alt"},
        {VK_BACK, "Backspace"},
        {VK_TAB, "Tab"},
        {VK_RETURN, "Enter"},
        {VK_SPACE, "Space"},
        {VK_CAPITAL, "Caps Lock"},
        {VK_ESCAPE, "Unassigned"},
        {VK_LEFT, "Left"},
        {VK_UP, "Up"},
        {VK_RIGHT, "Right"},
        {VK_DOWN, "Down"},
        {VK_DELETE, "Delete"}
    };

    if (auto it = keyNames.find(vKey); it != keyNames.end()) {
        return it->second;
    }
    
    if ((vKey >= '0' && vKey <= '9') || (vKey >= 'A' && vKey <= 'Z')) return std::string(1, (char)vKey);
    if (vKey >= VK_F1 && vKey <= VK_F12) return "F" + std::to_string(vKey - VK_F1 + 1);
    
    char keyName[32];
    UINT scanCode = MapVirtualKey(vKey, MAPVK_VK_TO_VSC);
    return GetKeyNameTextA(scanCode << 16, keyName, sizeof(keyName)) > 0 ? keyName : "Unknown";
}

static void RenderKeyButton(const std::string& id, bool& waitingForKey, const int& key) {
    const std::string keyText = waitingForKey ? "Press any key..." : GetKeyName(key);
    const bool isDisabled = key == VK_ESCAPE;
    
    if (isDisabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.12f, 0.09f, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.50f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.15f, 0.10f, 0.50f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    }
    
    ImGui::SetNextItemWidth(ImGui::CalcTextSize(keyText.c_str()).x + 20);
    if (ImGui::Button((keyText + id).c_str())) {
        waitingForKey = true;
    }
    
    if (isDisabled) {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }
}

static void RenderName(const std::string& name, bool isDisabled, const std::string& keyName) {
    if (isDisabled) {
        ImGui::TextColored(ImVec4(0.50f, 0.50f, 0.50f, 1.00f), "%s", name.c_str());
    } else {
        ImGui::Text("%s", name.c_str());
    }
}

static bool HandleKeyPress(bool& waitingForKey, int& key) {
    if (!waitingForKey) return false;
    
    for (int i = 0; i < 256; i++) {
        if (GetAsyncKeyState(i) & 0x8000) {
            key = i;
            waitingForKey = false;
            return true;
        }
    }
    return false;
}

void HookedFunction::Render() {
    const std::string id = std::format("##Hook_{}", name);
    
    RenderKeyButton(id + "_key", waitingForKey, key);
    ImGui::SameLine();
    
    const bool prevEnabled = isEnabled;
    const std::string checkId = "##check" + id;
    if (ImGui::Checkbox(checkId.c_str(), &isEnabled)) {
        if (isEnabled != prevEnabled) {
            if (isEnabled) {
                g_GameHook->RegisterHook(hookedFunction, callback);
            } else {
                g_GameHook->UnregisterHook(hookedFunction);
            }
        }
    }
    
    ImGui::SameLine();
    RenderName(name, !isEnabled && key == VK_ESCAPE, GetKeyName(key));
    
    if (HandleKeyPress(waitingForKey, key)) {
        isEnabled = !isEnabled;
        if (isEnabled) {
            g_GameHook->RegisterHook(hookedFunction, callback);
        } else {
            g_GameHook->UnregisterHook(hookedFunction);
        }
    }
}

void KeybindFunction::Render() {
    const std::string id = std::format("##Key_{}", name);
    
    RenderKeyButton(id, waitingForKey, *key);
    ImGui::SameLine();
    RenderName(name, *key == VK_ESCAPE, GetKeyName(*key));
    
    if (HandleKeyPress(waitingForKey, *key)) {
        if (isEnabled) {
            Gui::UnregisterKeybind(key);
        }
        
        isEnabled = (*key != VK_ESCAPE);
        if (isEnabled) {
            Gui::RegisterKeybind(key, callback);
        }
    }
}