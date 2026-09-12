#include "Utils/NPCPresetResolver.h"

#include <utility>

NPCPresetResolver::NPCPresetResolver(std::filesystem::path appDataRoot) : appDataRoot_(std::move(appDataRoot)) {}

PresetOperationResult NPCPresetData::ValidateForSave(const std::filesystem::path& appDataRoot) const {
    PresetResolveContext context;
    auto resolved = NPCPresetResolver(appDataRoot).Resolve(*this, context);
    return {.success = resolved.success, .path = std::move(resolved.path), .error = std::move(resolved.error)};
}

PresetResolveResult<ResolvedNPCPresetData> NPCPresetResolver::Resolve(
    const NPCPresetData& data, PresetResolveContext& context
) const {
    ResolvedNPCPresetData resolved{.preset = data};
    if (!IsEmptyPresetLink(data.loadout)) {
        auto loadout = LoadoutPresetResolver(appDataRoot_).Resolve(data.loadout, context);
        if (!loadout.success || !loadout.value)
            return PresetResolveFailure<ResolvedNPCPresetData>("Loadout", std::move(loadout));

        resolved.loadout = std::move(*loadout.value);
    }

    return ResolvedPreset(std::move(resolved));
}

PresetResolveResult<ResolvedNPCPresetData> NPCPresetResolver::Resolve(
    const PresetLink<NPCPresetData>& link, PresetResolveContext& context
) const {
    return NPCPresetSerializer::ResolveLinkAs<ResolvedNPCPresetData>(
        link, appDataRoot_, context,
        [this](const NPCPresetData& data, PresetResolveContext& nestedContext) { return Resolve(data, nestedContext); }
    );
}
