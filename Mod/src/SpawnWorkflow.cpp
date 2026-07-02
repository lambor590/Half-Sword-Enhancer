#include "Utils/SpawnWorkflow.h"

#include <utility>

#include "Hooks/GameHook.h"
#include "Utils/EquipmentApplication.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/NPCSpawnHelpers.h"
#include "Utils/Spawner.h"
#include "SDK/BP_Armor_Master_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"

namespace SpawnWorkflow {

    namespace {
        using PreviewPrepareFn = void (*)(SDK::AActor*);

        bool TryBuildSpawnTransform(
            const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::FTransform& outTransform
        ) {
            if (!snapshot.world || !snapshot.player) return false;
            outTransform = Spawner::BuildSpawnTransform(snapshot.player, spawn);
            return true;
        }

        template <typename SpawnFn>
        bool RunWithPlacement(const RuntimeContextSnapshot& runtime, const SpawnConfig& spawn, SpawnFn&& spawnFn) {
            SDK::FTransform transform{};
            if (!TryBuildSpawnTransform(runtime, spawn, transform)) return false;
            return spawnFn(runtime, transform, spawn.snapToGround);
        }

        template <typename SpawnFn>
        bool QueueWithPlacement(const RuntimeContextSnapshot& snapshot, SpawnConfig spawn, SpawnFn&& spawnFn) {
            if (!snapshot.world || !snapshot.player) return false;

            GameHook::QueueAction([spawnFn = std::forward<SpawnFn>(spawnFn),
                                   spawn](const RuntimeContextSnapshot& runtime) mutable {
                SDK::FTransform transform{};
                if (!TryBuildSpawnTransform(runtime, spawn, transform)) return;
                spawnFn(runtime, transform, spawn.snapToGround);
            });
            return true;
        }

        bool SpawnItemAt(
            SDK::UWorld* world, const ItemSpawnRequest& request, const SDK::FTransform& transform, bool snapToGround
        ) {
            if (!world) return false;

            switch (request.kind) {
                case ItemSpawnRequest::Kind::ClassPath: {
                    if (request.classPath.empty()) return false;
                    Spawner::SpawnActor(
                        world, request.classPath, transform, nullptr, snapToGround, request.tier, nullptr,
                        request.weaponSpecificType
                    );
                    return true;
                }
                case ItemSpawnRequest::Kind::GeneratedCustomizableWeapon: {
                    auto passport =
                        EquipmentGenerator::GenerateCustomizableWeapon(world, request.customizable, request.tier);
                    if (!EquipmentGenerator::IsPassportValid(passport)) return false;
                    Spawner::SpawnCustomizableFromPassport(world, passport, transform, snapToGround);
                    return true;
                }
                case ItemSpawnRequest::Kind::RandomArmor: {
                    auto passport =
                        EquipmentGenerator::GenerateArmor(world, request.tier, request.armorSlot, request.armorOptions);
                    if (!EquipmentGenerator::IsArmorPassportValid(passport)) return false;
                    Spawner::SpawnArmorFromPassport(world, passport, transform, snapToGround);
                    return true;
                }
                case ItemSpawnRequest::Kind::ModularArmor: {
                    if (request.classPath.empty()) return false;

                    auto* coreClass = Spawner::LoadClass(request.classPath);
                    if (!coreClass) return false;

                    SDK::FStr_Passport_Armor1 passport{};
                    passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = coreClass;
                    passport.Module1_5_46B7198E4341C93CBF6AE989EF9898E4 = request.modules[0];
                    passport.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0 = request.modules[1];
                    passport.Module3_9_E282C465414F6D4EF2A8039FBA847AD2 = request.modules[2];
                    Spawner::SpawnArmorFromPassport(world, passport, transform, snapToGround);
                    return true;
                }
                case ItemSpawnRequest::Kind::ArmorPreset: {
                    auto passport = request.armorPassport;
                    if (!request.armorCorePath.empty())
                        passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 =
                            Spawner::LoadClass(request.armorCorePath);
                    if (!EquipmentGenerator::IsArmorPassportValid(passport)) return false;
                    Spawner::SpawnArmorFromPassport(world, passport, transform, snapToGround);
                    return true;
                }
            }
            return false;
        }

        bool HasActiveNPCOverrides(const NPCOverrides& overrides) {
            NPCPresetData data{};
            data.overrides = overrides;
            for (const auto& group : NPCPresetData::GetOverrideGroups(data)) {
                if (CountActive(group.fields) > 0) return true;
            }
            return false;
        }

        SpawnConfig BuildNPCSpawnConfig(const SpawnConfig& spawn, const NPCOverrides& overrides) {
            SpawnConfig adjusted = spawn;
            if (overrides.heightRate.enabled) {
                adjusted.scale = static_cast<float>(0.875 + overrides.heightRate.value * 0.125);
            }
            return adjusted;
        }

        bool SpawnNPCAt(
            SDK::UWorld* world, const NPCSpawnRequest& request, int playerTeam, const SDK::FTransform& transform,
            bool snapToGround
        ) {
            if (!world || request.classPath.empty()) return false;

            const bool hasOverrides = HasActiveNPCOverrides(request.overrides);
            const bool hasLoadout = request.hasLoadout;

            auto preCallback = [nationality = request.nationality, tier = request.tier, mercenary = request.mercenary,
                                bodyguard = request.bodyguard, team = request.team, overrides = request.overrides,
                                hasOverrides, hasLoadout, playerTeam, world](SDK::AActor* actor) {
                auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);
                if (!npc) return;

                npc->Team_Int = bodyguard ? playerTeam : team;

                auto passport = EquipmentGenerator::GenerateCharacter(world, npc->Class, nationality, tier, mercenary);
                NPCSpawnHelpers::ApplyPassportOverrides(passport, overrides);
                npc->Character_Passport = passport;

                if (hasLoadout) npc->Spawn_in_Pants = true;
                if (hasOverrides) NPCSpawnHelpers::ApplyPropertyOverrides(npc, overrides);
            };

            auto postCallback = [world, overrides = request.overrides, hasOverrides, hasLoadout,
                                 loadout = request.loadout](SDK::AActor* actor) {
                auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);
                if (!npc) return;

                if (hasOverrides) NPCSpawnHelpers::ApplyHairColor(npc, overrides);
                if (hasLoadout) EquipmentApplication::ApplyNPCLoadoutNow(world, npc, loadout);
            };

            Spawner::SpawnActor(
                world, request.classPath, transform, preCallback, snapToGround, Spawner::DEFAULT_SPAWN_TIER,
                postCallback
            );
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
            const LivePreviewManager::PreviewRequestToken& token, SDK::UWorld* world, SDK::AActor* actor,
            ActorCallback& onSpawned, ActorCallback& onReady, PreviewPrepareFn prepare
        ) {
            if (!actor) return;
            if (!LivePreviewManager::IsRequestCurrent(token)) {
                actor->K2_DestroyActor();
                return;
            }

            if (onSpawned) onSpawned(actor);
            if (!LivePreviewManager::IsRequestCurrent(token)) {
                actor->K2_DestroyActor();
                return;
            }
            prepare(actor);
            if (onReady) onReady(actor);
            if (!LivePreviewManager::SetPreviewActor(token, actor, world)) actor->K2_DestroyActor();
        }

        template <typename ActorCallbackFn>
        bool SpawnWeaponPassportAt(
            SDK::UWorld* world, SDK::FStr_Passport_Weapon1& passport, const WeaponClassPaths& classPaths,
            const SDK::FTransform& transform, bool snapToGround, ActorCallbackFn&& onSpawned
        ) {
            if (!world) return false;
            EquipmentApplication::ResolveWeaponPassportClasses(passport, classPaths);
            if (!EquipmentGenerator::IsPassportValid(passport)) return false;
            Spawner::SpawnCustomizableFromPassport(
                world, passport, transform, snapToGround, std::forward<ActorCallbackFn>(onSpawned)
            );
            return true;
        }
    }

    bool QueueWeaponSpawn(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::FStr_Passport_Weapon1 passport,
        WeaponClassPaths classPaths, ActorCallback onSpawned
    ) {
        return QueueWithPlacement(
            snapshot, spawn,
            [passport, classPaths = std::move(classPaths), onSpawned = std::move(onSpawned)](
                const RuntimeContextSnapshot& runtime, const SDK::FTransform& transform, bool snapToGround
            ) mutable {
                SpawnWeaponPassportAt(runtime.world, passport, classPaths, transform, snapToGround, onSpawned);
            }
        );
    }

    bool QueueWeaponPreview(
        const RuntimeContextSnapshot& snapshot, LivePreviewManager& preview, const SpawnConfig& spawn,
        SDK::FStr_Passport_Weapon1 passport, WeaponClassPaths classPaths, ActorCallback onSpawned,
        ActorCallback onPreviewReady
    ) {
        auto token = preview.BeginSpawnRequest();

        return QueueWithPlacement(
            snapshot, spawn,
            [token, passport, classPaths = std::move(classPaths), onSpawned,
             onPreviewReady](
                const RuntimeContextSnapshot& runtime, const SDK::FTransform& transform, bool snapToGround
            ) mutable {
                auto* world = runtime.world;
                SpawnWeaponPassportAt(
                    world, passport, classPaths, transform, snapToGround,
                    [token, world, onSpawned, onPreviewReady](SDK::AActor* actor) mutable {
                        FinishPreviewActor(
                            token, world, actor, onSpawned, onPreviewReady, PrepareWeaponPreviewActor
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
        return QueueWithPlacement(
            snapshot, spawn,
            [passport,
            onSpawned = std::move(onSpawned)](
                const RuntimeContextSnapshot& runtime, const SDK::FTransform& transform, bool snapToGround
            ) {
                if (runtime.world) {
                    Spawner::SpawnArmorFromPassport(runtime.world, passport, transform, snapToGround, onSpawned);
                }
            }
        );
    }

    bool QueueArmorPreview(
        const RuntimeContextSnapshot& snapshot, LivePreviewManager& preview, const SpawnConfig& spawn,
        SDK::FStr_Passport_Armor1 passport, ActorCallback onPreviewReady
    ) {
        auto token = preview.BeginSpawnRequest();

        return QueueWithPlacement(
            snapshot, spawn,
            [token, passport,
             onPreviewReady](
                const RuntimeContextSnapshot& runtime, const SDK::FTransform& transform, bool snapToGround
            ) mutable {
                auto* world = runtime.world;
                if (!world) return;
                Spawner::SpawnArmorFromPassport(
                    world, passport, transform, snapToGround,
                    [token, world, onPreviewReady](SDK::AActor* actor) mutable {
                        ActorCallback onSpawned = nullptr;
                        FinishPreviewActor(
                            token, world, actor, onSpawned, onPreviewReady, PrepareArmorPreviewActor
                        );
                    }
                );
            }
        );
    }

    bool SpawnItem(const RuntimeContextSnapshot& runtime, const SpawnConfig& spawn, const ItemSpawnRequest& request) {
        return RunWithPlacement(
            runtime, spawn,
            [&request](
                const RuntimeContextSnapshot& runtime, const SDK::FTransform& transform, bool snapToGround
            ) {
                return SpawnItemAt(runtime.world, request, transform, snapToGround);
            }
        );
    }

    bool QueueItemSpawn(const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, ItemSpawnRequest request) {
        return QueueWithPlacement(
            snapshot, spawn,
            [request = std::move(request)](
                const RuntimeContextSnapshot& runtime, const SDK::FTransform& transform, bool snapToGround
            ) {
                SpawnItemAt(runtime.world, request, transform, snapToGround);
            }
        );
    }

    bool SpawnNPC(const RuntimeContextSnapshot& runtime, const SpawnConfig& spawn, const NPCSpawnRequest& request) {
        auto adjustedSpawn = BuildNPCSpawnConfig(spawn, request.overrides);

        return RunWithPlacement(
            runtime, adjustedSpawn,
            [&request](
                const RuntimeContextSnapshot& runtime, const SDK::FTransform& transform, bool snapToGround
            ) {
                if (!runtime.player) return false;
                return SpawnNPCAt(runtime.world, request, runtime.player->Team_Int, transform, snapToGround);
            }
        );
    }

    bool QueueNPCSpawn(const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, NPCSpawnRequest request) {
        auto adjustedSpawn = BuildNPCSpawnConfig(spawn, request.overrides);

        return QueueWithPlacement(
            snapshot, adjustedSpawn,
            [request = std::move(request)](
                const RuntimeContextSnapshot& runtime, const SDK::FTransform& transform, bool snapToGround
            ) {
                if (!runtime.player) return;
                SpawnNPCAt(runtime.world, request, runtime.player->Team_Int, transform, snapToGround);
            }
        );
    }
}
