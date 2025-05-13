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
    const char* keyText = waitingForKey ? "Press a key..." : KeybindManager::GetKeyName(key);
    const bool disabled = (key == -1);
    
    if (disabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.13f, 0.09f, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.45f, 0.35f, 0.60f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.20f, 0.12f, 0.40f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    } else {
        MedievalStyle::PushButtonStyle();
    }
    
    float textWidth = ImGui::CalcTextSize(keyText).x;
    ImGui::SetNextItemWidth(textWidth + 28);
    ImGui::PushID(id);
    if (ImGui::Button(keyText))
        waitingForKey = true;
    ImGui::PopID();
    
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextColored(MedievalStyle::parchment, "Change keybind");
        ImGui::EndTooltip();
    }
    
    if (disabled) {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    } else {
        MedievalStyle::PopButtonStyle();
    }
}

static inline void RenderName(const std::string& name, bool isDisabled) {
    ImGui::TextColored(
        isDisabled ? ImVec4(0.60f, 0.55f, 0.48f, 0.70f) : MedievalStyle::parchment, 
        "%s", name.c_str()
    );
}

static bool RenderParametersButton(const char* buttonId, const std::string& name) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 85);
    
    MedievalStyle::PushButtonStyle();
    
    bool clicked = ImGui::Button(buttonId);
    
    MedievalStyle::PopButtonStyle();
    
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Configure %s", name.c_str());
        ImGui::EndTooltip();
    }
    return clicked;
}

void KeyFunction::Render() {
    RenderKeyButton(keyId.c_str(), waitingForKey, *key);
    ImGui::SameLine();
    if (toggleable) {
        MedievalStyle::PushCheckboxStyle();
        
        bool currentEnabled = isEnabled;
        if (ImGui::Checkbox(checkId.c_str(), &currentEnabled) && currentEnabled != isEnabled)
            SetEnabled(currentEnabled);
        
        MedievalStyle::PopCheckboxStyle();
        
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
            MedievalStyle::PushPopupStyle();
            
            RenderParameters();
            
            MedievalStyle::PopPopupStyle();
            ImGui::EndPopup();
            popupWasOpen = true;
        } else if (popupWasOpen) {
            SaveParameters();
            popupWasOpen = false;
        }
    }

    if (KeybindManager::HandleKeyPress(waitingForKey, *key)) {
        int newKey = *key;
        if (newKey != -1 && KeybindManager::IsKeyBound(newKey, key)) {
            pendingConflictKey = newKey;
            pendingConflictKeyPtr = key;
            ImGui::OpenPopup(conflictPopupId.c_str());
        } else {
            OnKeyAssigned();
        }
    } else if (toggleable && !waitingForKey && *key != -1 && (GetAsyncKeyState(*key) & 1)) {
        SetEnabled(!isEnabled);
    }

    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0.6f));
    if (ImGui::BeginPopupModal(conflictPopupId.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto conflictFunc = KeybindManager::GetBoundFunction(pendingConflictKey, pendingConflictKeyPtr);
        const char* conflictName = conflictFunc ? conflictFunc->GetName().c_str() : "Unknown";
        ImGui::Text("Key %s is already bound to %s. What do you want to do?", 
                    KeybindManager::GetKeyName(pendingConflictKey), conflictName);
        if (ImGui::Button(replaceButtonId.c_str())) {
            KeybindManager::RemoveBinding(pendingConflictKey, pendingConflictKeyPtr);
            OnKeyAssigned();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(cancelButtonId.c_str())) {
            *pendingConflictKeyPtr = prevKey;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(chooseButtonId.c_str())) {
            *pendingConflictKeyPtr = prevKey;
            waitingForKey = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

void KeybindFunction::OnKeyAssigned() {
    if (*key != -1)
        KeybindManager::RegisterKeybind(key, [this]() { callback(isEnabled); }, this);
    UpdateKey();
}

void HookedFunction::OnKeyAssigned() {
    KeybindManager::UnregisterKeybind(key);
    if (*key != -1)
        KeybindManager::RegisterKeybind(key, [this]() { SetEnabled(!isEnabled); }, this);
    SetKey();
}

void Parameter::Render() {
    ImGui::PushItemWidth(160.0f);
    ImGui::AlignTextToFramePadding();
    
    ImGui::TextColored(MedievalStyle::parchmentDark, "%s", displayName.c_str());
    ImGui::SameLine();
    
    switch (type) {
        case Type::Int: {
            auto intPtr = std::get<int*>(valuePtr);
            
            MedievalStyle::PushInputStyle();
            
            ImGui::PushItemWidth(65.0f);
            char inputBuffer[16];
            snprintf(inputBuffer, sizeof(inputBuffer), "%d", *intPtr);
            if (ImGui::InputText((id + "_input").c_str(), inputBuffer, sizeof(inputBuffer), 
                                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                *intPtr = atoi(inputBuffer);
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, MedievalStyle::oldBrass);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, MedievalStyle::brightBrass);
            
            ImGui::SliderInt(id.c_str(), intPtr, std::get<int>(minValue), std::get<int>(maxValue));
            
            ImGui::PopStyleColor(2);
            MedievalStyle::PopInputStyle();
            break;
        }
        case Type::Float: {
            auto floatPtr = std::get<float*>(valuePtr);
            
            MedievalStyle::PushInputStyle();
            
            ImGui::PushItemWidth(65.0f);
            char inputBuffer[16];
            snprintf(inputBuffer, sizeof(inputBuffer), "%.2f", *floatPtr);
            if (ImGui::InputText((id + "_input").c_str(), inputBuffer, sizeof(inputBuffer), 
                                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                *floatPtr = static_cast<float>(atof(inputBuffer));
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, MedievalStyle::oldBrass);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, MedievalStyle::brightBrass);
            
            ImGui::SliderFloat(id.c_str(), floatPtr, std::get<float>(minValue), std::get<float>(maxValue), "%.2f");
            
            ImGui::PopStyleColor(2);
            MedievalStyle::PopInputStyle();
            break;
        }
        case Type::Bool:
            MedievalStyle::PushCheckboxStyle();
            
            ImGui::Checkbox(id.c_str(), std::get<bool*>(valuePtr));
            
            MedievalStyle::PopCheckboxStyle();
            break;
    }
    
    ImGui::PopItemWidth();
}