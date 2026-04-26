#pragma once

#include <string>
#include <vector>

#include "Menu/Preset.h"
#include "Utils/OverrideTypes.h"
#include "Utils/WeaponClassPaths.h"
#include "SDK/Str_Passport_Weapon1_structs.hpp"

#include "../../ext/SimpleIni.h"

enum class MeshType : uint8_t { Static, Skeletal };

struct MeshOverrideSettings {
    bool enabled = false;
    MeshType meshType = MeshType::Static;
    SDK::FVector scale = {1.0, 1.0, 1.0};
    SDK::FRotator rotation = {0.0, 0.0, 0.0};
    SDK::FVector offset = {0.0, 0.0, 0.0};
};

struct MeshOverridePreset : MeshOverrideSettings {
    std::string meshPath;
};

struct WeaponPresetData : PresetDataBase {
    static constexpr const char* K_PRESETS_SUBDIR = "weapon_presets";

    SDK::FStr_Passport_Weapon1 passport{};

    struct WeaponRuntimeProps {
        RuntimeOverride rigidity, edgeSharpness, rawDamage, cuttingRate, stabRate;
        RuntimeOverride defRating, gripRate, drawCutRate, tipSharpness, kickPower, matDensity;
        IntOverride dismemberSharp, dismemberBlunt;
        BoolOverride doubleEdged, piercing, noStab;
        RuntimeOverride staminaBurnR, staminaBurnL, staminaBurn2H, staminaBurn2HAlt;
        bool operator==(const WeaponRuntimeProps&) const = default;
    } runtimeProps{};

    static constexpr int MODULE_SLOT_COUNT = 4;
    MeshOverridePreset meshPresets[MODULE_SLOT_COUNT];

    WeaponClassPaths classPaths{};

    static std::vector<PresetFieldDescriptor> GetPresetFields(WeaponPresetData& data);
    static std::vector<OverrideGroupDescriptor> GetOverrideGroups(WeaponPresetData& data);
    static void SerializeCustom(const WeaponPresetData& data, CSimpleIniA& ini);
    static void DeserializeCustom(WeaponPresetData& data, const CSimpleIniA& ini);
};

using WeaponPresetSerializer = PresetSerializer<WeaponPresetData>;
