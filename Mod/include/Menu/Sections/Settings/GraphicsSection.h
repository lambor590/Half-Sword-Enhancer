#pragma once

#include <array>
#include <cstdio>
#include <string_view>

#include "Menu/ICollapsibleSection.h"
#include "Utils/ConfigUtils.h"
#include "Utils/GuiUtils.h"
#include "ComponentValidator.h"
#include "SDK/Engine_classes.hpp"
#include "Hooks/GameHook.h"

class GraphicsSection : public CollapsibleSection {
private:
    struct GraphicsSettings {
        bool applyOnStartup = false;
        int renderScale = 100;
        int sgShadowQuality = 0;
        int sgGlobalIlluminationQuality = 0;
        int sgReflectionQuality = 0;
        int sgPostProcessQuality = 0;
        int sgEffectsQuality = 0;
    };

    struct QualityComboInfo {
        const char* label;
        int GraphicsSettings::*memberPtr;
    };

    static constexpr std::array<const char*, 5> qualityLevels = {
        "Low", "Medium", "High", "Epic", "Cinematic"
    };

    struct ConsoleCommandInfo {
        const wchar_t* commandPrefix;
        int GraphicsSettings::*memberPtr;
    };

    static inline GraphicsSettings settings;

    static inline const std::array<QualityComboInfo, 5> qualityCombos = {{
        {"Shadow Quality", &GraphicsSettings::sgShadowQuality},
        {"Global Illumination Quality", &GraphicsSettings::sgGlobalIlluminationQuality},
        {"Reflection Quality", &GraphicsSettings::sgReflectionQuality},
        {"Post Process Quality", &GraphicsSettings::sgPostProcessQuality},
        {"Effects Quality", &GraphicsSettings::sgEffectsQuality}
    }};

    static inline const std::array<ConsoleCommandInfo, 6> consoleCommands = {{
        {L"r.ScreenPercentage ", &GraphicsSettings::renderScale},
        {L"sg.ShadowQuality ", &GraphicsSettings::sgShadowQuality},
        {L"sg.GlobalIlluminationQuality ", &GraphicsSettings::sgGlobalIlluminationQuality},
        {L"sg.ReflectionQuality ", &GraphicsSettings::sgReflectionQuality},
        {L"sg.PostProcessQuality ", &GraphicsSettings::sgPostProcessQuality},
        {L"sg.EffectsQuality ", &GraphicsSettings::sgEffectsQuality}
    }};

    static constexpr std::string_view graphicsConfigSection = "Graphics";

public:
    GraphicsSection() : CollapsibleSection("Graphics") {
        LoadSettings();
    }

    void RenderContent() override {
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

        if (ImGui::SliderInt("Render Scale (%)", &settings.renderScale, 1, 200)) {
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

private:
    void LoadSettings() {
        settings = LoadSettingsFromConfig();
    }

    void SaveSettings() {
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

    static void ExecuteConsoleCommands(SDK::UWorld* world, const GraphicsSettings& currentSettings) noexcept {
        wchar_t commandBuffer[128];
        
        for (const auto& cmd : consoleCommands) {
            const int value = currentSettings.*cmd.memberPtr;
            std::swprintf(commandBuffer, sizeof(commandBuffer) / sizeof(wchar_t), 
                         L"%ls%d", cmd.commandPrefix, value);
            SDK::UKismetSystemLibrary::ExecuteConsoleCommand(world, SDK::FString(commandBuffer), nullptr);
        }
    }

    void ApplySettings() {
        GameHook::QueueAction([currentSettings = settings]() {
            SDK::UWorld* world;
            if (!ComponentValidator::Validate(world)) return;
            ExecuteConsoleCommands(world, currentSettings);
        });
    }
    
    static GraphicsSettings LoadSettingsFromConfig() {
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

public:
    static void ApplyOnStartup() {
        auto& config = ConfigManager::Get();
        const bool applyOnStart = config.GetBool(graphicsConfigSection.data(), "apply_on_startup", false);
        if (!applyOnStart) [[likely]] return;
        
        GameHook::QueueAction([]() {
            SDK::UWorld* world;
            if (!ComponentValidator::Validate(world)) return;
            
            const GraphicsSettings gameStartSettings = LoadSettingsFromConfig();
            ExecuteConsoleCommands(world, gameStartSettings);
        });
    }
    
};