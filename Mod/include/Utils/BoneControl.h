#pragma once

#include "Core/ModContext.h"
#include "Hooks/GameHook.h"

namespace SDK {
    class AWillie_BP_C;
    class UObject;
}

namespace BoneControl {
    struct Settings {
        bool blockDislocation = false;
        float breakStrengthMultiplier = 1.0f;
        float massMultiplier = 1.0f;
    };

    SDK::AWillie_BP_C* WillieOwner(SDK::UObject* object);
    bool MatchesScope(SDK::AWillie_BP_C* willie, bool playerScope) noexcept;
    bool ShouldCancelBreak(GameHook::ProcessEventContext& context, SDK::AWillie_BP_C* willie);

    void MarkSpawnedWillie(SDK::AWillie_BP_C* willie);
    void ClearPendingSpawnMass(bool playerScope);
    void ApplyPendingSpawnMass(SDK::AWillie_BP_C* willie, const Settings& settings);
    void Apply(SDK::AWillie_BP_C* willie, const Settings& settings, bool force = false);
    void ApplyToScope(const RuntimeContextSnapshot& runtime, bool playerScope, const Settings& settings, bool force = true);
    void BreakAll(SDK::AWillie_BP_C* willie);
    void BreakEnemies(const RuntimeContextSnapshot& runtime);
}
