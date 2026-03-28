#pragma once

#include "Menu/CollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Utils/NPCPresetSerializer.h"
#include "Utils/PresetSectionState.h"
#include "Utils/PresetPickerState.h"
#include "Utils/LoadoutPresetSerializer.h"

#define WILLIE_PATH(s) "/Game/Character/Blueprints" s

class NPCEditorSection : public CollapsibleSection {
private:
    struct NPCTypeInfo {
        const char* displayName;
        const char* className;
    };

    static constexpr int SPECIAL_TEAM_ID = 1337;

    SectionConfig::NPCConfig& cfg = SectionConfig::npc;

    static constexpr NPCTypeInfo npcTypes[] = {
        {"Regular", WILLIE_PATH("/Willie_BP.Willie_BP_C")},
        {"No Brain", WILLIE_PATH("/Willie_BP_NoBrain.Willie_BP_NoBrain_C")},
        {"Zombie", WILLIE_PATH("/Willie_BP_Zombie.Willie_BP_Zombie_C")},
        {"DressUp", WILLIE_PATH("/Willie_BP_DressUp.Willie_BP_DressUp_C")},
        {"Torso", WILLIE_PATH("/Willie_Torso_BP.Willie_Torso_BP_C")},
        {"Falcon Boss", WILLIE_PATH("/Unique/Willie_BP_FalconBoss.Willie_BP_FalconBoss_C")},
        {"Grim Reaper", WILLIE_PATH("/Unique/Willie_BP_GrimReaper.Willie_BP_GrimReaper_C")}};
#undef WILLIE_PATH
    static constexpr int npcTypesCount = sizeof(npcTypes) / sizeof(npcTypes[0]);

    static constexpr const char* nationalityNames[] = {"English", "French", "German", "Italian",
                                                       "Spanish", "Slavic", "Nordic"};
    static constexpr int nationalityCount = 7;

    NPCOverrides overrides{};

    PresetSectionState<NPCPresetSerializer> presets;
    PresetPickerState<LoadoutPresetSerializer> loadoutPicker;
    int activeTab = 0;

    const char* getNPCClassName() const noexcept;
    int CountActiveOverrides() const;
    void SpawnNPC();
    NPCPresetData BuildPresetData() const;
    void ApplyPresetData(const NPCPresetData& d);
    void RenderPhysicalTab();
    void RenderCombatTab();
    void RenderBehaviorTab();
    void RenderBodyConditionTab();

public:
    explicit NPCEditorSection(ModContext& ctx);
    void Render() override;
};
