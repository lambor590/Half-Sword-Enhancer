#include "Menu/Sections/World/WorldActionsSection.h"
#include "Menu/SectionRegistry.h"
#include "Menu/SectionStyle.h"

REGISTER_SECTION(WorldActionsSection, MenuTab::World);
#include "Utils/Spawner.h"
#include "Utils/GameConstants.h"
#include "Utils/ActorUtils.h"
#include "ComponentValidator.h"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/BP_Armor_Master_classes.hpp"
#include "SDK/Blood_BP_P4_classes.hpp"
#include "SDK/Blood_BP_PT_classes.hpp"
#include "SDK/RunningBlood_BP_classes.hpp"
#include "SDK/BP_MeshBloodSim_classes.hpp"
#include "SDK/HSComputeShaders_classes.hpp"

WorldActionsSection::WorldActionsSection(ModContext& ctx) : Section(ctx, "Actions") {
    InitKeybinds();
}

void WorldActionsSection::Render() {
    const SectionStyle::StyleRAII style;
    KeybindUI::RenderKeybindList(keybinds);
}

void WorldActionsSection::InitKeybinds() {
    keybinds.push_back({
        .name = "Toggle Slow Motion",
        .configSection = "ToggleSlowMotion",
        .keyPtr = &cfg.sloMoKey,
        .callback =
            [this]([[maybe_unused]] bool) {
                if (!ComponentValidator::Validate(worldSettings)) return;
                worldSettings->TimeDilation = (worldSettings->TimeDilation == GameConstants::DEFAULT_TIME_DILATION)
                                                  ? cfg.slowMotionSpeed
                                                  : GameConstants::DEFAULT_TIME_DILATION;
            },
        .params = {KeybindParam(
            "speed", "Speed", &cfg.slowMotionSpeed, 0.01f, 0.99f, "The speed of the game when is enabled"
        )},
    });
    InitKeybindEntry(keybinds.back());

    keybinds.push_back({
        .name = "Toggle Custom Gravity",
        .configSection = "ToggleCustomGravity",
        .keyPtr = &cfg.customGravityKey,
        .callback =
            [this]([[maybe_unused]] bool) {
                if (!ComponentValidator::Validate(worldSettings)) return;
                worldSettings->bWorldGravitySet = true;
                worldSettings->WorldGravityZ = (worldSettings->WorldGravityZ == GameConstants::DEFAULT_GRAVITY)
                                                   ? cfg.customGravityValue
                                                   : GameConstants::DEFAULT_GRAVITY;
            },
        .params = {KeybindParam("gravity", "Gravity", &cfg.customGravityValue, -3000.0f, 3000.0f)},
    });
    InitKeybindEntry(keybinds.back());

    keybinds.push_back({
        .name = "Kill All Enemies",
        .configSection = "KillAllEnemies",
        .keyPtr = &cfg.killAllEnemiesKey,
        .callback =
            [this]([[maybe_unused]] bool) {
                if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;
                GameHook::QueueAction([this]() {
                    ActorUtils::ForEachWillieInRadius(
                        world, player, cfg.killAllEnemiesRadius,
                        [this](SDK::AWillie_BP_C* willie) {
                            if (cfg.snapNeckEnemies) {
                                willie->Snap_Neck();
                            } else {
                                willie->Death();
                            }
                        }
                    );
                });
            },
        .params =
            {KeybindParam("radius", "Radius", &cfg.killAllEnemiesRadius, 50.0f, 5000.0f),
             KeybindParam("snapNeck", "Snap Neck", &cfg.snapNeckEnemies, "Just another way to kill enemies")},
    });
    InitKeybindEntry(keybinds.back());

    keybinds.push_back({
        .name = "Toggle Enemy AI",
        .configSection = "ToggleEnemyAI",
        .keyPtr = &cfg.toggleEnemyAIKey,
        .callback =
            [this]([[maybe_unused]] bool) {
                if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;
                GameHook::QueueAction([this]() {
                    bool newTickEnabled = false;
                    bool computed = false;
                    ActorUtils::ForEachWillieInRadius(
                        world, player, cfg.toggleEnemyAIRadius,
                        [&](SDK::AWillie_BP_C* willie) {
                            if (auto* ctrl = static_cast<SDK::AAIController*>(willie->GetController())) {
                                if (!computed) {
                                    newTickEnabled = !ctrl->IsActorTickEnabled();
                                    computed = true;
                                }
                                ctrl->SetActorTickEnabled(newTickEnabled);
                            }
                        }
                    );
                });
            },
        .params = {KeybindParam("radius", "Radius", &cfg.toggleEnemyAIRadius, 50.0f, 5000.0f)},
    });
    InitKeybindEntry(keybinds.back());

    keybinds.push_back({
        .name = "Destroy All Willies",
        .tooltip = "Removes all NPCs from the world",
        .configSection = "DestroyAllWillies",
        .keyPtr = &cfg.destroyWilliesKey,
        .callback =
            [this]([[maybe_unused]] bool) {
                if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;
                GameHook::QueueAction([this]() {
                    ActorUtils::ForEachWillie(world, player, [this](SDK::AWillie_BP_C* willie) {
                        if (!cfg.destroyDeadOnly || willie->Health <= GameConstants::MIN_HEALTH) {
                            if (cfg.destroyDisintegrate) {
                                willie->Disintegrate_and_drop_armor(true);
                            } else {
                                willie->K2_DestroyActor();
                            }
                        }
                    });
                });
            },
        .params =
            {KeybindParam(
                 "dead_only", "Only Dead", &cfg.destroyDeadOnly, "Only destroy dead bodies, not living enemies"
             ),
             KeybindParam(
                 "disintegrate", "Disintegrate", &cfg.destroyDisintegrate,
                 "Use disintegration effect instead of instant removal"
             )},
    });
    InitKeybindEntry(keybinds.back());

    keybinds.push_back({
        .name = "Clear Blood",
        .tooltip = "Removes blood decals, emitters, and surface blood from the world",
        .configSection = "ClearBlood",
        .keyPtr = &cfg.clearBloodKey,
        .callback =
            [this]([[maybe_unused]] bool) {
                if (!ComponentValidator::Validate(world)) return;
                GameHook::QueueAction([this]() {
                    const SDK::FLinearColor clearColor{0.0f, 0.0f, 0.0f, 0.0f};
                    const SDK::FLinearColor defaultVertexColor{1.0f, 1.0f, 1.0f, 1.0f};

                    ActorUtils::ForEachWillie(world, nullptr, [](SDK::AWillie_BP_C* willie) {
                        willie->Reset_Blood_Bleed();
                        willie->Reset_Blood_Splash();
                        willie->Reset_Trail_Blood(0.0f);
                        willie->BloodyFoot_R = 0;
                        willie->BloodyFoot_L = 0;
                        willie->Mesh->ClearVertexColorOverride(0);
                        if (willie->CharacterMesh_Head) willie->CharacterMesh_Head->ClearVertexColorOverride(0);
                    });

                    ActorUtils::ForEachObjectOfType<SDK::ABP_MeshBloodSim_C>(
                        world,
                        [this, clearColor, defaultVertexColor](SDK::ABP_MeshBloodSim_C* sim) {
                            if (sim->MainRenderTarget)
                                SDK::UKismetRenderingLibrary::ClearRenderTarget2D(
                                    world, sim->MainRenderTarget, clearColor
                                );
                            if (sim->MeshToSimOn)
                                SDK::UMeshVertexPainterKismetLibrary::PaintVerticesSingleColor(
                                    sim->MeshToSimOn, defaultVertexColor, false
                                );
                            sim->RemainingSimulationTime = 0.0;
                        }
                    );

                    ActorUtils::ForEachObjectOfType<SDK::ACSBloodSimActor>(
                        world,
                        [this, clearColor, defaultVertexColor](SDK::ACSBloodSimActor* sim) {
                            if (sim->CurrentWrite)
                                SDK::UKismetRenderingLibrary::ClearRenderTarget2D(world, sim->CurrentWrite, clearColor);
                            if (sim->CurrentRead)
                                SDK::UKismetRenderingLibrary::ClearRenderTarget2D(world, sim->CurrentRead, clearColor);
                            if (sim->BoundMesh)
                                SDK::UMeshVertexPainterKismetLibrary::PaintVerticesSingleColor(
                                    sim->BoundMesh, defaultVertexColor, false
                                );
                        }
                    );

                    ActorUtils::ForEachObjectOfType<SDK::ABlood_BP_P4_C>(world, [](SDK::ABlood_BP_P4_C* blood) {
                        blood->K2_DestroyActor();
                    });
                    ActorUtils::ForEachObjectOfType<SDK::ABlood_BP_PT_C>(world, [](SDK::ABlood_BP_PT_C* blood) {
                        blood->K2_DestroyActor();
                    });
                    ActorUtils::ForEachComponentOfType<SDK::UDecalComponent>(world, [](SDK::UDecalComponent* decal) {
                        decal->K2_DestroyComponent(decal);
                    });
                    ActorUtils::ForEachObjectOfType<SDK::ADecalActor>(world, [](SDK::ADecalActor* decal) {
                        decal->K2_DestroyActor();
                    });
                    ActorUtils::ForEachObjectOfType<SDK::ARunningBlood_BP_C>(world, [](SDK::ARunningBlood_BP_C* blood) {
                        blood->K2_DestroyActor();
                    });
                    ActorUtils::ForEachObjectOfType<SDK::AHSWoundsController>(
                        world, [](SDK::AHSWoundsController* wounds) { wounds->K2_DestroyActor(); }
                    );
                });
            },
    });
    InitKeybindEntry(keybinds.back());

    keybinds.push_back({
        .name = "Clear Objects",
        .tooltip = "Removes dropped weapons and armor",
        .configSection = "ClearObjects",
        .keyPtr = &cfg.clearObjectsKey,
        .callback =
            [this]([[maybe_unused]] bool) {
                if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;
                GameHook::QueueAction([this]() {
                    std::vector<SDK::FVector> williePositions;
                    ActorUtils::ForEachWillie(world, nullptr, [&](SDK::AWillie_BP_C* willie) {
                        williePositions.push_back(willie->K2_GetActorLocation());
                    });
                    const double radiusSq =
                        static_cast<double>(cfg.clearObjectsRadius) * static_cast<double>(cfg.clearObjectsRadius);
                    auto shouldDestroy = [&](SDK::AActor* object) -> bool {
                        constexpr double MIN_DISTANCE_SQ = 120.0 * 120.0;
                        if (williePositions.empty()) return false;
                        SDK::FVector objPos = object->K2_GetActorLocation();
                        bool withinRadius = false;
                        for (const auto& williePos : williePositions) {
                            SDK::FVector diff = objPos - williePos;
                            double distanceSq = diff.Dot(diff);
                            if (distanceSq <= MIN_DISTANCE_SQ) return false;
                            if (distanceSq <= radiusSq) withinRadius = true;
                        }
                        return withinRadius;
                    };
                    ActorUtils::ForEachObjectOfType<SDK::AModularWeaponBP_C>(
                        world,
                        [&](SDK::AModularWeaponBP_C* weapon) {
                            if (shouldDestroy(weapon)) weapon->K2_DestroyActor();
                        }
                    );
                    ActorUtils::ForEachObjectOfType<SDK::ABP_Armor_Master_C>(
                        world,
                        [&](SDK::ABP_Armor_Master_C* armor) {
                            if (shouldDestroy(armor)) armor->K2_DestroyActor();
                        }
                    );
                });
            },
        .params = {KeybindParam("radius", "Radius", &cfg.clearObjectsRadius, 50.0f, 5000.0f)},
    });
    InitKeybindEntry(keybinds.back());

    keybinds.push_back({
        .name = "Toggle Game Paused",
        .tooltip = "Pauses/unpauses the entire game simulation",
        .configSection = "ToggleGamePaused",
        .keyPtr = &cfg.setGamePausedKey,
        .callback =
            [this]([[maybe_unused]] bool) {
                if (!ComponentValidator::Validate(world)) return;
                const bool isPaused = SDK::UGameplayStatics::IsGamePaused(world);
                SDK::UGameplayStatics::SetGamePaused(world, !isPaused);
            },
    });
    InitKeybindEntry(keybinds.back());
}
