#include "Menu/Sections/World/WorldActionsSection.h"

#include <unordered_set>

#include "SDK/BP_Armor_Master_classes.hpp"
#include "SDK/BP_BloodDecal_classes.hpp"
#include "SDK/BP_MeshBloodSim_classes.hpp"
#include "SDK/Blood_BP_P4_classes.hpp"
#include "SDK/Blood_BP_PT_classes.hpp"
#include "SDK/HSComputeShaders_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/RunningBlood_BP_classes.hpp"
#include "Utils/ActorUtils.h"
#include "Utils/GameConstants.h"

namespace {
    constexpr SDK::FLinearColor CLEAR_COLOR{0.0f, 0.0f, 0.0f, 0.0f};
    constexpr SDK::FLinearColor DEFAULT_VERTEX_COLOR{1.0f, 1.0f, 1.0f, 1.0f};

    template <typename... ActorTypes> void DestroyAllActors(SDK::UWorld* world) {
        const auto destroy = [](auto* actor) {
            actor->K2_DestroyActor();
        };
        (ActorUtils::ForEachObjectOfType<ActorTypes>(world, destroy), ...);
    }

    void ClearBlood(SDK::UWorld* world) {
        ActorUtils::ForEachWillie(world, nullptr, [](SDK::AWillie_BP_C* willie) {
            willie->Reset_Blood_Bleed();
            willie->Reset_Blood_Splash();
            willie->Reset_Trail_Blood(0.0f);
            willie->BloodyFoot_R = 0;
            willie->BloodyFoot_L = 0;
            if (willie->Mesh) willie->Mesh->ClearVertexColorOverride(0);
            if (willie->CharacterMesh_Head) willie->CharacterMesh_Head->ClearVertexColorOverride(0);
        });

        ActorUtils::ForEachObjectOfType<SDK::ABP_MeshBloodSim_C>(world, [world](SDK::ABP_MeshBloodSim_C* sim) {
            if (sim->MainRenderTarget) {
                SDK::UKismetRenderingLibrary::ClearRenderTarget2D(world, sim->MainRenderTarget, CLEAR_COLOR);
            }
            if (sim->MeshToSimOn) {
                SDK::UMeshVertexPainterKismetLibrary::PaintVerticesSingleColor(
                    sim->MeshToSimOn, DEFAULT_VERTEX_COLOR, false
                );
            }
            sim->RemainingSimulationTime = 0.0;
        });

        ActorUtils::ForEachObjectOfType<SDK::ACSBloodSimActor>(world, [world](SDK::ACSBloodSimActor* sim) {
            if (sim->CurrentWrite) {
                SDK::UKismetRenderingLibrary::ClearRenderTarget2D(world, sim->CurrentWrite, CLEAR_COLOR);
            }
            if (sim->CurrentRead) {
                SDK::UKismetRenderingLibrary::ClearRenderTarget2D(world, sim->CurrentRead, CLEAR_COLOR);
            }
            if (sim->BoundMesh) {
                SDK::UMeshVertexPainterKismetLibrary::PaintVerticesSingleColor(
                    sim->BoundMesh, DEFAULT_VERTEX_COLOR, false
                );
            }
        });

        DestroyAllActors<SDK::ABlood_BP_P4_C, SDK::ABlood_BP_PT_C>(world);
        DestroyAllActors<SDK::ABP_BloodDecal_C, SDK::ARunningBlood_BP_C, SDK::AHSWoundsController>(world);
    }

    void ClearDroppedObjects(SDK::UWorld* world, SDK::AWillie_BP_C* player, float configuredRadius) {
        std::unordered_set<SDK::AActor*> carried;
        ActorUtils::ForEachWillie(world, nullptr, [&](SDK::AWillie_BP_C* willie) {
            for (auto* weapon :
                 {willie->Weapon_R, willie->Weapon_L, willie->Weapon_Slot_R_1, willie->Weapon_Slot_R_2,
                  willie->Weapon_Slot_L_1, willie->Weapon_Slot_L_2, willie->Weapon_Slot_Back})
                if (weapon) carried.insert(weapon);
            for (auto* armor : {willie->Carried_Armor_R_Hand, willie->Carried_Armor_L_Hand})
                if (armor) carried.insert(armor);
            for (auto* component : {willie->Grab_Component_R, willie->Grab_Component_L})
                if (SDK::UKismetSystemLibrary::IsValid(component)) carried.insert(component->GetOwner());
        });
        const auto origin = player->K2_GetActorLocation();
        const double radiusSquared = static_cast<double>(configuredRadius) * configuredRadius;
        const auto shouldDestroy = [&](SDK::AActor* object) {
            return !carried.contains(object) &&
                   ActorUtils::DistanceSquared(origin, object->K2_GetActorLocation()) <= radiusSquared;
        };
        ActorUtils::ForEachObjectOfType<SDK::AModularWeaponBP_C>(world, [&](auto* object) {
            if (!object->Is_Held && !object->Sheathed && shouldDestroy(object)) object->K2_DestroyActor();
        });
        ActorUtils::ForEachObjectOfType<SDK::ABP_Armor_Master_C>(world, [&](auto* object) {
            if (shouldDestroy(object)) object->K2_DestroyActor();
        });
    }
}

WorldActionsSection::WorldActionsSection(ModContext& ctx) : Section(ctx, SECTION) {
    InitKeybinds();
}

void WorldActionsSection::Render() {
    keybinds.Render();
}

void WorldActionsSection::SyncStateWorld(SDK::UWorld* world) noexcept {
    if (stateWorld.load(std::memory_order_acquire) == world) return;
    slowMotionActive.store(false, std::memory_order_release);
    customGravityActive.store(false, std::memory_order_release);
    paused.store(false, std::memory_order_release);
    enemyAIStopped.store(false, std::memory_order_release);
    stoppedControllers.clear();
    GameHook::Get().Unsubscribe(pauseTick.hook);
    pauseTick = {};
    stateWorld.store(world, std::memory_order_release);
}

bool WorldActionsSection::CurrentWorldState(const std::atomic_bool& state) const noexcept {
    auto* world = RenderWorld();
    return world && stateWorld.load(std::memory_order_acquire) == world && state.load(std::memory_order_acquire);
}

void WorldActionsSection::RestorePauseTick() {
    GameHook::Get().Unsubscribe(pauseTick.hook);
    if (pauseTick.actor && SDK::UObject::GObjects->GetByIndex(pauseTick.objectIndex) == pauseTick.actor &&
        SDK::UKismetSystemLibrary::IsValid(pauseTick.actor) && !pauseTick.actor->IsActorBeingDestroyed()) {
        pauseTick.actor->SetActorTickInterval(pauseTick.tickInterval);
        pauseTick.actor->SetTickableWhenPaused(pauseTick.tickWhenPaused);
        pauseTick.actor->SetActorTickEnabled(pauseTick.tickEnabled);
    }
    pauseTick = {};
}

void WorldActionsSection::InitKeybinds() {
    keybinds.Add({
        .name = "Toggle Slow Motion",
        .configSection = "ToggleSlowMotion",
        .keyPtr = &cfg.sloMoKey,
        .callback =
            [this](bool active, const RuntimeContextSnapshot& runtime) {
                if (!runtime.world || !runtime.worldSettings) return;
                SyncStateWorld(runtime.world);
                if (active && !slowMotionActive.load(std::memory_order_acquire))
                    originalTimeDilation = SDK::UGameplayStatics::GetGlobalTimeDilation(runtime.world);
                if (active || slowMotionActive.load(std::memory_order_acquire))
                    SDK::UGameplayStatics::SetGlobalTimeDilation(
                        runtime.world, active ? cfg.slowMotionSpeed : originalTimeDilation
                    );
                slowMotionActive.store(active, std::memory_order_release);
            },
        .kind = KeybindKind::State,
        .stateGetter = [this]() { return CurrentWorldState(slowMotionActive); },
        .available = [this]() { return RenderSnapshot().worldSettings != nullptr; },
        .applyOnToggle = true,
        .params = {KeybindParam(
            "speed", "Game Speed", &cfg.slowMotionSpeed, 0.01f, 0.99f,
            "How quickly the game moves while Slow Motion is active"
        )},
        .group = "Game Speed & Gravity",
    });

    keybinds.Add({
        .name = "Toggle Custom Gravity",
        .configSection = "ToggleCustomGravity",
        .keyPtr = &cfg.customGravityKey,
        .callback =
            [this](bool active, const RuntimeContextSnapshot& runtime) {
                auto* worldSettings = runtime.worldSettings;
                if (!runtime.world || !worldSettings) return;
                SyncStateWorld(runtime.world);
                if (active) {
                    if (!customGravityActive.load(std::memory_order_acquire)) {
                        originalGravity = worldSettings->WorldGravityZ;
                        originalGravitySet = worldSettings->bWorldGravitySet;
                    }
                    worldSettings->bWorldGravitySet = true;
                    worldSettings->WorldGravityZ = cfg.customGravityValue;
                } else if (customGravityActive.load(std::memory_order_acquire)) {
                    worldSettings->WorldGravityZ = originalGravity;
                    worldSettings->bWorldGravitySet = originalGravitySet;
                }
                customGravityActive.store(active, std::memory_order_release);
            },
        .kind = KeybindKind::State,
        .stateGetter = [this]() { return CurrentWorldState(customGravityActive); },
        .available = [this]() { return RenderSnapshot().worldSettings != nullptr; },
        .applyOnToggle = true,
        .params = {KeybindParam(
            "gravity", "Gravity Strength", &cfg.customGravityValue, -3000.0f, 3000.0f,
            "Zero makes objects weightless; negative values pull down and positive values pull up"
        )},
        .group = "Game Speed & Gravity",
    });

    keybinds.Add({
        .name = "Toggle Game Paused",
        .tooltip = "Everything in the game stops moving while this is active",
        .configSection = "ToggleGamePaused",
        .keyPtr = &cfg.setGamePausedKey,
        .callback =
            [this](bool active, const RuntimeContextSnapshot& runtime) {
                auto* world = runtime.world;
                if (!world) return;
                SyncStateWorld(world);
                if (active && !pauseTick.actor) {
                    auto* actor = runtime.player;
                    if (!actor) return;
                    pauseTick =
                        {actor, actor->Index, actor->IsActorTickEnabled(),
                         actor->PrimaryActorTick.bTickEvenWhenPaused != 0, actor->GetActorTickInterval()};
                    // The queue drains before listeners. Keep that entry point alive but suspend the Blueprint.
                    pauseTick.hook = GameHook::Get().Subscribe(
                        "ReceiveTick", GameHook::HookPhase::Before,
                        [actor, world](GameHook::ProcessEventContext& context) {
                            if (context.object == actor && SDK::UGameplayStatics::IsGamePaused(world)) context.Cancel();
                        }
                    );
                    if (pauseTick.hook == GameHook::INVALID_HOOK_HANDLE) {
                        pauseTick = {};
                        return;
                    }
                    actor->SetActorTickInterval(0.0f);
                    actor->SetTickableWhenPaused(true);
                    actor->SetActorTickEnabled(true);
                }
                (void)SDK::UGameplayStatics::SetGamePaused(world, active);
                const bool isPaused = SDK::UGameplayStatics::IsGamePaused(world);
                if (!isPaused) RestorePauseTick();
                paused.store(isPaused, std::memory_order_release);
            },
        .kind = KeybindKind::State,
        .stateGetter = [this]() { return CurrentWorldState(paused); },
        .available = [this]() { return RenderSnapshot().player != nullptr; },
        .applyOnToggle = true,
        .group = "Game Speed & Gravity",
        .onRuntimeShutdown =
            [this](const RuntimeContextSnapshot& runtime) {
                if (stateWorld.load(std::memory_order_acquire) == runtime.world && pauseTick.actor)
                    (void)SDK::UGameplayStatics::SetGamePaused(runtime.world, false);
                RestorePauseTick();
                paused.store(false, std::memory_order_release);
            },
    });

    keybinds.Add({
        .name = "Kill All Enemies",
        .tooltip = "Kills NPCs within the selected distance",
        .configSection = "KillAllEnemies",
        .keyPtr = &cfg.killAllEnemiesKey,
        .callback =
            [this](bool, const RuntimeContextSnapshot& runtime) {
                auto* world = runtime.world;
                auto* player = runtime.player;
                if (!player || !world) return;
                const bool snapNeck = cfg.snapNeckEnemies;
                ActorUtils::ForEachWillieInRadius(
                    world, player, cfg.killAllEnemiesRadius, [snapNeck](SDK::AWillie_BP_C* willie) {
                        if (snapNeck) {
                            willie->Snap_Neck();
                        } else {
                            willie->Death();
                        }
                    }
                );
            },
        .params =
            {KeybindParam("radius", "Distance", &cfg.killAllEnemiesRadius, 50.0f, 5000.0f),
             KeybindParam("snapNeck", "Broken Necks", &cfg.snapNeckEnemies, "NPCs die with visibly broken necks")},
        .group = "NPCs",
        .destructive = true,
    });

    keybinds.Add({
        .name = "Toggle Enemy AI",
        .tooltip = "Pauses nearby NPC decisions and restores their previous behavior when disabled",
        .configSection = "ToggleEnemyAI",
        .keyPtr = &cfg.toggleEnemyAIKey,
        .callback =
            [this](bool active, const RuntimeContextSnapshot& runtime) {
                auto* world = runtime.world;
                auto* player = runtime.player;
                if (!player || !world) return;
                SyncStateWorld(world);
                if (active) {
                    ActorUtils::ForEachWillieInRadius(
                        world, player, cfg.toggleEnemyAIRadius, [this](SDK::AWillie_BP_C* willie) {
                            if (auto* controller = willie->Controller) {
                                auto found = stoppedControllers.find(controller);
                                if (found == stoppedControllers.end() || found->second.objectIndex != controller->Index)
                                    stoppedControllers.insert_or_assign(
                                        controller, ControllerState{controller->Index, controller->IsActorTickEnabled()}
                                    );
                                controller->SetActorTickEnabled(false);
                            }
                        }
                    );
                } else {
                    for (const auto& [controller, previous] : stoppedControllers)
                        if (SDK::UObject::GObjects->GetByIndex(previous.objectIndex) == controller &&
                            SDK::UKismetSystemLibrary::IsValid(controller) && !controller->IsActorBeingDestroyed())
                            controller->SetActorTickEnabled(previous.tickEnabled);
                    stoppedControllers.clear();
                }
                enemyAIStopped.store(active, std::memory_order_release);
            },
        .kind = KeybindKind::State,
        .stateGetter = [this]() { return CurrentWorldState(enemyAIStopped); },
        .available =
            [this]() {
                const auto runtime = RenderSnapshot();
                return runtime.world && runtime.player;
            },
        .applyOnToggle = true,
        .params = {KeybindParam("radius", "Distance", &cfg.toggleEnemyAIRadius, 50.0f, 5000.0f)},
        .group = "NPCs",
    });

    keybinds.Add({
        .name = "Destroy All Willies",
        .tooltip = "Makes NPCs except the player disappear; Bodies Only leaves living NPCs untouched",
        .configSection = "DestroyAllWillies",
        .keyPtr = &cfg.destroyWilliesKey,
        .callback =
            [this](bool, const RuntimeContextSnapshot& runtime) {
                auto* world = runtime.world;
                auto* player = runtime.player;
                if (!player || !world) return;
                const bool deadOnly = cfg.destroyDeadOnly;
                const bool disintegrate = cfg.destroyDisintegrate;
                ActorUtils::ForEachWillie(world, player, [deadOnly, disintegrate](SDK::AWillie_BP_C* willie) {
                    if (!deadOnly || willie->Health <= GameConstants::MIN_HEALTH) {
                        if (disintegrate) {
                            willie->Disintegrate_and_drop_armor(true);
                        } else {
                            willie->K2_DestroyActor();
                        }
                    }
                });
            },
        .params =
            {KeybindParam("dead_only", "Bodies Only", &cfg.destroyDeadOnly, "Leaves living NPCs untouched"),
             KeybindParam(
                 "disintegrate", "Disintegrate", &cfg.destroyDisintegrate,
                 "Removed NPCs disappear with a disintegration effect"
             )},
        .group = "NPCs",
        .destructive = true,
    });

    keybinds.Add({
        .name = "Clear Blood",
        .tooltip = "Removes all visible blood from the current map",
        .configSection = "ClearBlood",
        .keyPtr = &cfg.clearBloodKey,
        .callback =
            [](bool, const RuntimeContextSnapshot& runtime) {
                if (runtime.world) ClearBlood(runtime.world);
            },
        .group = "Map Cleanup",
    });

    keybinds.Add({
        .name = "Clear Objects",
        .tooltip = "Removes uncarried weapons and armor within the selected distance",
        .configSection = "ClearObjects",
        .keyPtr = &cfg.clearObjectsKey,
        .callback =
            [this](bool, const RuntimeContextSnapshot& runtime) {
                auto* world = runtime.world;
                auto* player = runtime.player;
                if (!player || !world) return;
                ClearDroppedObjects(world, player, cfg.clearObjectsRadius);
            },
        .params = {KeybindParam("radius", "Distance", &cfg.clearObjectsRadius, 50.0f, 5000.0f)},
        .group = "Map Cleanup",
        .destructive = true,
    });
}
