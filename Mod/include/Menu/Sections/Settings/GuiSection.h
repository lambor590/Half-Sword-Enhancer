#pragma once

#include <string>

#include "Menu/Section.h"
#include "Utils/FreeCameraManager.h"

class GuiSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::Settings, "Interface", "Choose menu shortcuts, help, messages, and visible HUD elements."
    };

private:
    static constexpr const char* TOGGLE_GUI_LABEL = "Open / Close Menu";
    static constexpr const char* UNBIND_LABEL = "Clear Shortcut";
    static constexpr const char* TOGGLE_TOOLTIP =
        "Choose the shortcut that opens and closes this menu. This shortcut cannot be removed.";
    static constexpr const char* UNBIND_TOOLTIP =
        "Choose the key that removes an assigned shortcut. This shortcut cannot be removed.";
    static constexpr const char* NOTIFICATIONS_LABEL = "Shortcut Notifications";
    static constexpr const char* NOTIFICATIONS_TOOLTIP =
        "Show a brief message when a shortcut runs an action or turns it on or off.";
    static constexpr const char* TOOLTIPS_LABEL = "Help Tooltips";
    static constexpr const char* TOOLTIPS_TOOLTIP = "Show helpful explanations when hovering over controls.";
    static constexpr const char* UE_CONSOLE_LABEL = "Game Console";
    static constexpr const char* UE_CONSOLE_TOOLTIP = "Open the game's command console with F2.";
    static constexpr const char* SCREEN_OVERLAYS_CONFIG = "ScreenOverlays";
    static constexpr const char* SCREEN_OVERLAYS_LABEL = "HUD & Result Screens";
    static constexpr const char* VISUAL_EFFECTS_LABEL = "Hide Visual Effects";
    static constexpr const char* VISUAL_EFFECTS_TOOLTIP =
        "Hide damage, blood, pain, wake-up, and victory effects shown on the screen.";
    static constexpr const char* RESULT_MENUS_LABEL = "Hide Result Menus";
    static constexpr const char* RESULT_MENUS_TOOLTIP =
        "Hide death, defeat, surrender, and victory screens, including the in-game victory banner.";
    static constexpr const char* ONLY_FREE_CAMERA_LABEL = "Keep HUD During Normal Play";
    static constexpr const char* ONLY_FREE_CAMERA_TOOLTIP = "Keep visual HUD effects visible while playing normally.";
    static constexpr const char* PRESS_KEY_TEXT = "Press a key...";

    bool notificationsEnabled;
    bool tooltipsEnabled = true;
    bool ueConsoleEnabled = false;
    int toggleGuiKey = 0;
    int unbindKey = 0;
    std::string shortcutError;
    ScreenOverlaySettings screenOverlays;

    [[nodiscard]] bool RenderKeybind(const char* label, const char* tooltip, int& key, bool menuShortcut);
    void RenderScreenOverlaySettings();

public:
    explicit GuiSection(ModContext& ctx);
    void Render() override;
};
