#pragma once

#include <array>

#include "Menu/Preset.h"
#include "Utils/PlayerEditorOverrides.h"

struct PlayerPresetData : PresetDataBase {
    static constexpr const char* K_PRESETS_SUBDIR = "player_presets";
    static constexpr const char* K_PRESET_KIND = "player";

    PlayerEditorOverrides overrides{};

    static std::array<PresetOverrideDescriptor, 57> GetPresetOverrides(PlayerPresetData& data);
    [[nodiscard]] PresetOperationResult ValidateForSave() const;
};

using PlayerPresetSerializer = PresetSerializer<PlayerPresetData>;
