#pragma once

#include <filesystem>
#include <optional>

#include "Utils/LoadoutPresetResolver.h"
#include "Utils/NPCPresetSerializer.h"

struct ResolvedNPCPresetData {
    NPCPresetData preset;
    std::optional<ResolvedLoadoutPresetData> loadout;
};

class NPCPresetResolver final {
public:
    explicit NPCPresetResolver(std::filesystem::path appDataRoot);

    [[nodiscard]] PresetResolveResult<ResolvedNPCPresetData> Resolve(
        const NPCPresetData& data, PresetResolveContext& context
    ) const;
    [[nodiscard]] PresetResolveResult<ResolvedNPCPresetData> Resolve(
        const PresetLink<NPCPresetData>& link, PresetResolveContext& context
    ) const;

private:
    std::filesystem::path appDataRoot_;
};
