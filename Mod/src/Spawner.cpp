#include "Menu/Utils/Spawner.h"

namespace Spawner {
    void SpawnActor(const SDK::UWorld* world, const std::string& className, const SDK::FTransform& transform) {
        SDK::UClass* actorClass = SDK::UObject::FindClassFast(className);
        SDK::AActor* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
            world,
            actorClass,
            transform,
            SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
            nullptr,
            SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime
        );
        SDK::UGameplayStatics::FinishSpawningActor(actor, transform, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);
    }
}