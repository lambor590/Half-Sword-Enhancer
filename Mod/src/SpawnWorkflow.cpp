#include "Utils/SpawnWorkflow.h"

#include <utility>

#include "Hooks/GameHook.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/Spawner.h"
#include "SDK/BP_Armor_Master_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"

namespace SpawnWorkflow {

    namespace {
        using PreviewPrepareFn = void (*)(SDK::AActor*);

        bool BuildQueuedTransform(
            const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::FTransform& outTransform
        ) {
            if (!snapshot.world || !snapshot.player) return false;
            outTransform = Spawner::BuildSpawnTransform(snapshot.player, spawn);
            return true;
        }

        void PrepareWeaponPreviewActor(SDK::AActor* actor) {
            auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
            weapon->Simulates_Physics = false;
            weapon->Turn_Off_Collision();
            actor->SetActorEnableCollision(false);
        }

        void PrepareArmorPreviewActor(SDK::AActor* actor) {
            auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(actor);
            armor->Simulates_Physics = false;
            if (armor->Armor_Mesh_Static) armor->Armor_Mesh_Static->SetSimulatePhysics(false);
            if (armor->Armor_Mesh_Skeletal) armor->Armor_Mesh_Skeletal->SetAllBodiesSimulatePhysics(false);
            if (armor->Armor_Mesh_Primitive) armor->Armor_Mesh_Primitive->SetSimulatePhysics(false);
            actor->SetActorEnableCollision(false);
        }

        void FinishPreviewActor(
            LivePreviewManager* preview, SDK::UWorld* world, SDK::AActor* actor, ActorCallback& onSpawned,
            ActorCallback& onReady, PreviewPrepareFn prepare
        ) {
            if (!actor) return;
            if (onSpawned) onSpawned(actor);
            if (!preview->IsEnabled()) {
                actor->K2_DestroyActor();
                return;
            }

            prepare(actor);
            if (onReady) onReady(actor);
            preview->SetPreviewActor(actor, world);
        }
    }

    bool QueueWeaponSpawn(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::FStr_Passport_Weapon1 passport,
        WeaponClassPaths classPaths, ActorCallback onSpawned
    ) {
        SDK::FTransform transform{};
        if (!BuildQueuedTransform(snapshot, spawn, transform)) return false;

        const bool snap = spawn.snapToGround;
        GameHook::QueueAction([passport, classPaths = std::move(classPaths), onSpawned = std::move(onSpawned),
                               transform, snap](const RuntimeContextSnapshot& runtime) mutable {
            if (!runtime.world) return;
            Spawner::LoadWeaponClasses(passport, classPaths);
            if (!EquipmentGenerator::IsPassportValid(passport)) return;
            Spawner::SpawnCustomizableFromPassport(runtime.world, passport, transform, snap, onSpawned);
        });
        return true;
    }

    bool QueueWeaponPreview(
        const RuntimeContextSnapshot& snapshot, LivePreviewManager& preview, const SpawnConfig& spawn,
        SDK::FStr_Passport_Weapon1 passport, WeaponClassPaths classPaths, ActorCallback onSpawned,
        ActorCallback onPreviewReady
    ) {
        preview.Destroy();

        SDK::FTransform transform{};
        if (!BuildQueuedTransform(snapshot, spawn, transform)) return false;

        LivePreviewManager* previewPtr = &preview;
        const bool snap = spawn.snapToGround;
        GameHook::QueueAction([previewPtr, passport, classPaths = std::move(classPaths),
                               onSpawned = std::move(onSpawned), onPreviewReady = std::move(onPreviewReady), transform,
                               snap](const RuntimeContextSnapshot& runtime) mutable {
            if (!runtime.world) return;
            auto* world = runtime.world;

            Spawner::LoadWeaponClasses(passport, classPaths);
            if (!EquipmentGenerator::IsPassportValid(passport)) return;
            Spawner::SpawnCustomizableFromPassport(
                world, passport, transform, snap,
                [previewPtr, world, onSpawned = std::move(onSpawned),
                 onPreviewReady = std::move(onPreviewReady)](SDK::AActor* actor) mutable {
                    FinishPreviewActor(previewPtr, world, actor, onSpawned, onPreviewReady, PrepareWeaponPreviewActor);
                }
            );
        });
        return true;
    }

    bool QueueArmorSpawn(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::FStr_Passport_Armor1 passport,
        ActorCallback onSpawned
    ) {
        SDK::FTransform transform{};
        if (!BuildQueuedTransform(snapshot, spawn, transform)) return false;

        const bool snap = spawn.snapToGround;
        GameHook::QueueAction([passport, onSpawned = std::move(onSpawned), transform,
                               snap](const RuntimeContextSnapshot& runtime) mutable {
            if (runtime.world) Spawner::SpawnArmorFromPassport(runtime.world, passport, transform, snap, onSpawned);
        });
        return true;
    }

    bool QueueArmorPreview(
        const RuntimeContextSnapshot& snapshot, LivePreviewManager& preview, const SpawnConfig& spawn,
        SDK::FStr_Passport_Armor1 passport, ActorCallback onPreviewReady
    ) {
        preview.Destroy();

        SDK::FTransform transform{};
        if (!BuildQueuedTransform(snapshot, spawn, transform)) return false;

        LivePreviewManager* previewPtr = &preview;
        const bool snap = spawn.snapToGround;
        GameHook::QueueAction([previewPtr, passport, onPreviewReady = std::move(onPreviewReady), transform,
                               snap](const RuntimeContextSnapshot& runtime) mutable {
            if (!runtime.world) return;
            auto* world = runtime.world;

            Spawner::SpawnArmorFromPassport(
                world, passport, transform, snap,
                [previewPtr, world, onPreviewReady = std::move(onPreviewReady)](SDK::AActor* actor) mutable {
                    ActorCallback onSpawned = nullptr;
                    FinishPreviewActor(previewPtr, world, actor, onSpawned, onPreviewReady, PrepareArmorPreviewActor);
                }
            );
        });
        return true;
    }
}
