#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"
#include "Menu/Utils/Spawner.h"

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
    }
};