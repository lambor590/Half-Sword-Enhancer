#pragma once

#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"

namespace Spawner {
    SDK::AActor* SpawnActor(const SDK::UWorld* world, const std::string& className, const SDK::FTransform& transform);
}