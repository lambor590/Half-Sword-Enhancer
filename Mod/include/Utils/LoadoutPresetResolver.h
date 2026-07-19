#pragma once

#include <array>
#include <filesystem>
#include <optional>

#include "Utils/LoadoutPresetSerializer.h"

/// Fully resolved, index-aligned weapon and armor preset snapshots.
struct ResolvedLoadoutPresetData {
    std::array<std::optional<WeaponPresetData>, LoadoutPresetData::K_WEAPON_SLOT_COUNT> weapons;
    std::array<std::optional<ArmorPresetData>, LoadoutPresetData::K_ARMOR_SLOT_COUNT> armor;
};

class LoadoutPresetResolver final {
public:
    LoadoutPresetResolver();
    explicit LoadoutPresetResolver(std::filesystem::path appDataRoot);

    [[nodiscard]] PresetResolveResult<ResolvedLoadoutPresetData> Resolve(
        const LoadoutPresetData& data, PresetResolveContext& context
    ) const;
    [[nodiscard]] PresetResolveResult<ResolvedLoadoutPresetData> Resolve(const LoadoutPresetData& data) const;

    [[nodiscard]] PresetResolveResult<ResolvedLoadoutPresetData> Resolve(
        const PresetLink<LoadoutPresetData>& link, PresetResolveContext& context
    ) const;
    [[nodiscard]] PresetResolveResult<ResolvedLoadoutPresetData> Resolve(
        const PresetLink<LoadoutPresetData>& link
    ) const;

private:
    std::filesystem::path appDataRoot_;
};
