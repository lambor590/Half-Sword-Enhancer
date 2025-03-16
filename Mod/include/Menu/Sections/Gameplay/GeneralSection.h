#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"

class GeneralSection : public CollapsibleSection {
private:
    static inline int sloMoKey = 0x5A; // Z
    static inline float slowMotionSpeed = 0.4f;

    static inline int customGravityKey = 0x4C; // L
    static inline float customGravityValue = 0.0f;

    static inline int saveLoadoutKey = 0x54; // T
    static inline int infiniteStaminaKey = 0x49; // I

    static inline int jumpKey = 0x4A; // J
    static inline float jumpForce = 3000.0f;

public:
    GeneralSection() : CollapsibleSection("General") {
        std::initializer_list<Parameter> slowMotionParams = {
            Parameter("speed", "Speed", &slowMotionSpeed, 0.01f, 0.99f)
        };

        BindWithParams("Toggle Slow Motion", &sloMoKey, slowMotionParams, [this]() {
            worldSettings->TimeDilation = (worldSettings->TimeDilation == 1.0f) ? slowMotionSpeed : 1.0f;
        }, worldSettings);

        std::initializer_list<Parameter> customGravityParams = {
            Parameter("gravity", "Gravity", &customGravityValue, -3000.0f, 3000.0f)
        };

        BindWithParams("Toggle Custom Gravity", &customGravityKey, customGravityParams, [this]() {
            worldSettings->bWorldGravitySet = true;
            worldSettings->WorldGravityZ = (worldSettings->WorldGravityZ == -980.0f) ? customGravityValue : -980.0f;
        }, worldSettings);


        Hook("Infinite Stamina", "OnWalkingOffLedge", &infiniteStaminaKey, [this]() {
            player->Stamina = 100.0f;
        }, player);

        Bind("Save Loadout", &saveLoadoutKey, [this]() {
            player->Save_Loadout();
        }, player);
        
        std::initializer_list<Parameter> jumpParams = {
            Parameter("force", "Force", &jumpForce, 1000.0f, 10000.0f)
        };

        BindWithParams("Jump", &jumpKey, jumpParams, [this]() {
            player->Mesh->AddImpulse(SDK::FVector(0.0f, 0.0f, jumpForce), SDK::FName(), true);
        }, player);
    }
};