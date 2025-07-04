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
    static constexpr float DEFAULT_HEALTH = 100.0f;
    static constexpr float DEFAULT_PAIN = 0.0f;
    static constexpr float DEFAULT_PLAYER_SPEED = 1.5f;
    static constexpr float DEFAULT_MUSCLE_POWER = 35.0f;
    static constexpr float DEFAULT_GRAB_FORCE = 10000.0f;
    static constexpr float DEFAULT_HANDS_RIGIDITY = 0.666f;
    static constexpr float DEFAULT_ALL_BODY_TONUS = 100.0f;
    static constexpr float MAX_DISTANCE = FLT_MAX;
    
    static inline int saveLoadoutKey = 0x54; // T
    static inline int infiniteStaminaKey = 0x49; // I
    static inline int infiniteConsciousnessKey = -1;
    static inline int enemyInfiniteConsciousnessKey = -1;
    static inline int getUpKey = -1;
    static inline int possessWillieKey = -1;
    static inline int invulnerabilityKey = -1;
    static inline int noPainKey = -1;
    static inline int noKickCooldownKey = -1;
    static inline int enemyNoPainKey = -1;
    static inline int ragdollKey = -1;
    static inline int enemyRagdollKey = -1;

    static inline int jumpKey = 0x4A; // J
    static inline float jumpForce = 5000.0f;

    static inline int playerSpeedKey = 0x50; // P
    static inline float playerRunMultiplier = 1.0f;
    static inline float playerWalkMultiplier = 1.0f;

    static inline int playerStrengthKey = -1;
    static inline float playerStrengthMultiplier = 1.0f;
    static inline float playerGrabForceMultiplier = 1.0f;
    static inline float playerHandsRigidityMultiplier = 1.0f;

    static inline int bodyTonusKey = -1;
    static inline float bodyTonusAllBodyMultiplier = 1.0f;
    static inline bool bodyTonusNoBodyWeakening = false;

    static inline int dashKey = -1;
    static inline float dashForce = 7000.0f;

    void applyNoPainEffect(SDK::AWillie_BP_C* willie) noexcept {
        willie->Health = DEFAULT_HEALTH;
        willie->Neck_Health = DEFAULT_HEALTH;
        willie->Head_Health = DEFAULT_HEALTH;
        willie->Body_Upper_Health = DEFAULT_HEALTH;
        willie->Body_Lower_Health = DEFAULT_HEALTH;
        willie->Arm_R_Health = DEFAULT_HEALTH;
        willie->Arm_L_Health = DEFAULT_HEALTH;
        willie->Leg_R_Health = DEFAULT_HEALTH;
        willie->Leg_L_Health = DEFAULT_HEALTH;
        willie->Head_Health__Crush_ = DEFAULT_HEALTH;
        willie->Pain_Lower_Body = DEFAULT_PAIN;
        willie->Pain_Upper_Body = DEFAULT_PAIN;
        willie->Pain_Neck = DEFAULT_PAIN;
        willie->Pain_Head = DEFAULT_PAIN;
        willie->Pain_Arm_R = DEFAULT_PAIN;
        willie->Pain_Arm_L = DEFAULT_PAIN;
        willie->Pain_Leg_R = DEFAULT_PAIN;
        willie->Pain_Leg_L = DEFAULT_PAIN;
        willie->Pain = DEFAULT_PAIN;
        willie->Pain_L_Arm_Alpha = DEFAULT_PAIN;
        willie->Pain_R_Arm_Alpha = DEFAULT_PAIN;
        willie->Pain_Shock = DEFAULT_PAIN;
        willie->Current_Pain_Threshold = DEFAULT_PAIN;
        willie->Pain_Grab_Rate = DEFAULT_PAIN;
        willie->Pain_Shock_Rate = DEFAULT_PAIN;
        willie->Pain_Shock_Interp = DEFAULT_PAIN;
        willie->Sustained_Damage = DEFAULT_PAIN;
    }

public:
    PlayerSection() : CollapsibleSection("Player") {
        Function("Infinite Stamina")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&infiniteStaminaKey)
            .WithTooltip("Keeps your stamina bar full at all times")
            .Action([this]() {
                player->Stamina = DEFAULT_HEALTH;
            }, player);

        Function("Infinite Consciousness")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&infiniteConsciousnessKey)
            .WithTooltip("Prevents you from losing consciousness, so you can't be knocked out")
            .Action([this]() {
                player->Consciousness_Cap = DEFAULT_HEALTH;
                player->Consciousness = DEFAULT_HEALTH;
                player->Consciousness_2__Legs_ = DEFAULT_HEALTH;
            }, player);

        Function("Enemy Infinite Consciousness")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&enemyInfiniteConsciousnessKey)
            .WithTooltip("Enemies can't be knocked out")
            .Action([this]() {
                SDK::TArray<SDK::AActor*> actors;
                SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AWillie_BP_C::StaticClass(), &actors);
                for (auto* actor : actors) {
                    auto* willie = static_cast<SDK::AWillie_BP_C*>(actor);
                    if (willie == player || !willie) [[unlikely]] continue;
                    willie->Consciousness_Cap = DEFAULT_HEALTH;
                    willie->Consciousness = DEFAULT_HEALTH;
                    willie->Consciousness_2__Legs_ = DEFAULT_HEALTH;
                }
            }, player, world);

        Function("Save Loadout")
            .WithKey(&saveLoadoutKey)
            .WithTooltip("Saves your current weapons and clothes for next fight. Only works in free mode.")
            .Action([this]() {
                player->Save_Loadout();
            }, player);

        Function("Jump")
            .WithKey(&jumpKey)
            .WithParams({ Parameter("force", "Force", &jumpForce, 1000.0f, 10000.0f, "Controls how high you jump") })
            .WithTooltip("Jump with configurable force. There's no way to make it more natural, so it will always be a bit floaty.")
            .Action([this]() {
                player->Mesh->AddImpulse(SDK::FVector(0.0f, 0.0f, jumpForce), SDK::FName(), true);
            }, player);

        std::initializer_list<Parameter> playerSpeedParams = {
            Parameter("run_speed_multiplier", "Run Speed Multiplier", &playerRunMultiplier, 1.0f, 100.0f, "Makes you run faster in a natural way"),
            Parameter("walk_speed_multiplier", "Walk Speed Multiplier", &playerWalkMultiplier, 1.0f, 100.0f, "Makes you walk faster in a natural way")
        };

        Function("Speed Multiplier")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&playerSpeedKey)
            .Toggle()
            .WithParams(playerSpeedParams)
            .WithTooltip("Speed multiplier for running and walking. More noticible when you run a long distance.")
            .Action([this](bool active) {
                player->Running_Speed_Rate = active ? (DEFAULT_PLAYER_SPEED * playerRunMultiplier) : DEFAULT_PLAYER_SPEED;
                player->Walk_Speed_Rate_Run = active ? (DEFAULT_PLAYER_SPEED * playerWalkMultiplier) : DEFAULT_PLAYER_SPEED;
            }, player);

        std::initializer_list<Parameter> playerStrengthParams = {
            Parameter("strength_multiplier", "Strength Multiplier", &playerStrengthMultiplier, 1.0f, 10.0f, "Makes your body more rigid and responsive. If set 4+, enable Custom Body Tonus and set its multiplier to a value in which you don't lose balance."),
            Parameter("grab_force_multiplier", "Grab Force Multiplier", &playerGrabForceMultiplier, 1.0f, 10.0f, "Makes it harder for your hands to loose grip. I believe there's a random chance to loose grip, so this is not 100% reliable. I haven't found a way to have permanent grip yet."),
            Parameter("hands_rigidity_multiplier", "Hands Rigidity Multiplier", &playerHandsRigidityMultiplier, 1.0f, 10.0f, "Makes your punches hit harder")
        };

        Function("Strength Multiplier")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&playerStrengthKey)
            .Toggle()
            .WithParams(playerStrengthParams)
            .WithTooltip("Strength multiplier for muscle power, grab force and hands rigidity")
            .Action([this](bool active) {
                player->Muscle_Power = active ? (DEFAULT_MUSCLE_POWER * playerStrengthMultiplier) : DEFAULT_MUSCLE_POWER;
                player->R_Grab_Force_Limit = active ? (DEFAULT_GRAB_FORCE * playerGrabForceMultiplier) : DEFAULT_GRAB_FORCE;
                player->L_Grab_Force_Limit = active ? (DEFAULT_GRAB_FORCE * playerGrabForceMultiplier) : DEFAULT_GRAB_FORCE;
                player->Hands_Rigidity__Gauntlets_ = active ? (DEFAULT_HANDS_RIGIDITY * playerHandsRigidityMultiplier) : DEFAULT_HANDS_RIGIDITY;
            }, player);

        std::initializer_list<Parameter> bodyTonusParams = {
            Parameter("all_body", "All Body Tonus Multiplier", &bodyTonusAllBodyMultiplier, 1.0f, 10.0f, "Controls overall body muscle tension and strength. Heavily affects your movement speed."),
            Parameter("no_body_weakening", "No Body Weakening", &bodyTonusNoBodyWeakening, "Prevents body parts from becoming weak or limp when getting hit")
        };

        Function("Custom Body Tonus")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&bodyTonusKey)
            .WithParams(bodyTonusParams)
            .WithTooltip("Adjusts muscle tension and prevents body weakening. Heavily affects your movement speed.")
            .Action([this]() {
                player->All_Body_Tonus = DEFAULT_ALL_BODY_TONUS * bodyTonusAllBodyMultiplier;
                if (bodyTonusNoBodyWeakening) [[unlikely]] {
                    constexpr float FULL_TONUS = 1.0f;
                    player->Head_Tonus = FULL_TONUS;
                    player->Arm_L_Tonus = FULL_TONUS;
                    player->Arm_R_Tonus = FULL_TONUS;
                    player->Leg_L_Tonus = FULL_TONUS;
                    player->Leg_R_Tonus = FULL_TONUS;
                }
            }, player);

        Function("Ragdoll")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&ragdollKey)
            .WithTooltip("Makes character go completely limp and ragdoll")
            .Action([this]() {
                player->All_Body_Tonus = DEFAULT_PAIN;
            }, player);

        Function("Enemy Ragdoll")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&enemyRagdollKey)
            .WithTooltip("Makes all enemies go limp and ragdoll")
            .Action([this]() {
                SDK::TArray<SDK::AActor*> actors;
                SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AWillie_BP_C::StaticClass(), &actors);
                for (auto* actor : actors) {
                    auto* willie = static_cast<SDK::AWillie_BP_C*>(actor);
                    if (willie == player || !willie) [[unlikely]] continue;
                    willie->All_Body_Tonus = DEFAULT_PAIN;
                }
            }, player, world);

        Function("No Kick Cooldown")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&noKickCooldownKey)
            .WithTooltip("Removes cooldown between kicks for rapid kicking")
            .Action([this]() {
                player->Kick_Cooldown = false;
            }, player);

        Function("Invulnerability")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&invulnerabilityKey)
            .Toggle()
            .WithTooltip("Makes you immune to all damage like a god")
            .Action([this](bool active) {
                player->BitPad_5C_0 = active;
                player->Invulnerable = active;
            }, player);

        Function("No Pain")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&noPainKey)
            .WithTooltip("Makes you immune to pain and removes all pain effects")
            .Action([this]() {
                this->applyNoPainEffect(player);
            }, player);

        Function("Enemy No Pain")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&enemyNoPainKey)
            .WithTooltip("Makes all enemies immune to pain and removes their pain effects")
            .Action([this]() {
                SDK::TArray<SDK::AActor*> actors;
                SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AWillie_BP_C::StaticClass(), &actors);
                for (auto* actor : actors) {
                    auto* willie = static_cast<SDK::AWillie_BP_C*>(actor);
                    if (willie == player || !willie) [[unlikely]] continue;
                    this->applyNoPainEffect(willie);
                }
            }, player, world);

        Function("Get Up")
            .WithKey(&getUpKey)
            .WithTooltip("Forces you to stand up when knocked down")
            .Action([this]() {
                constexpr float GET_UP_RATE = 1.0f;
                player->Get_Up_Rate = GET_UP_RATE;
            }, player);

        Function("Dash")
            .WithKey(&dashKey)
            .WithParams({ Parameter("force", "Force", &dashForce, 1000.0f, 10000.0f, "Controls how fast you dash") })
            .WithTooltip("Dash forward with configurable force")
            .Action([this]() {
                SDK::FVector forwardVector = player->GetActorForwardVector();
                player->Mesh->AddImpulse(forwardVector * dashForce, SDK::FName(), true);
            }, player);

        Function("Possess Nearest Willie")
            .WithKey(&possessWillieKey)
            .WithTooltip("Take control of the closest NPC")
            .Action([this]() {
                static SDK::AAIController* prevAIController = nullptr;
                static SDK::APawn* originalPawn = nullptr;
                static SDK::AWillie_BP_C* possessedWillie = nullptr;
                
                SDK::APawn* currentPawn = controller->K2_GetPawn();
                if (possessedWillie && currentPawn != possessedWillie) [[unlikely]] {
                    prevAIController = nullptr;
                    originalPawn = nullptr;
                    possessedWillie = nullptr;
                }

                if (!possessedWillie) [[likely]] {
                    originalPawn = currentPawn;
                    SDK::TArray<SDK::AActor*> actors;
                    SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AWillie_BP_C::StaticClass(), &actors);

                    SDK::AWillie_BP_C* nearest = nullptr;
                    float minDist = MAX_DISTANCE;
                    for (auto* actor : actors) {
                        auto* willie = static_cast<SDK::AWillie_BP_C*>(actor);
                        if (willie == player) [[unlikely]] continue;
                        
                        const float dist = player->GetDistanceTo(willie);
                        if (dist < minDist) [[likely]] {
                            minDist = dist;
                            nearest = willie;
                        }
                    }
                    if (!nearest) [[unlikely]] return;

                    if (!nearest->IsA(SDK::AWillie_BP_NoBrain_C::StaticClass())) [[likely]] {
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
                    if (prevAIController) [[likely]] {
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