#include "Menu/Sections/Settings/GuiSection.h"
#include "Hooks/GameHook.h"
#include "Menu/Keybind.h"
#include "Menu/SectionStyle.h"
#include "ConfigManager.h"
#include "KeybindManager.h"
#include "NotificationManager.h"
#include "Utils/GuiUtils.h"

GuiSection::GuiSection(ModContext& ctx)
    : Section(ctx, SECTION), notificationsEnabled(NotificationManager::IsEnabled()) {
    auto& config = ConfigManager::Get();
    screenOverlays.visualEffects =
        config.GetBool(SCREEN_OVERLAYS_CONFIG, "visual_effects", screenOverlays.visualEffects);
    screenOverlays.resultMenus = config.GetBool(SCREEN_OVERLAYS_CONFIG, "result_menus", screenOverlays.resultMenus);
    screenOverlays.onlyInFreeCamera =
        config.GetBool(SCREEN_OVERLAYS_CONFIG, "only_in_free_camera", screenOverlays.onlyInFreeCamera);
    FreeCameraManager::Get().ConfigureScreenOverlays(screenOverlays);
}

void GuiSection::Render() {
    const SectionStyle::StyleRAII style;

    bool changed =
        RenderKeybind(TOGGLE_GUI_LABEL, TOGGLE_TOOLTIP, waitingForToggleKey, KeybindManager::GetToggleGuiKey());

    ImGui::Spacing();

    changed |= RenderKeybind(UNBIND_LABEL, UNBIND_TOOLTIP, waitingForUnbindKey, KeybindManager::GetUnbindKey());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (GuiUtils::CheckboxWithTooltip(NOTIFICATIONS_LABEL, &notificationsEnabled, NOTIFICATIONS_TOOLTIP)) {
        NotificationManager::SetEnabled(notificationsEnabled);
    }

    ImGui::Spacing();

    static bool tooltipsEnabled = ConfigManager::Get().GetBool("GUI", "tooltips_enabled", true);
    if (GuiUtils::CheckboxWithTooltip(TOOLTIPS_LABEL, &tooltipsEnabled, TOOLTIPS_TOOLTIP)) {
        ConfigManager::Get().SetBool("GUI", "tooltips_enabled", tooltipsEnabled);
        TooltipHelper::InvalidateCache();
    }

    ImGui::Spacing();

    static bool ueConsoleEnabled = ConfigManager::Get().GetBool("UE", "console_enabled", false);
    if (GuiUtils::CheckboxWithTooltip(UE_CONSOLE_LABEL, &ueConsoleEnabled, UE_CONSOLE_TOOLTIP)) {
        ConfigManager::Get().BatchSave([&]() {
            ConfigManager::Get().SetBool("UE", "console_enabled", ueConsoleEnabled);
        });

        GameHook::Get().SetUEConsoleEnabled(ueConsoleEnabled);
    }

    RenderScreenOverlaySettings();

    if (changed) [[unlikely]]
        KeybindManager::SaveKeybinds();
}

bool GuiSection::RenderKeybind(const char* label, const char* tooltip, bool& waitingForKey, int& key) noexcept {
    const char* const keyName = waitingForKey ? PRESS_KEY_TEXT : KeybindManager::GetKeyName(key);

    ImGui::AlignTextToFramePadding();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize(keyName).x + BUTTON_PADDING);

    if (ImGui::Button(keyName)) [[unlikely]]
        waitingForKey = true;

    ImGui::SameLine();
    ImGui::Text(label);
    if (ImGui::IsItemHovered()) [[unlikely]] {
        GuiUtils::BeginStyledTooltip();
        ImGui::Text(tooltip);
        GuiUtils::EndStyledTooltip();
    }

    return KeybindManager::HandleKeyPress(waitingForKey, key);
}

void GuiSection::RenderScreenOverlaySettings() {
    ImGui::Spacing();
    ImGui::SeparatorText(SCREEN_OVERLAYS_LABEL);

    bool changed =
        GuiUtils::CheckboxWithTooltip(VISUAL_EFFECTS_LABEL, &screenOverlays.visualEffects, VISUAL_EFFECTS_TOOLTIP);
    changed |= GuiUtils::CheckboxWithTooltip(RESULT_MENUS_LABEL, &screenOverlays.resultMenus, RESULT_MENUS_TOOLTIP);
    changed |= GuiUtils::CheckboxWithTooltip(
        ONLY_FREE_CAMERA_LABEL, &screenOverlays.onlyInFreeCamera, ONLY_FREE_CAMERA_TOOLTIP
    );

    if (!changed) return;
    auto& config = ConfigManager::Get();
    config.BatchSave([&]() {
        config.SetBool(SCREEN_OVERLAYS_CONFIG, "visual_effects", screenOverlays.visualEffects);
        config.SetBool(SCREEN_OVERLAYS_CONFIG, "result_menus", screenOverlays.resultMenus);
        config.SetBool(SCREEN_OVERLAYS_CONFIG, "only_in_free_camera", screenOverlays.onlyInFreeCamera);
    });
    FreeCameraManager::Get().ConfigureScreenOverlays(screenOverlays);
}
