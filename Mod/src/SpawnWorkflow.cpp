#include "Utils/SpawnWorkflow.h"

#include <memory>
#include <string_view>
#include <utility>

#include "Hooks/GameHook.h"
#include "Utils/EquipmentApplication.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/GameClass.h"
#include "Utils/GameConstants.h"
#include "Utils/NPCSpawnHelpers.h"
#include "Utils/PresetApplication.h"
#include "Utils/PresetLinkResolution.h"
#include "Utils/Spawner.h"
#include "SDK/BP_Armor_Master_classes.hpp"
#include "SDK/BP_Armor_Modular_Core_Master_classes.hpp"
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

        bool IsUsableActor(const SDK::AActor* actor) {
            return actor && SDK::UKismetSystemLibrary::IsValid(actor) && !actor->IsActorBeingDestroyed();
        }

        SpawnResult FailedSpawn(std::string error) {
            return {.error = std::move(error)};
        }

        SpawnResult SpawnedActor(SDK::AActor* actor, std::string failure) {
            if (!IsUsableActor(actor)) return FailedSpawn(std::move(failure));
            return {.success = true, .actor = actor};
        }

        void CompleteSpawn(const SpawnCompletion& completion, SpawnResult result) {
            if (completion) completion(std::move(result));
        }

        template <typename SpawnFn>
        bool QueueWithPlacement(
            const RuntimeContextSnapshot& snapshot, SpawnConfig spawn, SpawnCompletion onComplete, SpawnFn&& spawnFn
        ) {
            if (!snapshot.world || !snapshot.player) {
                CompleteSpawn(onComplete, FailedSpawn("Enter a map with an active player"));
                return false;
            }

            auto rejectedCompletion = onComplete;
            const bool queued =
                GameHook::QueueAction([spawnFn = std::forward<SpawnFn>(spawnFn), spawn,
                                       onComplete =
                                           std::move(onComplete)](const RuntimeContextSnapshot& runtime) mutable {
                    SDK::FTransform transform{};
                    if (!TryBuildSpawnTransform(runtime, spawn, transform)) {
                        CompleteSpawn(onComplete, FailedSpawn("The player is no longer available"));
                        return;
                    }
                    spawnFn(runtime, transform, spawn.snapToGround);
                });
            if (!queued) CompleteSpawn(rejectedCompletion, FailedSpawn("The spawn couldn't be started right now"));
            return queued;
        }

        SDK::AActor* SpawnArmorPassportAt(
            SDK::UWorld* world, const SDK::FStr_Passport_Armor1& passport, const SDK::FTransform& transform,
            bool snapToGround, const ActorCallback& onSpawned
        ) {
            return Spawner::SpawnArmorFromPassport(world, passport, transform, snapToGround, onSpawned);
        }

        SpawnConfig ItemSpawnConfig(const ItemSpawnPresetData& data) {
            return {
                .distanceForward = static_cast<float>(data.spawn.distanceForward),
                .distanceUp = static_cast<float>(data.spawn.distanceUp),
                .scale = static_cast<float>(data.spawn.scale),
                .snapToGround = data.spawn.snapToGround,
            };
        }

        bool ResolveItemSpawnData(ItemSpawnPresetData& data, std::string& error) {
            if (auto validation = data.ValidateForSave(); !validation) {
                error = std::move(validation.error);
                return false;
            }
            if (data.source == ItemSpawnPresetSource::WeaponPreset && !GetPresetCopy(data.weaponPreset)) {
                auto resolved = PresetLinkResolution::Resolve<WeaponPresetSerializer>(data.weaponPreset);
                if (!resolved.success || !resolved.value) {
                    error = resolved.error.empty() ? "The selected weapon preset is unavailable" : resolved.error;
                    return false;
                }
                data.weaponPreset = MakePresetCopyLink(std::move(*resolved.value));
            } else if (data.source == ItemSpawnPresetSource::ArmorPreset && !GetPresetCopy(data.armorPreset)) {
                auto resolved = PresetLinkResolution::Resolve<ArmorPresetSerializer>(data.armorPreset);
                if (!resolved.success || !resolved.value) {
                    error = resolved.error.empty() ? "The selected armor preset is unavailable" : resolved.error;
                    return false;
                }
                data.armorPreset = MakePresetCopyLink(std::move(*resolved.value));
            }
            return true;
        }

        SpawnResult SpawnWeaponPresetAt(
            SDK::UWorld* world, const ItemSpawnPresetData& data, const SDK::FTransform& transform, bool snapToGround,
            const ActorCallback& onSpawned
        );

        SpawnResult SpawnItemAt(
            SDK::UWorld* world, const ItemSpawnPresetData& data, const SDK::FTransform& transform, bool snapToGround,
            const ActorCallback& onSpawned = nullptr
        ) {
            if (!world) return FailedSpawn("Enter a map first");

            const auto tier = static_cast<SDK::Enum_Ranks>(data.tier);
            switch (data.source) {
                case ItemSpawnPresetSource::ClassPath: {
                    auto* actor = Spawner::SpawnActor(
                        world, data.classPath, transform, onSpawned, snapToGround, tier, nullptr,
                        static_cast<SDK::Enum_WeaponType_Specific>(data.weaponSpecificType)
                    );
                    return SpawnedActor(actor, "The selected item could not be spawned");
                }
                case ItemSpawnPresetSource::CustomizableWeapon: {
                    auto passport = EquipmentGenerator::GenerateCustomizableWeapon(
                        world, static_cast<CustomizableWeapon>(data.customizableWeapon), tier
                    );
                    if (!EquipmentGenerator::IsPassportValid(passport))
                        return FailedSpawn("The game couldn't create this weapon");
                    auto* actor =
                        Spawner::SpawnCustomizableFromPassport(world, passport, transform, snapToGround, onSpawned);
                    return SpawnedActor(actor, "The weapon could not be placed in the world");
                }
                case ItemSpawnPresetSource::RandomArmor: {
                    const auto slot =
                        static_cast<SDK::EArmorSlots_Enum>(GameConstants::ARMOR_SLOTS[data.armorSlotIndex].slotEnum);
                    const EquipmentGenerator::ArmorGenerationOptions options{
                        .moduleChance = static_cast<float>(data.armorGeneration.moduleChance),
                        .forceMetalMaterial = data.armorGeneration.forceMetalMaterial,
                        .steelType = EquipmentGenerator::SteelTypeFromIndex(data.armorGeneration.steelType),
                        .metalPiecesType =
                            EquipmentGenerator::SecondaryMetalTypeFromIndex(data.armorGeneration.metalPiecesType),
                    };
                    auto passport = EquipmentGenerator::GenerateArmor(world, tier, slot, options);
                    if (!EquipmentGenerator::IsArmorPassportValid(passport))
                        return FailedSpawn("The game couldn't create this armor");
                    auto* actor = SpawnArmorPassportAt(world, passport, transform, snapToGround, onSpawned);
                    return SpawnedActor(actor, "The armor could not be placed in the world");
                }
                case ItemSpawnPresetSource::ModularArmor: {
                    auto* coreClass = Spawner::LoadClass(data.classPath);
                    if (!coreClass || !coreClass->ClassDefaultObject ||
                        !GameClass::IsModularArmor(coreClass->ClassDefaultObject))
                        return FailedSpawn("The selected armor is unavailable");

                    ArmorPresetData preset;
                    preset.armorCorePath = data.classPath;
                    preset.passport =
                        static_cast<SDK::ABP_Armor_Master_C*>(coreClass->ClassDefaultObject)->Armor_Passport;
                    preset.passport.Module1_5_46B7198E4341C93CBF6AE989EF9898E4 = data.modularArmorModules[0];
                    preset.passport.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0 = data.modularArmorModules[1];
                    preset.passport.Module3_9_E282C465414F6D4EF2A8039FBA847AD2 = data.modularArmorModules[2];
                    std::string error;
                    if (!PresetApplication::MaterializeArmorPreset(preset, &error) ||
                        !EquipmentGenerator::IsArmorPassportValid(preset.passport))
                        return FailedSpawn(error.empty() ? "The selected armor is unavailable" : std::move(error));

                    auto* actor = SpawnArmorPassportAt(world, preset.passport, transform, snapToGround, onSpawned);
                    return SpawnedActor(actor, "The armor could not be placed in the world");
                }
                case ItemSpawnPresetSource::WeaponPreset:
                    return SpawnWeaponPresetAt(world, data, transform, snapToGround, onSpawned);
                case ItemSpawnPresetSource::ArmorPreset: {
                    const auto* copy = GetPresetCopy(data.armorPreset);
                    if (!copy) return FailedSpawn("The selected armor preset is incomplete");
                    auto preset = *copy;
                    std::string error;
                    if (!PresetApplication::MaterializeArmorPreset(preset, &error) ||
                        !EquipmentGenerator::IsArmorPassportValid(preset.passport))
                        return FailedSpawn(
                            error.empty() ? "The selected armor preset is unavailable" : std::move(error)
                        );

                    auto* actor = SpawnArmorPassportAt(world, preset.passport, transform, snapToGround, nullptr);
                    if (actor && !PresetApplication::ApplyArmorRuntimeOverrides(actor, preset.runtimeProps)) {
                        actor->K2_DestroyActor();
                        return FailedSpawn("Some saved armor settings could not be restored");
                    }
                    if (actor && onSpawned) onSpawned(actor);
                    return SpawnedActor(actor, "The armor could not be placed in the world");
                }
            }
            return FailedSpawn("Choose an item to spawn");
        }

        bool HasActiveNPCOverrides(const NPCOverrides& overrides) {
            NPCPresetData data{};
            data.overrides = overrides;
            const auto fields = NPCPresetData::GetPresetOverrides(data);
            for (const auto& descriptor : fields)
                if (*descriptor.field.enabled) return true;
            return false;
        }

        bool SpawnNPCAtImpl(
            SDK::UWorld* world, const NPCSpawnParams& request, int playerTeam, const SDK::FTransform& transform,
            bool snapToGround
        ) {
            if (!world || request.classPath.empty()) {
                CompleteSpawn(
                    request.onComplete,
                    FailedSpawn(world ? "The selected NPC type is unavailable" : "Enter a map first")
                );
                return false;
            }
            const bool hasOverrides = HasActiveNPCOverrides(request.overrides);
            const bool replaceGeneratedEquipment = request.loadout.has_value();

            auto preCallback = [nationality = request.nationality, tier = request.tier, mercenary = request.mercenary,
                                bodyguard = request.bodyguard, team = request.team, overrides = request.overrides,
                                hasOverrides, replaceGeneratedEquipment, playerTeam, world](SDK::AActor* actor) {
                if (!IsUsableActor(actor) || !GameClass::IsWillie(actor)) return;
                auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);

                npc->Team_Int = bodyguard ? playerTeam : team;

                auto passport = EquipmentGenerator::GenerateCharacter(world, npc->Class, nationality, tier, mercenary);
                NPCSpawnHelpers::ApplyPassportOverrides(passport, overrides);
                npc->Character_Passport = passport;

                if (replaceGeneratedEquipment) npc->Spawn_in_Pants = true;
                if (hasOverrides) NPCSpawnHelpers::ApplyPropertyOverrides(npc, overrides);
            };

            bool postSpawnCalled = false;
            auto postCallback = [world, overrides = request.overrides, hasOverrides, loadout = request.loadout,
                                 spawnScale = transform.Scale3D, completion = request.onComplete,
                                 &postSpawnCalled](SDK::AActor* actor) mutable {
                postSpawnCalled = true;
                if (!IsUsableActor(actor) || !GameClass::IsWillie(actor)) {
                    if (IsUsableActor(actor)) actor->K2_DestroyActor();
                    CompleteSpawn(completion, FailedSpawn("The selected NPC type could not be spawned"));
                    return;
                }
                auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);

                if (hasOverrides) {
                    NPCSpawnHelpers::ApplyPassportOverrides(npc->Character_Passport, overrides);
                    NPCSpawnHelpers::ApplyPropertyOverrides(npc, overrides);
                    NPCSpawnHelpers::ApplyHairColor(npc, overrides);
                }
                npc->SetActorScale3D(spawnScale);
                auto finalizeSpawn = [npc, overrides, hasOverrides, spawnScale, hasLoadout = loadout.has_value(),
                                      completion](bool success) {
                    if (!success || !IsUsableActor(npc)) {
                        if (IsUsableActor(npc)) npc->K2_DestroyActor();
                        CompleteSpawn(
                            completion,
                            FailedSpawn(
                                hasLoadout ? "The NPC's saved equipment could not be equipped, so the NPC was removed"
                                           : "The NPC could not be prepared, so it was removed"
                            )
                        );
                        return;
                    }
                    if (hasOverrides) {
                        NPCSpawnHelpers::ApplyPassportOverrides(npc->Character_Passport, overrides);
                        NPCSpawnHelpers::ApplyPropertyOverrides(npc, overrides);
                        NPCSpawnHelpers::ApplyHairColor(npc, overrides);
                    }
                    npc->SetActorScale3D(spawnScale);
                    CompleteSpawn(completion, {.success = true, .actor = npc});
                };

                std::string error;
                const bool started =
                    loadout
                        ? EquipmentApplication::ApplyNPCLoadout(
                              world, npc, std::move(*loadout), &error, std::move(finalizeSpawn)
                          )
                        : EquipmentApplication::WaitForNPCInitialization(world, npc, std::move(finalizeSpawn), &error);
                if (!started) {
                    if (IsUsableActor(npc)) npc->K2_DestroyActor();
                    CompleteSpawn(
                        completion,
                        FailedSpawn(error.empty() ? "The NPC could not be prepared, so it was removed" : error)
                    );
                }
            };

            auto* actor = Spawner::SpawnActor(
                world, request.classPath, transform, preCallback, snapToGround, Spawner::DEFAULT_SPAWN_TIER,
                postCallback
            );
            if (!postSpawnCalled) {
                if (IsUsableActor(actor)) actor->K2_DestroyActor();
                CompleteSpawn(request.onComplete, FailedSpawn("The NPC could not be spawned"));
                return false;
            }
            return IsUsableActor(actor);
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
            const LivePreviewManager::PreviewToken& token, SDK::UWorld* world, SDK::AActor* actor,
            ActorCallback& onSpawned, ActorCallback& onReady, PreviewPrepareFn prepare
        ) {
            if (!actor) return;
            if (!LivePreviewManager::IsCurrent(token)) {
                actor->K2_DestroyActor();
                return;
            }

            if (onSpawned) onSpawned(actor);
            if (!LivePreviewManager::IsCurrent(token)) {
                actor->K2_DestroyActor();
                return;
            }
            prepare(actor);
            if (onReady) onReady(actor);
            if (!LivePreviewManager::SetPreviewActor(token, actor, world)) actor->K2_DestroyActor();
        }

        template <typename ActorCallbackFn>
        SDK::AActor* SpawnWeaponPassportAt(
            SDK::UWorld* world, SDK::FStr_Passport_Weapon1& passport, const WeaponClassPaths& classPaths,
            const SDK::FTransform& transform, bool snapToGround, ActorCallbackFn&& onSpawned,
            std::string_view deferredWeaponName = {}, std::string* error = nullptr
        ) {
            if (!world) {
                if (error) *error = "Enter a map first";
                return nullptr;
            }
            WeaponPresetData preset;
            preset.passport = passport;
            preset.classPaths = classPaths;
            preset.deferredWeaponName = std::string(deferredWeaponName);
            if (!PresetApplication::MaterializeWeaponPreset(preset, error)) return nullptr;
            passport = preset.passport;
            auto* actor = Spawner::SpawnCustomizableFromPassport(
                world, passport, transform, snapToGround, std::forward<ActorCallbackFn>(onSpawned)
            );
            if (!actor && error && error->empty()) *error = "The weapon could not be placed in the world";
            return actor;
        }

        SpawnResult SpawnWeaponPresetAt(
            SDK::UWorld* world, const ItemSpawnPresetData& data, const SDK::FTransform& transform, bool snapToGround,
            const ActorCallback& onSpawned
        ) {
            const auto* copy = GetPresetCopy(data.weaponPreset);
            if (!copy) return FailedSpawn("The selected weapon preset is incomplete");

            auto preset = *copy;
            bool overridesApplied = true;
            std::string error;
            auto* actor = SpawnWeaponPassportAt(
                world, preset.passport, preset.classPaths, transform, snapToGround,
                [&preset, &onSpawned, &overridesApplied](SDK::AActor* spawned) {
                    overridesApplied = PresetApplication::ApplyWeaponMeshOverrides(spawned, preset.meshPresets) &&
                                       PresetApplication::ApplyWeaponRuntimeOverrides(spawned, preset.runtimeProps);
                    if (overridesApplied && onSpawned) onSpawned(spawned);
                },
                preset.deferredWeaponName, &error
            );
            if (actor && !overridesApplied) {
                actor->K2_DestroyActor();
                return FailedSpawn("Some saved weapon settings could not be restored");
            }
            return SpawnedActor(
                actor, error.empty() ? "The weapon could not be placed in the world" : std::move(error)
            );
        }
    }

    bool QueueWeaponSpawn(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::FStr_Passport_Weapon1 passport,
        WeaponClassPaths classPaths, ActorSetup setupActor, std::string deferredWeaponName, SpawnCompletion onComplete
    ) {
        auto terminalCompletion = onComplete;
        return QueueWithPlacement(
            snapshot, spawn, std::move(onComplete),
            [passport, classPaths = std::move(classPaths), setupActor = std::move(setupActor),
             deferredWeaponName = std::move(deferredWeaponName), completion = std::move(terminalCompletion)](
                const RuntimeContextSnapshot& runtime, const SDK::FTransform& transform, bool snapToGround
            ) mutable {
                std::string error;
                std::string setupError;
                bool setupCompleted = !setupActor;
                auto* actor = SpawnWeaponPassportAt(
                    runtime.world, passport, classPaths, transform, snapToGround,
                    [&setupActor, &setupCompleted, &setupError](SDK::AActor* spawnedActor) {
                        if (!setupActor) return;
                        auto result = setupActor(spawnedActor);
                        setupCompleted = true;
                        if (!result)
                            setupError =
                                result.error().empty() ? "The weapon could not be fully prepared" : result.error();
                    },
                    deferredWeaponName, &error
                );
                if (actor && (!setupCompleted || !setupError.empty())) {
                    if (IsUsableActor(actor)) actor->K2_DestroyActor();
                    CompleteSpawn(
                        completion,
                        FailedSpawn(
                            setupError.empty() ? "The weapon could not be fully prepared" : std::move(setupError)
                        )
                    );
                    return;
                }
                if (error.empty()) error = "The weapon could not be fully prepared";
                CompleteSpawn(completion, SpawnedActor(actor, std::move(error)));
            }
        );
    }

    bool QueueWeaponPreview(
        const RuntimeContextSnapshot& snapshot, LivePreviewManager& preview, const SpawnConfig& spawn,
        SDK::FStr_Passport_Weapon1 passport, WeaponClassPaths classPaths, ActorCallback onSpawned,
        ActorCallback onPreviewReady, std::string deferredWeaponName
    ) {
        auto token = preview.BeginPreview();

        return QueueWithPlacement(
            snapshot, spawn, nullptr,
            [token, passport, classPaths = std::move(classPaths), onSpawned, onPreviewReady,
             deferredWeaponName = std::move(
                 deferredWeaponName
             )](const RuntimeContextSnapshot& runtime, const SDK::FTransform& transform, bool snapToGround) mutable {
                auto* world = runtime.world;
                SpawnWeaponPassportAt(
                    world, passport, classPaths, transform, snapToGround,
                    [token, world, onSpawned, onPreviewReady](SDK::AActor* actor) mutable {
                        FinishPreviewActor(token, world, actor, onSpawned, onPreviewReady, PrepareWeaponPreviewActor);
                    },
                    deferredWeaponName
                );
            }
        );
    }

    bool QueueArmorPreview(
        const RuntimeContextSnapshot& snapshot, LivePreviewManager& preview, const SpawnConfig& spawn,
        ArmorPresetData preset, ActorCallback onPreviewReady
    ) {
        auto token = preview.BeginPreview();

        return QueueWithPlacement(
            snapshot, spawn, nullptr,
            [token, preset = std::move(preset), onPreviewReady](
                const RuntimeContextSnapshot& runtime, const SDK::FTransform& transform, bool snapToGround
            ) mutable {
                auto* world = runtime.world;
                if (!world) return;
                if (!PresetApplication::MaterializeArmorPreset(preset)) return;
                Spawner::SpawnArmorFromPassport(
                    world, preset.passport, transform, snapToGround,
                    [token, world, onPreviewReady](SDK::AActor* actor) mutable {
                        ActorCallback onSpawned = nullptr;
                        FinishPreviewActor(token, world, actor, onSpawned, onPreviewReady, PrepareArmorPreviewActor);
                    }
                );
            }
        );
    }

    std::expected<NPCSpawnParams, std::string> BuildNPCSpawnParams(
        const NPCPresetData& preset, std::optional<ResolvedLoadoutPresetData> loadout
    ) {
        if (auto validation = preset.ValidateForSave(); !validation)
            return std::unexpected(std::move(validation.error));
        const auto& type = NPCPresetData::K_TYPES[static_cast<std::size_t>(preset.npcTypeIndex)];

        return NPCSpawnParams{
            .classPath = type.classPath,
            .nationality = static_cast<SDK::Enum_Nationalities>(preset.nationality),
            .tier = static_cast<SDK::Enum_Ranks>(preset.tier),
            .mercenary = preset.mercenary,
            .bodyguard = preset.bodyguard,
            .team = preset.team,
            .overrides = preset.overrides,
            .loadout = std::move(loadout),
        };
    }

    bool SpawnItemPreset(
        const RuntimeContextSnapshot& runtime, const ItemSpawnPresetData& data, const SpawnCompletion& onComplete,
        std::string* error
    ) {
        auto resolved = data;
        std::string buildError;
        if (!ResolveItemSpawnData(resolved, buildError)) {
            if (error) *error = buildError;
            CompleteSpawn(onComplete, FailedSpawn(std::move(buildError)));
            return false;
        }

        const auto spawn = ItemSpawnConfig(resolved);
        SDK::FTransform transform{};
        if (!TryBuildSpawnTransform(runtime, spawn, transform)) {
            CompleteSpawn(onComplete, FailedSpawn("The player is no longer available"));
            return false;
        }
        auto result = SpawnItemAt(runtime.world, resolved, transform, spawn.snapToGround);
        const bool success = result.success;
        CompleteSpawn(onComplete, std::move(result));
        return success;
    }

    bool QueueItemPresetSpawn(
        const RuntimeContextSnapshot& snapshot, const ItemSpawnPresetData& data, SpawnCompletion onComplete,
        std::string* error
    ) {
        auto resolved = data;
        std::string buildError;
        if (!ResolveItemSpawnData(resolved, buildError)) {
            if (error) *error = buildError;
            CompleteSpawn(onComplete, FailedSpawn(std::move(buildError)));
            return false;
        }
        auto terminalCompletion = onComplete;
        const auto spawn = ItemSpawnConfig(resolved);
        if (QueueWithPlacement(
                snapshot, spawn, std::move(onComplete),
                [data = std::move(resolved), completion = std::move(terminalCompletion)](
                    const RuntimeContextSnapshot& runtime, const SDK::FTransform& transform, bool snapToGround
                ) mutable { CompleteSpawn(completion, SpawnItemAt(runtime.world, data, transform, snapToGround)); }
            ))
            return true;
        if (error) *error = "The item couldn't be spawned right now";
        return false;
    }

    bool SpawnNPCAt(
        SDK::UWorld* world, int playerTeam, const SDK::FTransform& transform, bool snapToGround,
        const NPCSpawnParams& request
    ) {
        return SpawnNPCAtImpl(world, request, playerTeam, transform, snapToGround);
    }

    bool SpawnNPC(const RuntimeContextSnapshot& runtime, const SpawnConfig& spawn, const NPCSpawnParams& request) {
        SDK::FTransform transform{};
        if (!TryBuildSpawnTransform(runtime, spawn, transform) || !runtime.player) {
            CompleteSpawn(request.onComplete, FailedSpawn("The player is no longer available"));
            return false;
        }
        return SpawnNPCAt(runtime.world, runtime.player->Team_Int, transform, spawn.snapToGround, request);
    }

    bool QueueNPCSpawn(const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, NPCSpawnParams request) {
        auto completion = request.onComplete;

        return QueueWithPlacement(
            snapshot, spawn, completion,
            [request = std::move(
                 request
             )](const RuntimeContextSnapshot& runtime, const SDK::FTransform& transform, bool snapToGround) {
                if (!runtime.player) {
                    CompleteSpawn(request.onComplete, FailedSpawn("The player is no longer available"));
                    return;
                }
                SpawnNPCAt(runtime.world, runtime.player->Team_Int, transform, snapToGround, request);
            }
        );
    }
}
