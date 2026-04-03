#pragma once

#include <string>
#include <vector>

#include "Menu/Preset.h"
#include "Utils/PlayerEditorOverrides.h"

struct PlayerPresetData : PresetDataBase {
    static constexpr const char* K_PRESETS_SUBDIR = "player_presets";

    PlayerEditorOverrides overrides{};

    static std::vector<OverrideGroupDescriptor> GetOverrideGroups(PlayerPresetData& data);
};

using PlayerPresetSerializer = PresetSerializer<PlayerPresetData>;
