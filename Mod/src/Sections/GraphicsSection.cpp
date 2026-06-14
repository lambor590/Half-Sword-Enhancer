#include "Menu/Sections/Settings/GraphicsSection.h"
#include "Menu/SectionStyle.h"
#include "ConfigManager.h"
#include "Core/ModContext.h"
#include "Hooks/GameHook.h"
#include "Utils/GuiUtils.h"

#include "SDK/Engine_classes.hpp"

GraphicsSection::GraphicsSection(ModContext& ctx) : Section(ctx, SECTION) {
    LoadSettings();
}

void GraphicsSection::Render() {
    const SectionStyle::StyleRAII style;
    bool settingsChanged = false;

    if (ImGui::Checkbox("Apply on startup", &settings.applyOnStartup)) {
        settingsChanged = true;
    }
    if (ImGui::IsItemHovered()) {
        GuiUtils::BeginStyledTooltip();
        ImGui::Text("Apply these settings when the mod is initialized");
        GuiUtils::EndStyledTooltip();
    }

    ImGui::Spacing();

    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    if (GuiUtils::DebouncedDragInt("Render Scale (%)", &settings.renderScale, 1.0f, 1, 400)) {
        settingsChanged = true;
    }

    ImGui::Spacing();

    static float qualityComboW =
        GuiUtils::CalcComboWidth(QUALITY_LEVELS.data(), static_cast<int>(QUALITY_LEVELS.size()));
    for (const auto& combo : qualityCombos) {
        int currentValue = settings.*combo.memberPtr;
        GuiUtils::PrepareNextCombo(qualityComboW);
        if (ImGui::Combo(combo.label, &currentValue, QUALITY_LEVELS.data(), static_cast<int>(QUALITY_LEVELS.size()))) {
            settings.*combo.memberPtr = currentValue;
            settingsChanged = true;
        }
    }

    if (settingsChanged) [[unlikely]] {
        SaveSettings();
        ApplySettings();
    }
}

void GraphicsSection::LoadSettings() {
    settings = LoadSettingsFromConfig();
}

void GraphicsSection::SaveSettings() {
    auto& config = ConfigManager::Get();
    config.BatchSave([&]() {
        auto section = GRAPHICS_CONFIG_SECTION;

        config.SetBool(section, "apply_on_startup", settings.applyOnStartup);
        config.SetInt(section, "render_scale", settings.renderScale);
        config.SetInt(section, "sg_shadow_quality", settings.sgShadowQuality);
        config.SetInt(section, "sg_global_illumination_quality", settings.sgGlobalIlluminationQuality);
        config.SetInt(section, "sg_reflection_quality", settings.sgReflectionQuality);
        config.SetInt(section, "sg_post_process_quality", settings.sgPostProcessQuality);
        config.SetInt(section, "sg_effects_quality", settings.sgEffectsQuality);
    });
}

void GraphicsSection::ExecuteConsoleCommands(SDK::UWorld* world, const GraphicsSettings& currentSettings) noexcept {
    wchar_t commandBuffer[128];

    for (const auto& cmd : consoleCommands) {
        const int value = currentSettings.*cmd.memberPtr;
        std::swprintf(commandBuffer, sizeof(commandBuffer) / sizeof(wchar_t), L"%ls%d", cmd.commandPrefix, value);
        SDK::UKismetSystemLibrary::ExecuteConsoleCommand(world, SDK::FString(commandBuffer), nullptr);
    }
}

void GraphicsSection::ApplySettings() {
    GameHook::QueueAction([currentSettings = settings](const RuntimeContextSnapshot& runtime) {
        if (!runtime.world) return;
        ExecuteConsoleCommands(runtime.world, currentSettings);
    });
}

GraphicsSection::GraphicsSettings GraphicsSection::LoadSettingsFromConfig() {
    auto& config = ConfigManager::Get();
    auto section = GRAPHICS_CONFIG_SECTION;

    GraphicsSettings loadedSettings;
    loadedSettings.applyOnStartup = config.GetBool(section, "apply_on_startup", false);
    loadedSettings.renderScale = config.GetInt(section, "render_scale", 100);
    loadedSettings.sgShadowQuality = config.GetInt(section, "sg_shadow_quality", 0);
    loadedSettings.sgGlobalIlluminationQuality = config.GetInt(section, "sg_global_illumination_quality", 0);
    loadedSettings.sgReflectionQuality = config.GetInt(section, "sg_reflection_quality", 0);
    loadedSettings.sgPostProcessQuality = config.GetInt(section, "sg_post_process_quality", 0);
    loadedSettings.sgEffectsQuality = config.GetInt(section, "sg_effects_quality", 0);

    return loadedSettings;
}

void GraphicsSection::ApplyOnStartup() {
    auto& config = ConfigManager::Get();
    const bool applyOnStart = config.GetBool(GRAPHICS_CONFIG_SECTION, "apply_on_startup", false);
    if (!applyOnStart) [[likely]]
        return;

    GameHook::QueueAction([](const RuntimeContextSnapshot& runtime) {
        if (!runtime.world) return;

        const GraphicsSettings gameStartSettings = LoadSettingsFromConfig();
        ExecuteConsoleCommands(runtime.world, gameStartSettings);
    });
}
