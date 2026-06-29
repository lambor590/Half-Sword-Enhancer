#include <vector>

#include "Menu/Sections/Player/PlayerAbilitiesSection.h"
#include "Menu/SectionStyle.h"

#include "SDK/AIModule_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/Weapon_Feet_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"
#include "SDK/Willie_BP_parameters.hpp"
#include "Hooks/GameHook.h"
#include "Utils/GameConstants.h"
#include "Utils/ActorUtils.h"
#include "Utils/BoneControl.h"

namespace {
    constexpr double KICK_FOOT_MASS_WEIGHT = 1.0;
    constexpr double KICK_CALF_MASS_WEIGHT = 0.75;
    constexpr double KICK_THIGH_MASS_WEIGHT = 0.35;
    constexpr double KICK_IMPULSE_TRANSFER_WEIGHTS[] = {0.35, 0.40, 0.25};

    struct KickBoneSet {
        SDK::FName foot;
        SDK::FName calf;
        SDK::FName thigh;
    };

    SDK::AWeapon_Feet_C* GetKickFoot(SDK::AWillie_BP_C* player, bool leftKick) {
        auto* weapon = player ? (leftKick ? player->Foot_L_Weapon : player->Foot_R_Weapon) : nullptr;
        return weapon && weapon->IsA(SDK::AWeapon_Feet_C::StaticClass()) ? static_cast<SDK::AWeapon_Feet_C*>(weapon)
                                                                         : nullptr;
    }

    void BoostFootWeapon(SDK::AWeapon_Feet_C* weapon, float configuredMultiplier) noexcept {
        const auto multiplier = static_cast<double>(configuredMultiplier);
        if (!weapon || multiplier <= 1.0) return;

        if (weapon->Kick_Power < multiplier) {
            weapon->Kick_Power = multiplier;
        }
    }

    const KickBoneSet& KickBones(bool leftKick) {
        static KickBoneSet left{
            SDK::BasicFilesImplUtils::StringToName(L"foot_l"),
            SDK::BasicFilesImplUtils::StringToName(L"calf_l"),
            SDK::BasicFilesImplUtils::StringToName(L"thigh_l"),
        };
        static KickBoneSet right{
            SDK::BasicFilesImplUtils::StringToName(L"foot_r"),
            SDK::BasicFilesImplUtils::StringToName(L"calf_r"),
            SDK::BasicFilesImplUtils::StringToName(L"thigh_r"),
        };
        return leftKick ? left : right;
    }

    double KickLimbMass(SDK::USkeletalMeshComponent* mesh, bool leftKick) {
        if (!mesh) return 0.0;

        const auto& bones = KickBones(leftKick);
        return static_cast<double>(mesh->GetBoneMass(bones.foot, true)) * KICK_FOOT_MASS_WEIGHT
             + static_cast<double>(mesh->GetBoneMass(bones.calf, true)) * KICK_CALF_MASS_WEIGHT
             + static_cast<double>(mesh->GetBoneMass(bones.thigh, true)) * KICK_THIGH_MASS_WEIGHT;
    }

    double Dot(const SDK::FVector& a, const SDK::FVector& b) noexcept {
        return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
    }

    constexpr int KickImpulseTransferStepCount() noexcept {
        return static_cast<int>(sizeof(KICK_IMPULSE_TRANSFER_WEIGHTS) / sizeof(KICK_IMPULSE_TRANSFER_WEIGHTS[0]));
    }

}

PlayerAbilitiesSection::PlayerAbilitiesSection(ModContext& ctx) : Section(ctx, SECTION) {
    InitKeybinds();
}

void PlayerAbilitiesSection::Render() {
    const SectionStyle::StyleRAII style;
    keybinds.Render();
}

void PlayerAbilitiesSection::InitKeybinds() {
    keybinds.Add(
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

    keybinds.Add(
        {
            .name = "Enemy Infinite Stamina",
            .tooltip = "Keeps enemy stamina full at all times",
            .configSection = "EnemyInfiniteStamina",
            .keyPtr = &cfg.enemyInfiniteStaminaKey,
            .callback =
                [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    auto* world = runtime.world;
                    auto* player = runtime.player;
                    if (!player || !world) return;
                    ActorUtils::ForEachWillie(world, player, [](SDK::AWillie_BP_C* willie) {
                        willie->Stamina = GameConstants::DEFAULT_HEALTH;
                    });
                },
            .events = {GameEvent::OffLedge},
        }
    );

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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

    auto boneSettings = [this](bool playerScope, bool active) {
        if (!active) return BoneControl::Settings{};
        return BoneControl::Settings{
            .blockDislocation = playerScope ? cfg.blockBoneDislocation : cfg.blockEnemyBoneDislocation,
            .breakStrengthMultiplier = playerScope ? cfg.boneBreakStrengthMultiplier
                                                   : cfg.enemyBoneBreakStrengthMultiplier,
            .massMultiplier = playerScope ? cfg.boneMassMultiplier : cfg.enemyBoneMassMultiplier,
        };
    };

    auto handleBoneControlHook = [boneSettings](
                                     bool playerScope, bool cancelDislocationCheck,
                                     GameHook::ProcessEventContext& context
                                 ) {
        auto* willie = BoneControl::WillieOwner(context.object);
        if (!BoneControl::MatchesScope(willie, playerScope)) return;

        const auto settings = boneSettings(playerScope, true);
        const bool willCancel =
            cancelDislocationCheck && settings.blockDislocation && BoneControl::ShouldCancelBreak(context, willie);
        BoneControl::Apply(willie, settings, cancelDislocationCheck);
        if (willCancel) {
            context.Cancel();
        }
    };

    auto makeBoneControlHooks = [handleBoneControlHook](bool playerScope) {
        auto apply = [handleBoneControlHook, playerScope](GameHook::ProcessEventContext& context) {
            handleBoneControlHook(playerScope, false, context);
        };
        auto block = [handleBoneControlHook, playerScope](GameHook::ProcessEventContext& context) {
            handleBoneControlHook(playerScope, true, context);
        };

        return std::vector<KeybindFunctionHook>{
            {"ReceiveBeginPlay", apply, GameHook::HookPhase::After},
            {"Setup Simulated Bones Array", apply, GameHook::HookPhase::After},
            {"ReceiveTick", apply},
            {"Get Damage", apply},
            {"Get Damage", apply, GameHook::HookPhase::After},
            {"Event Check Bone Dislocation Status", block},
            {"Break Arm L", block},
            {"Break Arm R", block},
            {"Break Leg L", block},
            {"Break Leg R", block},
            {"Break Back", block},
            {"Break Head", block},
            {"Break L Constraint", block},
            {"Break R Constraint", block},
            {"BreakConstraint", block},
            {"Snap Neck", block},
        };
    };

    auto addBoneControl =
        [&](bool playerScope, const char* name, const char* tooltip, const char* section, int* key, bool* block,
            float* strength, float* mass, const char* blockTooltip, const char* strengthTooltip,
            const char* massTooltip) {
            keybinds.Add(
                {
                    .name = name,
                    .tooltip = tooltip,
                    .configSection = section,
                    .keyPtr = key,
                    .callback =
                        [boneSettings, playerScope](bool active, const RuntimeContextSnapshot& runtime) {
                            BoneControl::ApplyToScope(runtime, playerScope, boneSettings(playerScope, active));
                        },
                    .runOnToggle = true,
                    .functionHooks = makeBoneControlHooks(playerScope),
                    .params =
                        {KeybindParam("block_dislocation", "Disable Dislocation", block, blockTooltip),
                         KeybindParam(
                             "break_strength_multiplier", "Break Strength", strength, 0.0f, 0.0f, strengthTooltip
                         ),
                         KeybindParam("mass_multiplier", "Mass Multiplier", mass, 0.0f, 0.0f, massTooltip)},
                }
            );
        };

    addBoneControl(
        true, "Bone Control", "Controls your bone snapping, dislocation resistance, and body mass scaling",
        "BoneControl", &cfg.boneControlKey, &cfg.blockBoneDislocation, &cfg.boneBreakStrengthMultiplier,
        &cfg.boneMassMultiplier, "Prevents the bone dislocation check from applying to you",
        "Multiplies bone break thresholds when Disable Dislocation is off",
        "Scales physical body mass; high values strongly affect ragdoll and impact behavior"
    );
    addBoneControl(
        false, "Enemy Bone Control", "Controls enemy bone snapping, dislocation resistance, and body mass scaling",
        "EnemyBoneControl", &cfg.enemyBoneControlKey, &cfg.blockEnemyBoneDislocation,
        &cfg.enemyBoneBreakStrengthMultiplier, &cfg.enemyBoneMassMultiplier,
        "Prevents the bone dislocation check from applying to enemies",
        "Multiplies enemy bone break thresholds when Disable Dislocation is off",
        "Scales enemy physical body mass; high values strongly affect ragdoll and impact behavior"
    );

    keybinds.Add(
        {
            .name = "Break Enemy Bones",
            .tooltip = "Runs the game's bone break functions on all enemies",
            .configSection = "BreakEnemyBones",
            .keyPtr = &cfg.enemyBreakBonesKey,
            .callback =
                []([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                    BoneControl::BreakEnemies(runtime);
                },
        }
    );

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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

    auto clearPendingKickImpulse = [this]() {
        pendingKickImpulseComponent = nullptr;
        pendingKickImpulse = {};
        pendingKickImpulseLocation = {};
        pendingKickImpulseBone = {};
        pendingKickImpulseStep = 0;
    };

    auto applyPendingKickImpulse = [this, clearPendingKickImpulse]() {
        if (!pendingKickImpulseComponent) return;

        if (pendingKickImpulseStep >= KickImpulseTransferStepCount()) {
            clearPendingKickImpulse();
            return;
        }

        const double weight = KICK_IMPULSE_TRANSFER_WEIGHTS[pendingKickImpulseStep];
        const auto impulse = pendingKickImpulse * weight;
        pendingKickImpulseComponent->AddImpulseAtLocation(impulse, pendingKickImpulseLocation, pendingKickImpulseBone);

        ++pendingKickImpulseStep;
        if (pendingKickImpulseStep >= KickImpulseTransferStepCount()) {
            clearPendingKickImpulse();
        }
    };

    auto openKickWindow = [this, applyPendingKickImpulse](bool leftKick, GameHook::ProcessEventContext& context) {
        const auto& snapshot = ModContext::Get().GetRenderSnapshot();
        auto* player = snapshot.player;
        if (!player) return;

        auto* willie = context.object && context.object->IsA(SDK::AWillie_BP_C::StaticClass())
                         ? static_cast<SDK::AWillie_BP_C*>(context.object)
                         : nullptr;
        if (context.object != player && (!willie || !willie->Player)) return;

        const float multiplier = cfg.kickPowerMultiplier;
        auto* target = willie ? willie : player;
        auto* foot = GetKickFoot(target, leftKick);
        if (!foot || multiplier <= 1.0f) return;
        if (kickWindowFoot == foot) {
            applyPendingKickImpulse();
            return;
        }

        kickWindowFoot = foot;
        kickWindowLeft = leftKick;
        kickImpulseSpent = false;
        if (leftKick) {
            target->Kick_Rate_L *= multiplier;
        } else {
            target->Kick_Rate_R *= multiplier;
        }
        BoostFootWeapon(foot, multiplier);
    };

    auto closeKickWindow = [this, clearPendingKickImpulse](bool leftKick, GameHook::ProcessEventContext& context) {
        const auto& snapshot = ModContext::Get().GetRenderSnapshot();
        auto* player = snapshot.player;
        if (!player) return;

        auto* willie = context.object && context.object->IsA(SDK::AWillie_BP_C::StaticClass())
                         ? static_cast<SDK::AWillie_BP_C*>(context.object)
                         : nullptr;
        if (context.object != player && (!willie || !willie->Player)) return;

        auto* target = willie ? willie : player;
        if (kickWindowFoot != GetKickFoot(target, leftKick)) return;

        kickWindowFoot = nullptr;
        kickWindowLeft = false;
        kickImpulseSpent = false;
        clearPendingKickImpulse();
    };

    keybinds.Add(
        {
            .name = "Kick Multiplier",
            .tooltip = "Multiplies the physical impulse applied by kicks",
            .configSection = "KickMultiplier",
            .keyPtr = &cfg.kickMultiplierKey,
            .functionHooks =
                {
                    KeybindFunctionHook{
                        .functionName = "Kick L Timeline__UpdateFunc",
                        .callback =
                            [openKickWindow](GameHook::ProcessEventContext& context) {
                                openKickWindow(true, context);
                            },
                        .phase = GameHook::HookPhase::After,
                    },
                    KeybindFunctionHook{
                        .functionName = "Kick R Timeline__UpdateFunc",
                        .callback =
                            [openKickWindow](GameHook::ProcessEventContext& context) {
                                openKickWindow(false, context);
                            },
                        .phase = GameHook::HookPhase::After,
                    },
                    KeybindFunctionHook{
                        .functionName = "Kick L Timeline__FinishedFunc",
                        .callback =
                            [closeKickWindow](GameHook::ProcessEventContext& context) {
                                closeKickWindow(true, context);
                            },
                        .phase = GameHook::HookPhase::After,
                    },
                    KeybindFunctionHook{
                        .functionName = "Kick R Timeline__FinishedFunc",
                        .callback =
                            [closeKickWindow](GameHook::ProcessEventContext& context) {
                                closeKickWindow(false, context);
                            },
                        .phase = GameHook::HookPhase::After,
                    },
                    KeybindFunctionHook{
                        .functionName =
                            "BndEvt__BP_ThirdPersonCharacter_Mesh_K2Node_ComponentBoundEvent_0_ComponentHitSignature__DelegateSignature",
                        .callback =
                            [this, applyPendingKickImpulse](GameHook::ProcessEventContext& context) {
                                auto* params = context.Params<
                                    SDK::Params::
                                        Willie_BP_C_BndEvt__BP_ThirdPersonCharacter_Mesh_K2Node_ComponentBoundEvent_0_ComponentHitSignature__DelegateSignature>();
                                if (!params) return;

                                auto* target =
                                    context.object && context.object->IsA(SDK::AWillie_BP_C::StaticClass())
                                        ? static_cast<SDK::AWillie_BP_C*>(context.object)
                                        : nullptr;
                                auto* player = ModContext::Get().GetRenderSnapshot().player;
                                if (!target || !player || target == player || !params->HitComponent) return;
                                if (!kickWindowFoot || kickImpulseSpent) return;

                                auto* foot =
                                    params->OtherActor && params->OtherActor->IsA(SDK::AWeapon_Feet_C::StaticClass())
                                        ? static_cast<SDK::AWeapon_Feet_C*>(params->OtherActor)
                                        : nullptr;
                                if (foot != kickWindowFoot || foot->Parent_Actor != player) return;

                                const auto multiplier = static_cast<double>(cfg.kickPowerMultiplier);
                                const double footSpeed = foot->Weapon_Velocity.Magnitude();
                                if (multiplier <= 1.0 || footSpeed <= 0.001) return;

                                auto* targetMesh = params->HitComponent->IsA(SDK::USkeletalMeshComponent::StaticClass())
                                                     ? static_cast<SDK::USkeletalMeshComponent*>(params->HitComponent)
                                                     : nullptr;
                                if (!targetMesh) return;
                                const auto targetBone = params->Hit.MyBoneName;
                                if (targetBone.IsNone()) return;

                                const double targetMass = targetMesh->GetBoneMass(targetBone, true);
                                const double attackerMass = KickLimbMass(player->Mesh, kickWindowLeft);
                                if (targetMass <= 0.001 || attackerMass <= 0.001) return;

                                auto direction = target->K2_GetActorLocation() - player->K2_GetActorLocation();
                                direction.Z = 0.0;
                                const double distance = direction.Magnitude();
                                if (distance <= 1.0) return;

                                direction /= distance;
                                auto relativeVelocity =
                                    foot->Weapon_Velocity
                                    - targetMesh->GetPhysicsLinearVelocityAtPoint(params->Hit.ImpactPoint, targetBone);
                                const double outwardSpeed = Dot(relativeVelocity, direction);
                                if (outwardSpeed < 0.0) {
                                    relativeVelocity -= direction * outwardSpeed;
                                }

                                const double relativeSpeed = relativeVelocity.Magnitude();
                                if (relativeSpeed <= 0.001) return;

                                direction = relativeVelocity / relativeSpeed;
                                direction.Normalize();

                                const double effectiveMass = (attackerMass * targetMass) / (attackerMass + targetMass);
                                const auto impulse = direction * (relativeSpeed * effectiveMass * (multiplier - 1.0));
                                BoostFootWeapon(foot, cfg.kickPowerMultiplier);
                                pendingKickImpulseComponent = params->HitComponent;
                                pendingKickImpulse = impulse;
                                pendingKickImpulseLocation = params->Hit.ImpactPoint;
                                pendingKickImpulseBone = targetBone;
                                pendingKickImpulseStep = 0;
                                applyPendingKickImpulse();
                                kickImpulseSpent = true;
                            },
                    },
                },
            .params = {KeybindParam(
                "kick_power_multiplier", "Power Multiplier", &cfg.kickPowerMultiplier, 1.0f, 100.0f,
                "Multiplies the mass-aware physical impulse applied by kicks"
            )},
        }
    );

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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

    keybinds.Add(
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
