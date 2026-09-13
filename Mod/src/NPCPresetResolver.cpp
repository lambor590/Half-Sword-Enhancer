#include "Utils/NPCPresetResolver.h"

#include <utility>

namespace {
    PresetResolveResult<ResolvedLoadoutPresetData> ResolveNPCLoadout(
        const NPCPresetData& data, const std::filesystem::path& appDataRoot, PresetResolveContext& context
    ) {
        if (IsEmptyPresetLink(data.loadout)) return {.success = true};

        auto loadout = LoadoutPresetResolver(appDataRoot).Resolve(data.loadout, context);
        if (!loadout.success || !loadout.value)
            return PresetResolveFailure<ResolvedLoadoutPresetData>("Loadout", std::move(loadout));
        return loadout;
    }
}

NPCPresetResolver::NPCPresetResolver(std::filesystem::path appDataRoot) : appDataRoot_(std::move(appDataRoot)) {}

PresetOperationResult NPCPresetData::ValidateForSave(
    const std::filesystem::path& appDataRoot, PresetResolveContext& context
) const {
    auto resolved = ResolveNPCLoadout(*this, appDataRoot, context);
    if (!resolved.success) return {.path = std::move(resolved.path), .error = std::move(resolved.error)};
    return {.success = true};
}

PresetResolveResult<ResolvedNPCPresetData> NPCPresetResolver::Resolve(
    const NPCPresetData& data, PresetResolveContext& context
) const {
    auto loadout = ResolveNPCLoadout(data, appDataRoot_, context);
    if (!loadout.success) return PresetResolveFailure<ResolvedNPCPresetData>({}, std::move(loadout));
    return ResolvedPreset(ResolvedNPCPresetData{.preset = data, .loadout = std::move(loadout.value)});
}

PresetResolveResult<ResolvedNPCPresetData> NPCPresetResolver::Resolve(
    const PresetLink<NPCPresetData>& link, PresetResolveContext& context
) const {
    return NPCPresetSerializer::ResolveLinkAs<ResolvedNPCPresetData>(
        link, appDataRoot_, context,
        [this](const NPCPresetData& data, PresetResolveContext& nestedContext) { return Resolve(data, nestedContext); }
    );
}
