#include "Menu/Sections/World/WorldActionsSection.h"

#include <vector>

#include "SDK/BP_Armor_Master_classes.hpp"
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
    constexpr double MINIMUM_OBJECT_DISTANCE_SQUARED = 120.0 * 120.0;

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
            willie->Mesh->ClearVertexColorOverride(0);
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
        ActorUtils::ForEachComponentOfType<SDK::UDecalComponent>(world, [](SDK::UDecalComponent* decal) {
            decal->K2_DestroyComponent(decal);
        });
        DestroyAllActors<SDK::ADecalActor, SDK::ARunningBlood_BP_C, SDK::AHSWoundsController>(world);
    }

    void ClearDroppedObjects(SDK::UWorld* world, float configuredRadius) {
        std::vector<SDK::FVector> williePositions;
        ActorUtils::ForEachWillie(world, nullptr, [&](SDK::AWillie_BP_C* willie) {
            williePositions.push_back(willie->K2_GetActorLocation());
        });
        if (williePositions.empty()) return;

        const double radius = configuredRadius;
        const double radiusSquared = radius * radius;
        const auto shouldDestroy = [&](SDK::AActor* object) {
            const auto objectPosition = object->K2_GetActorLocation();
            bool withinRadius = false;
            for (const auto& williePosition : williePositions) {
                const auto difference = objectPosition - williePosition;
                const double distanceSquared = difference.Dot(difference);
                if (distanceSquared <= MINIMUM_OBJECT_DISTANCE_SQUARED) return false;
                withinRadius |= distanceSquared <= radiusSquared;
            }
            return withinRadius;
        };
        const auto destroyIfSafe = [&](auto* object) {
            if (shouldDestroy(object)) object->K2_DestroyActor();
        };
        ActorUtils::ForEachObjectOfType<SDK::AModularWeaponBP_C>(world, destroyIfSafe);
        ActorUtils::ForEachObjectOfType<SDK::ABP_Armor_Master_C>(world, destroyIfSafe);
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
    stateWorld.store(world, std::memory_order_release);
}

bool WorldActionsSection::CurrentWorldState(const std::atomic_bool& state) const noexcept {
    auto* world = RenderWorld();
    return world && stateWorld.load(std::memory_order_acquire) == world && state.load(std::memory_order_acquire);
}

void WorldActionsSection::InitKeybinds() {
    keybinds.Add({
        .name = "Toggle Slow Motion",
        .configSection = "ToggleSlowMotion",
        .keyPtr = &cfg.sloMoKey,
        .callback =
            [this](bool active, const RuntimeContextSnapshot& runtime) {
                auto* worldSettings = runtime.worldSettings;
                if (!runtime.world || !worldSettings) return;
                SyncStateWorld(runtime.world);
                worldSettings->TimeDilation = active ? cfg.slowMotionSpeed : GameConstants::DEFAULT_TIME_DILATION;
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
                worldSettings->bWorldGravitySet = true;
                worldSettings->WorldGravityZ = active ? cfg.customGravityValue : GameConstants::DEFAULT_GRAVITY;
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
                if (SDK::UGameplayStatics::SetGamePaused(world, active)) {
                    paused.store(active, std::memory_order_release);
                }
            },
        .kind = KeybindKind::State,
        .stateGetter = [this]() { return CurrentWorldState(paused); },
        .available = [this]() { return RenderWorld() != nullptr; },
        .applyOnToggle = true,
        .group = "Game Speed & Gravity",
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
        .tooltip = "Freezes moving NPCs or lets frozen NPCs move again",
        .configSection = "ToggleEnemyAI",
        .keyPtr = &cfg.toggleEnemyAIKey,
        .callback =
            [this](bool active, const RuntimeContextSnapshot& runtime) {
                auto* world = runtime.world;
                auto* player = runtime.player;
                if (!player || !world) return;
                SyncStateWorld(world);
                ActorUtils::ForEachWillieInRadius(
                    world, player, cfg.toggleEnemyAIRadius, [active](SDK::AWillie_BP_C* willie) {
                        if (auto* ctrl = static_cast<SDK::AAIController*>(willie->Controller)) {
                            ctrl->SetActorTickEnabled(!active);
                        }
                    }
                );
                enemyAIStopped.store(active, std::memory_order_release);
            },
        .kind = KeybindKind::State,
        .stateGetter = [this]() { return CurrentWorldState(enemyAIStopped); },
        .available = [this]() {
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
                ClearDroppedObjects(world, cfg.clearObjectsRadius);
            },
        .params = {KeybindParam("radius", "Distance", &cfg.clearObjectsRadius, 50.0f, 5000.0f)},
        .group = "Map Cleanup",
        .destructive = true,
    });
}
