#include <Windows.h>
#include <format>

#include "Menu/IMenuFunction.h"
#include "Menu/ICollapsibleSection.h"
#include "imgui/imgui.h"
#include "Gui.h"
#include "GlobalDefinitions.h"
#include "ConfigManager.h"

static std::string GetKeyName(int vKey) {
    static const std::unordered_map<int, const char*> keyNameMap = {
        {VK_LSHIFT, "Left Shift"}, {VK_RSHIFT, "Right Shift"}, {VK_SHIFT, "Shift"},
        {VK_CONTROL, "Control"}, {VK_LCONTROL, "Left Control"}, {VK_RCONTROL, "Right Control"},
        {VK_MENU, "Alt"}, {VK_LMENU, "Left Alt"}, {VK_RMENU, "Right Alt"},
        {VK_BACK, "Backspace"}, {VK_TAB, "Tab"}, {VK_RETURN, "Enter"},
        {VK_SPACE, "Space"}, {VK_CAPITAL, "Caps Lock"}, {VK_ESCAPE, "Escape"},
        {VK_LEFT, "Left"}, {VK_UP, "Up"}, {VK_RIGHT, "Right"}, {VK_DOWN, "Down"},
        {VK_DELETE, "Unassigned"}
    };

    if (auto it = keyNameMap.find(vKey); it != keyNameMap.end())
        return it->second;
    
    if ((vKey >= '0' && vKey <= '9') || (vKey >= 'A' && vKey <= 'Z'))
        return std::string(1, static_cast<char>(vKey));
        
    if (vKey >= VK_F1 && vKey <= VK_F12)
        return "F" + std::to_string(vKey - VK_F1 + 1);
    
    static char keyName[32];
    UINT scanCode = MapVirtualKey(vKey, MAPVK_VK_TO_VSC);
    return GetKeyNameTextA(scanCode << 16, keyName, sizeof(keyName)) > 0 ? keyName : "Unknown";
}

static void RenderKeyButton(const std::string& id, bool& waitingForKey, const int& key) {
    const std::string& keyText = waitingForKey ? "Press any key..." : GetKeyName(key);
    const bool isDisabled = key == VK_DELETE;
    
    if (isDisabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.12f, 0.09f, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.50f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.15f, 0.10f, 0.50f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    }
    
    ImGui::SetNextItemWidth(ImGui::CalcTextSize(keyText.c_str()).x + 20);
    if (ImGui::Button((keyText + id).c_str()))
        waitingForKey = true;
    
    if (isDisabled) {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }
}

static inline void RenderName(const std::string& name, bool isDisabled) {
    ImGui::TextColored(isDisabled ? ImVec4(0.50f, 0.50f, 0.50f, 1.00f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", name.c_str());
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

static bool RenderParametersButton(const std::string& id, const std::string& name) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 30);
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.25f, 0.16f, 0.80f));
    bool clicked = ImGui::Button(("##param_" + id).c_str());
    ImGui::PopStyleColor();
    
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Open configuration for %s", name.c_str());
        ImGui::EndTooltip();
    }
    
    return clicked;
}

static void RenderParametersPopup(const std::string& id, const std::string& name, IMenuFunction* function) {
    if (function->GetParameters().empty())
        return;
        
    if (RenderParametersButton(id, name))
        ImGui::OpenPopup(("ConfigParams" + id).c_str());
    
    static std::unordered_map<std::string, bool> popupOpenStates;
    bool& popupWasOpen = popupOpenStates[id];
    bool isPopupOpen = ImGui::BeginPopup(("ConfigParams" + id).c_str());
    
    if (isPopupOpen) {
        function->RenderParameters();
        ImGui::EndPopup();
        popupWasOpen = true;
    } else if (popupWasOpen) {
        function->SaveParameters();
        popupWasOpen = false;
    }
}

void HookedFunction::Render() {
    const std::string id = std::format("##Hook_{}", name);
    
    RenderKeyButton(id + "_key", waitingForKey, *key);
    ImGui::SameLine();
    
    bool currentEnabled = isEnabled;
    if (ImGui::Checkbox(("##check" + id).c_str(), &currentEnabled) && currentEnabled != isEnabled)
        SetEnabled(currentEnabled);
    
    ImGui::SameLine();
    RenderName(name, !isEnabled && *key == VK_DELETE);
    
    RenderParametersPopup(id, name, this);
    
    if (HandleKeyPress(waitingForKey, *key))
        SetKey(*key);
    else if (!waitingForKey && *key != VK_DELETE && (GetAsyncKeyState(*key) & 1))
        SetEnabled(!isEnabled);
}

void KeybindFunction::Render() {
    const std::string id = std::format("##Key_{}", name);
    
    RenderKeyButton(id, waitingForKey, *key);
    ImGui::SameLine();
    RenderName(name, *key == VK_DELETE);
    
    RenderParametersPopup(id, name, this);
    
    if (HandleKeyPress(waitingForKey, *key)) {
        if (*key != VK_DELETE) 
            Gui::RegisterKeybind(key, callback);
        UpdateKey(*key);
    }
}

void Parameter::Render() {
    ImGui::PushItemWidth(150.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", displayName.c_str());
    ImGui::SameLine();
    
    switch (type) {
        case Type::Int:
            ImGui::SliderInt(id.c_str(), std::get<int*>(valuePtr), std::get<int>(minValue), std::get<int>(maxValue));
            break;
        case Type::Float:
            ImGui::SliderFloat(id.c_str(), std::get<float*>(valuePtr), std::get<float>(minValue), std::get<float>(maxValue), "%.2f");
            break;
        case Type::Bool:
            ImGui::Checkbox(id.c_str(), std::get<bool*>(valuePtr));
            break;
    }
    
    ImGui::PopItemWidth();
}

void IMenuFunction::LoadParameters() {
    for (auto& param : parameters) {
        switch (param.GetType()) {
            case Parameter::Type::Int:
                *param.GetIntPtr() = GetConfig(param.GetName(), *param.GetIntPtr());
                break;
            case Parameter::Type::Float:
                *param.GetFloatPtr() = GetConfig(param.GetName(), *param.GetFloatPtr());
                break;
            case Parameter::Type::Bool:
                *param.GetBoolPtr() = GetConfig(param.GetName(), *param.GetBoolPtr());
                break;
        }
    }
}

void IMenuFunction::SaveParameters() const {
    for (const auto& param : parameters) {
        switch (param.GetType()) {
            case Parameter::Type::Int:
                SaveConfig(param.GetName(), *param.GetIntPtr());
                break;
            case Parameter::Type::Float:
                SaveConfig(param.GetName(), *param.GetFloatPtr());
                break;
            case Parameter::Type::Bool:
                SaveConfig(param.GetName(), *param.GetBoolPtr());
                break;
        }
    }
    
    ConfigManager::Get().SaveConfig();
}

void IMenuFunction::RenderParameters() {
    for (auto& param : parameters)
        param.Render();
}