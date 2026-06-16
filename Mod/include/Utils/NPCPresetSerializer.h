#pragma once

#include <string>
#include <vector>

#include "Menu/Preset.h"
#include "Utils/CharacterOverrideSets.h"

struct NPCOverrides : CharacterPhysicalOverrides, CharacterCombatOverrides, CharacterBodyConditionOverrides {
    IntOverride faceType;
    IntOverride eyeColor;
    RuntimeOverride hairLength;
    RuntimeOverride hairColor;

    RuntimeOverride aiInvincibility;
    RuntimeOverride aiArmorInvincibility;

    BoolOverride startKneeled;
    BoolOverride spawnInPants;
    BoolOverride clearSpawnArea;
    RuntimeOverride drunk;
    IntOverride boltsInQuiver;
};

struct NPCPresetData : PresetDataBase {
    static constexpr const char* K_PRESETS_SUBDIR = "npc_presets";

    int npcTypeIndex = 0;
    int nationality = 0;
    int tier = 4;
    bool mercenary = false;

    NPCOverrides overrides{};

    static std::vector<PresetFieldDescriptor> GetPresetFields(NPCPresetData& data);
    static std::vector<OverrideGroupDescriptor> GetOverrideGroups(NPCPresetData& data);
};

using NPCPresetSerializer = PresetSerializer<NPCPresetData>;
