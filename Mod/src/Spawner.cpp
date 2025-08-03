#include <queue>
#include <mutex>
#include <unordered_map>

#include "Utils/Spawner.h"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "Hooks/GameHook.h"

namespace Spawner {

    ActorType GetActorType(const std::string& classPath) {
        static const std::unordered_map<std::string, ActorType> typeMap = {
            {"Willie_BP", ActorType::Willie},
            {"ModularWeapon", ActorType::Weapon},
            {"Shield", ActorType::Shield},
            {"Dagger", ActorType::Tool},
            {"Tool", ActorType::Tool},
            {"Armor", ActorType::Armor}
        };

        for (const auto& [key, type] : typeMap) {
            if (classPath.find(key) != std::string::npos) {
                return type;
            }
        }
        return ActorType::Unknown;
    }

    float GetGroundOffsetForType(ActorType type, const SDK::FVector& scale) {
        static const std::unordered_map<ActorType, float> offsetMap = {
            {ActorType::Willie, 100.0f},
            {ActorType::Weapon, 30.0f},
            {ActorType::Shield, 30.0f},
            {ActorType::Tool, 15.0f},
            {ActorType::Armor, 25.0f},
            {ActorType::Unknown, 50.0f}
        };

        auto it = offsetMap.find(type);
        float baseOffset = (it != offsetMap.end()) ? it->second : 50.0f;
        
        return baseOffset * (static_cast<float>(scale.Z) * 1.2f);
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
        startPos.Z += traceDistance * 0.5f;
        SDK::FVector endPos = position;
        endPos.Z -= traceDistance * 0.5f;
        
        SDK::TArray<SDK::AActor*> actorsToIgnore;
        
        bool hit = SDK::UKismetSystemLibrary::LineTraceSingle(
            world,
            startPos,
            endPos,
            SDK::ETraceTypeQuery::TraceTypeQuery1,
            true,
            actorsToIgnore,
            SDK::EDrawDebugTrace::None,
            &hitResult,
            true,
            SDK::FLinearColor(1.0f, 0.0f, 0.0f, 1.0f),
            SDK::FLinearColor(0.0f, 1.0f, 0.0f, 1.0f),
            1.0f
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
        GameHook::QueueAction([world, className, transform, callback, snapToGround]() {
            SDK::FTransform finalTransform = transform;
            
            if (snapToGround) {
                ActorType actorType = GetActorType(className);
                float groundOffset = GetGroundOffsetForType(actorType, transform.Scale3D);
                SDK::FVector groundPosition = GetGroundPosition(world, transform.Translation, groundOffset);
                finalTransform.Translation = groundPosition;
            }
            
            SDK::AActor* spawnedActor = SpawnActorInternal(world, className, finalTransform);
            
            if (callback) {
                callback(spawnedActor);
            }
        });
    }

}