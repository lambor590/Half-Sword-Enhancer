#pragma once

#include <filesystem>
#include <string>
#include <type_traits>
#include <utility>

#include "ConfigManager.h"
#include "Utils/LoadoutPresetResolver.h"
#include "Utils/NPCPresetResolver.h"

namespace PresetLinkResolution {
    template <typename Data> [[nodiscard]] std::string FormatDiagnostic(const PresetResolveResult<Data>& result) {
        if (result.success) return {};
        return result.error.empty() ? "The selected preset is unavailable." : result.error;
    }

    namespace Detail {
        [[nodiscard]] inline PresetResolveResult<LoadoutPresetData> ResolveLoadout(
            const PresetLink<LoadoutPresetData>& link, const std::filesystem::path& appDataRoot,
            PresetResolveContext& context
        ) {
            return LoadoutPresetSerializer::ResolveLinkAs<LoadoutPresetData>(
                link, appDataRoot, context,
                [&appDataRoot](const LoadoutPresetData& data, PresetResolveContext& nestedContext) {
                    auto resolved = LoadoutPresetResolver(appDataRoot).Resolve(data, nestedContext);
                    if (!resolved.success)
                        return PresetResolveFailure<LoadoutPresetData>(std::string_view{}, std::move(resolved));
                    return ResolvedPreset(data);
                }
            );
        }

        [[nodiscard]] inline PresetResolveResult<NPCPresetData> ResolveNPC(
            const PresetLink<NPCPresetData>& link, const std::filesystem::path& appDataRoot,
            PresetResolveContext& context
        ) {
            auto composition = NPCPresetResolver(appDataRoot).Resolve(link, context);
            PresetResolveResult<NPCPresetData> result;
            result.success = composition.success;
            result.path = std::move(composition.path);
            result.error = std::move(composition.error);
            if (composition.value) result.value = std::move(composition.value->preset);
            return result;
        }
    }

    template <typename Serializer>
    [[nodiscard]] PresetResolveResult<typename Serializer::Data> Resolve(
        const PresetLink<typename Serializer::Data>& link, const std::filesystem::path& appDataRoot,
        PresetResolveContext& context
    ) {
        if constexpr (std::is_same_v<Serializer, LoadoutPresetSerializer>)
            return Detail::ResolveLoadout(link, appDataRoot, context);
        else if constexpr (std::is_same_v<Serializer, NPCPresetSerializer>)
            return Detail::ResolveNPC(link, appDataRoot, context);
        else
            return Serializer::ResolveLink(link, appDataRoot, context);
    }

    template <typename Serializer>
    [[nodiscard]] PresetResolveResult<typename Serializer::Data> Resolve(
        const PresetLink<typename Serializer::Data>& link,
        const std::filesystem::path& appDataRoot = ConfigManager::GetAppDataPath()
    ) {
        PresetResolveContext context;
        return Resolve<Serializer>(link, appDataRoot, context);
    }
}
