#include "Menu/Sections/Settings/GraphicsSection.h"
#include "ConfigManager.h"
#include "Core/ModContext.h"
#include "Hooks/GameHook.h"
#include "Utils/GuiUtils.h"

#include "SDK/Engine_classes.hpp"
#include "SDK/Engine_parameters.hpp"

GraphicsSection::GraphicsSection(ModContext& ctx) : Section(ctx, SECTION) {
    settings = LoadSettingsFromConfig();
}

void GraphicsSection::Render() {
    bool settingsChanged = false;

    ImGui::SeparatorText("Quick Profiles");
    ImGui::TextDisabled("Choose the overall balance between visual quality and performance.");
    ImGui::Spacing();

    const auto applyProfile = [&](const char* label, int renderScale, int quality) {
        if (!GuiUtils::Button(label)) return;
        settings.renderScale = renderScale;
        for (size_t index = 1; index < SETTING_INFO.size(); ++index)
            settings.*SETTING_INFO[index].member = quality;
        settingsChanged = true;
    };
    applyProfile("Performance", 75, 0);
    (void)GuiUtils::SameLineIfFitsButton("Balanced");
    applyProfile("Balanced", 100, 2);
    (void)GuiUtils::SameLineIfFitsButton("Maximum");
    applyProfile("Maximum", 100, 4);

    ImGui::SeparatorText("Custom Settings");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    if (GuiUtils::DebouncedDragInt(SETTING_INFO.front().label, &settings.renderScale, 1.0f, 1, 400)) {
        settingsChanged = true;
    }
    GuiUtils::HelpTooltip(
        "100% uses the game's normal resolution. Higher values look sharper but may reduce performance."
    );

    ImGui::Spacing();

    static float qualityComboW =
        GuiUtils::CalcComboWidth(QUALITY_LEVELS.data(), static_cast<int>(QUALITY_LEVELS.size()));
    for (size_t index = 1; index < SETTING_INFO.size(); ++index) {
        const auto& setting = SETTING_INFO[index];
        int currentValue = settings.*setting.member;
        GuiUtils::PrepareNextCombo(qualityComboW);
        if (ImGui::Combo(
                setting.label, &currentValue, QUALITY_LEVELS.data(), static_cast<int>(QUALITY_LEVELS.size())
            )) {
            settings.*setting.member = currentValue;
            settingsChanged = true;
        }
    }

    ImGui::SeparatorText("Startup");
    if (GuiUtils::CheckboxWithTooltip(
            "Use Every Time", &settings.applyOnStartup, "Keep this graphics setup for future game sessions."
        )) {
        settingsChanged = true;
    }

    if (settingsChanged) [[unlikely]] {
        SaveSettings();
        ApplySettings();
    }
}

void GraphicsSection::SaveSettings() {
    auto& config = ConfigManager::Get();
    config.BatchSave([&]() {
        config.SetBool(GRAPHICS_CONFIG_SECTION, "apply_on_startup", settings.applyOnStartup);
        for (const auto& setting : SETTING_INFO)
            config.SetInt(GRAPHICS_CONFIG_SECTION, setting.configKey, settings.*setting.member);
    });
}

void GraphicsSection::ExecuteConsoleCommands(SDK::UWorld* world, const GraphicsSettings& currentSettings) {
    static auto* executeCommand = [] {
        auto* systemLibraryClass = SDK::UKismetSystemLibrary::StaticClass();
        return systemLibraryClass ? systemLibraryClass->GetFunction("KismetSystemLibrary", "ExecuteConsoleCommand")
                                  : nullptr;
    }();
    static auto* systemLibrary = executeCommand ? SDK::UKismetSystemLibrary::GetDefaultObj() : nullptr;
    if (!systemLibrary) return;

    const auto flags = executeCommand->FunctionFlags;
    executeCommand->FunctionFlags |= 0x400;
    wchar_t commandBuffer[128];

    for (const auto& setting : SETTING_INFO) {
        const int value = currentSettings.*setting.member;
        std::swprintf(commandBuffer, sizeof(commandBuffer) / sizeof(wchar_t), L"%ls%d", setting.commandPrefix, value);
        SDK::Params::KismetSystemLibrary_ExecuteConsoleCommand params{};
        params.WorldContextObject = world;
        params.Command = SDK::FString(commandBuffer);
        systemLibrary->ProcessEvent(executeCommand, &params);
    }
    executeCommand->FunctionFlags = flags;
}

void GraphicsSection::ApplySettings() {
    GameHook::QueueAction([currentSettings = settings](const RuntimeContextSnapshot& runtime) {
        if (!runtime.world) return;
        ExecuteConsoleCommands(runtime.world, currentSettings);
    });
}

GraphicsSection::GraphicsSettings GraphicsSection::LoadSettingsFromConfig() {
    auto& config = ConfigManager::Get();
    GraphicsSettings loadedSettings;
    loadedSettings.applyOnStartup = config.GetBool(GRAPHICS_CONFIG_SECTION, "apply_on_startup", false);
    for (const auto& setting : SETTING_INFO) {
        loadedSettings.*setting.member =
            config.GetInt(GRAPHICS_CONFIG_SECTION, setting.configKey, setting.defaultValue);
    }

    return loadedSettings;
}

void GraphicsSection::ApplyOnStartup() {
    const GraphicsSettings startupSettings = LoadSettingsFromConfig();
    if (!startupSettings.applyOnStartup) [[likely]]
        return;

    GameHook::QueueAction([startupSettings](const RuntimeContextSnapshot& runtime) {
        if (!runtime.world) return;
        ExecuteConsoleCommands(runtime.world, startupSettings);
    });
}
