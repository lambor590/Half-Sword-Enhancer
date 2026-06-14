#pragma once

#include "Menu/Section.h"

class GuiSection : public Section {
public:
    static constexpr SectionDefinition SECTION{MenuTab::Settings, "GUI"};

private:
    static constexpr const char* TOGGLE_GUI_LABEL = "Toggle GUI Key";
    static constexpr const char* UNBIND_LABEL = "Unbind Key";
    static constexpr const char* TOGGLE_TOOLTIP = "Change key to show/hide interface";
    static constexpr const char* UNBIND_TOOLTIP = "Change key to unbind shortcuts";
    static constexpr const char* NOTIFICATIONS_LABEL = "Enable Notifications";
    static constexpr const char* NOTIFICATIONS_TOOLTIP = "Show notifications when keybinds are activated";
    static constexpr const char* TOOLTIPS_LABEL = "Enable Tooltips";
    static constexpr const char* TOOLTIPS_TOOLTIP = "Show helpful tooltips when hovering over interface elements";
    static constexpr const char* UE_CONSOLE_LABEL = "Unlock UE Console";
    static constexpr const char* UE_CONSOLE_TOOLTIP = "Unlock access to Unreal Engine console (F2)";
    static constexpr const char* PRESS_KEY_TEXT = "Press any key...";
    static constexpr float BUTTON_PADDING = 20.0f;

    bool waitingForToggleKey = false;
    bool waitingForUnbindKey = false;
    bool notificationsEnabled;

    [[nodiscard]] bool RenderKeybind(const char* label, const char* tooltip, bool& waitingForKey, int& key) noexcept;

public:
    explicit GuiSection(ModContext& ctx);
    void Render() override;
};
