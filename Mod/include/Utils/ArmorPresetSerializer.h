#pragma once

#include <string>
#include <vector>

#include "Menu/Preset.h"
#include "Utils/OverrideTypes.h"
#include "SDK/Str_Passport_Armor1_structs.hpp"
#include "SDK/BP_Armor_Master_classes.hpp"

struct ArmorPresetData : PresetDataBase {
    static constexpr const char* kPresetsSubdir = "armor_presets";

    SDK::FStr_Passport_Armor1 passport{};

    struct {
        RuntimeOverride protectionBlunt, protectionCut, protectionStab;
        RuntimeOverride materialDensity, massScale;
        RuntimeOverride handsRigidity, strapPower, aiInvincibilityRate, price;
        BoolOverride pickUp;
    } runtimeProps{};

    std::string armorCorePath;

    static std::vector<PresetFieldDescriptor> GetPresetFields(ArmorPresetData& data);
    static std::vector<OverrideGroupDescriptor> GetOverrideGroups(ArmorPresetData& data);
    static void SerializeCustom(const ArmorPresetData& data, CSimpleIniA& ini);
    static void DeserializeCustom(ArmorPresetData& data, const CSimpleIniA& ini);
};

using ArmorPresetSerializer = PresetSerializer<ArmorPresetData>;
