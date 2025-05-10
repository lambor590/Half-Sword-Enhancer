#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"
#include "Menu/Utils/Spawner.h"
#include "SDK/Arena_Cutting_Map_classes.hpp"

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

public:
    WorldSection() : CollapsibleSection("World") {
        std::initializer_list<Parameter> slowMotionParams = {
            Parameter("speed", "Speed", &slowMotionSpeed, 0.01f, 0.99f)
        };

        Function("Toggle Slow Motion")
            .WithKey(&sloMoKey)
            .WithParams(slowMotionParams)
            .Action([this]() {
                worldSettings->TimeDilation = (worldSettings->TimeDilation == 1.0f) ? slowMotionSpeed : 1.0f;
            }, worldSettings);

        std::initializer_list<Parameter> customGravityParams = {
            Parameter("gravity", "Gravity", &customGravityValue, -3000.0f, 3000.0f)
        };

        Function("Toggle Custom Gravity")
            .WithKey(&customGravityKey)
            .WithParams(customGravityParams)
            .Action([this]() {
                worldSettings->bWorldGravitySet = true;
                worldSettings->WorldGravityZ = (worldSettings->WorldGravityZ == -980.0f) ? customGravityValue : -980.0f;
            }, worldSettings);

        std::initializer_list<Parameter> killAllEnemiesParams = {
            Parameter("radius", "Radius", &killAllEnemiesRadius, 50.0f, 5000.0f),
            Parameter("snapNeck", "Snap Neck", &snapNeckEnemies)
        };

        Function("Kill All Enemies")
            .WithKey(&killAllEnemiesKey)
            .WithParams(killAllEnemiesParams)
            .Action([this]() {
                SDK::TArray<SDK::AActor*> actors;
                SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AWillie_BP_C::StaticClass(), &actors);
                for (SDK::AActor* actor : actors) {
                    SDK::AWillie_BP_C* willie = static_cast<SDK::AWillie_BP_C*>(actor);
                    if (willie == player || player->GetDistanceTo(willie) > killAllEnemiesRadius) continue;
                    if (snapNeckEnemies)
                        willie->Snap_Neck();
                    else
                        willie->Death();
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
                    SDK::AWillie_BP_C* willie = static_cast<SDK::AWillie_BP_C*>(actor);
                    if (willie == player || player->GetDistanceTo(willie) > toggleEnemyAIRadius) continue;
                    if (SDK::AAIController* controller = static_cast<SDK::AAIController*>(willie->GetController())) {
                        if (!computed) {
                            newTickEnabled = !controller->IsActorTickEnabled();
                            computed = true;
                        }
                        controller->SetActorTickEnabled(newTickEnabled);
                    }
                }
            }, player, world);

        std::initializer_list<Parameter> destroyWilliesParams = {
            Parameter("deadOnly", "Only Dead", &destroyDeadOnly),
            Parameter("disintegrate", "Disintegrate", &destroyDisintegrate)
        };

        Function("Destroy All Willies")
            .WithKey(&destroyWilliesKey)
            .WithParams(destroyWilliesParams)
            .Action([this]() {
                SDK::TArray<SDK::AActor*> actors;
                SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AWillie_BP_C::StaticClass(), &actors);
                for (auto* actor : actors)
                    if (auto* w = static_cast<SDK::AWillie_BP_C*>(actor);
                        w != player && (!destroyDeadOnly || w->Health <= 0))
                        destroyDisintegrate
                            ? w->Disintegrate_and_drop_armor(true)
                            : w->K2_DestroyActor();
            }, player, world);

        std::initializer_list<Parameter> clearBloodParams = {
            Parameter("amount", "Amount", &clearBloodAmount, 0.0f, 1.0f)
        };

        Function("Clear Blood")
            .WithKey(&clearBloodKey)
            .WithParams(clearBloodParams)
            .Action([this]() {
                static_cast<SDK::AArena_Cutting_Map_C*>(world->PersistentLevel->LevelScriptActor)->Clean_Blood(clearBloodAmount);
            }, world);
    }
};