#include <algorithm>

#include "Menu/Sections/Settings/GuiSection.h"
#include "Hooks/GameHook.h"
#include "Menu/Keybind.h"
#include "ConfigManager.h"
#include "KeybindManager.h"
#include "NotificationManager.h"
#include "Utils/GuiUtils.h"

GuiSection::GuiSection(ModContext& ctx)
    : Section(ctx, SECTION), notificationsEnabled(NotificationManager::IsEnabled()) {
    auto& config = ConfigManager::Get();
    tooltipsEnabled = config.GetBool("GUI", "tooltips_enabled", true);
    ueConsoleEnabled = config.GetBool("UE", "console_enabled", false);
    toggleGuiKey = KeybindManager::GetToggleGuiKey();
    unbindKey = KeybindManager::GetUnbindKey();
    GuiUtils::SetHelpTooltipsEnabled(tooltipsEnabled);
    screenOverlays.visualEffects =
        config.GetBool(SCREEN_OVERLAYS_CONFIG, "visual_effects", screenOverlays.visualEffects);
    screenOverlays.resultMenus = config.GetBool(SCREEN_OVERLAYS_CONFIG, "result_menus", screenOverlays.resultMenus);
    screenOverlays.onlyInFreeCamera =
        config.GetBool(SCREEN_OVERLAYS_CONFIG, "only_in_free_camera", screenOverlays.onlyInFreeCamera);
    FreeCameraManager::Get().ConfigureScreenOverlays(screenOverlays);
}

void GuiSection::Render() {
    ImGui::SeparatorText("Shortcuts");
    GuiUtils::TextDisabledWrapped("Select a shortcut, then press a key. Escape cancels.");
    ImGui::Spacing();

    const bool toggleChanged = RenderKeybind(TOGGLE_GUI_LABEL, TOGGLE_TOOLTIP, toggleGuiKey, true);
    const bool unbindChanged = RenderKeybind(UNBIND_LABEL, UNBIND_TOOLTIP, unbindKey, false);
    if (!shortcutError.empty()) {
        const auto result =
            GuiUtils::RenderCallout("shortcut-error", shortcutError, GuiUtils::CalloutTone::Error, true);
        if (result.dismissed) shortcutError.clear();
    }

    ImGui::SeparatorText("Help & Messages");

    if (GuiUtils::CheckboxWithTooltip(NOTIFICATIONS_LABEL, &notificationsEnabled, NOTIFICATIONS_TOOLTIP)) {
        NotificationManager::SetEnabled(notificationsEnabled);
    }

    if (GuiUtils::CheckboxWithTooltip(TOOLTIPS_LABEL, &tooltipsEnabled, TOOLTIPS_TOOLTIP)) {
        auto& config = ConfigManager::Get();
        config.SetBool("GUI", "tooltips_enabled", tooltipsEnabled);
        config.SaveConfig();
        GuiUtils::SetHelpTooltipsEnabled(tooltipsEnabled);
    }

    RenderScreenOverlaySettings();

    ImGui::SeparatorText("Game Console");
    if (GuiUtils::CheckboxWithTooltip(UE_CONSOLE_LABEL, &ueConsoleEnabled, UE_CONSOLE_TOOLTIP)) {
        auto& config = ConfigManager::Get();
        config.SetBool("UE", "console_enabled", ueConsoleEnabled);
        config.SaveConfig();
        GameHook::Get().SetUEConsoleEnabled(ueConsoleEnabled);
    }

    if (toggleChanged || unbindChanged) [[unlikely]] {
        if (toggleChanged) KeybindManager::SetToggleGuiKey(toggleGuiKey);
        if (unbindChanged) KeybindManager::SetUnbindKey(unbindKey);
        KeybindManager::SaveKeybinds();
    }
}

bool GuiSection::RenderKeybind(const char* label, const char* tooltip, int& key, bool menuShortcut) {
    const void* owner = &key;
    const bool waitingForKey = KeybindManager::IsRebinding(owner);
    const char* const keyName = waitingForKey ? PRESS_KEY_TEXT : KeybindManager::GetKeyName(key);

    ImGui::PushID(owner);
    const float availableWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
    const float keyWidth = KeybindUi::CalculateKeycapWidth(keyName, availableWidth);

    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    GuiUtils::HelpTooltip(tooltip);
    (void)GuiUtils::SameLineIfFits(keyWidth);
    if (KeybindUi::RenderKeycap("shortcut", keyName, keyWidth)) [[unlikely]]
        KeybindManager::BeginRebind(owner);
    GuiUtils::HelpTooltip(waitingForKey ? "Press a key. Escape cancels." : "Change shortcut");
    ImGui::EndGroup();
    ImGui::PopID();

    int capturedKey = key;
    if (KeybindManager::PollRebind(owner, capturedKey) != KeybindManager::RebindResult::Assigned) return false;
    if (capturedKey < 0) {
        shortcutError = "Menu shortcuts cannot be removed.";
        return false;
    }
    if (menuShortcut && KeybindManager::GetBindingCount(capturedKey) > 0) {
        const std::string boundName = KeybindManager::GetBoundName(capturedKey);
        shortcutError = KeybindManager::GetKeyName(capturedKey);
        shortcutError += " is already used by ";
        shortcutError += boundName.empty() ? "another action" : boundName;
        shortcutError += ". Choose another menu shortcut.";
        return false;
    }
    shortcutError.clear();
    key = capturedKey;
    return true;
}

void GuiSection::RenderScreenOverlaySettings() {
    ImGui::SeparatorText(SCREEN_OVERLAYS_LABEL);

    bool changed =
        GuiUtils::CheckboxWithTooltip(VISUAL_EFFECTS_LABEL, &screenOverlays.visualEffects, VISUAL_EFFECTS_TOOLTIP);
    changed |= GuiUtils::CheckboxWithTooltip(RESULT_MENUS_LABEL, &screenOverlays.resultMenus, RESULT_MENUS_TOOLTIP);
    const bool hasOverlayRule = screenOverlays.visualEffects || screenOverlays.resultMenus;
    if (!hasOverlayRule) ImGui::BeginDisabled();
    changed |= GuiUtils::CheckboxWithTooltip(
        ONLY_FREE_CAMERA_LABEL, &screenOverlays.onlyInFreeCamera, ONLY_FREE_CAMERA_TOOLTIP
    );
    if (!hasOverlayRule) ImGui::EndDisabled();

    if (!changed) return;
    auto& config = ConfigManager::Get();
    config.BatchSave([&]() {
        config.SetBool(SCREEN_OVERLAYS_CONFIG, "visual_effects", screenOverlays.visualEffects);
        config.SetBool(SCREEN_OVERLAYS_CONFIG, "result_menus", screenOverlays.resultMenus);
        config.SetBool(SCREEN_OVERLAYS_CONFIG, "only_in_free_camera", screenOverlays.onlyInFreeCamera);
    });
    FreeCameraManager::Get().ConfigureScreenOverlays(screenOverlays);
}
