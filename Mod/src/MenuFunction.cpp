#include <Windows.h>
#include <string>
#include <algorithm>

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
    
    const ButtonStyleRAII style(disabled);
    
    const float textWidth = ImGui::CalcTextSize(keyText).x;
    ImGui::SetNextItemWidth(textWidth + buttonWidthPadding);
    ImGui::PushID(id);
    
    if (ImGui::Button(keyText)) {
        waitingForKey = true;
    }
    
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
    
    const bool clicked = ImGui::Button(buttonId);
    
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
        if (ImGui::Checkbox(GetCheckId(), &currentEnabled) && currentEnabled != isEnabled) {
            SetEnabled(currentEnabled);
        }
        
        ImGui::SameLine();
        RenderName(name, !isEnabled && *key == -1);
    } else {
        RenderName(name, *key == -1);
    }

    if (!GetParameters().empty()) {
        if (RenderParametersButton(GetParamButtonId(), name)) {
            ImGui::OpenPopup(GetPopupId());
        }
        
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
        const int newKey = *key;
        if (newKey != -1 && KeybindManager::IsKeyBound(newKey, key)) {
            pendingConflictKey = newKey;
            pendingConflictKeyPtr = key;
            ImGui::OpenPopup(GetConflictPopupId());
        } else {
            OnKeyAssigned();
        }
    }

    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, modalDimColor);
    if (ImGui::BeginPopupModal(GetConflictPopupId(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto conflictFunc = KeybindManager::GetBoundFunction(pendingConflictKey, pendingConflictKeyPtr);
        const std::string conflictName = conflictFunc ? std::string(conflictFunc->GetName()) : unknownText;
        
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

namespace {
    template<typename T>
    void RenderParameterImpl(const Parameter& param) noexcept {
        ImGui::PushItemWidth(itemWidth160);
        ImGui::AlignTextToFramePadding();
        
        ImGui::TextColored(DefaultStyle::parchmentDark, "%.*s", 
                          static_cast<int>(param.displayName.size()), param.displayName.data());
        ImGui::SameLine();
        
        const auto valuePtr = static_cast<T*>(param.valuePtr);
        
        if constexpr (std::is_same_v<T, bool>) {
            ImGui::Checkbox(param.id.c_str(), valuePtr);
        } else {
            ImGui::PushItemWidth(itemWidth65);
            char inputBuffer[16];
            
            if constexpr (std::is_same_v<T, int>) {
                snprintf(inputBuffer, sizeof(inputBuffer), "%d", *valuePtr);
                
                std::string inputId;
                inputId.reserve(param.id.length() + 7);
                inputId = param.id + "_input";
                
                if (ImGui::InputText(inputId.c_str(), inputBuffer, sizeof(inputBuffer), 
                                    ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                    *valuePtr = atoi(inputBuffer);
                }
                ImGui::PopItemWidth();
                ImGui::SameLine();
                const SliderStyleRAII sliderStyle;
                ImGui::SliderInt(param.id.c_str(), valuePtr, param.minValue.intMin, param.maxValue.intMax);
            } else {
                snprintf(inputBuffer, sizeof(inputBuffer), "%.2f", *valuePtr);
                
                std::string inputId;
                inputId.reserve(param.id.length() + 7);
                inputId = param.id + "_input";
                
                if (ImGui::InputText(inputId.c_str(), inputBuffer, sizeof(inputBuffer), 
                                    ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                    *valuePtr = static_cast<float>(atof(inputBuffer));
                }
                ImGui::PopItemWidth();
                ImGui::SameLine();
                const SliderStyleRAII sliderStyle;
                ImGui::SliderFloat(param.id.c_str(), valuePtr, param.minValue.floatMin, param.maxValue.floatMax, "%.2f");
            }
        }
        ImGui::PopItemWidth();
    }

    template<typename T>
    void LoadParameterImpl(const Parameter& param, const IMenuFunction* func) noexcept {
        auto valuePtr = static_cast<T*>(param.valuePtr);
        *valuePtr = func->GetConfig(param.name, *valuePtr);
    }

    template<typename T>
    void SaveParameterImpl(const Parameter& param, const IMenuFunction* func) noexcept {
        auto valuePtr = static_cast<T*>(param.valuePtr);
        func->SaveConfig(param.name, *valuePtr);
    }
}

void Parameter::RenderInt(const Parameter& param) noexcept {
    RenderParameterImpl<int>(param);
}

void Parameter::RenderFloat(const Parameter& param) noexcept {
    RenderParameterImpl<float>(param);
}

void Parameter::RenderBool(const Parameter& param) noexcept {
    RenderParameterImpl<bool>(param);
}

void Parameter::LoadInt(const Parameter& param, const IMenuFunction* func) noexcept {
    LoadParameterImpl<int>(param, func);
}

void Parameter::LoadFloat(const Parameter& param, const IMenuFunction* func) noexcept {
    LoadParameterImpl<float>(param, func);
}

void Parameter::LoadBool(const Parameter& param, const IMenuFunction* func) noexcept {
    LoadParameterImpl<bool>(param, func);
}

void Parameter::SaveInt(const Parameter& param, const IMenuFunction* func) noexcept {
    SaveParameterImpl<int>(param, func);
}

void Parameter::SaveFloat(const Parameter& param, const IMenuFunction* func) noexcept {
    SaveParameterImpl<float>(param, func);
}

void Parameter::SaveBool(const Parameter& param, const IMenuFunction* func) noexcept {
    SaveParameterImpl<bool>(param, func);
}