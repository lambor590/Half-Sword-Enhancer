#include <Windows.h>
#include <format>

#include "Menu/IMenuFunction.h"
#include "Menu/ICollapsibleSection.h"
#include "imgui/imgui.h"
#include "Gui.h"
#include "GlobalDefinitions.h"

static std::string GetKeyName(int vKey) {
    static const std::unordered_map<int, const char*> keyNameMap = {
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

    auto it = keyNameMap.find(vKey);
    if (it != keyNameMap.end()) return it->second;
    
    if ((vKey >= '0' && vKey <= '9') || (vKey >= 'A' && vKey <= 'Z')) return std::string(1, static_cast<char>(vKey));
    if (vKey >= VK_F1 && vKey <= VK_F12) return "F" + std::to_string(vKey - VK_F1 + 1);
    
    static char keyName[32];
    UINT scanCode = MapVirtualKey(vKey, MAPVK_VK_TO_VSC);
    return GetKeyNameTextA(scanCode << 16, keyName, sizeof(keyName)) > 0 ? keyName : "Unknown";
}

static void RenderKeyButton(const std::string& id, bool& waitingForKey, const int& key) {
    const std::string& keyText = waitingForKey ? "Press any key..." : GetKeyName(key);
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

static inline void RenderName(const std::string& name, bool isDisabled) {
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
    
    int currentKey = *key;
    
    RenderKeyButton(id + "_key", waitingForKey, currentKey);
    ImGui::SameLine();
    
    const bool prevEnabled = isEnabled;
    bool currentEnabled = isEnabled;
    
    if (ImGui::Checkbox(("##check" + id).c_str(), &currentEnabled)) {
        if (currentEnabled != prevEnabled) {
            SetEnabled(currentEnabled);
        }
    }
    
    ImGui::SameLine();
    RenderName(name, !isEnabled && currentKey == VK_ESCAPE);
    
    int tempKey = currentKey;
    if (HandleKeyPress(waitingForKey, tempKey)) {
        SetKey(tempKey);
    } else if (!waitingForKey && currentKey != VK_ESCAPE && (GetAsyncKeyState(currentKey) & 1)) {
        SetEnabled(!isEnabled);
    }
}

void KeybindFunction::Render() {
    const std::string id = std::format("##Key_{}", name);
    
    RenderKeyButton(id, waitingForKey, *key);
    ImGui::SameLine();
    RenderName(name, *key == VK_ESCAPE);
    
    int tempKey = *key;
    if (HandleKeyPress(waitingForKey, tempKey)) {
        UpdateKey(tempKey);
        
        if (isEnabled) Gui::RegisterKeybind(key, callback);
        else Gui::UnregisterKeybind(key);
    }
}