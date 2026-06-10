#include "Utils/SpawnWorkflow.h"

#include <utility>

#include "Hooks/GameHook.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/NPCSpawnHelpers.h"
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

        bool SpawnItemAt(
            SDK::UWorld* world, const ItemSpawnRequest& request, const SDK::FTransform& transform, bool snapToGround
        ) {
            if (!world) return false;

            switch (request.kind) {
                case ItemSpawnRequest::Kind::ClassPath: {
                    if (request.classPath.empty()) return false;
                    Spawner::SpawnActor(world, request.classPath, transform, nullptr, snapToGround, request.tier);
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
                    auto passport = EquipmentGenerator::GenerateArmor(world, request.tier, request.armorSlot, 0.5);
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

                NPCSpawnHelpers::ApplyAIFearlessOverride(npc, overrides);
                if (hasOverrides) NPCSpawnHelpers::ApplyHairColor(npc, overrides);
                if (hasLoadout) NPCSpawnHelpers::ApplyNPCLoadout(world, npc, loadout);
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

    ItemSpawnRequest ItemSpawnRequest::ClassPath(std::string classPath, SDK::Enum_Ranks tier) {
        ItemSpawnRequest request{};
        request.kind = Kind::ClassPath;
        request.classPath = std::move(classPath);
        request.tier = tier;
        return request;
    }

    ItemSpawnRequest ItemSpawnRequest::GeneratedCustomizableWeapon(CustomizableWeapon type, SDK::Enum_Ranks tier) {
        ItemSpawnRequest request{};
        request.kind = Kind::GeneratedCustomizableWeapon;
        request.customizable = type;
        request.tier = tier;
        return request;
    }

    ItemSpawnRequest ItemSpawnRequest::RandomArmor(SDK::EArmorSlots_Enum slot, SDK::Enum_Ranks tier) {
        ItemSpawnRequest request{};
        request.kind = Kind::RandomArmor;
        request.armorSlot = slot;
        request.tier = tier;
        return request;
    }

    ItemSpawnRequest ItemSpawnRequest::ModularArmor(std::string classPath, std::array<int, 3> modules) {
        ItemSpawnRequest request{};
        request.kind = Kind::ModularArmor;
        request.classPath = std::move(classPath);
        request.modules = modules;
        return request;
    }

    ItemSpawnRequest ItemSpawnRequest::ArmorPreset(SDK::FStr_Passport_Armor1 passport, std::string armorCorePath) {
        ItemSpawnRequest request{};
        request.kind = Kind::ArmorPreset;
        request.armorPassport = passport;
        request.armorCorePath = std::move(armorCorePath);
        return request;
    }

    bool SpawnItem(const RuntimeContextSnapshot& runtime, const SpawnConfig& spawn, const ItemSpawnRequest& request) {
        return SpawnNow(
            runtime, spawn,
            [&request](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) {
                return SpawnItemAt(world, request, transform, snapToGround);
            }
        );
    }

    bool QueueItemSpawn(const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, ItemSpawnRequest request) {
        return QueueSpawn(
            snapshot, spawn,
            [request = std::move(request)](SDK::UWorld* world, const SDK::FTransform& transform, bool snapToGround) {
                SpawnItemAt(world, request, transform, snapToGround);
            }
        );
    }

    bool SpawnNPC(const RuntimeContextSnapshot& runtime, const SpawnConfig& spawn, const NPCSpawnRequest& request) {
        auto adjustedSpawn = BuildNPCSpawnConfig(spawn, request.overrides);

        SDK::FTransform transform{};
        if (!BuildSpawnTransform(runtime, adjustedSpawn, transform)) return false;

        return SpawnNPCAt(runtime.world, request, runtime.player->Team_Int, transform, adjustedSpawn.snapToGround);
    }

    bool QueueNPCSpawn(const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, NPCSpawnRequest request) {
        auto adjustedSpawn = BuildNPCSpawnConfig(spawn, request.overrides);

        SDK::FTransform transform{};
        if (!BuildSpawnTransform(snapshot, adjustedSpawn, transform)) return false;

        const int playerTeam = snapshot.player->Team_Int;
        const bool snap = adjustedSpawn.snapToGround;
        GameHook::QueueAction([request = std::move(request), transform, playerTeam,
                               snap](const RuntimeContextSnapshot& runtime) {
            SpawnNPCAt(runtime.world, request, playerTeam, transform, snap);
        });
        return true;
    }
}
