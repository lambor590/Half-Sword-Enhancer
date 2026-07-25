#pragma once

#include <array>
#include <string>
#include <string_view>

#include "Menu/Preset.h"
#include "Utils/CharacterOverrideSets.h"
#include "Utils/LoadoutPresetSerializer.h"

struct NPCOverrides : CharacterPhysicalOverrides, CharacterCombatOverrides, CharacterBodyConditionOverrides {
    IntOverride faceType;
    IntOverride eyeColor;
    RuntimeOverride hairLength;
    RuntimeOverride hairColor;

    RuntimeOverride aiInvincibility;
    RuntimeOverride aiArmorInvincibility;

    BoolOverride startKneeled;
    BoolOverride spawnInPants;
    BoolOverride blossfechtenGear;
    BoolOverride clearSpawnArea;
    RuntimeOverride drunk;
    IntOverride boltsInQuiver;
};

struct NPCPresetData : PresetDataBase {
    struct TypeInfo {
        const char* displayName;
        const char* classPath;
    };

    static constexpr const char* K_PRESETS_SUBDIR = "npc_presets";
    static constexpr const char* K_PRESET_KIND = "npc";
    inline static constexpr std::array<TypeInfo, 6> K_TYPES{{
        {"Regular", "/Game/Character/Blueprints/Willie_BP.Willie_BP_C"},
        {"Inert", "/Game/Character/Blueprints/Willie_BP_NoBrain.Willie_BP_NoBrain_C"},
        {"Zombie", "/Game/Character/Blueprints/Willie_BP_Zombie.Willie_BP_Zombie_C"},
        {"Mannequin", "/Game/Character/Blueprints/Willie_BP_DressUp.Willie_BP_DressUp_C"},
        {"Falcon Boss", "/Game/Character/Blueprints/Unique/Willie_BP_FalconBoss.Willie_BP_FalconBoss_C"},
        {"Grim Reaper", "/Game/Character/Blueprints/Unique/Willie_BP_GrimReaper.Willie_BP_GrimReaper_C"},
    }};
    inline static constexpr std::array<const char*, 7> K_NATIONALITY_NAMES{
        "English", "French", "German", "Italian", "Spanish", "Slavic", "Nordic",
    };
    static constexpr int K_MIN_TIER = 0;
    static constexpr int K_MAX_TIER = 8;
    static constexpr int K_MIN_TEAM = 0;
    static constexpr int K_MAX_TEAM = 9;
    static constexpr int K_SPECIAL_TEAM = 1337;
    static constexpr double K_MIN_SPAWN_DISTANCE_FORWARD = 100.0;
    static constexpr double K_MAX_SPAWN_DISTANCE_FORWARD = 500.0;
    static constexpr double K_MIN_SPAWN_DISTANCE_UP = 0.0;
    static constexpr double K_MAX_SPAWN_DISTANCE_UP = 300.0;
    static constexpr double K_MIN_SPAWN_SCALE = 0.1;
    static constexpr double K_MAX_SPAWN_SCALE = 4.0;

    int npcTypeIndex = 0;
    int nationality = 0;
    int tier = 4;
    bool mercenary = false;
    bool bodyguard = false;
    int team = 0;
    double spawnDistanceForward = 200.0;
    double spawnDistanceUp = 0.0;
    double spawnScale = 1.0;
    bool snapToGround = true;

    NPCOverrides overrides{};
    PresetLink<LoadoutPresetData> loadout;

    [[nodiscard]] PresetOperationResult ValidateForSave() const;
    [[nodiscard]] PresetOperationResult ValidateForSave(const std::filesystem::path& appDataRoot) const;
    static std::array<PresetFieldDescriptor, 10> GetPresetFields(NPCPresetData& data);
    static std::array<PresetOverrideDescriptor, 28> GetPresetOverrides(NPCPresetData& data);
    static void SerializeCustom(const NPCPresetData& data, CSimpleIniA& ini, std::string_view sectionPrefix = {});
    [[nodiscard]] static PresetOperationResult DeserializeCustom(
        NPCPresetData& data, const CSimpleIniA& ini, std::string_view sectionPrefix = {}
    );
};

using NPCPresetSerializer = PresetSerializer<NPCPresetData>;
