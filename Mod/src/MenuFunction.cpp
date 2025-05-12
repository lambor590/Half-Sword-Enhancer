#include <Windows.h>
#include <string>

#include "Menu/IMenuFunction.h"
#include "Menu/ICollapsibleSection.h"
#include "imgui/imgui.h"
#include "Gui.h"
#include "GlobalDefinitions.h"
#include "ConfigManager.h"
#include "KeybindManager.h"

static void RenderKeyButton(const char* id, bool& waitingForKey, int key) {
    const char* keyText = waitingForKey ? "Press any key..." : KeybindManager::GetKeyName(key);
    const bool disabled = (key == -1);
    if (disabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.12f, 0.09f, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.50f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.15f, 0.10f, 0.50f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    }
    ImGui::SetNextItemWidth(ImGui::CalcTextSize(keyText).x + 20);
    ImGui::PushID(id);
    if (ImGui::Button(keyText))
        waitingForKey = true;
    ImGui::PopID();
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Change keybind");
        ImGui::EndTooltip();
    }
    if (disabled) {
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

static bool RenderParametersButton(const char* buttonId, const std::string& name) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 75);
    bool clicked = ImGui::Button(buttonId);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Open configuration for %s", name.c_str());
        ImGui::EndTooltip();
    }
    return clicked;
}

void KeyFunction::Render() {
    RenderKeyButton(keyId.c_str(), waitingForKey, *key);
    ImGui::SameLine();
    if (toggleable) {
        bool currentEnabled = isEnabled;
        if (ImGui::Checkbox(checkId.c_str(), &currentEnabled) && currentEnabled != isEnabled)
            SetEnabled(currentEnabled);
        ImGui::SameLine();
        RenderName(name, !isEnabled && *key == -1);
    } else {
        RenderName(name, *key == -1);
    }

    if (!GetParameters().empty()) {
        if (RenderParametersButton(paramButtonId.c_str(), name))
            ImGui::OpenPopup(popupId.c_str());
        bool isPopupOpen = ImGui::BeginPopup(popupId.c_str());
        if (isPopupOpen) {
            RenderParameters();
            ImGui::EndPopup();
            popupWasOpen = true;
        } else if (popupWasOpen) {
            SaveParameters();
            popupWasOpen = false;
        }
    }

    if (KeybindManager::HandleKeyPress(waitingForKey, *key))
        OnKeyAssigned();
    else if (toggleable && !waitingForKey && *key != -1 && (GetAsyncKeyState(*key) & 1))
        SetEnabled(!isEnabled);
}

void KeybindFunction::OnKeyAssigned() {
    if (*key != -1)
        KeybindManager::RegisterKeybind(key, [this]() { callback(isEnabled); });
    UpdateKey();
}

void HookedFunction::OnKeyAssigned() {
    SetKey();
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