#include "Utils/NPCPresetResolver.h"

#include <utility>

#include "ConfigManager.h"
#include "Utils/PresetLinkResolution.h"

NPCPresetResolver::NPCPresetResolver() : appDataRoot_(ConfigManager::GetAppDataPath()) {}

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

        const auto validation = LoadoutPresetResolver::ValidateForNPC(*loadout.value);
        if (!validation) {
            PresetResolveResult<ResolvedNPCPresetData> result;
            result.path = std::move(loadout.path);
            result.error = "Loadout: " + validation.error;
            return result;
        }
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
