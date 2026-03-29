#pragma once

#include <string>
#include <vector>

#include "Menu/Preset.h"
#include "SDK/Str_Loadout_Equipment_structs.hpp"
#include "SDK/Str_ArmorElements_structs.hpp"
#include "SDK/ArmorSlots_Enum_structs.hpp"

class CSimpleIniA;

struct LoadoutPresetData : PresetDataBase {
    static constexpr const char* kPresetsSubdir = "loadout_presets";

    struct ArmorSlotData {
        SDK::EArmorSlots_Enum slot{};
        std::string armorClass;
        SDK::FLinearColor color1{0.5f, 0.5f, 0.5f, 1.0f};
        SDK::FLinearColor color2{0.5f, 0.5f, 0.5f, 1.0f};
        SDK::FLinearColor color3{0.5f, 0.5f, 0.5f, 1.0f};
    };

    struct WeaponSlotData {
        std::string weaponClass;
        std::string gripModule;
        std::string headModule;
        std::string guardModule;
        std::string pommelModule;
        std::string subModule1;
        std::string subModule2;
        SDK::FVector headSize{1.0, 1.0, 1.0};
        SDK::FVector guardSize{1.0, 1.0, 1.0};
        SDK::FVector pommelSize{1.0, 1.0, 1.0};
        int32_t coaInt = 0;
    };

    std::vector<ArmorSlotData> armorSlots;
    WeaponSlotData weaponSlots[7];

    /// Utility to access a weapon slot from the SDK struct by index.
    static SDK::FStr_WeaponParts& GetWeaponSlot(SDK::FStr_Loadout_Weapons& weapons, int index);

    /// Read weapon slot data from an SDK weapon parts struct.
    static void ReadWeaponSlot(const SDK::FStr_WeaponParts& wp, WeaponSlotData& out);

    /// Build a LoadoutPresetData from live equipment.
    static LoadoutPresetData ReadFromEquipment(const SDK::FStr_Loadout_Equipment& equip);

    /// Custom serialization for armor/weapon slot arrays.
    static void SerializeCustom(const LoadoutPresetData& data, CSimpleIniA& ini);
    static void DeserializeCustom(LoadoutPresetData& data, const CSimpleIniA& ini);
};

using LoadoutPresetSerializer = PresetSerializer<LoadoutPresetData>;
