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

        bool BuildSpawnTransform(
            const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::FTransform& outTransform
        ) {
            if (!snapshot.world || !snapshot.player) return false;
            outTransform = Spawner::BuildSpawnTransform(snapshot.player, spawn);
            return true;
        }

        template <typename SpawnFn>
        bool SpawnNow(const RuntimeContextSnapshot& runtime, const SpawnConfig& spawn, SpawnFn&& spawnFn) {
            SDK::FTransform transform{};
            if (!BuildSpawnTransform(runtime, spawn, transform)) return false;
            return spawnFn(runtime.world, transform, spawn.snapToGround);
        }

        template <typename SpawnFn>
        bool QueueSpawn(const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SpawnFn&& spawnFn) {
            SDK::FTransform transform{};
            if (!BuildSpawnTransform(snapshot, spawn, transform)) return false;

            const bool snap = spawn.snapToGround;
            GameHook::QueueAction([spawnFn = std::forward<SpawnFn>(spawnFn), transform,
                                   snap](const RuntimeContextSnapshot& runtime) mutable {
                spawnFn(runtime.world, transform, snap);
            });
            return true;
        }

        bool SpawnClassPathAt(
            SDK::UWorld* world, const std::string& classPath, const SDK::FTransform& transform, bool snapToGround,
            SDK::Enum_Ranks tier
        ) {
            if (!world || classPath.empty()) return false;
            Spawner::SpawnActor(world, classPath, transform, nullptr, snapToGround, tier);
            return true;
        }

        bool SpawnGeneratedCustomizableWeaponAt(
            SDK::UWorld* world, CustomizableWeapon type, const SDK::FTransform& transform, bool snapToGround,
            SDK::Enum_Ranks tier
        ) {
            if (!world) return false;
            auto passport = EquipmentGenerator::GenerateCustomizableWeapon(world, type, tier);
            if (!EquipmentGenerator::IsPassportValid(passport)) return false;
            Spawner::SpawnCustomizableFromPassport(world, passport, transform, snapToGround);
            return true;
        }

        bool SpawnRandomArmorAt(
            SDK::UWorld* world, SDK::EArmorSlots_Enum slot, const SDK::FTransform& transform, bool snapToGround,
            SDK::Enum_Ranks tier
        ) {
            if (!world) return false;
            auto passport = EquipmentGenerator::GenerateArmor(world, tier, slot, 0.5);
            if (!EquipmentGenerator::IsArmorPassportValid(passport)) return false;
            Spawner::SpawnArmorFromPassport(world, passport, transform, snapToGround);
            return true;
        }

        bool SpawnModularArmorAt(
            SDK::UWorld* world, const std::string& classPath, const std::array<int, 3>& modules,
            const SDK::FTransform& transform, bool snapToGround
        ) {
            if (!world || classPath.empty()) return false;

            auto* coreClass = Spawner::LoadClass(classPath);
            if (!coreClass) return false;

            SDK::FStr_Passport_Armor1 passport{};
            passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = coreClass;
            passport.Module1_5_46B7198E4341C93CBF6AE989EF9898E4 = modules[0];
            passport.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0 = modules[1];
            passport.Module3_9_E282C465414F6D4EF2A8039FBA847AD2 = modules[2];
            Spawner::SpawnArmorFromPassport(world, passport, transform, snapToGround);
            return true;
        }

        bool SpawnArmorWithCorePathAt(
            SDK::UWorld* world, SDK::FStr_Passport_Armor1 passport, const std::string& armorCorePath,
            const SDK::FTransform& transform, bool snapToGround, ActorCallback onSpawned
        ) {
            if (!world) return false;
            if (!armorCorePath.empty())
                passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = Spawner::LoadClass(armorCorePath);
            if (!EquipmentGenerator::IsArmorPassportValid(passport)) return false;
            Spawner::SpawnArmorFromPassport(world, passport, transform, snapToGround, std::move(onSpawned));
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
        return QueueSpawn(
            snapshot, spawn,
            [passport, classPaths = std::move(classPaths),
             onSpawned](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) mutable {
                if (!world) return;
                Spawner::LoadWeaponClasses(passport, classPaths);
                if (!EquipmentGenerator::IsPassportValid(passport)) return;
                Spawner::SpawnCustomizableFromPassport(world, passport, transform, snapToGround, onSpawned);
            }
        );
    }

    bool QueueWeaponPreview(
        const RuntimeContextSnapshot& snapshot, LivePreviewManager& preview, const SpawnConfig& spawn,
        SDK::FStr_Passport_Weapon1 passport, WeaponClassPaths classPaths, ActorCallback onSpawned,
        ActorCallback onPreviewReady
    ) {
        preview.Destroy();

        LivePreviewManager* previewPtr = &preview;
        return QueueSpawn(
            snapshot, spawn,
            [previewPtr, passport, classPaths = std::move(classPaths), onSpawned,
             onPreviewReady](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) mutable {
                if (!world) return;
                Spawner::LoadWeaponClasses(passport, classPaths);
                if (!EquipmentGenerator::IsPassportValid(passport)) return;
                Spawner::SpawnCustomizableFromPassport(
                    world, passport, transform, snapToGround,
                    [previewPtr, world, onSpawned, onPreviewReady](SDK::AActor* actor) mutable {
                        FinishPreviewActor(
                            previewPtr, world, actor, onSpawned, onPreviewReady, PrepareWeaponPreviewActor
                        );
                    }
                );
            }
        );
    }

    bool QueueArmorSpawn(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::FStr_Passport_Armor1 passport,
        ActorCallback onSpawned
    ) {
        return QueueSpawn(
            snapshot, spawn,
            [passport, onSpawned](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) {
                if (world) Spawner::SpawnArmorFromPassport(world, passport, transform, snapToGround, onSpawned);
            }
        );
    }

    bool QueueArmorSpawnWithCorePath(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::FStr_Passport_Armor1 passport,
        std::string armorCorePath, ActorCallback onSpawned
    ) {
        return QueueSpawn(
            snapshot, spawn,
            [passport, armorCorePath = std::move(armorCorePath),
             onSpawned](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) mutable {
                SpawnArmorWithCorePathAt(world, passport, armorCorePath, transform, snapToGround, onSpawned);
            }
        );
    }

    bool QueueArmorPreview(
        const RuntimeContextSnapshot& snapshot, LivePreviewManager& preview, const SpawnConfig& spawn,
        SDK::FStr_Passport_Armor1 passport, ActorCallback onPreviewReady
    ) {
        preview.Destroy();

        LivePreviewManager* previewPtr = &preview;
        return QueueSpawn(
            snapshot, spawn,
            [previewPtr, passport,
             onPreviewReady](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) mutable {
                if (!world) return;
                Spawner::SpawnArmorFromPassport(
                    world, passport, transform, snapToGround,
                    [previewPtr, world, onPreviewReady](SDK::AActor* actor) mutable {
                        ActorCallback onSpawned = nullptr;
                        FinishPreviewActor(
                            previewPtr, world, actor, onSpawned, onPreviewReady, PrepareArmorPreviewActor
                        );
                    }
                );
            }
        );
    }

    bool SpawnClassPath(
        const RuntimeContextSnapshot& runtime, const SpawnConfig& spawn, const std::string& classPath,
        SDK::Enum_Ranks tier
    ) {
        return SpawnNow(
            runtime, spawn,
            [&classPath, tier](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) {
                return SpawnClassPathAt(world, classPath, transform, snapToGround, tier);
            }
        );
    }

    bool QueueClassPathSpawn(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, std::string classPath, SDK::Enum_Ranks tier
    ) {
        return QueueSpawn(
            snapshot, spawn,
            [classPath = std::move(classPath),
             tier](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) {
                SpawnClassPathAt(world, classPath, transform, snapToGround, tier);
            }
        );
    }

    bool SpawnGeneratedCustomizableWeapon(
        const RuntimeContextSnapshot& runtime, const SpawnConfig& spawn, CustomizableWeapon type, SDK::Enum_Ranks tier
    ) {
        return SpawnNow(
            runtime, spawn,
            [type, tier](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) {
                return SpawnGeneratedCustomizableWeaponAt(world, type, transform, snapToGround, tier);
            }
        );
    }

    bool QueueGeneratedCustomizableWeaponSpawn(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, CustomizableWeapon type, SDK::Enum_Ranks tier
    ) {
        return QueueSpawn(
            snapshot, spawn,
            [type, tier](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) {
                SpawnGeneratedCustomizableWeaponAt(world, type, transform, snapToGround, tier);
            }
        );
    }

    bool SpawnRandomArmor(
        const RuntimeContextSnapshot& runtime, const SpawnConfig& spawn, SDK::EArmorSlots_Enum slot,
        SDK::Enum_Ranks tier
    ) {
        return SpawnNow(
            runtime, spawn,
            [slot, tier](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) {
                return SpawnRandomArmorAt(world, slot, transform, snapToGround, tier);
            }
        );
    }

    bool QueueRandomArmorSpawn(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::EArmorSlots_Enum slot,
        SDK::Enum_Ranks tier
    ) {
        return QueueSpawn(
            snapshot, spawn,
            [slot, tier](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) {
                SpawnRandomArmorAt(world, slot, transform, snapToGround, tier);
            }
        );
    }

    bool SpawnModularArmor(
        const RuntimeContextSnapshot& runtime, const SpawnConfig& spawn, const std::string& classPath,
        const std::array<int, 3>& modules
    ) {
        return SpawnNow(
            runtime, spawn,
            [&classPath, &modules](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) {
                return SpawnModularArmorAt(world, classPath, modules, transform, snapToGround);
            }
        );
    }

    bool QueueModularArmorSpawn(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, std::string classPath,
        std::array<int, 3> modules
    ) {
        return QueueSpawn(
            snapshot, spawn,
            [classPath = std::move(classPath),
             modules](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) {
                SpawnModularArmorAt(world, classPath, modules, transform, snapToGround);
            }
        );
    }
}
