#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"
#include "Utils/Spawner.h"
#include "SDK/Arena_Cutting_Map_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/BP_Armor_Master_classes.hpp"
#include "Utils/GameConstants.h"
#include "Utils/ActorUtils.h"

class WorldSection : public CollapsibleSection {
private:

    static inline int sloMoKey = 0x5A; // Z
    static inline float slowMotionSpeed = 0.4f;

    static inline int customGravityKey = 0x4C; // L
    static inline float customGravityValue = 0.0f;

    static inline int killAllEnemiesKey = -1;
    static inline float killAllEnemiesRadius = 1000.0f;
    static inline bool snapNeckEnemies = false;

    static inline int toggleEnemyAIKey = -1;
    static inline float toggleEnemyAIRadius = 1000.0f;

    static inline int destroyWilliesKey = -1;
    static inline bool destroyDeadOnly = true;
    static inline bool destroyDisintegrate = true;

    static inline int clearBloodKey = -1;
    static inline float clearBloodAmount = 0.1f;

    static inline int clearObjectsKey = -1;
    static inline float clearObjectsRadius = 1000.0f;
    
    static inline int setGamePausedKey = -1;

public:
    WorldSection() : CollapsibleSection("World") {
        std::initializer_list<Parameter> slowMotionParams = {
            Parameter("speed", "Speed", &slowMotionSpeed, 0.01f, 0.99f, "The speed of the game when is enabled")
        };

        Function("Toggle Slow Motion")
            .WithKey(&sloMoKey)
            .WithParams(slowMotionParams)
            .Action([this]() {
                worldSettings->TimeDilation = (worldSettings->TimeDilation == GameConstants::DEFAULT_TIME_DILATION) ? slowMotionSpeed : GameConstants::DEFAULT_TIME_DILATION;
            }, worldSettings);

        std::initializer_list<Parameter> customGravityParams = {
            Parameter("gravity", "Gravity", &customGravityValue, -3000.0f, 3000.0f)
        };

        Function("Toggle Custom Gravity")
            .WithKey(&customGravityKey)
            .WithParams(customGravityParams)
            .Action([this]() {
                worldSettings->bWorldGravitySet = true;
                worldSettings->WorldGravityZ = (worldSettings->WorldGravityZ == GameConstants::DEFAULT_GRAVITY) ? customGravityValue : GameConstants::DEFAULT_GRAVITY;
            }, worldSettings);

        std::initializer_list<Parameter> killAllEnemiesParams = {
            Parameter("radius", "Radius", &killAllEnemiesRadius, 50.0f, 5000.0f),
            Parameter("snapNeck", "Snap Neck", &snapNeckEnemies, "Just another way to kill enemies")
        };

        Function("Kill All Enemies")
            .WithKey(&killAllEnemiesKey)
            .WithParams(killAllEnemiesParams)
            .Action([this]() {
                ActorUtils::ForEachWillieInRadius(world, player, killAllEnemiesRadius, [this](SDK::AWillie_BP_C* willie) {
                    if (snapNeckEnemies) {
                        willie->Snap_Neck();
                    } else {
                        willie->Death();
                    }
                });
            }, player, world);

        std::initializer_list<Parameter> toggleEnemyAIRadiusParams = {
            Parameter("radius", "Radius", &toggleEnemyAIRadius, 50.0f, 5000.0f)
        };

        Function("Toggle Enemy AI")
            .WithKey(&toggleEnemyAIKey)
            .WithParams(toggleEnemyAIRadiusParams)
            .Action([this]() {
                bool newTickEnabled = false;
                bool computed = false;

                ActorUtils::ForEachWillieInRadius(world, player, toggleEnemyAIRadius, [&](SDK::AWillie_BP_C* willie) {
                    if (auto* controller = static_cast<SDK::AAIController*>(willie->GetController())) {
                        if (!computed) {
                            newTickEnabled = !controller->IsActorTickEnabled();
                            computed = true;
                        }
                        controller->SetActorTickEnabled(newTickEnabled);
                    }
                });
            }, player, world);

        std::initializer_list<Parameter> destroyWilliesParams = {
            Parameter("dead_only", "Only Dead", &destroyDeadOnly, "Only destroy dead bodies, not living enemies"),
            Parameter("disintegrate", "Disintegrate", &destroyDisintegrate, "Use disintegration effect instead of instant removal")
        };

        Function("Destroy All Willies")
            .WithKey(&destroyWilliesKey)
            .WithParams(destroyWilliesParams)
            .WithTooltip("Removes all NPCs from the world")
            .Action([this]() {
                ActorUtils::ForEachWillie(world, player, [this](SDK::AWillie_BP_C* willie) {
                    if (!destroyDeadOnly || willie->Health <= GameConstants::MIN_HEALTH) {
                        if (destroyDisintegrate) {
                            willie->Disintegrate_and_drop_armor(true);
                        } else {
                            willie->K2_DestroyActor();
                        }
                    }
                });
            }, player, world);

        std::initializer_list<Parameter> clearBloodParams = {
            Parameter("amount", "Amount", &clearBloodAmount, 0.0f, 1.0f, "How much blood to remove (0.0 = none, 1.0 = all)")
        };

        Function("Clear Blood")
            .WithKey(&clearBloodKey)
            .WithParams(clearBloodParams)
            .WithTooltip("Removes blood decals from the arena with configurable intensity. Only works in the arena.")
            .Action([this]() {
                static_cast<SDK::AArena_Cutting_Map_C*>(world->PersistentLevel->LevelScriptActor)->Clean_Blood(clearBloodAmount);
            }, world);

        std::initializer_list<Parameter> clearObjectsParams = {
            Parameter("radius", "Radius", &clearObjectsRadius, 50.0f, 5000.0f)
        };

        Function("Clear Objects")
            .WithKey(&clearObjectsKey)
            .WithParams(clearObjectsParams)
            .WithTooltip("Removes dropped weapons and armor")
            .Action([this]() {
                constexpr float MIN_DISTANCE_FROM_WILLIES = 120.0f;

                std::vector<SDK::FVector> williePositions;
                ActorUtils::ForEachWillie(world, nullptr, [&](SDK::AWillie_BP_C* willie) {
                    williePositions.push_back(willie->K2_GetActorLocation());
                });

                auto shouldDestroy = [&](SDK::AActor* object) -> bool {
                    if (williePositions.empty()) return false;

                    SDK::FVector objPos = object->K2_GetActorLocation();
                    bool withinRadius = false;

                    for (const auto& williePos : williePositions) {
                        float distance = static_cast<float>(objPos.GetDistanceTo(williePos));
                        if (distance <= MIN_DISTANCE_FROM_WILLIES) return false;
                        if (distance <= clearObjectsRadius) withinRadius = true;
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
            .WithKey(&setGamePausedKey)
            .WithTooltip("Pauses/unpauses the entire game simulation")
            .Action([this]() {
                const bool isPaused = SDK::UGameplayStatics::IsGamePaused(world);
                SDK::UGameplayStatics::SetGamePaused(world, !isPaused);
            }, world);
    }
};