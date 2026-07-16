#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "Menu/Preset.h"
#include "Utils/ArmorPresetSerializer.h"
#include "Utils/WeaponPresetSerializer.h"
#include "SDK/Str_Loadout_Weapons_structs.hpp"

#include "../../ext/SimpleIni.h"

struct LoadoutPresetData : PresetDataBase {
    static constexpr const char* K_PRESETS_SUBDIR = "loadout_presets";
    static constexpr const char* K_PRESET_KIND = "loadout";
    static constexpr std::size_t K_WEAPON_SLOT_COUNT = 7;
    static constexpr std::size_t K_ARMOR_SLOT_COUNT = 17;
    inline static constexpr std::array<std::string_view, K_WEAPON_SLOT_COUNT> K_WEAPON_SLOT_KEYS = {
        "HandR", "HandL", "SlotR1", "SlotR2", "SlotL1", "SlotL2", "Back",
    };
    inline static constexpr std::array<std::string_view, K_WEAPON_SLOT_COUNT> K_WEAPON_SLOT_LABELS = {
        "Right Hand", "Left Hand", "Right Slot 1", "Right Slot 2", "Left Slot 1", "Left Slot 2", "Back",
    };
    inline static constexpr std::array<std::string_view, K_ARMOR_SLOT_COUNT> K_ARMOR_SLOT_KEYS = {
        "Head",          "Hands",     "Reserved2",    "Reserved3",   "Neck_Bevor", "Neck_Standard",
        "Arms",          "Shoulders", "Tabard",       "Chest_Plate", "Hauberk",    "Cuisses",
        "Body_Clothing", "Waist",     "Legs_Greaves", "Feet",        "Hosen",
    };
    inline static constexpr std::array<std::string_view, K_ARMOR_SLOT_COUNT> K_ARMOR_SLOT_LABELS = {
        "Head",          "Hands",     "Armor Slot 3", "Armor Slot 4", "Neck (Bevor)", "Neck (Standard)",
        "Arms",          "Shoulders", "Tabard",       "Chest Plate",  "Hauberk",      "Cuisses",
        "Body Clothing", "Waist",     "Leg Greaves",  "Feet",         "Hosen",
    };

    std::array<PresetLink<WeaponPresetData>, K_WEAPON_SLOT_COUNT> weaponSlots;
    std::array<PresetLink<ArmorPresetData>, K_ARMOR_SLOT_COUNT> armorSlots;

    static SDK::FStr_WeaponParts& GetWeaponSlot(SDK::FStr_Loadout_Weapons& weapons, int index);
    static const SDK::FStr_WeaponParts& GetWeaponSlot(const SDK::FStr_Loadout_Weapons& weapons, int index);
    [[nodiscard]] PresetOperationResult ValidateForSave(const std::filesystem::path& appDataRoot) const;
    static void SerializeCustom(const LoadoutPresetData& data, CSimpleIniA& ini, std::string_view sectionPrefix = {});
    [[nodiscard]] static PresetOperationResult DeserializeCustom(
        LoadoutPresetData& data, const CSimpleIniA& ini, std::string_view sectionPrefix = {}
    );
};

using LoadoutPresetSerializer = PresetSerializer<LoadoutPresetData>;
