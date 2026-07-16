#pragma once

#include <array>
#include <string>
#include <string_view>

#include "Menu/Preset.h"
#include "Utils/OverrideTypes.h"
#include "SDK/Str_Passport_Armor1_structs.hpp"
#include "SDK/BP_Armor_Master_classes.hpp"

struct ArmorRuntimeProps {
    RuntimeOverride protectionBlunt, protectionCut, protectionStab;
    RuntimeOverride materialDensity, massScale;
    RuntimeOverride handsRigidity, strapPower, aiInvincibilityRate, price;
    BoolOverride pickUp;
    bool operator==(const ArmorRuntimeProps&) const = default;
};

struct ArmorPresetData : PresetDataBase {
    static constexpr const char* K_PRESETS_SUBDIR = "armor_presets";
    static constexpr const char* K_PRESET_KIND = "armor";

    SDK::FStr_Passport_Armor1 passport{};
    ArmorRuntimeProps runtimeProps{};

    std::string armorCorePath;

    static std::array<PresetFieldDescriptor, 1> GetPresetFields(ArmorPresetData& data);
    static std::array<PresetOverrideDescriptor, 10> GetPresetOverrides(ArmorPresetData& data);
    [[nodiscard]] PresetOperationResult ValidateForSave() const;
    static void SerializeCustom(const ArmorPresetData& data, CSimpleIniA& ini, std::string_view sectionPrefix = {});
    [[nodiscard]] static PresetOperationResult DeserializeCustom(
        ArmorPresetData& data, const CSimpleIniA& ini, std::string_view sectionPrefix = {}
    );
};

using ArmorPresetSerializer = PresetSerializer<ArmorPresetData>;
