#pragma once

#include <array>

#include "Menu/Section.h"

class GraphicsSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::Settings, "Graphics", "Change image quality and game performance."
    };

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

    struct SettingInfo {
        const char* label;
        const char* configKey;
        const wchar_t* commandPrefix;
        int GraphicsSettings::* member;
        int defaultValue;
    };

    GraphicsSettings settings;
    static constexpr std::array<const char*, 5> QUALITY_LEVELS = {"Low", "Medium", "High", "Epic", "Cinematic"};
    static constexpr std::array<SettingInfo, 6> SETTING_INFO = {{
        {"Resolution Scale (%)", "render_scale", L"r.ScreenPercentage ", &GraphicsSettings::renderScale, 100},
        {"Shadow Quality", "sg_shadow_quality", L"sg.ShadowQuality ", &GraphicsSettings::sgShadowQuality, 0},
        {"Lighting Quality", "sg_global_illumination_quality", L"sg.GlobalIlluminationQuality ",
         &GraphicsSettings::sgGlobalIlluminationQuality, 0},
        {"Reflection Quality", "sg_reflection_quality", L"sg.ReflectionQuality ",
         &GraphicsSettings::sgReflectionQuality, 0},
        {"Image Effects Quality", "sg_post_process_quality", L"sg.PostProcessQuality ",
         &GraphicsSettings::sgPostProcessQuality, 0},
        {"Effects Quality", "sg_effects_quality", L"sg.EffectsQuality ", &GraphicsSettings::sgEffectsQuality, 0},
    }};
    static constexpr const char* GRAPHICS_CONFIG_SECTION = "Graphics";

    void SaveSettings();
    static void ExecuteConsoleCommands(SDK::UWorld* world, const GraphicsSettings& currentSettings);
    void ApplySettings();
    static GraphicsSettings LoadSettingsFromConfig();

public:
    explicit GraphicsSection(ModContext& ctx);
    void Render() override;
    static void ApplyOnStartup();
};
