#include <Windows.h>
#include <format>

#include "Menu/IMenuFunction.h"
#include "Menu/ICollapsibleSection.h"
#include "imgui/imgui.h"
#include "Gui.h"
#include "GlobalDefinitions.h"
#include "ConfigManager.h"
#include "KeybindManager.h"

static void RenderKeyButton(const std::string& id, bool& waitingForKey, const int& key) {
    const char* keyText = waitingForKey ? "Press any key..." : KeybindManager::GetKeyName(key);
    const bool isDisabled = key == -1;
    
    if (isDisabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.12f, 0.09f, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.50f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.15f, 0.10f, 0.50f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    }
    
    ImGui::SetNextItemWidth(ImGui::CalcTextSize(keyText).x + 20);
    if (ImGui::Button((keyText + id).c_str()))
        waitingForKey = true;
    
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Change keybind");
        ImGui::EndTooltip();
    }
    
    if (isDisabled) {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }
}

static inline void RenderName(const std::string& name, bool isDisabled) {
    ImGui::TextColored(
        isDisabled ? ImVec4(0.50f, 0.50f, 0.50f, 1.00f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 
        "%s", name.c_str()
    );
}

static bool RenderParametersButton(const std::string& id, const std::string& name) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 75);
    
    bool clicked = ImGui::Button(("Config##param_" + id).c_str());
    
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
        
    static std::unordered_map<std::string, bool> popupOpenStates;
    bool& popupWasOpen = popupOpenStates[id];
    
    if (RenderParametersButton(id, name))
        ImGui::OpenPopup(("ConfigParams" + id).c_str());
    
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
    RenderName(name, !isEnabled && *key == -1);
    
    RenderParametersPopup(id, name, this);
    
    if (KeybindManager::HandleKeyPress(waitingForKey, *key))
        SetKey();
    else if (!waitingForKey && *key != -1 && (GetAsyncKeyState(*key) & 1))
        SetEnabled(!isEnabled);
}

void KeybindFunction::Render() {
    const std::string id = std::format("##Key_{}", name);
    
    RenderKeyButton(id, waitingForKey, *key);
    ImGui::SameLine();
    RenderName(name, *key == -1);
    
    RenderParametersPopup(id, name, this);
    
    if (KeybindManager::HandleKeyPress(waitingForKey, *key)) {
        if (*key != -1)
            KeybindManager::RegisterKeybind(key, callback);
        UpdateKey();
    }
}

void Parameter::Render() {
    ImGui::PushItemWidth(150.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", displayName.c_str());
    ImGui::SameLine();
    
    switch (type) {
        case Type::Int: {
            auto intPtr = std::get<int*>(valuePtr);
            ImGui::PushItemWidth(60.0f);
            char inputBuffer[16];
            snprintf(inputBuffer, sizeof(inputBuffer), "%d", *intPtr);
            if (ImGui::InputText((id + "_input").c_str(), inputBuffer, sizeof(inputBuffer), 
                                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                *intPtr = atoi(inputBuffer);
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::SliderInt(id.c_str(), intPtr, std::get<int>(minValue), std::get<int>(maxValue));
            break;
        }
        case Type::Float: {
            auto floatPtr = std::get<float*>(valuePtr);
            ImGui::PushItemWidth(60.0f);
            char inputBuffer[16];
            snprintf(inputBuffer, sizeof(inputBuffer), "%.2f", *floatPtr);
            if (ImGui::InputText((id + "_input").c_str(), inputBuffer, sizeof(inputBuffer), 
                                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                *floatPtr = static_cast<float>(atof(inputBuffer));
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::SliderFloat(id.c_str(), floatPtr, std::get<float>(minValue), std::get<float>(maxValue), "%.2f");
            break;
        }
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
    
    g_ConfigManager.SaveConfig();
}

void IMenuFunction::RenderParameters() {
    for (auto& param : parameters)
        param.Render();
}