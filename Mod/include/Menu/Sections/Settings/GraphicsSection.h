#pragma once

#include <array>
#include <string_view>

#include "Menu/Section.h"

class GraphicsSection : public Section {
public:
    static constexpr SectionDefinition SECTION{MenuTab::Settings, "Graphics"};

private:
    struct GraphicsSettings {
        bool applyOnStartup;
        int renderScale;
        int sgShadowQuality;
        int sgGlobalIlluminationQuality;
        int sgReflectionQuality;
        int sgPostProcessQuality;
        int sgEffectsQuality;

        constexpr GraphicsSettings() noexcept
            : applyOnStartup(false),
              renderScale(100),
              sgShadowQuality(0),
              sgGlobalIlluminationQuality(0),
              sgReflectionQuality(0),
              sgPostProcessQuality(0),
              sgEffectsQuality(0) {}
    };

    struct QualityComboInfo {
        const char* label;
        int GraphicsSettings::* memberPtr;
    };

    static constexpr std::array<const char*, 5> QUALITY_LEVELS = {"Low", "Medium", "High", "Epic", "Cinematic"};

    struct ConsoleCommandInfo {
        const wchar_t* commandPrefix;
        int GraphicsSettings::* memberPtr;
    };

    static inline GraphicsSettings settings{};

    static inline const std::array<QualityComboInfo, 5> qualityCombos = {
        {{"Shadow Quality", &GraphicsSettings::sgShadowQuality},
         {"Global Illumination Quality", &GraphicsSettings::sgGlobalIlluminationQuality},
         {"Reflection Quality", &GraphicsSettings::sgReflectionQuality},
         {"Post Process Quality", &GraphicsSettings::sgPostProcessQuality},
         {"Effects Quality", &GraphicsSettings::sgEffectsQuality}}
    };

    static inline const std::array<ConsoleCommandInfo, 6> consoleCommands = {
        {{L"r.ScreenPercentage ", &GraphicsSettings::renderScale},
         {L"sg.ShadowQuality ", &GraphicsSettings::sgShadowQuality},
         {L"sg.GlobalIlluminationQuality ", &GraphicsSettings::sgGlobalIlluminationQuality},
         {L"sg.ReflectionQuality ", &GraphicsSettings::sgReflectionQuality},
         {L"sg.PostProcessQuality ", &GraphicsSettings::sgPostProcessQuality},
         {L"sg.EffectsQuality ", &GraphicsSettings::sgEffectsQuality}}
    };

    static constexpr std::string_view GRAPHICS_CONFIG_SECTION = "Graphics";

    void LoadSettings();
    void SaveSettings();
    static void ExecuteConsoleCommands(SDK::UWorld* world, const GraphicsSettings& currentSettings) noexcept;
    void ApplySettings();
    static GraphicsSettings LoadSettingsFromConfig();

public:
    explicit GraphicsSection(ModContext& ctx);
    void Render() override;
    static void ApplyOnStartup();
};
