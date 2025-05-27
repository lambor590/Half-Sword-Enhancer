#include <queue>
#include <mutex>

#include "Menu/Utils/Spawner.h"

namespace Spawner {
    static std::queue<SpawnRequest> spawnQueue;
    static std::mutex queueMutex;

    static SDK::AActor* SpawnActorInternal(const SDK::UWorld* world, const std::string& classPath, const SDK::FTransform& transform) {
        std::wstring wClassName(classPath.begin(), classPath.end());
        SDK::FString classPathFStr(wClassName.c_str());
        SDK::FSoftClassPath softClassPath = SDK::UKismetSystemLibrary::MakeSoftClassPath(classPathFStr);
        SDK::TSoftClassPtr<SDK::UClass> softClassRef = SDK::UKismetSystemLibrary::Conv_SoftClassPathToSoftClassRef(softClassPath);
        SDK::UClass* actorClass = SDK::UKismetSystemLibrary::LoadClassAsset_Blocking(softClassRef);
        SDK::AActor* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
            world,
            actorClass,
            transform,
            SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
            nullptr,
            SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime
        );
        SDK::UGameplayStatics::FinishSpawningActor(actor, transform, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);
        return actor;
    }

    void SpawnActor(const SDK::UWorld* world, const std::string& className, const SDK::FTransform& transform, std::function<void(SDK::AActor*)> callback) {
        std::lock_guard<std::mutex> lock(queueMutex);
        spawnQueue.push({world, className, transform, callback});
    }

    void ProcessSpawnQueue() {
        std::lock_guard<std::mutex> lock(queueMutex);
        
        while (!spawnQueue.empty()) {
            const auto& request = spawnQueue.front();
            
            SDK::AActor* spawnedActor = SpawnActorInternal(request.world, request.classPath, request.transform);
            
            if (request.callback) {
                request.callback(spawnedActor);
            }
            
            spawnQueue.pop();
        }
    }
}