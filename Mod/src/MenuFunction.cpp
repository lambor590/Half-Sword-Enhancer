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
        
        float textWidth = ImGui::CalcTextSize(keyText).x;
        ImGui::SetNextItemWidth(textWidth + 28);
        ImGui::PushID(id);
        if (ImGui::Button(keyText))
            waitingForKey = true;
        ImGui::PopID();
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    } else {
        float textWidth = ImGui::CalcTextSize(keyText).x;
        ImGui::SetNextItemWidth(textWidth + 28);
        ImGui::PushID(id);
        if (ImGui::Button(keyText))
            waitingForKey = true;
        ImGui::PopID();
    }
    
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextColored(DefaultStyle::parchment, "Change keybind");
        ImGui::EndTooltip();
    }
}

static inline void RenderName(const std::string& name, bool isDisabled) {
    ImGui::TextColored(
        isDisabled ? ImVec4(0.60f, 0.55f, 0.48f, 0.70f) : DefaultStyle::parchment, 
        "%s", name.c_str()
    );
}

static bool RenderParametersButton(const char* buttonId, const std::string& name) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 85);
    
    bool clicked = ImGui::Button(buttonId);
    
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Configure %s", name.c_str());
        ImGui::EndTooltip();
    }
    return clicked;
}

template<typename Derived>
void KeyFunction<Derived>::Render() {
    RenderKeyButton(GetKeyId(), waitingForKey, *key);
    ImGui::SameLine();
    if (toggleable) {
        bool currentEnabled = isEnabled;
        if (ImGui::Checkbox(GetCheckId(), &currentEnabled) && currentEnabled != isEnabled)
            SetEnabled(currentEnabled);
        
        ImGui::SameLine();
        RenderName(name, !isEnabled && *key == -1);
    } else {
        RenderName(name, *key == -1);
    }

    if (!GetParameters().empty()) {
        if (RenderParametersButton(GetParamButtonId(), name))
            ImGui::OpenPopup(GetPopupId());
        if (ImGui::BeginPopup(GetPopupId())) {
            RenderParameters();
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
            ImGui::OpenPopup(GetConflictPopupId());
        } else {
            OnKeyAssigned();
        }
    }

    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0.6f));
    if (ImGui::BeginPopupModal(GetConflictPopupId(), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto conflictFunc = KeybindManager::GetBoundFunction(pendingConflictKey, pendingConflictKeyPtr);
        std::string conflictName = conflictFunc ? std::string(conflictFunc->GetName()) : "Unknown";
        ImGui::Text("Key %s is already bound to %s. What do you want to do?", 
                    KeybindManager::GetKeyName(pendingConflictKey), conflictName.c_str());
        ImGui::Spacing();
        
        if (ImGui::Button("Replace")) {
            KeybindManager::RemoveBinding(pendingConflictKey, pendingConflictKeyPtr);
            OnKeyAssigned();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            *pendingConflictKeyPtr = prevKey;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Choose Another")) {
            *pendingConflictKeyPtr = prevKey;
            waitingForKey = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

template void KeyFunction<HookedFunction>::Render();
template void KeyFunction<KeybindFunction>::Render();

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

void Parameter::RenderInt(const Parameter& param) {
    ImGui::PushItemWidth(160.0f);
    ImGui::AlignTextToFramePadding();
    
    ImGui::TextColored(DefaultStyle::parchmentDark, "%s", std::string(param.displayName).c_str());
    ImGui::SameLine();
    
    auto intPtr = static_cast<int*>(param.valuePtr);
    
    ImGui::PushItemWidth(65.0f);
    char inputBuffer[16];
    snprintf(inputBuffer, sizeof(inputBuffer), "%d", *intPtr);
    if (ImGui::InputText((param.id + "_input").c_str(), inputBuffer, sizeof(inputBuffer), 
                        ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        *intPtr = atoi(inputBuffer);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, DefaultStyle::oldBrass);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, DefaultStyle::brightBrass);
    
    ImGui::SliderInt(param.id.c_str(), intPtr, param.GetIntMin(), param.GetIntMax());
    
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();
}

void Parameter::RenderFloat(const Parameter& param) {
    ImGui::PushItemWidth(160.0f);
    ImGui::AlignTextToFramePadding();
    
    ImGui::TextColored(DefaultStyle::parchmentDark, "%s", std::string(param.displayName).c_str());
    ImGui::SameLine();
    
    auto floatPtr = static_cast<float*>(param.valuePtr);
    
    ImGui::PushItemWidth(65.0f);
    char inputBuffer[16];
    snprintf(inputBuffer, sizeof(inputBuffer), "%.2f", *floatPtr);
    if (ImGui::InputText((param.id + "_input").c_str(), inputBuffer, sizeof(inputBuffer), 
                        ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        *floatPtr = static_cast<float>(atof(inputBuffer));
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, DefaultStyle::oldBrass);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, DefaultStyle::brightBrass);
    
    ImGui::SliderFloat(param.id.c_str(), floatPtr, param.GetFloatMin(), param.GetFloatMax(), "%.2f");
    
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();
}

void Parameter::RenderBool(const Parameter& param) {
    ImGui::PushItemWidth(160.0f);
    ImGui::AlignTextToFramePadding();
    
    ImGui::TextColored(DefaultStyle::parchmentDark, "%s", std::string(param.displayName).c_str());
    ImGui::SameLine();
    
    ImGui::Checkbox(param.id.c_str(), static_cast<bool*>(param.valuePtr));
    ImGui::PopItemWidth();
}

void Parameter::LoadInt(const Parameter& param, const IMenuFunction* func) {
    auto intPtr = static_cast<int*>(param.valuePtr);
    *intPtr = func->GetConfig(param.GetName(), *intPtr);
}

void Parameter::LoadFloat(const Parameter& param, const IMenuFunction* func) {
    auto floatPtr = static_cast<float*>(param.valuePtr);
    *floatPtr = func->GetConfig(param.GetName(), *floatPtr);
}

void Parameter::LoadBool(const Parameter& param, const IMenuFunction* func) {
    auto boolPtr = static_cast<bool*>(param.valuePtr);
    *boolPtr = func->GetConfig(param.GetName(), *boolPtr);
}

void Parameter::SaveInt(const Parameter& param, const IMenuFunction* func) {
    auto intPtr = static_cast<int*>(param.valuePtr);
    func->SaveConfig(param.GetName(), *intPtr);
}

void Parameter::SaveFloat(const Parameter& param, const IMenuFunction* func) {
    auto floatPtr = static_cast<float*>(param.valuePtr);
    func->SaveConfig(param.GetName(), *floatPtr);
}

void Parameter::SaveBool(const Parameter& param, const IMenuFunction* func) {
    auto boolPtr = static_cast<bool*>(param.valuePtr);
    func->SaveConfig(param.GetName(), *boolPtr);
}