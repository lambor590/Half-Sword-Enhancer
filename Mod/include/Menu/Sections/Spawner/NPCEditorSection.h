#pragma once

#include <vector>
#include <memory>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "Menu/Override.h"
#include "Menu/SectionConfig.h"
#include "Utils/NPCPresetSerializer.h"
#include "Utils/PresetSectionState.h"
#include "Utils/PresetPickerState.h"
#include "Utils/LoadoutPresetSerializer.h"

#define WILLIE_PATH(s) "/Game/Character/Blueprints" s

class NPCEditorSection : public Section {
public:
    struct Config {
        SpawnConfig spawn{.distanceForward = 200.0f, .distanceUp = 0.0f};
        bool bodyguard = false;
        int npcTeam = 0;
        int npcTypeIndex = 0;
        int npcNationality = 0;
        int npcTier = 4;
        bool npcMercenary = false;
    };

private:
    struct SpawnBinding {
        int id = 0;
        int key = -1;
        char name[64] = "";
        SpawnConfig spawn{.distanceForward = 200.0f, .distanceUp = 0.0f};
        bool bodyguard = false;
        int team = 0;
        NPCPresetData npc;
        bool hasLoadout = false;
        std::string loadoutPath;
        std::string summary;
        KeybindEntry keybind;
    };

    struct NPCTypeInfo {
        const char* displayName;
        const char* className;
    };

    static constexpr int SPECIAL_TEAM_ID = 1337;

    Config cfg;

    static constexpr NPCTypeInfo NPC_TYPES[] = {
        {"Regular", WILLIE_PATH("/Willie_BP.Willie_BP_C")},
        {"No Brain", WILLIE_PATH("/Willie_BP_NoBrain.Willie_BP_NoBrain_C")},
        {"Zombie", WILLIE_PATH("/Willie_BP_Zombie.Willie_BP_Zombie_C")},
        {"DressUp", WILLIE_PATH("/Willie_BP_DressUp.Willie_BP_DressUp_C")},
        {"Torso", WILLIE_PATH("/Willie_Torso_BP.Willie_Torso_BP_C")},
        {"Falcon Boss", WILLIE_PATH("/Unique/Willie_BP_FalconBoss.Willie_BP_FalconBoss_C")},
        {"Grim Reaper", WILLIE_PATH("/Unique/Willie_BP_GrimReaper.Willie_BP_GrimReaper_C")}};
#undef WILLIE_PATH
    static constexpr int NPC_TYPES_COUNT = sizeof(NPC_TYPES) / sizeof(NPC_TYPES[0]);

    static constexpr const char* NATIONALITY_NAMES[] = {"English", "French", "German", "Italian",
                                                        "Spanish", "Slavic", "Nordic"};
    static constexpr int NATIONALITY_COUNT = 7;

    NPCOverrides overrides{};
    std::vector<std::shared_ptr<SpawnBinding>> spawnBindings;
    int nextBindingId = 1;
    int pendingDeleteBindingId = -1;

    PresetSectionState<NPCPresetSerializer> presets;
    PresetPickerState<LoadoutPresetSerializer> loadoutPicker;
    int activeTab = 0;

    std::vector<OverrideDescriptor> physicalFields;
    std::vector<OverrideDescriptor> combatFields;
    std::vector<OverrideDescriptor> behaviorFields;
    std::vector<OverrideDescriptor> bodyConditionFields;

    void BuildDescriptors();
    int CountAllActive() const;
    const char* GetNPCClassName(int npcTypeIndex) const noexcept;
    const char* GetNPCClassName() const noexcept;
    void SpawnNPC();
    void SpawnBindingNPC(const SpawnBinding& binding, const RuntimeContextSnapshot& runtime) const;
    NPCPresetData BuildPresetData() const;
    void ApplyPresetData(const NPCPresetData& d);
    void RenderPhysicalTab();
    void RenderCombatTab();
    void RenderBehaviorTab();
    void RenderBodyConditionTab();
    void InitBindingKeybind(const std::shared_ptr<SpawnBinding>& binding);
    void AddBindingFromCurrentSelection();
    void LoadSpawnBindings();
    void SaveSpawnBindings();
    void RenderSpawnBindings();

public:
    explicit NPCEditorSection(ModContext& ctx);
    void Render() override;
};
