#include <Windows.h>
#include <string>

#include "Menu/IMenuFunction.h"
#include "Menu/ICollapsibleSection.h"
#include "imgui/imgui.h"
#include "Gui.h"
#include "GlobalDefinitions.h"
#include "ConfigManager.h"
#include "KeybindManager.h"

namespace {
    constexpr ImVec4 disabledButtonColor = ImVec4(0.18f, 0.13f, 0.09f, 0.50f);
    constexpr ImVec4 disabledTextColor = ImVec4(0.55f, 0.45f, 0.35f, 0.60f);
    constexpr ImVec4 disabledBorderColor = ImVec4(0.28f, 0.20f, 0.12f, 0.40f);
    constexpr ImVec4 disabledNameColor = ImVec4(0.60f, 0.55f, 0.48f, 0.70f);
    constexpr ImVec4 modalDimColor = ImVec4(0, 0, 0, 0.6f);
    
    constexpr float buttonWidthPadding = 28.0f;
    constexpr float parameterButtonOffset = 85.0f;
    constexpr float itemWidth160 = 160.0f;
    constexpr float itemWidth65 = 65.0f;
    constexpr float frameBorderSize = 1.0f;
    
    constexpr const char* pressKeyText = "Press a key...";
    constexpr const char* configureText = "Configure %s";
    constexpr const char* changeKeybindText = "Change keybind";
    constexpr const char* replaceButtonText = "Replace";
    constexpr const char* cancelButtonText = "Cancel";
    constexpr const char* chooseAnotherText = "Choose Another";
    constexpr const char* unknownText = "Unknown";
    constexpr const char* keyConflictFormat = "Key %s is already bound to %s. What do you want to do?";
    
    struct ButtonStyleRAII {
        explicit ButtonStyleRAII(bool disabled) : pushCount(disabled ? 4 : 0) {
            if (disabled) {
                ImGui::PushStyleColor(ImGuiCol_Button, disabledButtonColor);
                ImGui::PushStyleColor(ImGuiCol_Text, disabledTextColor);
                ImGui::PushStyleColor(ImGuiCol_Border, disabledBorderColor);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, frameBorderSize);
            }
        }
        ~ButtonStyleRAII() {
            if (pushCount) {
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
            }
        }
        int pushCount;
    };
    
    struct SliderStyleRAII {
        SliderStyleRAII() {
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, DefaultStyle::oldBrass);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, DefaultStyle::brightBrass);
        }
        ~SliderStyleRAII() {
            ImGui::PopStyleColor(2);
        }
    };
}

static void RenderKeyButton(const char* id, bool& waitingForKey, int key) {
    const char* keyText = waitingForKey ? pressKeyText : KeybindManager::GetKeyName(key);
    const bool disabled = (key == -1);
    
    ButtonStyleRAII style(disabled);
    
    float textWidth = ImGui::CalcTextSize(keyText).x;
    ImGui::SetNextItemWidth(textWidth + buttonWidthPadding);
    ImGui::PushID(id);
    if (ImGui::Button(keyText))
        waitingForKey = true;
    ImGui::PopID();
    
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextColored(DefaultStyle::parchment, changeKeybindText);
        ImGui::EndTooltip();
    }
}

static inline void RenderName(const std::string& name, bool isDisabled) {
    ImGui::TextColored(
        isDisabled ? disabledNameColor : DefaultStyle::parchment, 
        "%s", name.c_str()
    );
}

static bool RenderParametersButton(const char* buttonId, const std::string& name) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - parameterButtonOffset);
    
    bool clicked = ImGui::Button(buttonId);
    
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text(configureText, name.c_str());
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

    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, modalDimColor);
    if (ImGui::BeginPopupModal(GetConflictPopupId(), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto conflictFunc = KeybindManager::GetBoundFunction(pendingConflictKey, pendingConflictKeyPtr);
        std::string conflictName = conflictFunc ? std::string(conflictFunc->GetName()) : unknownText;
        ImGui::Text(keyConflictFormat, 
                    KeybindManager::GetKeyName(pendingConflictKey), conflictName.c_str());
        ImGui::Spacing();
        
        if (ImGui::Button(replaceButtonText)) {
            KeybindManager::RemoveBinding(pendingConflictKey, pendingConflictKeyPtr);
            OnKeyAssigned();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(cancelButtonText)) {
            *pendingConflictKeyPtr = prevKey;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(chooseAnotherText)) {
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
    ImGui::PushItemWidth(itemWidth160);
    ImGui::AlignTextToFramePadding();
    
    ImGui::TextColored(DefaultStyle::parchmentDark, "%s", std::string(param.displayName).c_str());
    ImGui::SameLine();
    
    auto intPtr = static_cast<int*>(param.valuePtr);
    
    ImGui::PushItemWidth(itemWidth65);
    char inputBuffer[16];
    snprintf(inputBuffer, sizeof(inputBuffer), "%d", *intPtr);
    if (ImGui::InputText((param.id + "_input").c_str(), inputBuffer, sizeof(inputBuffer), 
                        ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        *intPtr = atoi(inputBuffer);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    SliderStyleRAII sliderStyle;
    ImGui::SliderInt(param.id.c_str(), intPtr, param.GetIntMin(), param.GetIntMax());
    
    ImGui::PopItemWidth();
}

void Parameter::RenderFloat(const Parameter& param) {
    ImGui::PushItemWidth(itemWidth160);
    ImGui::AlignTextToFramePadding();
    
    ImGui::TextColored(DefaultStyle::parchmentDark, "%s", std::string(param.displayName).c_str());
    ImGui::SameLine();
    
    auto floatPtr = static_cast<float*>(param.valuePtr);
    
    ImGui::PushItemWidth(itemWidth65);
    char inputBuffer[16];
    snprintf(inputBuffer, sizeof(inputBuffer), "%.2f", *floatPtr);
    if (ImGui::InputText((param.id + "_input").c_str(), inputBuffer, sizeof(inputBuffer), 
                        ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
        *floatPtr = static_cast<float>(atof(inputBuffer));
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    SliderStyleRAII sliderStyle;
    ImGui::SliderFloat(param.id.c_str(), floatPtr, param.GetFloatMin(), param.GetFloatMax(), "%.2f");
    
    ImGui::PopItemWidth();
}

void Parameter::RenderBool(const Parameter& param) {
    ImGui::PushItemWidth(itemWidth160);
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