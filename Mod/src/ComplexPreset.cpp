#include "Utils/ItemSpawnPresetSerializer.h"
#include "Utils/MapScenarioPresetSerializer.h"

#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#include "Utils/CustomizableWeapon.h"
#include "Utils/GameConstants.h"
#include "Utils/PresetLinkResolution.h"
#include "SDK/Enum_WeaponType_Specific_structs.hpp"

namespace {
    template <typename Value>
    PresetOperationResult ValidateRange(Value value, Value minimum, Value maximum, std::string_view qualifiedField) {
        if (value >= minimum && value <= maximum) return {.success = true};
        return {
            .error = std::string(qualifiedField) + " must be between " + std::to_string(minimum) + " and " +
                     std::to_string(maximum)
        };
    }

    PresetOperationResult ValidateFiniteRange(
        double value, double minimum, double maximum, std::string_view qualifiedField
    ) {
        if (std::isfinite(value) && value >= minimum && value <= maximum) return {.success = true};
        return {
            .error = std::string(qualifiedField) + " must be between " + std::to_string(minimum) + " and " +
                     std::to_string(maximum)
        };
    }

    PresetOperationResult ValidateSerializedText(const std::string& value, std::string_view qualifiedField) {
        if (PresetUtils::IsSafeIniValue(value)) return {.success = true};
        return {.error = std::string(qualifiedField) + " contains text that cannot be used"};
    }

    template <typename DataType>
    PresetOperationResult DeserializeLink(
        PresetLink<DataType>& link, const CSimpleIniA& ini, std::string_view section, std::string_view label
    ) {
        auto result = DeserializePresetLink<DataType>(ini, section);
        if (!result.success) return {.error = std::string(label) + ": " + result.error};
        link = std::move(result.value);
        return {.success = true};
    }
}

PresetOperationResult MapScenarioPresetData::ValidateForSave() const {
    if (packageName.empty()) return {.error = "Choose a map"};
    if (auto validation = ValidateSerializedText(packageName, "Map"); !validation) return validation;
    if (auto validation = ValidateRange(preLoad.foesAmount, 0, 7, "Enemy count"); !validation) return validation;
    if (auto validation = ValidateRange(preLoad.foeTier, 0, 8, "Enemy tier"); !validation) return validation;
    if (auto validation = ValidateRange(preLoad.combatantsAmount, 0, 7, "Ally count"); !validation) return validation;
    if (auto validation = ValidateRange(preLoad.opponentTier, 0, 8, "Ally tier"); !validation) return validation;
    if (auto validation = ValidateRange(autoSpawn.npcCount, 0, 10, "Starting NPC count"); !validation)
        return validation;
    if (autoSpawn.enabled && autoSpawn.npcCount > 0 && IsEmptyPresetLink(autoSpawn.npcPreset))
        return {.error = "Choose an NPC preset when starting with NPCs"};
    return {.success = true};
}

PresetOperationResult MapScenarioPresetData::ValidateForSave(const std::filesystem::path& appDataRoot) const {
    return PresetLinkResolution::ValidateForSave<MapScenarioPresetSerializer>(*this, appDataRoot);
}

std::array<PresetFieldDescriptor, 11> MapScenarioPresetData::GetPresetFields(MapScenarioPresetData& data) {
    return {
        PresetField::String("Map", "packageName", &data.packageName),
        PresetField::Bool("PreLoad", "freshStart", &data.preLoad.freshStart),
        PresetField::Bool("PreLoad", "tutorial", &data.preLoad.tutorial),
        PresetField::Bool("PreLoad", "freeMode", &data.preLoad.freeMode),
        PresetField::Bool("PreLoad", "carnage", &data.preLoad.carnage),
        PresetField::Int("PreLoad", "foesAmount", &data.preLoad.foesAmount),
        PresetField::Int("PreLoad", "foeTier", &data.preLoad.foeTier),
        PresetField::Int("PreLoad", "combatantsAmount", &data.preLoad.combatantsAmount),
        PresetField::Int("PreLoad", "opponentTier", &data.preLoad.opponentTier),
        PresetField::Bool("AutoSpawn", "enabled", &data.autoSpawn.enabled),
        PresetField::Int("AutoSpawn", "npcCount", &data.autoSpawn.npcCount),
    };
}

void MapScenarioPresetData::SerializeCustom(
    const MapScenarioPresetData& data, CSimpleIniA& ini, std::string_view sectionPrefix
) {
    SerializePresetLink(data.autoSpawn.playerPreset, ini, PresetSectionName(sectionPrefix, "Links.Player"));
    SerializePresetLink(data.autoSpawn.loadoutPreset, ini, PresetSectionName(sectionPrefix, "Links.Loadout"));
    SerializePresetLink(data.autoSpawn.npcPreset, ini, PresetSectionName(sectionPrefix, "Links.NPC"));
}

PresetOperationResult MapScenarioPresetData::DeserializeCustom(
    MapScenarioPresetData& data, const CSimpleIniA& ini, std::string_view sectionPrefix
) {
    if (auto result = DeserializeLink(
            data.autoSpawn.playerPreset, ini, PresetSectionName(sectionPrefix, "Links.Player"), "Starting player"
        );
        !result)
        return result;
    if (auto result = DeserializeLink(
            data.autoSpawn.loadoutPreset, ini, PresetSectionName(sectionPrefix, "Links.Loadout"), "Starting equipment"
        );
        !result)
        return result;
    if (auto result = DeserializeLink(
            data.autoSpawn.npcPreset, ini, PresetSectionName(sectionPrefix, "Links.NPC"), "Starting NPCs"
        );
        !result)
        return result;
    return {.success = true};
}

PresetOperationResult ItemSpawnPresetData::ValidateForSave() const {
    const int sourceValue = static_cast<int>(source);
    constexpr int LAST_SOURCE = static_cast<int>(ItemSpawnPresetSource::ArmorPreset);
    if (sourceValue > LAST_SOURCE) return {.error = "Choose a valid item type"};

    if (auto validation = ValidateFiniteRange(spawn.distanceForward, 50.0, 300.0, "Forward distance"); !validation)
        return validation;
    if (auto validation = ValidateFiniteRange(spawn.distanceUp, 0.0, 200.0, "Height"); !validation) return validation;
    if (auto validation = ValidateFiniteRange(spawn.scale, 0.1, 5.0, "Size"); !validation) return validation;

    const bool hasWeaponLink = !IsEmptyPresetLink(weaponPreset);
    const bool hasArmorLink = !IsEmptyPresetLink(armorPreset);
    const auto validateTier = [this] {
        return ValidateRange(tier, 0, 8, "Tier");
    };
    const auto validateClassPath = [this] {
        if (classPath.empty()) return PresetOperationResult{.error = "Choose an item"};
        return ValidateSerializedText(classPath, "Selected item");
    };

    switch (source) {
        case ItemSpawnPresetSource::ClassPath: {
            if (hasWeaponLink || hasArmorLink) return {.error = "Remove the saved weapon or armor selection"};
            if (auto validation = validateClassPath(); !validation) return validation;
            if (auto validation = validateTier(); !validation) return validation;
            return ValidateRange(
                weaponSpecificType, 0, static_cast<int>(SDK::Enum_WeaponType_Specific::Enum_WeaponType_MAX) - 1,
                "Weapon type"
            );
        }
        case ItemSpawnPresetSource::CustomizableWeapon: {
            if (hasWeaponLink || hasArmorLink) return {.error = "Remove the saved weapon or armor selection"};
            if (auto validation = ValidateRange(
                    customizableWeapon, static_cast<int>(CustomizableWeapon::None) + 1,
                    static_cast<int>(CustomizableWeapon::Messer), "Weapon"
                );
                !validation)
                return validation;
            return validateTier();
        }
        case ItemSpawnPresetSource::RandomArmor: {
            if (hasWeaponLink || hasArmorLink) return {.error = "Remove the saved weapon or armor selection"};
            if (auto validation = validateTier(); !validation) return validation;
            if (auto validation = ValidateRange(armorSlotIndex, 0, GameConstants::ARMOR_SLOT_COUNT - 1, "Armor slot");
                !validation)
                return validation;
            if (auto validation = ValidateFiniteRange(armorGeneration.moduleChance, 0.0, 1.0, "Armor coverage");
                !validation)
                return validation;
            if (auto validation = ValidateRange(armorGeneration.steelType, 0, 11, "Main metal"); !validation)
                return validation;
            return ValidateRange(armorGeneration.metalPiecesType, 0, 7, "Accent metal");
        }
        case ItemSpawnPresetSource::ModularArmor:
            if (hasWeaponLink || hasArmorLink) return {.error = "Remove the saved weapon or armor selection"};
            if (auto validation = validateClassPath(); !validation) return validation;
            for (std::size_t index = 0; index < modularArmorModules.size(); ++index) {
                if (modularArmorModules[index] < 0)
                    return {.error = "Armor part " + std::to_string(index + 1) + " is unavailable"};
            }
            return {.success = true};
        case ItemSpawnPresetSource::WeaponPreset:
            if (!hasWeaponLink) return {.error = "Choose a weapon preset"};
            if (hasArmorLink) return {.error = "Remove the armor preset"};
            return {.success = true};
        case ItemSpawnPresetSource::ArmorPreset:
            if (!hasArmorLink) return {.error = "Choose an armor preset"};
            if (hasWeaponLink) return {.error = "Remove the weapon preset"};
            return {.success = true};
    }
    return {.error = "Choose a valid item type"};
}

PresetOperationResult ItemSpawnPresetData::ValidateForSave(const std::filesystem::path& appDataRoot) const {
    return PresetLinkResolution::ValidateForSave<ItemSpawnPresetSerializer>(*this, appDataRoot);
}

std::array<PresetFieldDescriptor, 16> ItemSpawnPresetData::GetPresetFields(ItemSpawnPresetData& data) {
    return {
        PresetField::String("Item", "classPath", &data.classPath),
        PresetField::Int("Item", "tier", &data.tier),
        PresetField::Int("Item", "weaponSpecificType", &data.weaponSpecificType),
        PresetField::Int("Item", "customizableWeapon", &data.customizableWeapon),
        PresetField::Int("Item", "armorSlotIndex", &data.armorSlotIndex),
        PresetField::Int("ModularArmor", "module1", &data.modularArmorModules[0]),
        PresetField::Int("ModularArmor", "module2", &data.modularArmorModules[1]),
        PresetField::Int("ModularArmor", "module3", &data.modularArmorModules[2]),
        PresetField::Double("Spawn", "distanceForward", &data.spawn.distanceForward),
        PresetField::Double("Spawn", "distanceUp", &data.spawn.distanceUp),
        PresetField::Double("Spawn", "scale", &data.spawn.scale),
        PresetField::Bool("Spawn", "snapToGround", &data.spawn.snapToGround),
        PresetField::Double("ArmorGeneration", "moduleChance", &data.armorGeneration.moduleChance),
        PresetField::Bool("ArmorGeneration", "forceMetalMaterial", &data.armorGeneration.forceMetalMaterial),
        PresetField::Int("ArmorGeneration", "steelType", &data.armorGeneration.steelType),
        PresetField::Int("ArmorGeneration", "metalPiecesType", &data.armorGeneration.metalPiecesType),
    };
}

void ItemSpawnPresetData::SerializeCustom(
    const ItemSpawnPresetData& data, CSimpleIniA& ini, std::string_view sectionPrefix
) {
    const auto itemSection = PresetSectionName(sectionPrefix, "Item");
    ini.SetValue(itemSection.c_str(), "source", std::to_string(static_cast<int>(data.source)).c_str());
    SerializePresetLink(data.weaponPreset, ini, PresetSectionName(sectionPrefix, "Links.Weapon"));
    SerializePresetLink(data.armorPreset, ini, PresetSectionName(sectionPrefix, "Links.Armor"));
}

PresetOperationResult ItemSpawnPresetData::DeserializeCustom(
    ItemSpawnPresetData& data, const CSimpleIniA& ini, std::string_view sectionPrefix
) {
    const auto itemSection = PresetSectionName(sectionPrefix, "Item");
    const char* sourceText = ini.GetValue(itemSection.c_str(), "source", nullptr);
    int source = 0;
    constexpr int LAST_SOURCE = static_cast<int>(ItemSpawnPresetSource::ArmorPreset);
    if (!PresetUtils::TryParseInt(sourceText ? sourceText : "", source) || source < 0 || source > LAST_SOURCE) {
        return {.error = "Choose a valid item type"};
    }
    data.source = static_cast<ItemSpawnPresetSource>(source);

    if (auto result =
            DeserializeLink(data.weaponPreset, ini, PresetSectionName(sectionPrefix, "Links.Weapon"), "Weapon preset");
        !result)
        return result;
    return DeserializeLink(data.armorPreset, ini, PresetSectionName(sectionPrefix, "Links.Armor"), "Armor preset");
}
