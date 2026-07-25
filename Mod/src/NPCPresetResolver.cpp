#include "Utils/NPCPresetResolver.h"

#include <utility>

#include "Utils/PresetLinkResolution.h"

NPCPresetResolver::NPCPresetResolver(std::filesystem::path appDataRoot) : appDataRoot_(std::move(appDataRoot)) {}

PresetOperationResult NPCPresetData::ValidateForSave(const std::filesystem::path& appDataRoot) const {
    return PresetLinkResolution::ValidateForSave<NPCPresetSerializer>(*this, appDataRoot);
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
