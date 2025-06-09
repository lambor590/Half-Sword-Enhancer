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

namespace GuiConstants {
    constexpr ImVec4 DISABLED_BUTTON_COLOR{0.18f, 0.13f, 0.09f, 0.50f};
    constexpr ImVec4 DISABLED_TEXT_COLOR{0.55f, 0.45f, 0.35f, 0.60f};
    constexpr ImVec4 DISABLED_BORDER_COLOR{0.28f, 0.20f, 0.12f, 0.40f};
    constexpr ImVec4 DISABLED_NAME_COLOR{0.60f, 0.55f, 0.48f, 0.70f};
    constexpr ImVec4 MODAL_DIM_COLOR{0, 0, 0, 0.6f};
    
    constexpr float BUTTON_WIDTH_PADDING = 28.0f;
    constexpr float PARAMETER_BUTTON_OFFSET = 85.0f;
    constexpr float ITEM_WIDTH_160 = 160.0f;
    constexpr float ITEM_WIDTH_65 = 65.0f;
    constexpr float FRAME_BORDER_SIZE = 1.0f;
    constexpr size_t INPUT_BUFFER_SIZE = 16;
    
    constexpr std::string_view PRESS_KEY_TEXT = "Press a key...";
    constexpr std::string_view CONFIGURE_TEXT = "Configure %s";
    constexpr std::string_view CHANGE_KEYBIND_TEXT = "Change keybind";
    constexpr std::string_view REPLACE_BUTTON_TEXT = "Replace";
    constexpr std::string_view CANCEL_BUTTON_TEXT = "Cancel";
    constexpr std::string_view CHOOSE_ANOTHER_TEXT = "Choose Another";
    constexpr std::string_view UNKNOWN_TEXT = "Unknown";
    constexpr std::string_view KEY_CONFLICT_FORMAT = "Key %s is already bound to %s. What do you want to do?";
    constexpr std::string_view INPUT_SUFFIX = "_input";
}

namespace {
    struct ButtonStyleRAII {
        explicit ButtonStyleRAII(bool disabled) : pushCount(disabled ? 4 : 0) {
            if (disabled) {
                ImGui::PushStyleColor(ImGuiCol_Button, GuiConstants::DISABLED_BUTTON_COLOR);
                ImGui::PushStyleColor(ImGuiCol_Text, GuiConstants::DISABLED_TEXT_COLOR);
                ImGui::PushStyleColor(ImGuiCol_Border, GuiConstants::DISABLED_BORDER_COLOR);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, GuiConstants::FRAME_BORDER_SIZE);
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
    const char* keyText = waitingForKey ? GuiConstants::PRESS_KEY_TEXT.data() : KeybindManager::GetKeyName(key);
    const bool disabled = (key == -1);
    
    const ButtonStyleRAII style(disabled);
    
    const float textWidth = ImGui::CalcTextSize(keyText).x;
    ImGui::SetNextItemWidth(textWidth + GuiConstants::BUTTON_WIDTH_PADDING);
    ImGui::PushID(id);
    
    if (ImGui::Button(keyText)) {
        waitingForKey = true;
    }
    
    ImGui::PopID();
    
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextColored(DefaultStyle::parchment, GuiConstants::CHANGE_KEYBIND_TEXT.data());
        ImGui::EndTooltip();
    }
}

static inline void RenderName(const std::string& name, bool isDisabled) {
    ImGui::TextColored(
        isDisabled ? GuiConstants::DISABLED_NAME_COLOR : DefaultStyle::parchment, 
        "%s", name.c_str()
    );
}

static bool RenderParametersButton(const char* buttonId, const std::string& name) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - GuiConstants::PARAMETER_BUTTON_OFFSET);
    
    const bool clicked = ImGui::Button(buttonId);
    
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text(GuiConstants::CONFIGURE_TEXT.data(), name.c_str());
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

    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, GuiConstants::MODAL_DIM_COLOR);
    if (ImGui::BeginPopupModal(GetConflictPopupId(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto conflictFunc = KeybindManager::GetBoundFunction(pendingConflictKey, pendingConflictKeyPtr);
        const std::string conflictName = conflictFunc ? std::string(conflictFunc->GetName()) : std::string(GuiConstants::UNKNOWN_TEXT);
        
        ImGui::Text(GuiConstants::KEY_CONFLICT_FORMAT.data(), 
                    KeybindManager::GetKeyName(pendingConflictKey), conflictName.c_str());
        ImGui::Spacing();
        
        if (ImGui::Button(GuiConstants::REPLACE_BUTTON_TEXT.data())) {
            KeybindManager::RemoveBinding(pendingConflictKey, pendingConflictKeyPtr);
            OnKeyAssigned();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(GuiConstants::CANCEL_BUTTON_TEXT.data())) {
            *pendingConflictKeyPtr = prevKey;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(GuiConstants::CHOOSE_ANOTHER_TEXT.data())) {
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
        ImGui::PushItemWidth(GuiConstants::ITEM_WIDTH_160);
        ImGui::AlignTextToFramePadding();
        
        ImGui::TextColored(DefaultStyle::parchmentDark, "%.*s", 
                          static_cast<int>(param.displayName.size()), param.displayName.data());
        ImGui::SameLine();
        
        auto* valuePtr = static_cast<T*>(param.valuePtr);
        
        if constexpr (std::is_same_v<T, bool>) {
            ImGui::Checkbox(param.id.c_str(), valuePtr);
        } else {
            ImGui::PushItemWidth(GuiConstants::ITEM_WIDTH_65);
            
            char inputBuffer[GuiConstants::INPUT_BUFFER_SIZE];
            
            if constexpr (std::is_integral_v<T>) {
                snprintf(inputBuffer, GuiConstants::INPUT_BUFFER_SIZE, "%d", *valuePtr);
            } else {
                snprintf(inputBuffer, GuiConstants::INPUT_BUFFER_SIZE, "%.2f", *valuePtr);
            }
            
            thread_local static std::string inputId;
            inputId = param.id;
            inputId += GuiConstants::INPUT_SUFFIX;
            
            constexpr ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue;
            
            if (ImGui::InputText(inputId.c_str(), inputBuffer, GuiConstants::INPUT_BUFFER_SIZE, flags)) {
                if constexpr (std::is_integral_v<T>) {
                    *valuePtr = static_cast<T>(atoi(inputBuffer));
                } else {
                    *valuePtr = static_cast<T>(atof(inputBuffer));
                }
            }
            
            ImGui::PopItemWidth();
            ImGui::SameLine();
            
            const SliderStyleRAII sliderStyle;
            if constexpr (std::is_integral_v<T>) {
                ImGui::SliderInt(param.id.c_str(), valuePtr, param.minValue.intMin, param.maxValue.intMax);
            } else {
                ImGui::SliderFloat(param.id.c_str(), valuePtr, param.minValue.floatMin, param.maxValue.floatMax, "%.2f");
            }
        }
        
        ImGui::PopItemWidth();
    }

    template<typename T>
    void LoadParameterImpl(const Parameter& param, const IMenuFunction* func) noexcept {
        auto* valuePtr = static_cast<T*>(param.valuePtr);
        *valuePtr = func->GetConfig(param.name, *valuePtr);
    }

    template<typename T>
    void SaveParameterImpl(const Parameter& param, const IMenuFunction* func) noexcept {
        auto* valuePtr = static_cast<T*>(param.valuePtr);
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