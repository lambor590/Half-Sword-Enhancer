#include "Utils/NPCPresetSerializer.h"

#include <cmath>
#include <string>

namespace {
    bool IsFiniteInRange(double value, double minimum, double maximum) {
        return std::isfinite(value) && value >= minimum && value <= maximum;
    }
}

PresetOperationResult NPCPresetData::ValidateForSave() const {
    const auto& data = *this;
    if (data.npcTypeIndex < 0 || data.npcTypeIndex >= static_cast<int>(K_TYPES.size()))
        return {.error = "Choose a valid NPC type"};
    if (data.nationality < 0 || data.nationality >= static_cast<int>(K_NATIONALITY_NAMES.size()))
        return {.error = "Choose a valid nationality"};
    if (data.tier < K_MIN_TIER || data.tier > K_MAX_TIER) return {.error = "Choose a tier from 0 to 8"};
    if (data.team < K_MIN_TEAM || (data.team > K_MAX_TEAM && data.team != K_SPECIAL_TEAM))
        return {.error = "Choose a valid alliance"};
    if (!IsFiniteInRange(data.spawnDistanceForward, K_MIN_SPAWN_DISTANCE_FORWARD, K_MAX_SPAWN_DISTANCE_FORWARD))
        return {.error = "Forward distance must be between 100 and 500"};
    if (!IsFiniteInRange(data.spawnDistanceUp, K_MIN_SPAWN_DISTANCE_UP, K_MAX_SPAWN_DISTANCE_UP))
        return {.error = "Height must be between 0 and 300"};
    if (!IsFiniteInRange(data.spawnScale, K_MIN_SPAWN_SCALE, K_MAX_SPAWN_SCALE))
        return {.error = "Size must be between 0.1 and 4"};

    auto overrideValidation =
        ValidatePresetOverrideValuesForSave(GetPresetOverrides(const_cast<NPCPresetData&>(*this)), K_PRESET_KIND);
    if (!overrideValidation) return overrideValidation;

    return {.success = true};
}

void NPCPresetData::SerializeCustom(const NPCPresetData& data, CSimpleIniA& ini, std::string_view sectionPrefix) {
    const auto section = PresetSectionName(sectionPrefix, "Links.Loadout");
    SerializePresetLink(data.loadout, ini, section);
}

PresetOperationResult NPCPresetData::DeserializeCustom(
    NPCPresetData& data, const CSimpleIniA& ini, std::string_view sectionPrefix
) {
    const auto section = PresetSectionName(sectionPrefix, "Links.Loadout");
    auto link = DeserializePresetLink<LoadoutPresetData>(ini, section);
    if (link.success)
        data.loadout = std::move(link.value);
    else
        return {.error = "NPC loadout: " + link.error};
    return {.success = true};
}
