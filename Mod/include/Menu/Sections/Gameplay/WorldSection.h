#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Utils/Spawner.h"
#include "SDK/Arena_Cutting_Map_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/BP_Armor_Master_classes.hpp"
#include "Utils/GameConstants.h"
#include "Utils/ActorUtils.h"

class WorldSection : public CollapsibleSection {
private:
    SectionConfig::WorldConfig& cfg = SectionConfig::world;

public:
    WorldSection() : CollapsibleSection("World") {
        Function("Toggle Slow Motion")
            .WithKey(&cfg.sloMoKey)
            .WithParams({ Parameter("speed", "Speed", &cfg.slowMotionSpeed, 0.01f, 0.99f, "The speed of the game when is enabled") })
            .Action([this]() {
                worldSettings->TimeDilation = (worldSettings->TimeDilation == GameConstants::DEFAULT_TIME_DILATION) ? cfg.slowMotionSpeed : GameConstants::DEFAULT_TIME_DILATION;
            }, worldSettings);

        Function("Toggle Custom Gravity")
            .WithKey(&cfg.customGravityKey)
            .WithParams({ Parameter("gravity", "Gravity", &cfg.customGravityValue, -3000.0f, 3000.0f) })
            .Action([this]() {
                worldSettings->bWorldGravitySet = true;
                worldSettings->WorldGravityZ = (worldSettings->WorldGravityZ == GameConstants::DEFAULT_GRAVITY) ? cfg.customGravityValue : GameConstants::DEFAULT_GRAVITY;
            }, worldSettings);

        Function("Kill All Enemies")
            .WithKey(&cfg.killAllEnemiesKey)
            .WithParams({
                Parameter("radius", "Radius", &cfg.killAllEnemiesRadius, 50.0f, 5000.0f),
                Parameter("snapNeck", "Snap Neck", &cfg.snapNeckEnemies, "Just another way to kill enemies")
            })
            .Action([this]() {
                ActorUtils::ForEachWillieInRadius(world, player, cfg.killAllEnemiesRadius, [this](SDK::AWillie_BP_C* willie) {
                    if (cfg.snapNeckEnemies) {
                        willie->Snap_Neck();
                    } else {
                        willie->Death();
                    }
                });
            }, player, world);

        Function("Toggle Enemy AI")
            .WithKey(&cfg.toggleEnemyAIKey)
            .WithParams({ Parameter("radius", "Radius", &cfg.toggleEnemyAIRadius, 50.0f, 5000.0f) })
            .Action([this]() {
                bool newTickEnabled = false;
                bool computed = false;

                ActorUtils::ForEachWillieInRadius(world, player, cfg.toggleEnemyAIRadius, [&](SDK::AWillie_BP_C* willie) {
                    if (auto* controller = static_cast<SDK::AAIController*>(willie->GetController())) {
                        if (!computed) {
                            newTickEnabled = !controller->IsActorTickEnabled();
                            computed = true;
                        }
                        controller->SetActorTickEnabled(newTickEnabled);
                    }
                });
            }, player, world);

        Function("Destroy All Willies")
            .WithKey(&cfg.destroyWilliesKey)
            .WithParams({
                Parameter("dead_only", "Only Dead", &cfg.destroyDeadOnly, "Only destroy dead bodies, not living enemies"),
                Parameter("disintegrate", "Disintegrate", &cfg.destroyDisintegrate, "Use disintegration effect instead of instant removal")
            })
            .WithTooltip("Removes all NPCs from the world")
            .Action([this]() {
                ActorUtils::ForEachWillie(world, player, [this](SDK::AWillie_BP_C* willie) {
                    if (!cfg.destroyDeadOnly || willie->Health <= GameConstants::MIN_HEALTH) {
                        if (cfg.destroyDisintegrate) {
                            willie->Disintegrate_and_drop_armor(true);
                        } else {
                            willie->K2_DestroyActor();
                        }
                    }
                });
            }, player, world);

        Function("Clear Blood")
            .WithKey(&cfg.clearBloodKey)
            .WithParams({ Parameter("amount", "Amount", &cfg.clearBloodAmount, 0.0f, 1.0f, "How much blood to remove (0.0 = none, 1.0 = all)") })
            .WithTooltip("Removes blood decals from the arena with configurable intensity. Only works in the arena.")
            .Action([this]() {
                static_cast<SDK::AArena_Cutting_Map_C*>(world->PersistentLevel->LevelScriptActor)->Clean_Blood(cfg.clearBloodAmount);
            }, world);

        Function("Clear Objects")
            .WithKey(&cfg.clearObjectsKey)
            .WithParams({ Parameter("radius", "Radius", &cfg.clearObjectsRadius, 50.0f, 5000.0f) })
            .WithTooltip("Removes dropped weapons and armor")
            .Action([this]() {
                constexpr double MIN_DISTANCE_SQ = 120.0 * 120.0;

                std::vector<SDK::FVector> williePositions;
                ActorUtils::ForEachWillie(world, nullptr, [&](SDK::AWillie_BP_C* willie) {
                    williePositions.push_back(willie->K2_GetActorLocation());
                });

                const double radiusSq = static_cast<double>(cfg.clearObjectsRadius) * static_cast<double>(cfg.clearObjectsRadius);

                auto shouldDestroy = [&](SDK::AActor* object) -> bool {
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

                ActorUtils::ForEachObjectOfType<SDK::AModularWeaponBP_C>(world, [&](SDK::AModularWeaponBP_C* weapon) {
                    if (shouldDestroy(weapon)) {
                        weapon->K2_DestroyActor();
                    }
                });

                ActorUtils::ForEachObjectOfType<SDK::ABP_Armor_Master_C>(world, [&](SDK::ABP_Armor_Master_C* armor) {
                    if (shouldDestroy(armor)) {
                        armor->K2_DestroyActor();
                    }
                });
            }, player, world);

        Function("Toggle Game Paused")
            .WithKey(&cfg.setGamePausedKey)
            .WithTooltip("Pauses/unpauses the entire game simulation")
            .Action([this]() {
                const bool isPaused = SDK::UGameplayStatics::IsGamePaused(world);
                SDK::UGameplayStatics::SetGamePaused(world, !isPaused);
            }, world);
    }
};
