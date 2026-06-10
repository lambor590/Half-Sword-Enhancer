#pragma once

#include <functional>

#include "Core/ModContext.h"
#include "Menu/SectionConfig.h"
#include "Utils/LivePreviewManager.h"
#include "Utils/WeaponClassPaths.h"
#include "SDK/Engine_classes.hpp"
#include "SDK/Str_Passport_Armor1_structs.hpp"
#include "SDK/Str_Passport_Weapon1_structs.hpp"

namespace SpawnWorkflow {
    using ActorCallback = std::function<void(SDK::AActor*)>;

    bool QueueWeaponSpawn(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::FStr_Passport_Weapon1 passport,
        WeaponClassPaths classPaths, ActorCallback onSpawned = nullptr
    );
    bool QueueWeaponPreview(
        const RuntimeContextSnapshot& snapshot, LivePreviewManager& preview, const SpawnConfig& spawn,
        SDK::FStr_Passport_Weapon1 passport, WeaponClassPaths classPaths, ActorCallback onSpawned,
        ActorCallback onPreviewReady
    );

    bool QueueArmorSpawn(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::FStr_Passport_Armor1 passport,
        ActorCallback onSpawned = nullptr
    );
    bool QueueArmorPreview(
        const RuntimeContextSnapshot& snapshot, LivePreviewManager& preview, const SpawnConfig& spawn,
        SDK::FStr_Passport_Armor1 passport, ActorCallback onPreviewReady = nullptr
    );
}
