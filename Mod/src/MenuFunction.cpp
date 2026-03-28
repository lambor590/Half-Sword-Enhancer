#include <string>
#include <algorithm>

#include "Menu/IMenuFunction.h"
#include "Menu/ICollapsibleSection.h"
#include "imgui/imgui.h"
#include "Gui.h"
#include "GlobalDefinitions.h"
#include "ConfigManager.h"
#include "KeybindManager.h"
#include "Utils/GuiUtils.h"

namespace GuiConstants {
    constexpr ImVec4 DISABLED_BUTTON_COLOR{0.18f, 0.13f, 0.09f, 0.50f};
    constexpr ImVec4 DISABLED_TEXT_COLOR{0.55f, 0.45f, 0.35f, 0.60f};
    constexpr ImVec4 DISABLED_BORDER_COLOR{0.28f, 0.20f, 0.12f, 0.40f};
    constexpr ImVec4 DISABLED_NAME_COLOR{0.60f, 0.55f, 0.48f, 0.70f};
    constexpr ImVec4 MODAL_DIM_COLOR{0, 0, 0, 0.6f};

    constexpr float BUTTON_WIDTH_PADDING = 28.0f;
    constexpr float PARAMETER_BUTTON_OFFSET = 95.0f;
    constexpr float ITEM_WIDTH_160 = 160.0f;
    constexpr float FRAME_BORDER_SIZE = 1.0f;

    constexpr std::string_view PRESS_KEY_TEXT = "Press a key...";
    constexpr std::string_view CONFIGURE_TEXT = "Configure %s";
    constexpr std::string_view CHANGE_KEYBIND_TEXT = "Change keybind";
    constexpr std::string_view REPLACE_BUTTON_TEXT = "Replace";
    constexpr std::string_view SHARE_KEY_BUTTON_TEXT = "Share Key";
    constexpr std::string_view CANCEL_BUTTON_TEXT = "Cancel";
    constexpr std::string_view CHOOSE_ANOTHER_TEXT = "Choose Another";
    constexpr std::string_view UNKNOWN_TEXT = "Unknown";
    constexpr std::string_view KEY_CONFLICT_FORMAT = "Key %s is already bound to %s. What do you want to do?";
    constexpr std::string_view KEY_MULTI_CONFLICT_FORMAT =
        "Key %s is already bound to %d functions. What do you want to do?";
}

namespace {
    constexpr bool IsKeyUnbound(int key) noexcept {
        return key == -1 || key == 255;
    }

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
        ~SliderStyleRAII() { ImGui::PopStyleColor(2); }
    };
}

static void RenderKeyButton(const char* id, bool& waitingForKey, int key, int& pendingOriginalKey) {
    const char* keyText = waitingForKey ? GuiConstants::PRESS_KEY_TEXT.data() : KeybindManager::GetKeyName(key);
    const ButtonStyleRAII style(IsKeyUnbound(key));

    const float textWidth = ImGui::CalcTextSize(keyText).x;
    ImGui::SetNextItemWidth(textWidth + GuiConstants::BUTTON_WIDTH_PADDING);
    ImGui::PushID(id);

    if (ImGui::Button(keyText)) {
        waitingForKey = true;
        pendingOriginalKey = key;
    }

    ImGui::PopID();

    if (ImGui::IsItemHovered()) [[unlikely]] {
        GuiUtils::BeginStyledTooltip();
        ImGui::TextColored(DefaultStyle::parchment, GuiConstants::CHANGE_KEYBIND_TEXT.data());
        GuiUtils::EndStyledTooltip();
    }
}

static inline void RenderName(std::string_view name, bool isDisabled) {
    ImGui::TextColored(
        isDisabled ? GuiConstants::DISABLED_NAME_COLOR : DefaultStyle::parchment, "%.*s", static_cast<int>(name.size()),
        name.data()
    );
}

static bool RenderParametersButton(const char* buttonId, const std::string& name) {
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - GuiConstants::PARAMETER_BUTTON_OFFSET);

    const bool clicked = ImGui::Button(buttonId);

    if (ImGui::IsItemHovered()) {
        GuiUtils::BeginStyledTooltip();
        ImGui::Text(GuiConstants::CONFIGURE_TEXT.data(), name.c_str());
        GuiUtils::EndStyledTooltip();
    }
    return clicked;
}

template <typename Derived> void KeyFunction<Derived>::Render() {
    RenderKeyButton(GetKeyId(), waitingForKey, *key, pendingOriginalKey);
    ImGui::SameLine();

    if (toggleable) {
        bool currentEnabled = isEnabled;
        if (ImGui::Checkbox(GetCheckId(), &currentEnabled) && currentEnabled != isEnabled) {
            SetEnabled(currentEnabled);
        }

        ImGui::SameLine();
        RenderName(name, !isEnabled && IsKeyUnbound(*key));
        TooltipHelper::ShowTooltip(tooltip);
    } else {
        RenderName(name, IsKeyUnbound(*key));
        TooltipHelper::ShowTooltip(tooltip);
    }

    if (!GetParameters().empty()) {
        if (RenderParametersButton(GetParamButtonId(), name)) {
            ImGui::OpenPopup(GetPopupId());
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, GuiUtils::kPopupPadding);
        if (ImGui::BeginPopup(GetPopupId())) {
            RenderParameters();
            ImGui::EndPopup();
            popupWasOpen = true;
        } else if (popupWasOpen) {
            SaveParameters();
            popupWasOpen = false;
        }
        ImGui::PopStyleVar();
    }

    if (KeybindManager::HandleKeyPress(waitingForKey, *key)) {
        const int newKey = *key;
        if (newKey != -1) {
            cachedBindingCount = KeybindManager::GetBindingCount(newKey, key);
            if (cachedBindingCount > 0) {
                *key = pendingOriginalKey;
                pendingConflictKey = newKey;
                pendingConflictKeyPtr = key;
                cachedBoundFunctions = KeybindManager::GetAllBoundFunctions(pendingConflictKey, pendingConflictKeyPtr);
                ImGui::OpenPopup(GetConflictPopupId());
            } else {
                OnKeyAssigned();
            }
        } else {
            OnKeyAssigned();
        }
    }

    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, GuiConstants::MODAL_DIM_COLOR);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, GuiUtils::kPopupPadding);
    if (ImGui::BeginPopupModal(
            GetConflictPopupId(), nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings
        )) {
        if (cachedBindingCount == 1) {
            std::string conflictName{GuiConstants::UNKNOWN_TEXT};
            if (!cachedBoundFunctions.empty() && cachedBoundFunctions[0] &&
                !cachedBoundFunctions[0]->GetName().empty()) {
                conflictName = std::string(cachedBoundFunctions[0]->GetName());
            }

            ImGui::Text(
                GuiConstants::KEY_CONFLICT_FORMAT.data(), KeybindManager::GetKeyName(pendingConflictKey),
                conflictName.c_str()
            );
        } else {
            ImGui::Text(
                GuiConstants::KEY_MULTI_CONFLICT_FORMAT.data(), KeybindManager::GetKeyName(pendingConflictKey),
                cachedBindingCount
            );

            ImGui::Spacing();
            ImGui::Text("Currently bound to:");
            for (size_t i = 0; i < cachedBoundFunctions.size() && i < 5; ++i) {
                if (cachedBoundFunctions[i] && !cachedBoundFunctions[i]->GetName().empty()) {
                    ImGui::BulletText("%s", cachedBoundFunctions[i]->GetName().data());
                } else {
                    ImGui::BulletText("Unknown Function");
                }
            }
            if (cachedBoundFunctions.size() > 5) {
                ImGui::BulletText("... and %zu more", cachedBoundFunctions.size() - 5);
            }
        }
        ImGui::Spacing();

        const char* replaceButtonText = (cachedBindingCount > 1) ? "Remove One" : "Replace";
        if (ImGui::Button(replaceButtonText)) {
            KeybindManager::RemoveBinding(pendingConflictKey, pendingConflictKeyPtr);
            *pendingConflictKeyPtr = pendingConflictKey;
            OnKeyAssigned();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(GuiConstants::SHARE_KEY_BUTTON_TEXT.data())) {
            *pendingConflictKeyPtr = pendingConflictKey;
            OnKeyAssigned();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(GuiConstants::CANCEL_BUTTON_TEXT.data())) {
            *pendingConflictKeyPtr = pendingOriginalKey;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(GuiConstants::CHOOSE_ANOTHER_TEXT.data())) {
            *pendingConflictKeyPtr = pendingOriginalKey;
            waitingForKey = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

template void KeyFunction<HookedFunction>::Render();
template void KeyFunction<KeybindFunction>::Render();

void KeybindFunction::OnKeyAssigned() {
    KeybindManager::UnregisterKeybind(key);
    if (*key != -1) KeybindManager::RegisterKeybind(key, [this]() { callback(isEnabled); }, this);
    UpdateKey();
}

void HookedFunction::OnKeyAssigned() {
    KeybindManager::UnregisterKeybind(key);
    if (*key != -1) KeybindManager::RegisterKeybind(key, [this]() { SetEnabled(!isEnabled); }, this);
    SetKey();
}

namespace {
    template <typename T> void RenderParameterImpl(const Parameter& param) noexcept {
        ImGui::PushItemWidth(GuiConstants::ITEM_WIDTH_160);
        ImGui::AlignTextToFramePadding();

        ImGui::TextColored(
            DefaultStyle::parchmentDark, "%.*s", static_cast<int>(param.displayName.size()), param.displayName.data()
        );
        TooltipHelper::ShowTooltip(param.tooltip);
        ImGui::SameLine();

        auto* const valuePtr = static_cast<T*>(param.valuePtr);

        if constexpr (std::is_same_v<T, bool>) {
            ImGui::Checkbox(param.id.c_str(), valuePtr);
        } else {
            const SliderStyleRAII sliderStyle;
            if constexpr (std::is_integral_v<T>) {
                ImGui::DragInt(param.id.c_str(), valuePtr, 1.0f, 0, 0);
            } else {
                ImGui::DragFloat(param.id.c_str(), valuePtr, 0.01f, 0.0f, 0.0f, "%.2f");
            }
        }

        ImGui::PopItemWidth();
    }

    template <typename T> void LoadParameterImpl(const Parameter& param, const IMenuFunction* func) noexcept {
        auto* valuePtr = static_cast<T*>(param.valuePtr);
        *valuePtr = func->GetConfig(param.name, *valuePtr);
    }

    template <typename T> void SaveParameterImpl(const Parameter& param, const IMenuFunction* func) noexcept {
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

namespace {
    struct TooltipState {
        bool enabled = true;
        uint8_t counter = 0;

        [[nodiscard]] inline bool IsEnabled() noexcept {
            if (++counter >= 60) [[unlikely]] {
                enabled = g_ConfigManager.GetBool("GUI", "tooltips_enabled", true);
                counter = 0;
            }
            return enabled;
        }

        inline void Refresh() noexcept {
            enabled = g_ConfigManager.GetBool("GUI", "tooltips_enabled", true);
            counter = 0;
        }
    };

    thread_local TooltipState g_state;
}

void TooltipHelper::ShowTooltip(std::string_view tooltip) {
    if (tooltip.empty()) [[unlikely]]
        return;
    if (!g_state.IsEnabled()) [[likely]]
        return;

    if (ImGui::IsItemHovered()) [[unlikely]] {
        GuiUtils::BeginStyledTooltip();
        ImGui::TextColored(DefaultStyle::parchment, "%.*s", static_cast<int>(tooltip.size()), tooltip.data());
        GuiUtils::EndStyledTooltip();
    }
}

void TooltipHelper::InvalidateCache() {
    g_state.Refresh();
}