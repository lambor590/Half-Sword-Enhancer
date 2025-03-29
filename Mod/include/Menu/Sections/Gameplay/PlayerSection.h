#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"

class PlayerSection : public CollapsibleSection {
private:
    static inline int saveLoadoutKey = 0x54; // T
    static inline int infiniteStaminaKey = 0x49; // I

    static inline int jumpKey = 0x4A; // J
    static inline float jumpForce = 5000.0f;

    static inline int playerSpeedKey = 0x50; // P
    static inline float playerRunMultiplier = 1.0f;
    static inline float playerWalkMultiplier = 1.0f;

    static inline int playerStrengthKey = -1;
    static inline float playerStrengthMultiplier = 1.0f;
    static inline float playerGrabForceMultiplier = 1000.0f;

    static inline int godModeKey = -1;

public:
    PlayerSection() : CollapsibleSection("Player") {
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

        std::initializer_list<Parameter> playerSpeedParams = {
            Parameter("run_speed", "Run Speed", &playerRunMultiplier, 1.0f, 100.0f),
            Parameter("walk_speed", "Walk Speed", &playerWalkMultiplier, 1.0f, 100.0f)
        };

        BindWithParams("Toggle Speed Multiplier", &playerSpeedKey, playerSpeedParams, [this]() {
            player->Walk_Speed_Rate_Run = (player->Walk_Speed_Rate_Run == 1.5f) ? (1.5f * playerWalkMultiplier) : 1.5f;
            player->Running_Speed_Rate = (player->Running_Speed_Rate == 1.5f) ? (1.5f * playerRunMultiplier) : 1.5f;
        }, player);

        std::initializer_list<Parameter> playerStrengthParams = {
            Parameter("strength", "Strength Multiplier", &playerStrengthMultiplier, 1.0f, 10.0f),
            Parameter("grab_force", "Grab Force Limit", &playerGrabForceMultiplier, 1.0f, 10.0f)
        };

        BindWithParams("Toggle Strength Multiplier", &playerStrengthKey, playerStrengthParams, [this]() {
            player->Muscle_Power = (player->Muscle_Power == 35.0f) ? (35.0f * playerStrengthMultiplier) : 35.0f;
            player->R_Grab_Force_Limit = (player->R_Grab_Force_Limit == 10000.0f) ? (10000.0f * playerGrabForceMultiplier) : 10000.0f;
            player->L_Grab_Force_Limit = (player->L_Grab_Force_Limit == 10000.0f) ? (10000.0f * playerGrabForceMultiplier) : 10000.0f;
        }, player);

        Bind("Toggle God Mode", &godModeKey, [this]() {
            player->bCanBeDamaged = ~player->bCanBeDamaged;
            player->Invulnerable = !player->Invulnerable;
        }, player);
    }
};