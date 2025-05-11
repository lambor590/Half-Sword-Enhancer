#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"
#include "SDK/AIModule_classes.hpp"
#include "SDK/Willie_BP_NoBrain_classes.hpp"
#include "Hooks/GameHook.h"

class PlayerSection : public CollapsibleSection {
private:
    static inline int saveLoadoutKey = 0x54; // T
    static inline int infiniteStaminaKey = 0x49; // I
    static inline int infiniteCounciousnessKey = -1;
    static inline int getUpKey = -1;
    static inline int possessWillieKey = -1;
    static inline int invulnerabilityKey = -1;
    static inline int noPainKey = -1;
    static inline int noKickCooldownKey = -1;

    static inline int jumpKey = 0x4A; // J
    static inline float jumpForce = 5000.0f;

    static inline int playerSpeedKey = 0x50; // P
    static inline float playerRunMultiplier = 1.0f;
    static inline float playerWalkMultiplier = 1.0f;

    static inline int playerStrengthKey = -1;
    static inline float playerStrengthMultiplier = 1.0f;
    static inline float playerGrabForceMultiplier = 1.0f;
    static inline float playerHandsRigidityMultiplier = 1.0f;

    static inline int dashKey = -1;
    static inline float dashForce = 7000.0f;

public:
    PlayerSection() : CollapsibleSection("Player") {
        Function("Infinite Stamina")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&infiniteStaminaKey)
            .Action([this]() {
                player->Stamina = 100.0f;
            }, player);

        Function("Infinite Counciousness")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&infiniteCounciousnessKey)
            .Action([this]() {
                player->Consciousness = 100.0f;
            }, player);

        Function("Save Loadout")
            .WithKey(&saveLoadoutKey)
            .Action([this]() {
                player->Save_Loadout();
            }, player);

        Function("Jump")
            .WithKey(&jumpKey)
            .WithParams({ Parameter("force", "Force", &jumpForce, 1000.0f, 10000.0f) })
            .Action([this]() {
                player->Mesh->AddImpulse(SDK::FVector(0.0f, 0.0f, jumpForce), SDK::FName(), true);
            }, player);

        std::initializer_list<Parameter> playerSpeedParams = {
            Parameter("run_speed_multiplier", "Run Speed Multiplier", &playerRunMultiplier, 1.0f, 100.0f),
            Parameter("walk_speed_multiplier", "Walk Speed Multiplier", &playerWalkMultiplier, 1.0f, 100.0f)
        };

        Function("Speed Multiplier")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&playerSpeedKey)
            .Toggle()
            .WithParams(playerSpeedParams)
            .Action([this](bool active) {
                player->Running_Speed_Rate = active ? (1.5f * playerRunMultiplier) : 1.5f;
                player->Walk_Speed_Rate_Run = active ? (1.5f * playerWalkMultiplier) : 1.5f;
            }, player);

        std::initializer_list<Parameter> playerStrengthParams = {
            Parameter("strength_multiplier", "Strength Multiplier", &playerStrengthMultiplier, 1.0f, 3.5f),
            Parameter("grab_force_multiplier", "Grab Force Multiplier", &playerGrabForceMultiplier, 1.0f, 10.0f),
            Parameter("hands_rigidity_multiplier", "Hands Rigidity Multiplier", &playerHandsRigidityMultiplier, 1.0f, 10.0f)
        };

        Function("Strength Multiplier")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&playerStrengthKey)
            .Toggle()
            .WithParams(playerStrengthParams)
            .Action([this](bool active) {
                player->Muscle_Power = active ? (35.0f * playerStrengthMultiplier) : 35.0f;
                player->R_Grab_Force_Limit = active ? (10000.0f * playerGrabForceMultiplier) : 10000.0f;
                player->L_Grab_Force_Limit = active ? (10000.0f * playerGrabForceMultiplier) : 10000.0f;
                player->Hands_Rigidity__Gauntlets_ = active ? (0.666f * playerHandsRigidityMultiplier) : 0.666f;
            }, player);

        Function("No Kick Cooldown")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&noKickCooldownKey)
            .Action([this]() {
                player->Kick_Cooldown = false;
            }, player);

        Function("Invulnerability")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&invulnerabilityKey)
            .Toggle()
            .Action([this](bool active) {
                player->BitPad_5C_0 = active;
                player->Invulnerable = active;
            }, player);

        Function("No Pain")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&noPainKey)
            .Action([this]() {
                player->Health = 100.0f;
                player->Neck_Health = 10000000.0f;
                player->Head_Health = 10000000.0f;
                player->Body_Upper_Health = 100.0f;
                player->Body_Lower_Health = 100.0f;
                player->Arm_R_Health = 100.0f;
                player->Arm_L_Health = 100.0f;
                player->Leg_R_Health = 100.0f;
                player->Leg_L_Health = 100.0f;
                player->Head_Health__Crush_ = 100.0f;
                player->Pain_Lower_Body = 0.0f;
                player->Pain_Upper_Body = 0.0f;
                player->Pain_Neck = 0.0f;
                player->Pain_Head = 0.0f;
                player->Pain_Arm_R = 0.0f;
                player->Pain_Arm_L = 0.0f;
                player->Pain_Leg_R = 0.0f;
                player->Pain_Leg_L = 0.0f;
                player->Pain = 0.0f;
                player->Pain_L_Arm_Alpha = 0.0f;
                player->Pain_R_Arm_Alpha = 0.0f;
                player->Pain_Shock = 0.0f;
                player->Current_Pain_Threshold = 0.0f;
                player->Pain_Grab_Rate = 0.0f;
                player->Pain_Shock_Rate = 0.0f;
                player->Pain_Shock_Interp = 0.0f;
                player->Health_Threshold_For_Dismemberment = 0.0f;
                player->Sustained_Damage = 0.0f;
            }, player);

        Function("Get Up")
            .WithKey(&getUpKey)
            .Action([this]() {
                player->Get_Up_Rate = 1.0f;
            }, player);

        Function("Dash")
            .WithKey(&dashKey)
            .WithParams({ Parameter("force", "Force", &dashForce, 1000.0f, 10000.0f) })
            .Action([this]() {
                SDK::FVector forwardVector = player->GetActorForwardVector();
                player->Mesh->AddImpulse(forwardVector * dashForce, SDK::FName(), true);
            }, player);

        Function("Possess Nearest Willie")
            .WithKey(&possessWillieKey)
            .Action([this]() {
                static SDK::AAIController* prevAIController = nullptr;
                static SDK::APawn* originalPawn = nullptr;
                static SDK::AWillie_BP_C* possessedWillie = nullptr;
                SDK::APawn* currentPawn = controller->K2_GetPawn();
                if (possessedWillie && currentPawn != possessedWillie) {
                    prevAIController = nullptr;
                    originalPawn = nullptr;
                    possessedWillie = nullptr;
                }

                if (!possessedWillie) {
                    originalPawn = currentPawn;
                    SDK::TArray<SDK::AActor*> actors;
                    SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AWillie_BP_C::StaticClass(), &actors);

                    SDK::AWillie_BP_C* nearest = nullptr;
                    float minDist = FLT_MAX;
                    for (auto* actor : actors) {
                        auto* willie = static_cast<SDK::AWillie_BP_C*>(actor);
                        if (willie == player) continue;
                        float dist = player->GetDistanceTo(willie);
                        if (dist < minDist) {
                            minDist = dist;
                            nearest = willie;
                        }
                    }
                    if (!nearest) return;

                    if (!nearest->IsA(SDK::AWillie_BP_NoBrain_C::StaticClass())) {
                        prevAIController = static_cast<SDK::AAIController*>(nearest->GetController());
                        prevAIController->SetActorTickEnabled(false);
                    }
                    controller->Possess(nearest);
                    nearest->Player = true;
                    possessedWillie = nearest;
                } else {
                    auto* williePawn = static_cast<SDK::AWillie_BP_C*>(currentPawn);
                    controller->Possess(originalPawn);
                    williePawn->Player = false;
                    if (prevAIController) {
                        prevAIController->Possess(williePawn);
                        prevAIController->SetActorTickEnabled(true);
                        prevAIController = nullptr;
                    }
                    possessedWillie = nullptr;
                    originalPawn = nullptr;
                }
            }, player, controller, world);
    }
};