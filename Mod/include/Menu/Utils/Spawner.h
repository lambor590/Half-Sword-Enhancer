#pragma once

#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"

namespace Spawner {
    void SpawnActor(const SDK::UWorld* world, const std::string& className, const SDK::FTransform& transform);
}