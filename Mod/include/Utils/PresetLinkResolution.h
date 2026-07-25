#pragma once

#include <filesystem>
#include <string>
#include <type_traits>
#include <utility>

#include "ConfigManager.h"
#include "Utils/ItemSpawnPresetSerializer.h"
#include "Utils/LoadoutPresetResolver.h"
#include "Utils/MapScenarioPresetSerializer.h"
#include "Utils/NPCPresetResolver.h"

namespace PresetLinkResolution {
    template <typename Data> [[nodiscard]] std::string FormatDiagnostic(const PresetResolveResult<Data>& result) {
        if (result.success) return {};
        return result.error.empty() ? "The selected preset is unavailable." : result.error;
    }

    namespace Detail {
        [[nodiscard]] inline PresetResolveResult<MapScenarioPresetData> ValidateMapScenarioLinks(
            const MapScenarioPresetData& data, const std::filesystem::path& appDataRoot, PresetResolveContext& context
        );
        [[nodiscard]] inline PresetResolveResult<ItemSpawnPresetData> ValidateItemSpawnLinks(
            const ItemSpawnPresetData& data, const std::filesystem::path& appDataRoot, PresetResolveContext& context
        );

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

        [[nodiscard]] inline PresetResolveResult<MapScenarioPresetData> ResolveMapScenario(
            const PresetLink<MapScenarioPresetData>& link, const std::filesystem::path& appDataRoot,
            PresetResolveContext& context
        ) {
            return MapScenarioPresetSerializer::ResolveLinkAs<MapScenarioPresetData>(
                link, appDataRoot, context,
                [&appDataRoot](const MapScenarioPresetData& data, PresetResolveContext& nestedContext) {
                    return ValidateMapScenarioLinks(data, appDataRoot, nestedContext);
                }
            );
        }

        [[nodiscard]] inline PresetResolveResult<MapScenarioPresetData> ValidateMapScenarioLinks(
            const MapScenarioPresetData& data, const std::filesystem::path& appDataRoot, PresetResolveContext& context
        ) {
            if (!data.autoSpawn.enabled) return ResolvedPreset(data);

            auto player = PlayerPresetSerializer::ResolveLink(data.autoSpawn.playerPreset, appDataRoot, context);
            if (!player.success)
                return PresetResolveFailure<MapScenarioPresetData>("Starting Player", std::move(player));

            auto loadout = LoadoutPresetResolver(appDataRoot).Resolve(data.autoSpawn.loadoutPreset, context);
            if (!loadout.success)
                return PresetResolveFailure<MapScenarioPresetData>("Starting Equipment", std::move(loadout));

            if (data.autoSpawn.npcCount > 0) {
                auto npc = ResolveNPC(data.autoSpawn.npcPreset, appDataRoot, context);
                if (!npc.success) return PresetResolveFailure<MapScenarioPresetData>("Starting NPCs", std::move(npc));
            }

            return ResolvedPreset(data);
        }

        [[nodiscard]] inline PresetResolveResult<ItemSpawnPresetData> ResolveItemSpawn(
            const PresetLink<ItemSpawnPresetData>& link, const std::filesystem::path& appDataRoot,
            PresetResolveContext& context
        ) {
            return ItemSpawnPresetSerializer::ResolveLinkAs<ItemSpawnPresetData>(
                link, appDataRoot, context,
                [&appDataRoot](const ItemSpawnPresetData& data, PresetResolveContext& nestedContext) {
                    return ValidateItemSpawnLinks(data, appDataRoot, nestedContext);
                }
            );
        }

        [[nodiscard]] inline PresetResolveResult<ItemSpawnPresetData> ValidateItemSpawnLinks(
            const ItemSpawnPresetData& data, const std::filesystem::path& appDataRoot, PresetResolveContext& context
        ) {
            if (data.source == ItemSpawnPresetSource::WeaponPreset) {
                auto weapon = WeaponPresetSerializer::ResolveLink(data.weaponPreset, appDataRoot, context);
                if (!weapon.success) return PresetResolveFailure<ItemSpawnPresetData>("Weapon", std::move(weapon));
            } else if (data.source == ItemSpawnPresetSource::ArmorPreset) {
                auto armor = ArmorPresetSerializer::ResolveLink(data.armorPreset, appDataRoot, context);
                if (!armor.success) return PresetResolveFailure<ItemSpawnPresetData>("Armor", std::move(armor));
            }
            return ResolvedPreset(data);
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
        else if constexpr (std::is_same_v<Serializer, MapScenarioPresetSerializer>)
            return Detail::ResolveMapScenario(link, appDataRoot, context);
        else if constexpr (std::is_same_v<Serializer, ItemSpawnPresetSerializer>)
            return Detail::ResolveItemSpawn(link, appDataRoot, context);
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

    template <typename Serializer>
    [[nodiscard]] PresetOperationResult ValidateForSave(
        const typename Serializer::Data& data, const std::filesystem::path& appDataRoot
    ) {
        PresetResolveContext context;
        if constexpr (std::is_same_v<Serializer, LoadoutPresetSerializer>) {
            auto resolved = LoadoutPresetResolver(appDataRoot).Resolve(data, context);
            return {.success = resolved.success, .path = std::move(resolved.path), .error = std::move(resolved.error)};
        } else if constexpr (std::is_same_v<Serializer, NPCPresetSerializer>) {
            auto resolved = NPCPresetResolver(appDataRoot).Resolve(data, context);
            return {.success = resolved.success, .path = std::move(resolved.path), .error = std::move(resolved.error)};
        } else if constexpr (std::is_same_v<Serializer, MapScenarioPresetSerializer>) {
            auto resolved = Detail::ValidateMapScenarioLinks(data, appDataRoot, context);
            return {.success = resolved.success, .path = std::move(resolved.path), .error = std::move(resolved.error)};
        } else {
            static_assert(std::is_same_v<Serializer, ItemSpawnPresetSerializer>);
            auto resolved = Detail::ValidateItemSpawnLinks(data, appDataRoot, context);
            return {.success = resolved.success, .path = std::move(resolved.path), .error = std::move(resolved.error)};
        }
    }
}
