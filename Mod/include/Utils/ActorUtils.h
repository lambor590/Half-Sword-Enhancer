#pragma once

#include "SDK/AI_BP_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "Utils/GameConstants.h"
#include "Utils/PossessState.h"

namespace ActorUtils {
    inline SDK::AAI_BP_C* GetAIController(SDK::AWillie_BP_C* willie) {
        if (!willie) return nullptr;
        auto* controller = willie->GetController();
        return controller && controller->IsA(SDK::AAI_BP_C::StaticClass()) ? static_cast<SDK::AAI_BP_C*>(controller)
                                                                           : nullptr;
    }

    inline void ApplyFearlessEffect(SDK::AWillie_BP_C* willie, bool recoverKneel = true) noexcept {
        if (!willie) return;

        willie->Fearless = true;
        willie->Fear = 0.0;
        willie->Give_Up = false;
        willie->Give_Up_2__Temp_ = false;
        willie->Retreat = false;
        willie->Panic_Rate = 0.0;
        willie->AI_Immediate_Threat = false;
        willie->Pain_Shock = false;
        const bool wasKneeling = willie->L_Kneel || willie->R_Kneel;
        if (recoverKneel && wasKneeling) willie->Un_Kneel_Event();
        willie->L_Kneel = false;
        willie->R_Kneel = false;
        willie->L_Kneel_Falling = false;
        willie->R_Kneel_Falling = false;
        willie->Give_Up_Weapon_To_Throat_Int = 0;

        auto* ai = GetAIController(willie);
        if (!ai) return;

        ai->Fearless = true;
        ai->My_Give_Up = false;
        ai->Give_Up_Meter = 0.0;
        ai->Threat_Level = 0.0;
        ai->Being_Threatened = false;
        ai->AI_Immediate_Threat = false;
        ai->AI_Threat = false;
        ai->Pain_Shock = false;
        ai->Retreat = false;
        ai->Retreat_Intent = 0.0;
        ai->Lost_Interest = false;
        ai->Kneel = false;
        ai->AI_Kneel = false;
        ai->Target_Give_Up = false;
        ai->Target_Unarmed = false;
    }

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
        if (!willie) return;

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
