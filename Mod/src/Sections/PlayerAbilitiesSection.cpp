#include "Menu/Sections/Player/PlayerAbilitiesSection.h"
#include "Menu/SectionRegistry.h"
#include "Menu/SectionStyle.h"

REGISTER_SECTION(PlayerAbilitiesSection, MenuTab::Player);
#include "SDK/AIModule_classes.hpp"
#include "Hooks/GameHook.h"
#include "Utils/GameConstants.h"
#include "Utils/ActorUtils.h"
#include "Utils/PossessState.h"

PlayerAbilitiesSection::PlayerAbilitiesSection(ModContext& ctx) : Section(ctx, "Abilities") {
    InitKeybinds();
}

void PlayerAbilitiesSection::Render() {
    const SectionStyle::StyleRAII style;
    KeybindUI::RenderKeybindList(keybinds);
}

void PlayerAbilitiesSection::InitKeybinds() {
    AddKeybind(
        keybinds,
        {
            .name = "Infinite Stamina",
            .tooltip = "Keeps your stamina bar full at all times",
            .configSection = "InfiniteStamina",
            .keyPtr = &cfg.infiniteStaminaKey,
            .callback =
                [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    auto* p = runtime.player;
                    if (!p) return;
                    p->Stamina = GameConstants::DEFAULT_HEALTH;
                },
            .events = {GameEvent::OffLedge},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Infinite Consciousness",
            .tooltip = "Prevents you from losing consciousness, so you can't be knocked out",
            .configSection = "InfiniteConsciousness",
            .keyPtr = &cfg.infiniteConsciousnessKey,
            .callback =
                [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    auto* p = runtime.player;
                    if (!p) return;
                    ActorUtils::SetInfiniteConsciousness(p);
                },
            .events = {GameEvent::OffLedge},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Enemy Infinite Consciousness",
            .tooltip = "Enemies can't be knocked out",
            .configSection = "EnemyInfiniteConsciousness",
            .keyPtr = &cfg.enemyInfiniteConsciousnessKey,
            .callback =
                [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    auto* world = runtime.world;
                    auto* player = runtime.player;
                    if (!player || !world) return;
                    ActorUtils::ForEachWillie(world, player, ActorUtils::SetInfiniteConsciousness);
                },
            .events = {GameEvent::OffLedge},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Consciousness Multiplier",
            .tooltip = "Multiply your consciousness cap to resist knockouts",
            .configSection = "ConsciousnessMultiplier",
            .keyPtr = &cfg.consciousnessMultiplierKey,
            .callback =
                [this](bool active, const RuntimeContextSnapshot& runtime) {
                    auto* p = runtime.player;
                    if (!p) return;
                    float cap = GameConstants::DEFAULT_HEALTH * (active ? cfg.consciousnessMultiplier : 1.0f);
                    p->Consciousness_Cap = cap;
                },
            .runOnToggle = true,
            .events = {GameEvent::OffLedge},
            .params = {KeybindParam(
                "consciousness_multiplier", "Multiplier", &cfg.consciousnessMultiplier, 1.0f, 100.0f,
                "Multiplies consciousness cap"
            )},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Enemy Consciousness Multiplier",
            .tooltip = "Multiply enemy consciousness cap to make them harder to knock out",
            .configSection = "EnemyConsciousnessMultiplier",
            .keyPtr = &cfg.enemyConsciousnessMultiplierKey,
            .callback =
                [this](bool active, const RuntimeContextSnapshot& runtime) {
                    auto* world = runtime.world;
                    auto* player = runtime.player;
                    if (!player || !world) return;
                    float cap = GameConstants::DEFAULT_HEALTH * (active ? cfg.enemyConsciousnessMultiplier : 1.0f);
                    ActorUtils::ForEachWillie(world, player, [cap](SDK::AWillie_BP_C* willie) {
                        willie->Consciousness_Cap = cap;
                    });
                },
            .runOnToggle = true,
            .events = {GameEvent::OffLedge},
            .params = {KeybindParam(
                "enemy_consciousness_multiplier", "Multiplier", &cfg.enemyConsciousnessMultiplier, 1.0f, 100.0f,
                "Multiplies enemy consciousness cap"
            )},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Jump",
            .tooltip = "Jump with configurable force. There's no way to make it more natural, so it will always be a "
                       "bit floaty.",
            .configSection = "Jump",
            .keyPtr = &cfg.jumpKey,
            .callback =
                [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    auto* p = runtime.player;
                    if (!p) return;
                    p->Mesh->AddImpulse(SDK::FVector(0.0f, 0.0f, cfg.jumpForce), SDK::FName(), true);
                },
            .params = {KeybindParam("force", "Force", &cfg.jumpForce, 1000.0f, 10000.0f, "Controls how high you jump")},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Speed Multiplier",
            .tooltip = "Speed multiplier for running and walking. More noticible when you run a long distance.",
            .configSection = "SpeedMultiplier",
            .keyPtr = &cfg.playerSpeedKey,
            .callback =
                [this](bool active, const RuntimeContextSnapshot& runtime) {
                    auto* p = runtime.player;
                    if (!p) return;
                    p->Running_Speed_Rate = active ? (GameConstants::DEFAULT_PLAYER_SPEED * cfg.playerRunMultiplier)
                                                   : GameConstants::DEFAULT_PLAYER_SPEED;
                    p->Walk_Speed_Rate_Run = active ? (GameConstants::DEFAULT_PLAYER_SPEED * cfg.playerWalkMultiplier)
                                                    : GameConstants::DEFAULT_PLAYER_SPEED;
                },
            .runOnToggle = true,
            .events = {GameEvent::OffLedge},
            .params =
                {KeybindParam(
                     "run_speed_multiplier", "Run Speed Multiplier", &cfg.playerRunMultiplier, 1.0f, 100.0f,
                     "Makes you run faster in a natural way"
                 ),
                 KeybindParam(
                     "walk_speed_multiplier", "Walk Speed Multiplier", &cfg.playerWalkMultiplier, 1.0f, 100.0f,
                     "Makes you walk faster in a natural way"
                 )},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Strength Multiplier",
            .tooltip = "Strength multiplier for muscle power, grab force and hands rigidity",
            .configSection = "StrengthMultiplier",
            .keyPtr = &cfg.playerStrengthKey,
            .callback =
                [this](bool active, const RuntimeContextSnapshot& runtime) {
                    auto* p = runtime.player;
                    if (!p) return;
                    p->Muscle_Power = active ? (GameConstants::DEFAULT_MUSCLE_POWER * cfg.playerStrengthMultiplier)
                                             : GameConstants::DEFAULT_MUSCLE_POWER;
                    p->R_Grab_Force_Limit = active ? (GameConstants::DEFAULT_GRAB_FORCE * cfg.playerGrabForceMultiplier)
                                                   : GameConstants::DEFAULT_GRAB_FORCE;
                    p->L_Grab_Force_Limit = active ? (GameConstants::DEFAULT_GRAB_FORCE * cfg.playerGrabForceMultiplier)
                                                   : GameConstants::DEFAULT_GRAB_FORCE;
                    p->Hands_Rigidity__Gauntlets_ =
                        active ? (GameConstants::DEFAULT_HANDS_RIGIDITY * cfg.playerHandsRigidityMultiplier)
                               : GameConstants::DEFAULT_HANDS_RIGIDITY;
                },
            .runOnToggle = true,
            .events = {GameEvent::OffLedge},
            .params =
                {KeybindParam(
                     "strength_multiplier", "Strength Multiplier", &cfg.playerStrengthMultiplier, 1.0f, 10.0f,
                     "Makes your body more rigid and responsive."
                 ),
                 KeybindParam(
                     "grab_force_multiplier", "Grab Force Multiplier", &cfg.playerGrabForceMultiplier, 1.0f, 10.0f,
                     "Makes it harder for your hands to loose grip."
                 ),
                 KeybindParam(
                     "hands_rigidity_multiplier", "Hands Rigidity Multiplier", &cfg.playerHandsRigidityMultiplier, 1.0f,
                     10.0f, "Makes your punches hit harder"
                 )},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Custom Body Tonus",
            .tooltip = "Adjusts muscle tension and prevents body weakening. Heavily affects your movement speed.",
            .configSection = "CustomBodyTonus",
            .keyPtr = &cfg.bodyTonusKey,
            .callback =
                [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    auto* p = runtime.player;
                    if (!p) return;
                    p->All_Body_Tonus = GameConstants::DEFAULT_ALL_BODY_TONUS * cfg.bodyTonusAllBodyMultiplier;
                    if (cfg.bodyTonusNoBodyWeakening) [[unlikely]] {
                        p->Head_Tonus = GameConstants::FULL_TONUS;
                        p->Arm_L_Tonus = GameConstants::FULL_TONUS;
                        p->Arm_R_Tonus = GameConstants::FULL_TONUS;
                        p->Leg_L_Tonus = GameConstants::FULL_TONUS;
                        p->Leg_R_Tonus = GameConstants::FULL_TONUS;
                    }
                },
            .events = {GameEvent::OffLedge},
            .params =
                {KeybindParam(
                     "all_body", "All Body Tonus Multiplier", &cfg.bodyTonusAllBodyMultiplier, 1.0f, 10.0f,
                     "Controls overall body muscle tension and strength."
                 ),
                 KeybindParam(
                     "no_body_weakening", "No Body Weakening", &cfg.bodyTonusNoBodyWeakening,
                     "Prevents body parts from becoming weak or limp when getting hit"
                 )},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Ragdoll",
            .tooltip = "Makes character go completely limp and ragdoll",
            .configSection = "Ragdoll",
            .keyPtr = &cfg.ragdollKey,
            .callback =
                [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    auto* p = runtime.player;
                    if (!p) return;
                    p->All_Body_Tonus = GameConstants::DEFAULT_PAIN;
                },
            .events = {GameEvent::OffLedge},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Enemy Ragdoll",
            .tooltip = "Makes all enemies go limp and ragdoll",
            .configSection = "EnemyRagdoll",
            .keyPtr = &cfg.enemyRagdollKey,
            .callback =
                [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    auto* world = runtime.world;
                    auto* player = runtime.player;
                    if (!player || !world) return;
                    ActorUtils::ForEachWillie(world, player, [](SDK::AWillie_BP_C* willie) {
                        willie->All_Body_Tonus = GameConstants::DEFAULT_PAIN;
                    });
                },
            .events = {GameEvent::OffLedge},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Drunk Enemies",
            .tooltip = "Makes all enemies stumble around drunk",
            .configSection = "DrunkEnemies",
            .keyPtr = &cfg.enemyDrunkKey,
            .callback =
                [this](bool active, const RuntimeContextSnapshot& runtime) {
                    auto* world = runtime.world;
                    auto* player = runtime.player;
                    if (!player || !world) return;
                    ActorUtils::ForEachWillie(world, player, [this, active](SDK::AWillie_BP_C* willie) {
                        willie->Drunk = active ? static_cast<double>(cfg.enemyDrunkLevel) : 0.0;
                    });
                },
            .runOnToggle = true,
            .events = {GameEvent::OffLedge},
            .params = {KeybindParam(
                "drunk_level", "Drunk Level", &cfg.enemyDrunkLevel, 0.0f, 1.0f,
                "How drunk the enemies are (0 = sober, 1 = fully drunk)"
            )},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "No Kick Cooldown",
            .tooltip = "Removes cooldown between kicks for rapid kicking",
            .configSection = "NoKickCooldown",
            .keyPtr = &cfg.noKickCooldownKey,
            .callback =
                [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    auto* p = runtime.player;
                    if (!p) return;
                    p->Kick_Cooldown = false;
                },
            .events = {GameEvent::OffLedge},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Invulnerability",
            .tooltip = "Makes you immune to all damage like a god",
            .configSection = "Invulnerability",
            .keyPtr = &cfg.invulnerabilityKey,
            .callback =
                [this](bool active, const RuntimeContextSnapshot& runtime) {
                    auto* p = runtime.player;
                    if (!p) return;
                    p->BitPad_5C_0 = active;
                    p->Invulnerable = active;
                },
            .runOnToggle = true,
            .events = {GameEvent::OffLedge},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "No Pain",
            .tooltip = "Makes you immune to pain and removes all pain effects",
            .configSection = "NoPain",
            .keyPtr = &cfg.noPainKey,
            .callback =
                [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    auto* p = runtime.player;
                    if (!p) return;
                    ActorUtils::ApplyNoPainEffect(p);
                },
            .events = {GameEvent::OffLedge},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Enemy No Pain",
            .tooltip = "Makes all enemies immune to pain and removes their pain effects",
            .configSection = "EnemyNoPain",
            .keyPtr = &cfg.enemyNoPainKey,
            .callback =
                [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    auto* world = runtime.world;
                    auto* player = runtime.player;
                    if (!player || !world) return;
                    ActorUtils::ForEachWillie(world, player, ActorUtils::ApplyNoPainEffect);
                },
            .events = {GameEvent::OffLedge},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Get Up",
            .tooltip = "Forces you to stand up when knocked down",
            .configSection = "GetUp",
            .keyPtr = &cfg.getUpKey,
            .callback =
                [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    auto* p = runtime.player;
                    if (!p) return;
                    p->Get_Up_Rate = GameConstants::GET_UP_RATE;
                },
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Dash",
            .tooltip = "Dash forward with configurable force",
            .configSection = "Dash",
            .keyPtr = &cfg.dashKey,
            .callback =
                [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    auto* p = runtime.player;
                    if (!p) return;
                    SDK::FVector forwardVector = p->GetActorForwardVector();
                    p->Mesh->AddImpulse(forwardVector * cfg.dashForce, SDK::FName(), true);
                },
            .params = {KeybindParam("force", "Force", &cfg.dashForce, 1000.0f, 10000.0f, "Controls how fast you dash")},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Bite Attack",
            .tooltip = "Bite the nearest enemy like a zombie",
            .configSection = "BiteAttack",
            .keyPtr = &cfg.biteAttackKey,
            .callback =
                [this](bool active, const RuntimeContextSnapshot& runtime) {
                    ActorUtils::ApplyBiteState(runtime.player, active);
                },
            .runOnToggle = true,
            .events = {GameEvent::OffLedge},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Enemy Bite",
            .tooltip = "Make the nearest enemy bite another enemy",
            .configSection = "EnemyBite",
            .keyPtr = &cfg.enemyBiteKey,
            .callback =
                [this](bool active, const RuntimeContextSnapshot& runtime) {
                    auto* world = runtime.world;
                    auto* player = runtime.player;
                    if (!player || !world) return;
                    if (active) {
                        ActorUtils::ApplyBiteState(
                            ActorUtils::FindNearestWillie(world, player, player, cfg.biteRange), true
                        );
                        return;
                    }
                    ActorUtils::ForEachWillieInRadius(world, player, cfg.biteRange, [](SDK::AWillie_BP_C* willie) {
                        ActorUtils::ApplyBiteState(willie, false);
                    });
                },
            .runOnToggle = true,
            .events = {GameEvent::OffLedge},
            .params =
                {KeybindParam("range", "Range", &cfg.biteRange, 50.0f, 2000.0f, "Detection range for bite target")},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Enemy Bite All",
            .tooltip = "Make all enemies within range bite each other",
            .configSection = "EnemyBiteAll",
            .keyPtr = &cfg.enemyBiteAllKey,
            .callback =
                [this](bool active, const RuntimeContextSnapshot& runtime) {
                    auto* world = runtime.world;
                    auto* player = runtime.player;
                    if (!player || !world) return;
                    ActorUtils::ForEachWillieInRadius(
                        world, player, cfg.biteAllRange,
                        [active](SDK::AWillie_BP_C* willie) { ActorUtils::ApplyBiteState(willie, active); }
                    );
                },
            .runOnToggle = true,
            .events = {GameEvent::OffLedge},
            .params =
                {KeybindParam("range", "Range", &cfg.biteAllRange, 50.0f, 2000.0f, "Detection range for mass bite")},
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Possess Nearest Willie",
            .tooltip = "Take control of the closest NPC",
            .configSection = "PossessNearestWillie",
            .keyPtr = &cfg.possessWillieKey,
            .callback =
                [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    auto* world = runtime.world;
                    auto* player = runtime.player;
                    auto* controller = runtime.controller;
                    if (!player || !controller || !world) return;

                    if (PossessState::lastWorld != world) {
                        PossessState::Reset();
                        PossessState::lastWorld = world;
                    }

                    SDK::APawn* currentPawn = controller->K2_GetPawn();
                    if (PossessState::possessed && currentPawn != PossessState::possessed) [[unlikely]] {
                        PossessState::Reset();
                    }

                    if (!PossessState::possessed) [[likely]] {
                        PossessState::originalPawn = currentPawn;
                        SDK::AWillie_BP_C* nearest = nullptr;
                        float minDist = GameConstants::MAX_DISTANCE;

                        ActorUtils::ForEachWillie(world, player, [&](SDK::AWillie_BP_C* willie) {
                            const float dist = player->GetDistanceTo(willie);
                            if (dist < minDist) {
                                minDist = dist;
                                nearest = willie;
                            }
                        });

                        if (!nearest) [[unlikely]]
                            return;

                        if (nearest->IsA(SDK::AWillie_BP_C::StaticClass())) [[likely]] {
                            PossessState::prevController = static_cast<SDK::AAIController*>(nearest->GetController());
                            if (PossessState::prevController) [[likely]] {
                                PossessState::prevController->SetActorTickEnabled(false);
                            }
                        }
                        controller->Possess(nearest);
                        nearest->Player = true;
                        PossessState::possessed = nearest;
                    } else {
                        auto* williePawn = static_cast<SDK::AWillie_BP_C*>(currentPawn);
                        controller->Possess(PossessState::originalPawn);
                        williePawn->Player = false;
                        if (PossessState::prevController) [[likely]] {
                            PossessState::prevController->Possess(williePawn);
                            PossessState::prevController->SetActorTickEnabled(true);
                        }
                        PossessState::Reset();
                    }
                },
        }
    );
}
