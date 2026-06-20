#pragma once

#include <string>
#include <vector>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "Menu/Override.h"
#include "Menu/SectionConfig.h"
#include "Utils/ArmorGenerationOptions.h"
#include "Utils/ArmorPresetSerializer.h"
#include "Utils/GameConstants.h"
#include "Utils/LivePreviewManager.h"
#include "Utils/PresetSectionState.h"

class ArmorEditorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{MenuTab::Equipment, "Armor Editor"};

    struct Config {
        int armorSlotIndex = 0;
        int armorTier = 4;
        EquipmentGenerator::ArmorGenerationOptions armorOptions;
        SpawnConfig spawn{.distanceForward = 150.0f, .distanceUp = 50.0f};
        int spawnKey = -1;
        PreviewConfig preview;
    };

private:
    Config cfg;

    static constexpr auto& ARMOR_SLOTS = GameConstants::ARMOR_SLOTS;
    static constexpr int ARMOR_SLOT_COUNT = GameConstants::ARMOR_SLOT_COUNT;

    KeybindList keybinds;
    SDK::FStr_Passport_Armor1 armorPassport{};
    bool armorGenerationPending = false;

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

    std::vector<OverrideDescriptor> protectionFields;
    std::vector<OverrideDescriptor> physicsFields;
    std::vector<OverrideDescriptor> behaviorFields;

    void BuildDescriptors();
    int CountAllActive() const;

    bool IsModularCore() const;
    void PopulateModulePoolForCurrentCore();
    void CreateBlankArmorPassport();
    void QueueGeneration(
        SDK::EArmorSlots_Enum slot, SDK::Enum_Ranks tier, EquipmentGenerator::ArmorGenerationOptions options
    );
    void GenerateArmorPassport();
    void RandomizeArmorPassport();
    void ApplyOverridesToActor(SDK::AActor* actor) const;
    void SpawnPreview();
    static bool PassportChanged(const SDK::FStr_Passport_Armor1& a, const SDK::FStr_Passport_Armor1& b);
    void RenderArmorTierCombo();
    void SpawnFromPassport();
    void RenderGenerationControls();
    void RenderModulesTab();
    void RenderColorsTab();
    void RenderStatsTab();
    ArmorPresetData BuildPresetData() const;
    void ApplyPresetData(const ArmorPresetData& d);
    void InitKeybinds();

public:
    explicit ArmorEditorSection(ModContext& ctx);
    void Render() override;
};
