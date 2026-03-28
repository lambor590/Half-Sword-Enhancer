#pragma once

#include <string>
#include <vector>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "Menu/SectionConfig.h"
#include "Utils/ArmorPresetSerializer.h"
#include "Utils/GameConstants.h"
#include "Utils/LivePreviewManager.h"
#include "Utils/PresetSectionState.h"

class ArmorEditorSection : public Section {
private:
    SectionConfig::ArmorEditorConfig& cfg = SectionConfig::armorEditor;

    static constexpr auto& ARMOR_SLOTS = GameConstants::ARMOR_SLOTS;
    static constexpr int ARMOR_SLOT_COUNT = GameConstants::ARMOR_SLOT_COUNT;

    std::vector<KeybindEntry> keybinds;
    SDK::FStr_Passport_Armor1 armorPassport{};
    bool armorGenerationPending = false;

    using ArmorRuntimeProps = decltype(ArmorPresetData::runtimeProps);

    ArmorRuntimeProps runtimeProps{};

    LivePreviewManager preview{cfg.preview};
    SDK::FStr_Passport_Armor1 lastPreviewedPassport{};
    ArmorRuntimeProps lastPreviewedProps{};

    struct ModuleEntry {
        SDK::UClass* cls;
        std::string name;
    };

    struct ArmorModulePool {
        std::vector<ModuleEntry> modules1, modules2, modules3;
        float cachedWidths[3] = {};
        bool populated = false;
        SDK::UClass* populatedForCore = nullptr;
    } armorModules;

    char moduleFilters[3][64] = {};

    PresetSectionState<ArmorPresetSerializer> presets;
    int activeTab = 0;

    bool IsModularCore() const;
    void PopulateModulePoolForCurrentCore();
    void CreateBlankArmorPassport();
    void QueueGeneration(SDK::EArmorSlots_Enum slot, SDK::Enum_Ranks tier, double moduleChance);
    void GenerateArmorPassport();
    void RandomizeArmorPassport();
    static void ApplyRuntimeProps(SDK::AActor* actor, const ArmorRuntimeProps& props);
    int CountActiveOverrides() const;
    void SpawnPreview();
    static bool PassportChanged(const SDK::FStr_Passport_Armor1& a, const SDK::FStr_Passport_Armor1& b);
    void SpawnFromPassport();
    void RenderGenerationControls();
    void RenderModulesTab();
    void RenderColorsTab();
    void RenderStatsTab();
    ArmorPresetData BuildPresetData() const;
    void ApplyPresetData(ArmorPresetData d);
    void RenderSpawnFooter();
    void InitKeybinds();

public:
    explicit ArmorEditorSection(ModContext& ctx);
    void Render() override;
};
