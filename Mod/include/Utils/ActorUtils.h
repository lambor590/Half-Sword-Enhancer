#pragma once

#include "SDK/Willie_BP_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "Utils/GameConstants.h"
#include "Utils/PossessState.h"

namespace ActorUtils {
    template <typename Func>
    void ForEachWillieInRadius(SDK::UWorld* world, SDK::AWillie_BP_C* player, float radius, Func&& func) {
        SDK::TArray<SDK::AActor*> actors;
        SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AWillie_BP_C::StaticClass(), &actors);

        auto* originalPawn = PossessState::GetOriginalPawn();

        for (auto* actor : actors) {
            auto* willie = static_cast<SDK::AWillie_BP_C*>(actor);
            if (willie == player || willie == originalPawn || !willie) continue;

            if (radius == GameConstants::MAX_DISTANCE || player->GetDistanceTo(willie) <= radius) {
                func(willie);
            }
        }
    }

    template <typename Func> void ForEachWillie(SDK::UWorld* world, SDK::AWillie_BP_C* player, Func&& func) {
        ForEachWillieInRadius(world, player, GameConstants::MAX_DISTANCE, std::forward<Func>(func));
    }

    inline void ApplyBiteState(SDK::AWillie_BP_C* willie, bool active) noexcept {
        if (active && !willie->Biting)
            willie->Bite_Event();
        else if (!active && willie->Biting)
            willie->Un_Bite_Event();
    }

    inline void SetInfiniteConsciousness(SDK::AWillie_BP_C* willie) noexcept {
        willie->Consciousness_Cap = GameConstants::DEFAULT_HEALTH;
        willie->Consciousness = GameConstants::DEFAULT_HEALTH;
        willie->Consciousness_2__Legs_ = GameConstants::DEFAULT_HEALTH;
    }

    inline void ApplyNoPainEffect(SDK::AWillie_BP_C* willie) noexcept {
        willie->Health = GameConstants::DEFAULT_HEALTH;
        willie->Neck_Health = GameConstants::DEFAULT_HEALTH;
        willie->Head_Health = GameConstants::DEFAULT_HEALTH;
        willie->Body_Upper_Health = GameConstants::DEFAULT_HEALTH;
        willie->Body_Lower_Health = GameConstants::DEFAULT_HEALTH;
        willie->Arm_R_Health = GameConstants::DEFAULT_HEALTH;
        willie->Arm_L_Health = GameConstants::DEFAULT_HEALTH;
        willie->Leg_R_Health = GameConstants::DEFAULT_HEALTH;
        willie->Leg_L_Health = GameConstants::DEFAULT_HEALTH;
        willie->Head_Health__Crush_ = GameConstants::DEFAULT_HEALTH;
        willie->Back_Health = GameConstants::DEFAULT_HEALTH;

        willie->Back_Broken = false;
        willie->Head_Broken = false;

        willie->Pain_Lower_Body = GameConstants::DEFAULT_PAIN;
        willie->Pain_Upper_Body = GameConstants::DEFAULT_PAIN;
        willie->Pain_Neck = GameConstants::DEFAULT_PAIN;
        willie->Pain_Head = GameConstants::DEFAULT_PAIN;
        willie->Pain_Arm_R = GameConstants::DEFAULT_PAIN;
        willie->Pain_Arm_L = GameConstants::DEFAULT_PAIN;
        willie->Pain_Leg_R = GameConstants::DEFAULT_PAIN;
        willie->Pain_Leg_L = GameConstants::DEFAULT_PAIN;
        willie->Pain = GameConstants::DEFAULT_PAIN;
        willie->Pain_L_Arm_Alpha = GameConstants::DEFAULT_PAIN;
        willie->Pain_R_Arm_Alpha = GameConstants::DEFAULT_PAIN;
        willie->Pain_Shock = GameConstants::DEFAULT_PAIN;
        willie->Current_Pain_Threshold = GameConstants::DEFAULT_PAIN;
        willie->Pain_Grab_Rate = GameConstants::DEFAULT_PAIN;
        willie->Pain_Shock_Rate = GameConstants::DEFAULT_PAIN;
        willie->Pain_Shock_Interp = GameConstants::DEFAULT_PAIN;
        willie->Sustained_Damage = GameConstants::DEFAULT_PAIN;
        willie->Ball_Pain = GameConstants::DEFAULT_PAIN;
        willie->Liver_Pain = GameConstants::DEFAULT_PAIN;
        willie->Last_Pain = GameConstants::DEFAULT_PAIN;

        willie->Voice_Pain = false;

        willie->PainFlinchDirection_Current = SDK::FRotator{};
        willie->PainFlinchDirection_Latest = SDK::FRotator{};
        willie->Pain_Wound_Direction = SDK::FRotator{};
        willie->Pain_Stumble_Immediate = SDK::FVector{};
        willie->Pain_Stumble_Delayed = SDK::FVector{};
    }

    template <typename ComponentClass, typename Func> void ForEachComponentOfType(SDK::UWorld* world, Func&& func) {
        SDK::TArray<SDK::AActor*> actors;
        SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AActor::StaticClass(), &actors);

        for (auto* actor : actors) {
            if (!actor) continue;
            SDK::TArray<SDK::UActorComponent*> components =
                actor->K2_GetComponentsByClass(ComponentClass::StaticClass());
            for (auto* component : components) {
                if (auto* typed = static_cast<ComponentClass*>(component)) {
                    func(typed);
                }
            }
        }
    }

    inline SDK::AWillie_BP_C* FindNearestWillie(
        SDK::UWorld* world, SDK::AWillie_BP_C* player, SDK::AActor* origin, float maxRange,
        SDK::AWillie_BP_C* additionalExclude = nullptr
    ) {
        SDK::AWillie_BP_C* nearest = nullptr;
        float nearestDist = maxRange;
        ForEachWillie(world, player, [&](SDK::AWillie_BP_C* willie) {
            if (willie == additionalExclude) return;
            float dist = origin->GetDistanceTo(willie);
            if (dist < nearestDist) {
                nearestDist = dist;
                nearest = willie;
            }
        });
        return nearest;
    }

    template <typename ObjectClass, typename Func> void ForEachObjectOfType(SDK::UWorld* world, Func&& func) {
        SDK::TArray<SDK::AActor*> objects;
        SDK::UGameplayStatics::GetAllActorsOfClass(world, ObjectClass::StaticClass(), &objects);

        for (auto* object : objects) {
            if (auto* typedObject = static_cast<ObjectClass*>(object)) {
                func(typedObject);
            }
        }
    }
}
