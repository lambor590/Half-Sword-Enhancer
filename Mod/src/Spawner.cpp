#include <queue>
#include <mutex>
#include <iostream>

#include "Utils/Spawner.h"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"

namespace Spawner {
    static std::queue<SpawnRequest> spawnQueue;
    static std::mutex queueMutex;

    static float GetGroundOffset(const std::string& classPath, const SDK::FVector& actorScale) {
        float baseOffset = 50.0f;
        
        if (classPath.find("Willie_BP") != std::string::npos) {
            baseOffset = 100.0f;
        }
        else if (classPath.find("ModularWeapon") != std::string::npos || 
                 classPath.find("Shield") != std::string::npos) {
            baseOffset = 30.0f;
        }
        else if (classPath.find("Dagger") != std::string::npos || 
                 classPath.find("Tool") != std::string::npos) {
            baseOffset = 15.0f;
        }
        else if (classPath.find("Armor") != std::string::npos) {
            baseOffset = 25.0f;
        }
        
        float scaledOffset = baseOffset * (static_cast<float>(actorScale.Z) * 1.2f);
        return scaledOffset;
    }

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

    SDK::FVector GetGroundPosition(const SDK::UWorld* world, SDK::FVector position, float groundOffset, float traceDistance) {
        SDK::FHitResult hitResult;
        SDK::FVector startPos = position;
        startPos.Z += 1000.0f;
        SDK::FVector endPos = position;
        endPos.Z -= 1000.0f;
        
        SDK::TArray<SDK::AActor*> actorsToIgnore;
        
        bool hit = SDK::UKismetSystemLibrary::LineTraceSingle(
            world,
            startPos,
            endPos,
            SDK::ETraceTypeQuery::TraceTypeQuery1,
            true,
            actorsToIgnore,
            SDK::EDrawDebugTrace::ForOneFrame,
            &hitResult,
            true,
            SDK::FLinearColor(1.0f, 0.0f, 0.0f, 1.0f),
            SDK::FLinearColor(0.0f, 1.0f, 0.0f, 1.0f),
            5.0f
        );
        
        if (hit && hitResult.bBlockingHit) {
            SDK::FVector groundPos = hitResult.Location;
            groundPos.Z += groundOffset;
            return groundPos;
        }
        
        SDK::FVector groundLevel = position;
        groundLevel.Z = groundOffset;
        return groundLevel;
    }

    void SpawnActor(const SDK::UWorld* world, const std::string& className, const SDK::FTransform& transform, std::function<void(SDK::AActor*)> callback, bool snapToGround) {
        std::lock_guard<std::mutex> lock(queueMutex);
        spawnQueue.push({world, className, transform, callback, snapToGround});
    }

    void ProcessSpawnQueue() {
        std::lock_guard<std::mutex> lock(queueMutex);
        
        while (!spawnQueue.empty()) {
            const auto& request = spawnQueue.front();
            
            SDK::FTransform finalTransform = request.transform;
            
            if (request.snapToGround) {
                float groundOffset = GetGroundOffset(request.classPath, request.transform.Scale3D);
                SDK::FVector groundPosition = GetGroundPosition(request.world, request.transform.Translation, groundOffset);
                finalTransform.Translation = groundPosition;
            }
            
            SDK::AActor* spawnedActor = SpawnActorInternal(request.world, request.classPath, finalTransform);
            
            if (request.callback) {
                request.callback(spawnedActor);
            }
            
            spawnQueue.pop();
        }
    }
}