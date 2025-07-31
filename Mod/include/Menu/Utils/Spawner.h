#pragma once

#include <queue>
#include <mutex>
#include <functional>

#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"

namespace Spawner {
    struct SpawnRequest {
        const SDK::UWorld* world;
        std::string classPath;
        SDK::FTransform transform;
        std::function<void(SDK::AActor*)> callback;
        bool snapToGround;
    };

    SDK::FVector GetGroundPosition(const SDK::UWorld* world, SDK::FVector position, float groundOffset = 50.0f, float traceDistance = 1000.0f);
    void SpawnActor(const SDK::UWorld* world, const std::string& className, const SDK::FTransform& transform, std::function<void(SDK::AActor*)> callback = nullptr, bool snapToGround = false);
    void ProcessSpawnQueue();
}