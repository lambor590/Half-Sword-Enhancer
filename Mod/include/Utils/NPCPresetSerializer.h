#pragma once

#include <string>
#include <vector>

#include "Menu/Preset.h"
#include "Utils/OverrideTypes.h"

struct NPCOverrides {
    RuntimeOverride heightRate;
    RuntimeOverride muscleRate;
    RuntimeOverride scaleMutationInhibitor;

    IntOverride faceType;
    IntOverride eyeColor;
    RuntimeOverride hairLength;
    RuntimeOverride hairColor;

    RuntimeOverride damageRate;
    RuntimeOverride limbDamageRate;
    RuntimeOverride dismemberThreshold;
    RuntimeOverride regenRate;
    RuntimeOverride aiInvincibility;
    RuntimeOverride aiArmorInvincibility;
    RuntimeOverride bodySkill;

    BoolOverride fearless;
    BoolOverride startKneeled;
    BoolOverride spawnInPants;
    BoolOverride clearSpawnArea;
    RuntimeOverride drunk;
    IntOverride boltsInQuiver;

    RuntimeOverride headHealth;
    RuntimeOverride neckHealth;
    RuntimeOverride armRHealth;
    RuntimeOverride armLHealth;
    RuntimeOverride bodyUpperHealth;
    RuntimeOverride bodyLowerHealth;
    RuntimeOverride legRHealth;
    RuntimeOverride legLHealth;
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
