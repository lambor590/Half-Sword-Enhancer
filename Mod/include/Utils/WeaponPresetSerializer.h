#pragma once

#include <array>
#include <string>
#include <string_view>

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
    static constexpr const char* K_PRESET_KIND = "weapon";

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
    std::string gripMeshPath;
    int coaInt = 0;

    // FName cannot be reconstructed safely from the render/filesystem thread.
    // The serializer keeps its raw string here for a later game-thread materialization.
    std::string deferredWeaponName;

    static std::array<PresetFieldDescriptor, 18> GetPresetFields(WeaponPresetData& data);
    static std::array<PresetOverrideDescriptor, 20> GetPresetOverrides(WeaponPresetData& data);
    [[nodiscard]] PresetOperationResult ValidateForSave() const;
    static void SerializeCustom(const WeaponPresetData& data, CSimpleIniA& ini, std::string_view sectionPrefix = {});
    [[nodiscard]] static PresetOperationResult DeserializeCustom(
        WeaponPresetData& data, const CSimpleIniA& ini, std::string_view sectionPrefix = {}
    );
};

using WeaponPresetSerializer = PresetSerializer<WeaponPresetData>;
