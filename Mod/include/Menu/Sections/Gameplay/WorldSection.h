#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"
#include "Menu/Utils/Spawner.h"
#include "SDK/Arena_Cutting_Map_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/BP_Armor_Master_classes.hpp"

class WorldSection : public CollapsibleSection {
private:
    static constexpr float DEFAULT_TIME_DILATION = 1.0f;
    static constexpr float DEFAULT_GRAVITY = -980.0f;
    static constexpr float MIN_HEALTH = 0.0f;

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
                worldSettings->TimeDilation = (worldSettings->TimeDilation == DEFAULT_TIME_DILATION) ? slowMotionSpeed : DEFAULT_TIME_DILATION;
            }, worldSettings);

        std::initializer_list<Parameter> customGravityParams = {
            Parameter("gravity", "Gravity", &customGravityValue, -3000.0f, 3000.0f)
        };

        Function("Toggle Custom Gravity")
            .WithKey(&customGravityKey)
            .WithParams(customGravityParams)
            .Action([this]() {
                worldSettings->bWorldGravitySet = true;
                worldSettings->WorldGravityZ = (worldSettings->WorldGravityZ == DEFAULT_GRAVITY) ? customGravityValue : DEFAULT_GRAVITY;
            }, worldSettings);

        std::initializer_list<Parameter> killAllEnemiesParams = {
            Parameter("radius", "Radius", &killAllEnemiesRadius, 50.0f, 5000.0f),
            Parameter("snapNeck", "Snap Neck", &snapNeckEnemies, "Just another way to kill enemies")
        };

        Function("Kill All Enemies")
            .WithKey(&killAllEnemiesKey)
            .WithParams(killAllEnemiesParams)
            .Action([this]() {
                SDK::TArray<SDK::AActor*> actors;
                SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AWillie_BP_C::StaticClass(), &actors);
                for (SDK::AActor* actor : actors) {
                    auto* willie = static_cast<SDK::AWillie_BP_C*>(actor);
                    if (willie == player || player->GetDistanceTo(willie) > killAllEnemiesRadius) [[unlikely]] continue;
                    if (snapNeckEnemies) [[unlikely]] {
                        willie->Snap_Neck();
                    } else {
                        willie->Death();
                    }
                }
            }, player, world);

        std::initializer_list<Parameter> toggleEnemyAIRadiusParams = {
            Parameter("radius", "Radius", &toggleEnemyAIRadius, 50.0f, 5000.0f)
        };

        Function("Toggle Enemy AI")
            .WithKey(&toggleEnemyAIKey)
            .WithParams(toggleEnemyAIRadiusParams)
            .Action([this]() {
                SDK::TArray<SDK::AActor*> actors;
                SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AWillie_BP_C::StaticClass(), &actors);
                bool newTickEnabled = false;
                bool computed = false;

                for (SDK::AActor* actor : actors) {
                    auto* willie = static_cast<SDK::AWillie_BP_C*>(actor);
                    if (willie == player || player->GetDistanceTo(willie) > toggleEnemyAIRadius) [[unlikely]] continue;
                    if (auto* controller = static_cast<SDK::AAIController*>(willie->GetController())) [[likely]] {
                        if (!computed) [[unlikely]] {
                            newTickEnabled = !controller->IsActorTickEnabled();
                            computed = true;
                        }
                        controller->SetActorTickEnabled(newTickEnabled);
                    }
                }
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
                SDK::TArray<SDK::AActor*> actors;
                SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AWillie_BP_C::StaticClass(), &actors);
                for (auto* actor : actors) {
                    if (auto* w = static_cast<SDK::AWillie_BP_C*>(actor);
                        w != player && (!destroyDeadOnly || w->Health <= MIN_HEALTH)) [[likely]] {
                        if (destroyDisintegrate) [[unlikely]] {
                            w->Disintegrate_and_drop_armor(true);
                        } else {
                            w->K2_DestroyActor();
                        }
                    }
                }
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
                SDK::TArray<SDK::AActor*> allWillies;
                SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AWillie_BP_C::StaticClass(), &allWillies);
                
                auto clearObjectsOfType = [&](SDK::UClass* objectClass) {
                    SDK::TArray<SDK::AActor*> objects;
                    SDK::UGameplayStatics::GetAllActorsOfClass(world, objectClass, &objects);
                    for (auto* object : objects) {                        
                        bool isWithinRadiusOfAnyWillie = false;
                        bool isFarFromAllWillies = true;
                        
                        for (auto* willieActor : allWillies) {
                            if (auto* willie = static_cast<SDK::AWillie_BP_C*>(willieActor)) {
                                float distance = willie->GetDistanceTo(object);
                                if (distance <= clearObjectsRadius) {
                                    isWithinRadiusOfAnyWillie = true;
                                }
                                if (distance <= 100.0f) {
                                    isFarFromAllWillies = false;
                                }
                                if (isWithinRadiusOfAnyWillie && !isFarFromAllWillies) {
                                    break;
                                }
                            }
                        }
                        
                        if (isWithinRadiusOfAnyWillie && isFarFromAllWillies) {
                            object->K2_DestroyActor();
                        }
                    }
                };
                
                clearObjectsOfType(SDK::AModularWeaponBP_C::StaticClass());
                clearObjectsOfType(SDK::ABP_Armor_Master_C::StaticClass());
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