#pragma once

#include <functional>

#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"

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
    void SpawnActor(const SDK::UWorld* world, const std::string& className, const SDK::FTransform& transform, std::function<void(SDK::AActor*)> callback = nullptr, bool snapToGround = false);
}