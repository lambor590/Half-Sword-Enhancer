#pragma once

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "Core/ModContext.h"
#include "Menu/SectionConfig.h"
#include "Utils/ArmorPresetSerializer.h"
#include "Utils/ItemSpawnPresetSerializer.h"
#include "Utils/LivePreviewManager.h"
#include "Utils/LoadoutPresetResolver.h"
#include "Utils/NPCPresetSerializer.h"
#include "Utils/WeaponClassPaths.h"
#include "SDK/Engine_classes.hpp"
#include "SDK/Enum_Nationalities_structs.hpp"
#include "SDK/Str_Passport_Weapon1_structs.hpp"

namespace SpawnWorkflow {
    using ActorCallback = std::function<void(SDK::AActor*)>;
    using ActorSetup = std::function<std::expected<void, std::string>(SDK::AActor*)>;

    struct SpawnResult {
        bool success = false;
        SDK::AActor* actor = nullptr;
        std::string error;
    };
    using SpawnCompletion = std::function<void(SpawnResult)>;

    // Queue* returns whether the request was accepted/enqueued. setupActor is part of the transaction: a failure
    // removes the new actor and onComplete reports it instead of publishing a partial spawn as successful.
    bool QueueWeaponSpawn(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::FStr_Passport_Weapon1 passport,
        WeaponClassPaths classPaths, ActorSetup setupActor = nullptr, std::string deferredWeaponName = {},
        SpawnCompletion onComplete = nullptr
    );
    bool QueueWeaponPreview(
        const RuntimeContextSnapshot& snapshot, LivePreviewManager& preview, const SpawnConfig& spawn,
        SDK::FStr_Passport_Weapon1 passport, WeaponClassPaths classPaths, ActorCallback onSpawned,
        ActorCallback onPreviewReady, std::string deferredWeaponName = {}
    );

    bool QueueArmorPreview(
        const RuntimeContextSnapshot& snapshot, LivePreviewManager& preview, const SpawnConfig& spawn,
        ArmorPresetData preset, ActorCallback onPreviewReady = nullptr
    );

    bool SpawnItemPreset(
        const RuntimeContextSnapshot& runtime, const ItemSpawnPresetData& data,
        const SpawnCompletion& onComplete = nullptr, std::string* error = nullptr
    );
    bool QueueItemPresetSpawn(
        const RuntimeContextSnapshot& snapshot, const ItemSpawnPresetData& data, SpawnCompletion onComplete = nullptr,
        std::string* error = nullptr
    );

    struct NPCSpawnParams {
        std::string classPath;
        SDK::Enum_Nationalities nationality{};
        SDK::Enum_Ranks tier{};
        bool mercenary = false;
        bool bodyguard = false;
        int team = 0;
        NPCOverrides overrides{};
        std::optional<ResolvedLoadoutPresetData> loadout;
        SpawnCompletion onComplete = nullptr;
    };

    [[nodiscard]] std::expected<NPCSpawnParams, std::string> BuildNPCSpawnParams(
        const NPCPresetData& preset, std::optional<ResolvedLoadoutPresetData> loadout = std::nullopt
    );

    // Game-thread only. Uses the same NPC initialization and cleanup path as the queued spawn helpers,
    // but preserves an explicit caller-provided transform for scenario and batch placement.
    bool SpawnNPCAt(
        SDK::UWorld* world, int playerTeam, const SDK::FTransform& transform, bool snapToGround,
        const NPCSpawnParams& request
    );
    bool SpawnNPC(const RuntimeContextSnapshot& runtime, const SpawnConfig& spawn, const NPCSpawnParams& request);
    bool QueueNPCSpawn(const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, NPCSpawnParams request);
}
