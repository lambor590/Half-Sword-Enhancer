#pragma once

#include "SDK/Willie_BP_classes.hpp"
#include "SDK/Engine_classes.hpp"
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