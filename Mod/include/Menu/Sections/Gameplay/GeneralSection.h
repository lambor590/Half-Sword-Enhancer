#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"

class GeneralSection : public CollapsibleSection {
private:
    static inline int sloMoKey = 0x5A; // Z
    static inline float slowMotionSpeed = 0.3f;

    static inline int lowGravityKey = 0x4C; // L
    static inline float lowGravityValue = -100.0f;

    static inline int saveLoadoutKey = 0x54; // T
    static inline int infiniteStaminaKey = 0x49; // I

    static inline int jumpKey = 0x4A; // J
    static inline float jumpForce = 2000.0f;

public:
    GeneralSection() : CollapsibleSection("General") {
        std::initializer_list<Parameter> slowMotionParams = {
            Parameter("speed", "Speed", &slowMotionSpeed, 0.01f, 0.99f)
        };

        BindWithParams("Toggle Slow Motion", &sloMoKey, slowMotionParams, [this]() {
            worldSettings->TimeDilation = (worldSettings->TimeDilation == 1.0f) ? slowMotionSpeed : 1.0f;
        }, worldSettings);

        std::initializer_list<Parameter> lowGravityParams = {
            Parameter("gravity", "Gravity", &lowGravityValue, -5000.0f, 5000.0f)
        };

        BindWithParams("Toggle Low Gravity", &lowGravityKey, lowGravityParams, [this]() {
            worldSettings->bWorldGravitySet = true;
            worldSettings->WorldGravityZ = (worldSettings->WorldGravityZ == -980.0f) ? lowGravityValue : -980.0f;
        }, worldSettings);

        std::initializer_list<Parameter> jumpParams = {
            Parameter("force", "Force", &jumpForce, 1000.0f, 15000.0f)
        };

        BindWithParams("Jump", &jumpKey, jumpParams, [this]() {
            player->Mesh->AddImpulse(SDK::FVector(0.0f, 0.0f, jumpForce), SDK::FName(), true);
        }, player);

        Hook("Infinite Stamina", "OnWalkingOffLedge", &infiniteStaminaKey, [this]() {
            player->Stamina = 100.0f;
        }, player);

        Bind("Save Loadout", &saveLoadoutKey, [this]() {
            player->Save_Loadout();
        }, player);
    }
};