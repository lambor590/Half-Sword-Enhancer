#pragma once

#include <functional>

#include "Utils/CustomizableWeapon.h"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/Str_Passport_Weapon1_structs.hpp"
#include "SDK/Str_Passport_Armor1_structs.hpp"

namespace Spawner {
    enum class ActorType {
        Willie,
        Weapon,
        Shield,
        Tool,
        Armor,
        Unknown
    };

    ActorType GetActorType(const std::string& classPath);
    float GetGroundOffsetForType(ActorType type, const SDK::FVector& scale = {1.0f, 1.0f, 1.0f});
    void ClearCache();

    SDK::FVector GetGroundPosition(const SDK::UWorld* world, SDK::FVector position, float groundOffset = 50.0f, float traceDistance = 1000.0f);
    void SpawnActor(const SDK::UWorld* world, const std::string& className, const SDK::FTransform& transform, std::function<void(SDK::AActor*)> callback = nullptr, bool snapToGround = false, int tier = 4);
    void SpawnCustomizableWeapon(const SDK::UWorld* world, CustomizableWeapon type, const SDK::FTransform& transform, bool snapToGround = false, int tier = 4);
    void SpawnWeaponFromPassport(const SDK::UWorld* world, const SDK::FStr_Passport_Weapon1& passport, const SDK::FTransform& transform, bool snapToGround = false);
    void SpawnArmorFromPassport(const SDK::UWorld* world, const SDK::FStr_Passport_Armor1& passport, const SDK::FTransform& transform, bool snapToGround = false);
    void SpawnCustomizableFromPassport(const SDK::UWorld* world, const SDK::FStr_Passport_Weapon1& passport, const SDK::FTransform& transform, bool snapToGround = false, std::function<void(SDK::AActor*)> callback = nullptr);

    SDK::UClass* LoadClass(const std::string& classPath);
}