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
    };

    void SpawnActor(const SDK::UWorld* world, const std::string& className, const SDK::FTransform& transform, std::function<void(SDK::AActor*)> callback = nullptr);
    void ProcessSpawnQueue();
}