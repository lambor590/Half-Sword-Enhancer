#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "Menu/Preset.h"
#include "Utils/ArmorPresetSerializer.h"
#include "Utils/WeaponPresetSerializer.h"

enum class ItemSpawnPresetSource : uint8_t {
    ClassPath,
    CustomizableWeapon,
    RandomArmor,
    ModularArmor,
    WeaponPreset,
    ArmorPreset,
};

struct ItemSpawnPresetData : PresetDataBase {
    static constexpr const char* K_PRESETS_SUBDIR = "item_spawn_presets";
    static constexpr const char* K_PRESET_KIND = "item_spawn";

    struct SpawnOptions {
        double distanceForward = 150.0;
        double distanceUp = 50.0;
        double scale = 1.0;
        bool snapToGround = true;
    } spawn{};

    struct ArmorGenerationOptions {
        double moduleChance = 0.5;
        bool forceMetalMaterial = false;
        int steelType = 0;
        int metalPiecesType = 0;
    } armorGeneration{};

    ItemSpawnPresetSource source = ItemSpawnPresetSource::ClassPath;
    std::string classPath;
    int tier = 4;
    int weaponSpecificType = 0;
    int customizableWeapon = 0;
    int armorSlotIndex = 0;
    std::array<int, 3> modularArmorModules{};

    PresetLink<WeaponPresetData> weaponPreset;
    PresetLink<ArmorPresetData> armorPreset;

    [[nodiscard]] PresetOperationResult ValidateForSave() const;
    [[nodiscard]] PresetOperationResult ValidateForSave(const std::filesystem::path& appDataRoot) const;
    static std::array<PresetFieldDescriptor, 16> GetPresetFields(ItemSpawnPresetData& data);
    static void SerializeCustom(const ItemSpawnPresetData& data, CSimpleIniA& ini, std::string_view sectionPrefix = {});
    [[nodiscard]] static PresetOperationResult DeserializeCustom(
        ItemSpawnPresetData& data, const CSimpleIniA& ini, std::string_view sectionPrefix = {}
    );
};

using ItemSpawnPresetSerializer = PresetSerializer<ItemSpawnPresetData>;
