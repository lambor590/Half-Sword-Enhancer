#include "Menu/Sections/Settings/GraphicsSection.h"
#include "Menu/SectionRegistry.h"
#include "Menu/SectionStyle.h"
#include "ConfigManager.h"

REGISTER_SECTION(GraphicsSection, MenuTab::Settings);
#include "ComponentValidator.h"
#include "Utils/ConfigUtils.h"
#include "Utils/GuiUtils.h"
#include "Hooks/GameHook.h"
#include "SDK/Engine_classes.hpp"

GraphicsSection::GraphicsSection(ModContext& ctx) : Section(ctx, "Graphics") {
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

    ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
    if (ImGui::DragInt("Render Scale (%)", &settings.renderScale, 1.0f, 1, 400)) {
        settingsChanged = true;
    }

    ImGui::Spacing();

    static float qualityComboW = GuiUtils::CalcComboWidth(qualityLevels.data(), static_cast<int>(qualityLevels.size()));
    for (const auto& combo : qualityCombos) {
        int currentValue = settings.*combo.memberPtr;
        ImGui::SetNextItemWidth(qualityComboW);
        if (ImGui::Combo(combo.label, &currentValue, qualityLevels.data(), static_cast<int>(qualityLevels.size()))) {
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
    ConfigUtils::BatchUpdate([&](ConfigUtils::ConfigTransaction& config) {
        const char* section = graphicsConfigSection.data();

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
    GameHook::QueueAction([currentSettings = settings]() {
        SDK::UWorld* world;
        if (!ComponentValidator::Validate(world)) return;
        ExecuteConsoleCommands(world, currentSettings);
    });
}

GraphicsSection::GraphicsSettings GraphicsSection::LoadSettingsFromConfig() {
    auto& config = ConfigManager::Get();
    const char* section = graphicsConfigSection.data();

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
    const bool applyOnStart = config.GetBool(graphicsConfigSection.data(), "apply_on_startup", false);
    if (!applyOnStart) [[likely]]
        return;

    GameHook::QueueAction([]() {
        SDK::UWorld* world;
        if (!ComponentValidator::Validate(world)) return;

        const GraphicsSettings gameStartSettings = LoadSettingsFromConfig();
        ExecuteConsoleCommands(world, gameStartSettings);
    });
}
