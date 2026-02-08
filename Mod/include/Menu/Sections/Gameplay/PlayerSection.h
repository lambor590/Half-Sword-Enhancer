#pragma once

#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "SDK/AIModule_classes.hpp"
#include "Hooks/GameHook.h"
#include "Utils/GameConstants.h"
#include "Utils/ActorUtils.h"

class PlayerSection : public CollapsibleSection {
private:
    SectionConfig::PlayerConfig& cfg = SectionConfig::player;

public:
    PlayerSection() : CollapsibleSection("Player") {
        Function("Infinite Stamina")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&cfg.infiniteStaminaKey)
            .WithTooltip("Keeps your stamina bar full at all times")
            .Action([this]() {
                player->Stamina = GameConstants::DEFAULT_HEALTH;
            }, player);

        Function("Infinite Consciousness")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&cfg.infiniteConsciousnessKey)
            .WithTooltip("Prevents you from losing consciousness, so you can't be knocked out")
            .Action([this]() {
                ActorUtils::SetInfiniteConsciousness(player);
            }, player);

        Function("Enemy Infinite Consciousness")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&cfg.enemyInfiniteConsciousnessKey)
            .WithTooltip("Enemies can't be knocked out")
            .Action([this]() {
                ActorUtils::ForEachWillie(world, player, ActorUtils::SetInfiniteConsciousness);
            }, player, world);

        Function("Jump")
            .WithKey(&cfg.jumpKey)
            .WithParams({ Parameter("force", "Force", &cfg.jumpForce, 1000.0f, 10000.0f, "Controls how high you jump") })
            .WithTooltip("Jump with configurable force. There's no way to make it more natural, so it will always be a bit floaty.")
            .Action([this]() {
                player->Mesh->AddImpulse(SDK::FVector(0.0f, 0.0f, cfg.jumpForce), SDK::FName(), true);
            }, player);

        Function("Speed Multiplier")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&cfg.playerSpeedKey)
            .Toggle()
            .WithParams({
                Parameter("run_speed_multiplier", "Run Speed Multiplier", &cfg.playerRunMultiplier, 1.0f, 100.0f, "Makes you run faster in a natural way"),
                Parameter("walk_speed_multiplier", "Walk Speed Multiplier", &cfg.playerWalkMultiplier, 1.0f, 100.0f, "Makes you walk faster in a natural way")
            })
            .WithTooltip("Speed multiplier for running and walking. More noticible when you run a long distance.")
            .Action([this](bool active) {
                player->Running_Speed_Rate = active ? (GameConstants::DEFAULT_PLAYER_SPEED * cfg.playerRunMultiplier) : GameConstants::DEFAULT_PLAYER_SPEED;
                player->Walk_Speed_Rate_Run = active ? (GameConstants::DEFAULT_PLAYER_SPEED * cfg.playerWalkMultiplier) : GameConstants::DEFAULT_PLAYER_SPEED;
            }, player);

        Function("Strength Multiplier")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&cfg.playerStrengthKey)
            .Toggle()
            .WithParams({
                Parameter("strength_multiplier", "Strength Multiplier", &cfg.playerStrengthMultiplier, 1.0f, 10.0f, "Makes your body more rigid and responsive. If set 4+, enable Custom Body Tonus and set its multiplier to a value in which you don't lose balance."),
                Parameter("grab_force_multiplier", "Grab Force Multiplier", &cfg.playerGrabForceMultiplier, 1.0f, 10.0f, "Makes it harder for your hands to loose grip. I believe there's a random chance to loose grip, so this is not 100% reliable. I haven't found a way to have permanent grip yet."),
                Parameter("hands_rigidity_multiplier", "Hands Rigidity Multiplier", &cfg.playerHandsRigidityMultiplier, 1.0f, 10.0f, "Makes your punches hit harder")
            })
            .WithTooltip("Strength multiplier for muscle power, grab force and hands rigidity")
            .Action([this](bool active) {
                player->Muscle_Power = active ? (GameConstants::DEFAULT_MUSCLE_POWER * cfg.playerStrengthMultiplier) : GameConstants::DEFAULT_MUSCLE_POWER;
                player->R_Grab_Force_Limit = active ? (GameConstants::DEFAULT_GRAB_FORCE * cfg.playerGrabForceMultiplier) : GameConstants::DEFAULT_GRAB_FORCE;
                player->L_Grab_Force_Limit = active ? (GameConstants::DEFAULT_GRAB_FORCE * cfg.playerGrabForceMultiplier) : GameConstants::DEFAULT_GRAB_FORCE;
                player->Hands_Rigidity__Gauntlets_ = active ? (GameConstants::DEFAULT_HANDS_RIGIDITY * cfg.playerHandsRigidityMultiplier) : GameConstants::DEFAULT_HANDS_RIGIDITY;
            }, player);

        Function("Custom Body Tonus")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&cfg.bodyTonusKey)
            .WithParams({
                Parameter("all_body", "All Body Tonus Multiplier", &cfg.bodyTonusAllBodyMultiplier, 1.0f, 10.0f, "Controls overall body muscle tension and strength. Heavily affects your movement speed."),
                Parameter("no_body_weakening", "No Body Weakening", &cfg.bodyTonusNoBodyWeakening, "Prevents body parts from becoming weak or limp when getting hit")
            })
            .WithTooltip("Adjusts muscle tension and prevents body weakening. Heavily affects your movement speed.")
            .Action([this]() {
                player->All_Body_Tonus = GameConstants::DEFAULT_ALL_BODY_TONUS * cfg.bodyTonusAllBodyMultiplier;
                if (cfg.bodyTonusNoBodyWeakening) [[unlikely]] {
                    player->Head_Tonus = GameConstants::FULL_TONUS;
                    player->Arm_L_Tonus = GameConstants::FULL_TONUS;
                    player->Arm_R_Tonus = GameConstants::FULL_TONUS;
                    player->Leg_L_Tonus = GameConstants::FULL_TONUS;
                    player->Leg_R_Tonus = GameConstants::FULL_TONUS;
                }
            }, player);

        Function("Ragdoll")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&cfg.ragdollKey)
            .WithTooltip("Makes character go completely limp and ragdoll")
            .Action([this]() {
                player->All_Body_Tonus = GameConstants::DEFAULT_PAIN;
            }, player);

        Function("Enemy Ragdoll")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&cfg.enemyRagdollKey)
            .WithTooltip("Makes all enemies go limp and ragdoll")
            .Action([this]() {
                ActorUtils::ForEachWillie(world, player, [](SDK::AWillie_BP_C* willie) {
                    willie->All_Body_Tonus = GameConstants::DEFAULT_PAIN;
                });
            }, player, world);

        Function("No Kick Cooldown")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&cfg.noKickCooldownKey)
            .WithTooltip("Removes cooldown between kicks for rapid kicking")
            .Action([this]() {
                player->Kick_Cooldown = false;
            }, player);

        Function("Invulnerability")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&cfg.invulnerabilityKey)
            .Toggle()
            .WithTooltip("Makes you immune to all damage like a god")
            .Action([this](bool active) {
                player->BitPad_5C_0 = active;
                player->Invulnerable = active;
            }, player);

        Function("No Pain")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&cfg.noPainKey)
            .WithTooltip("Makes you immune to pain and removes all pain effects")
            .Action([this]() {
                ActorUtils::ApplyNoPainEffect(player);
            }, player);

        Function("Enemy No Pain")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&cfg.enemyNoPainKey)
            .WithTooltip("Makes all enemies immune to pain and removes their pain effects")
            .Action([this]() {
                ActorUtils::ForEachWillie(world, player, ActorUtils::ApplyNoPainEffect);
            }, player, world);

        Function("Get Up")
            .WithKey(&cfg.getUpKey)
            .WithTooltip("Forces you to stand up when knocked down")
            .Action([this]() {
                player->Get_Up_Rate = GameConstants::GET_UP_RATE;
            }, player);

        Function("Dash")
            .WithKey(&cfg.dashKey)
            .WithParams({ Parameter("force", "Force", &cfg.dashForce, 1000.0f, 10000.0f, "Controls how fast you dash") })
            .WithTooltip("Dash forward with configurable force")
            .Action([this]() {
                SDK::FVector forwardVector = player->GetActorForwardVector();
                player->Mesh->AddImpulse(forwardVector * cfg.dashForce, SDK::FName(), true);
            }, player);

        Function("Possess Nearest Willie")
            .WithKey(&cfg.possessWillieKey)
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
                    SDK::AWillie_BP_C* nearest = nullptr;
                    float minDist = GameConstants::MAX_DISTANCE;

                    ActorUtils::ForEachWillie(world, player, [&](SDK::AWillie_BP_C* willie) {
                        const float dist = player->GetDistanceTo(willie);
                        if (dist < minDist) {
                            minDist = dist;
                            nearest = willie;
                        }
                    });

                    if (!nearest) [[unlikely]] return;

                    if (nearest->IsA(SDK::AWillie_BP_C::StaticClass())) [[likely]] {
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
