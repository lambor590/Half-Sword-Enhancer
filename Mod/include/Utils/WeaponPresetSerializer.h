#pragma once

#include <string>
#include <vector>

#include "Menu/Preset.h"
#include "Utils/OverrideTypes.h"
#include "SDK/Str_Passport_Weapon1_structs.hpp"

class CSimpleIniA;

enum class MeshType : uint8_t { Static, Skeletal };

struct MeshOverridePreset {
    bool enabled = false;
    std::string meshPath;
    MeshType meshType = MeshType::Static;
    SDK::FVector scale = {1.0, 1.0, 1.0};
    SDK::FRotator rotation = {0.0, 0.0, 0.0};
    SDK::FVector offset = {0.0, 0.0, 0.0};
};

struct WeaponPresetData : PresetDataBase {
    static constexpr const char* kPresetsSubdir = "weapon_presets";

    SDK::FStr_Passport_Weapon1 passport{};

    struct {
        RuntimeOverride rigidity, edgeSharpness, rawDamage, cuttingRate, stabRate;
        RuntimeOverride defRating, gripRate, drawCutRate, tipSharpness, kickPower, matDensity;
        IntOverride dismemberSharp, dismemberBlunt;
        BoolOverride doubleEdged, piercing, noStab;
        RuntimeOverride staminaBurnR, staminaBurnL, staminaBurn2H, staminaBurn2HAlt;
    } runtimeProps{};

    static constexpr int MODULE_SLOT_COUNT = 4;
    MeshOverridePreset meshPresets[MODULE_SLOT_COUNT];

    struct {
        std::string weaponClass;
        std::string headModule, guardModule, gripModule, pommelModule;
        std::string subModule1, subModule2;
    } classPaths{};

    static std::vector<PresetFieldDescriptor> GetPresetFields(WeaponPresetData& data);
    static std::vector<OverrideGroupDescriptor> GetOverrideGroups(WeaponPresetData& data);
    static void SerializeCustom(const WeaponPresetData& data, CSimpleIniA& ini);
    static void DeserializeCustom(WeaponPresetData& data, const CSimpleIniA& ini);
};

using WeaponPresetSerializer = PresetSerializer<WeaponPresetData>;
