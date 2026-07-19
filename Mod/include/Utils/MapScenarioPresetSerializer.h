#pragma once

#include <array>
#include <string>
#include <string_view>

#include "Menu/Preset.h"
#include "Utils/LoadoutPresetSerializer.h"
#include "Utils/NPCPresetSerializer.h"
#include "Utils/PlayerPresetSerializer.h"

struct MapScenarioPresetData : PresetDataBase {
    static constexpr const char* K_PRESETS_SUBDIR = "map_scenario_presets";
    static constexpr const char* K_PRESET_KIND = "map_scenario";

    struct PreLoadOptions {
        bool freshStart = false;
        bool tutorial = false;
        bool freeMode = false;
        bool carnage = false;
        int foesAmount = 3;
        int foeTier = 0;
        int combatantsAmount = 3;
        int opponentTier = 0;
    } preLoad{};

    struct AutoSpawnOptions {
        bool enabled = false;
        int npcCount = 0;
        PresetLink<PlayerPresetData> playerPreset;
        PresetLink<LoadoutPresetData> loadoutPreset;
        PresetLink<NPCPresetData> npcPreset;
    } autoSpawn{};

    std::string packageName;

    [[nodiscard]] PresetOperationResult ValidateForSave() const;
    [[nodiscard]] PresetOperationResult ValidateForSave(const std::filesystem::path& appDataRoot) const;
    static std::array<PresetFieldDescriptor, 11> GetPresetFields(MapScenarioPresetData& data);
    static void SerializeCustom(
        const MapScenarioPresetData& data, CSimpleIniA& ini, std::string_view sectionPrefix = {}
    );
    [[nodiscard]] static PresetOperationResult DeserializeCustom(
        MapScenarioPresetData& data, const CSimpleIniA& ini, std::string_view sectionPrefix = {}
    );
};

using MapScenarioPresetSerializer = PresetSerializer<MapScenarioPresetData>;
