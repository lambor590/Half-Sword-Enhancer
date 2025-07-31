#pragma once

#include "Menu/ICollapsibleSection.h"
#include "ConfigManager.h"
#include "KeybindManager.h"
#include "Menu/IMenuFunction.h"
#include "NotificationManager.h"
#include "Hooks/GameHook.h"
#include "Utils/GuiUtils.h"
#include "Utils/ConfigUtils.h"

class GuiSection : public CollapsibleSection {
    static constexpr const char* GUI_SECTION_NAME = "GUI";
    static constexpr const char* TOGGLE_GUI_LABEL = "Toggle GUI Key";
    static constexpr const char* UNBIND_LABEL = "Unbind Key";
    static constexpr const char* TOGGLE_TOOLTIP = "Change key to show/hide interface";
    static constexpr const char* UNBIND_TOOLTIP = "Change key to unbind shortcuts";
    static constexpr const char* NOTIFICATIONS_LABEL = "Enable Notifications";
    static constexpr const char* NOTIFICATIONS_TOOLTIP = "Show notifications when keybinds are activated";
    static constexpr const char* TOOLTIPS_LABEL = "Enable Tooltips";
    static constexpr const char* TOOLTIPS_TOOLTIP = "Show helpful tooltips when hovering over interface elements";
    static constexpr const char* TOOLTIPS_ID = "##tooltips";
    static constexpr const char* UE_CONSOLE_LABEL = "Unlock UE Console";
    static constexpr const char* UE_CONSOLE_TOOLTIP = "Unlock access to Unreal Engine console (F2)";
    static constexpr const char* PRESS_KEY_TEXT = "Press any key...";
    static constexpr float BUTTON_PADDING = 20.0f;

    bool waitingForToggleKey = false;
    bool waitingForUnbindKey = false;
    bool notificationsEnabled;

public:
    GuiSection() : CollapsibleSection(GUI_SECTION_NAME), notificationsEnabled(NotificationManager::IsEnabled()) {}

    void Render() override {
        if (!ImGui::CollapsingHeader(name.c_str())) [[likely]] return;
        
        const SectionStyle::StyleRAII style;
        
        bool changed = RenderKeybind(TOGGLE_GUI_LABEL, TOGGLE_TOOLTIP, waitingForToggleKey, KeybindManager::GetToggleGuiKey());
        
        ImGui::Spacing();
        
        changed |= RenderKeybind(UNBIND_LABEL, UNBIND_TOOLTIP, waitingForUnbindKey, KeybindManager::GetUnbindKey());
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (GuiUtils::CheckboxWithTooltip(NOTIFICATIONS_LABEL, &notificationsEnabled, NOTIFICATIONS_TOOLTIP)) {
            NotificationManager::SetEnabled(notificationsEnabled);
        }
        
        ImGui::Spacing();
        
        if (GuiUtils::CheckboxWithConfig(TOOLTIPS_LABEL, "GUI", "tooltips_enabled", true, TOOLTIPS_TOOLTIP)) {
            TooltipHelper::InvalidateCache();
        }
        
        ImGui::Spacing();
        
        static bool ueConsoleEnabled = ConfigManager::Get().GetBool("UE", "console_enabled", false);
        if (GuiUtils::CheckboxWithTooltip(UE_CONSOLE_LABEL, &ueConsoleEnabled, UE_CONSOLE_TOOLTIP)) {
            ConfigUtils::BatchUpdate([&](ConfigUtils::ConfigTransaction& config) {
                config.SetBool("UE", "console_enabled", ueConsoleEnabled);
            });
            
            if (ueConsoleEnabled) {
                GameHook::Get().UnlockUEConsole();
            } else {
                GameHook::Get().LockUEConsole();
            }
        }
        
        if (changed) [[unlikely]] KeybindManager::SaveKeybinds();
    }

private:
    [[nodiscard]] bool RenderKeybind(const char* label, const char* tooltip, bool& waitingForKey, int& key) noexcept {
        const char* const keyName = waitingForKey ? PRESS_KEY_TEXT : KeybindManager::GetKeyName(key);
        
        ImGui::AlignTextToFramePadding();
        ImGui::SetNextItemWidth(ImGui::CalcTextSize(keyName).x + BUTTON_PADDING);
        
        if (ImGui::Button(keyName)) [[unlikely]] waitingForKey = true;
        
        ImGui::SameLine();
        ImGui::Text(label);
        if (ImGui::IsItemHovered()) [[unlikely]] {
            ImGui::BeginTooltip();
            ImGui::Text(tooltip);
            ImGui::EndTooltip();
        }
        
        return KeybindManager::HandleKeyPress(waitingForKey, key);
    }
};