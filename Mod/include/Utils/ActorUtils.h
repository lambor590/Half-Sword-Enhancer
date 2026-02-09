#pragma once

#include "SDK/Willie_BP_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/BP_Constraint_Bite_classes.hpp"
#include "Utils/GameConstants.h"

namespace ActorUtils {
    template<typename Func>
    void ForEachWillieInRadius(SDK::UWorld* world, SDK::AWillie_BP_C* player, float radius, Func&& func) {
        SDK::TArray<SDK::AActor*> actors;
        SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AWillie_BP_C::StaticClass(), &actors);
        
        for (auto* actor : actors) {
            auto* willie = static_cast<SDK::AWillie_BP_C*>(actor);
            if (willie == player || !willie) continue;
            
            if (radius == GameConstants::MAX_DISTANCE || player->GetDistanceTo(willie) <= radius) {
                func(willie);
            }
        }
    }

    template<typename Func>
    void ForEachWillie(SDK::UWorld* world, SDK::AWillie_BP_C* player, Func&& func) {
        ForEachWillieInRadius(world, player, GameConstants::MAX_DISTANCE, std::forward<Func>(func));
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

    template<typename ComponentClass, typename Func>
    void ForEachComponentOfType(SDK::UWorld* world, Func&& func) {
        SDK::TArray<SDK::AActor*> actors;
        SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AActor::StaticClass(), &actors);

        for (auto* actor : actors) {
            if (!actor) continue;
            SDK::TArray<SDK::UActorComponent*> components = actor->K2_GetComponentsByClass(ComponentClass::StaticClass());
            for (auto* component : components) {
                if (auto* typed = static_cast<ComponentClass*>(component)) {
                    func(typed);
                }
            }
        }
    }

    inline SDK::AWillie_BP_C* FindNearestWillie(
        SDK::UWorld* world, SDK::AWillie_BP_C* player, SDK::AActor* origin,
        float maxRange, SDK::AWillie_BP_C* additionalExclude = nullptr) noexcept
    {
        SDK::AWillie_BP_C* nearest = nullptr;
        float nearestDist = maxRange;
        ForEachWillie(world, player, [&](SDK::AWillie_BP_C* willie) {
            if (willie == additionalExclude) return;
            float dist = origin->GetDistanceTo(willie);
            if (dist < nearestDist) { nearestDist = dist; nearest = willie; }
        });
        return nearest;
    }

    inline SDK::ABP_Constraint_Bite_C* SpawnBiteConstraint(
        SDK::UWorld* world, SDK::AWillie_BP_C* biter, SDK::AWillie_BP_C* target) noexcept
    {
        SDK::FTransform transform{};
        transform.Rotation = SDK::FQuat{ 0.0, 0.0, 0.0, 1.0 };
        transform.Translation = biter->K2_GetActorLocation();
        transform.Scale3D = { 1.0, 1.0, 1.0 };

        auto* biteActor = static_cast<SDK::ABP_Constraint_Bite_C*>(
            SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
                world, SDK::ABP_Constraint_Bite_C::StaticClass(), transform,
                SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
                nullptr, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime));
        if (!biteActor) [[unlikely]] return nullptr;

        static const SDK::FName headBone = SDK::BasicFilesImpleUtils::StringToName(L"head");

        biteActor->Bitten_Actor = target;
        biteActor->Bitten_Component = target->Mesh;
        biteActor->Bitten_Bone = headBone;
        biteActor->Bitter_Component = biter->Mesh;

        SDK::UGameplayStatics::FinishSpawningActor(
            biteActor, transform, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);

        biter->Bite_Damage_Actor = biteActor;
        biter->Biting = true;
        biter->Is_Zombie_ = true;
        return biteActor;
    }

    inline void ReleaseBite(SDK::AWillie_BP_C* willie) noexcept {
        if (willie->Bite_Damage_Actor) {
            willie->Bite_Damage_Actor->K2_DestroyActor();
        }
        willie->Bite_Damage_Actor = nullptr;
        willie->Biting = false;
        willie->Is_Zombie_ = false;
    }

    template<typename ObjectClass, typename Func>
    void ForEachObjectOfType(SDK::UWorld* world, Func&& func) {
        SDK::TArray<SDK::AActor*> objects;
        SDK::UGameplayStatics::GetAllActorsOfClass(world, ObjectClass::StaticClass(), &objects);
        
        for (auto* object : objects) {
            if (auto* typedObject = static_cast<ObjectClass*>(object)) {
                func(typedObject);
            }
        }
    }
}