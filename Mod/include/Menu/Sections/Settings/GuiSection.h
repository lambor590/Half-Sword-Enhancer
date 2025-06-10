#pragma once

#include "Menu/ICollapsibleSection.h"
#include "ConfigManager.h"
#include "KeybindManager.h"
#include "Menu/IMenuFunction.h"
#include "NotificationManager.h"

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
        
        if (ImGui::Checkbox(NOTIFICATIONS_LABEL, &notificationsEnabled)) {
            if (notificationsEnabled) {
                NotificationManager::SetEnabled(true);
            } else {
                NotificationManager::SetEnabled(false);
            }
        }
        if (ImGui::IsItemHovered()) [[unlikely]] {
            ImGui::BeginTooltip();
            ImGui::Text(NOTIFICATIONS_TOOLTIP);
            ImGui::EndTooltip();
        }
        
        ImGui::Spacing();
        
        static bool tooltipsEnabled = g_ConfigManager.GetBool("GUI", "tooltips_enabled", true);
        if (ImGui::Checkbox(TOOLTIPS_LABEL, &tooltipsEnabled)) {
            if (tooltipsEnabled) {
                g_ConfigManager.SetBool("GUI", "tooltips_enabled", true);
                g_ConfigManager.SaveConfig();
                TooltipHelper::InvalidateCache();
            } else {
                g_ConfigManager.SetBool("GUI", "tooltips_enabled", false);
                g_ConfigManager.SaveConfig();
                TooltipHelper::InvalidateCache();
            }
        }
        if (ImGui::IsItemHovered()) [[unlikely]] {
            ImGui::BeginTooltip();
            ImGui::Text(TOOLTIPS_TOOLTIP);
            ImGui::EndTooltip();
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