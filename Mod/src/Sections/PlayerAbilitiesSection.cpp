#include <array>
#include <chrono>
#include <cmath>
#include <vector>

#include "Menu/Sections/Player/PlayerAbilitiesSection.h"

#include "SDK/AIModule_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/Weapon_Feet_classes.hpp"
#include "SDK/Weapon_Fists_classes.hpp"
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
    constexpr std::array KICK_IMPULSE_TRANSFER_WEIGHTS{0.35, 0.40, 0.25};
    constexpr double KNOCKBACK_EPSILON = 0.001;
    constexpr double KNOCKBACK_MIN_SPEED_SQ = KNOCKBACK_EPSILON * KNOCKBACK_EPSILON;
    constexpr double KNOCKBACK_MIN_DISTANCE_SQ = 1.0;
    constexpr auto PUNCH_KNOCKBACK_CONTACT_GAP = std::chrono::milliseconds(150);
    constexpr double PUNCH_KNOCKBACK_RESET_RATIO = 0.35;

    struct BoneControlDefinition {
        const char* name;
        const char* tooltip;
        const char* configSection;
        int PlayerAbilitiesSection::Config::* key;
        bool PlayerAbilitiesSection::Config::* blockDislocation;
        float PlayerAbilitiesSection::Config::* breakStrengthMultiplier;
        float PlayerAbilitiesSection::Config::* massMultiplier;
        const char* blockTooltip;
        const char* strengthTooltip;
        const char* massTooltip;
    };

    constexpr std::array BONE_CONTROL_DEFINITIONS{
        BoneControlDefinition{
            "Bone Durability",
            "Make your bones harder or easier to break, prevent dislocations, and change your weight",
            "BoneControl",
            &PlayerAbilitiesSection::Config::boneControlKey,
            &PlayerAbilitiesSection::Config::blockBoneDislocation,
            &PlayerAbilitiesSection::Config::boneBreakStrengthMultiplier,
            &PlayerAbilitiesSection::Config::boneMassMultiplier,
            "Keeps your joints from dislocating",
            "Controls how easily your bones break",
            "Controls how heavy you feel during movement, falls, and impacts",
        },
        BoneControlDefinition{
            "Enemy Bone Durability",
            "Make enemy bones harder or easier to break, prevent dislocations, and change their weight",
            "EnemyBoneControl",
            &PlayerAbilitiesSection::Config::enemyBoneControlKey,
            &PlayerAbilitiesSection::Config::blockEnemyBoneDislocation,
            &PlayerAbilitiesSection::Config::enemyBoneBreakStrengthMultiplier,
            &PlayerAbilitiesSection::Config::enemyBoneMassMultiplier,
            "Keeps enemy joints from dislocating",
            "Controls how easily enemy bones break",
            "Controls how heavy enemies feel during movement, falls, and impacts",
        },
    };

    constexpr std::array<const char*, 2> BONE_APPLY_HOOKS{"ReceiveTick", "Get Damage"};
    constexpr std::array<const char*, 11> BONE_BLOCK_HOOKS{
        "Event Check Bone Dislocation Status",
        "Break Arm L",
        "Break Arm R",
        "Break Leg L",
        "Break Leg R",
        "Break Back",
        "Break Head",
        "Break L Constraint",
        "Break R Constraint",
        "BreakConstraint",
        "Snap Neck",
    };

    enum class BoneHookAction : std::uint8_t { MarkSpawned, ApplyPendingMass, BlockDislocation };

    struct KickBoneSet {
        SDK::FName foot;
        SDK::FName calf;
        SDK::FName thigh;
    };

    struct PunchKnockbackContact {
        SDK::AWeapon_Fists_C* fist = nullptr;
        SDK::AWillie_BP_C* target = nullptr;
        SDK::FName targetBone{};
        std::chrono::steady_clock::time_point lastSeen;
        double appliedImpulse = 0.0;
    };

    std::vector<PunchKnockbackContact> g_punchKnockbackContacts;

    void ActivatePossessionCamera(SDK::APlayerController* controller, SDK::AWillie_BP_C* willie) {
        if (!controller || !willie) return;

        willie->Initialize_Camera_Settings();
        controller->SetViewTargetWithBlend(
            willie, 0.0f, SDK::EViewTargetBlendFunction::VTBlend_Linear, 0.0f, false
        );
        if (controller->PlayerCameraManager) controller->PlayerCameraManager->SetGameCameraCutThisFrame();
    }

    void QueuePossessionCamera(SDK::APlayerController* controller, SDK::AWillie_BP_C* willie) {
        GameHook::QueueAction([controller, willie](const RuntimeContextSnapshot& runtime) {
            if (runtime.controller != controller || controller->K2_GetPawn() != willie) return;
            ActivatePossessionCamera(controller, willie);
        });
    }

    SDK::AWeapon_Feet_C* GetKickFoot(SDK::AWillie_BP_C* player, bool leftKick) {
        auto* weapon = player ? (leftKick ? player->Foot_L_Weapon : player->Foot_R_Weapon) : nullptr;
        return weapon && weapon->IsA(SDK::AWeapon_Feet_C::StaticClass()) ? static_cast<SDK::AWeapon_Feet_C*>(weapon)
                                                                         : nullptr;
    }

    void BoostWeaponKnockback(SDK::AModularWeaponBP_C* weapon, float configuredMultiplier) noexcept {
        if (!weapon || configuredMultiplier <= 1.0f) return;

        const auto multiplier = static_cast<double>(configuredMultiplier);
        if (weapon->Kick_Power < multiplier) {
            weapon->Kick_Power = multiplier;
        }
    }

    const KickBoneSet& KickBones(bool leftKick) {
        static const KickBoneSet LEFT{
            SDK::BasicFilesImplUtils::StringToName(L"foot_l"),
            SDK::BasicFilesImplUtils::StringToName(L"calf_l"),
            SDK::BasicFilesImplUtils::StringToName(L"thigh_l"),
        };
        static const KickBoneSet RIGHT{
            SDK::BasicFilesImplUtils::StringToName(L"foot_r"),
            SDK::BasicFilesImplUtils::StringToName(L"calf_r"),
            SDK::BasicFilesImplUtils::StringToName(L"thigh_r"),
        };
        return leftKick ? LEFT : RIGHT;
    }

    double KickLimbMass(SDK::USkeletalMeshComponent* mesh, bool leftKick) {
        if (!mesh) return 0.0;

        const auto& bones = KickBones(leftKick);
        return static_cast<double>(mesh->GetBoneMass(bones.foot, true)) * KICK_FOOT_MASS_WEIGHT +
               static_cast<double>(mesh->GetBoneMass(bones.calf, true)) * KICK_CALF_MASS_WEIGHT +
               static_cast<double>(mesh->GetBoneMass(bones.thigh, true)) * KICK_THIGH_MASS_WEIGHT;
    }

    double BoneMass(SDK::USkeletalMeshComponent* mesh, const SDK::TArray<SDK::FName>& bones) {
        if (!mesh) return 0.0;

        double mass = 0.0;
        for (const auto& bone : bones) {
            if (!bone.IsNone()) mass += static_cast<double>(mesh->GetBoneMass(bone, true));
        }
        return mass;
    }

    double PunchLimbMass(SDK::AWillie_BP_C* attacker, SDK::AWeapon_Fists_C* fist) {
        if (!attacker || !attacker->Mesh || !fist) return 0.0;

        if (fist == attacker->Weapon_L || fist == attacker->Weapon_L_0) {
            return BoneMass(attacker->Mesh, attacker->L_Arm_Bones);
        }
        if (fist == attacker->Weapon_R || fist == attacker->Weapon_R_0) {
            return BoneMass(attacker->Mesh, attacker->R_Arm_Bones);
        }
        return 0.0;
    }

    [[nodiscard]] bool ComputeKnockbackImpulse(
        SDK::AWillie_BP_C* attacker, SDK::AWillie_BP_C* target, SDK::UPrimitiveComponent* hitComponent,
        const SDK::FVector& weaponVelocity, const SDK::FVector& impactPoint, const SDK::FName& targetBone,
        double attackerMass, double multiplier, SDK::FVector& impulse
    ) {
        if (!attacker || !target || attacker == target || !hitComponent || multiplier <= 1.0) return false;

        const double weaponSpeedSq = weaponVelocity.Dot(weaponVelocity);
        if (weaponSpeedSq <= KNOCKBACK_MIN_SPEED_SQ) return false;

        auto* targetMesh = hitComponent->IsA(SDK::USkeletalMeshComponent::StaticClass())
                               ? static_cast<SDK::USkeletalMeshComponent*>(hitComponent)
                               : nullptr;
        if (!targetMesh || targetBone.IsNone()) return false;

        const double targetMass = targetMesh->GetBoneMass(targetBone, true);
        if (targetMass <= KNOCKBACK_EPSILON || attackerMass <= KNOCKBACK_EPSILON) return false;

        auto outwardDirection = target->K2_GetActorLocation() - attacker->K2_GetActorLocation();
        outwardDirection.Z = 0.0;
        const double distanceSq = outwardDirection.Dot(outwardDirection);
        if (distanceSq <= KNOCKBACK_MIN_DISTANCE_SQ) return false;

        const double weaponSpeed = std::sqrt(weaponSpeedSq);
        const auto direction = weaponVelocity * (1.0 / weaponSpeed);
        if (direction.Dot(outwardDirection) < 0.0) return false;

        const auto relativeVelocity =
            weaponVelocity - targetMesh->GetPhysicsLinearVelocityAtPoint(impactPoint, targetBone);
        const double relativeSpeed = relativeVelocity.Dot(direction);
        if (relativeSpeed <= KNOCKBACK_EPSILON) return false;

        const double effectiveMass = (attackerMass * targetMass) / (attackerMass + targetMass);
        impulse = direction * (relativeSpeed * effectiveMass * (multiplier - 1.0));
        return true;
    }

    PunchKnockbackContact* TouchPunchKnockbackContact(
        SDK::AWeapon_Fists_C* fist, SDK::AWillie_BP_C* target, const SDK::FName& targetBone
    ) {
        if (!fist || !target || targetBone.IsNone()) return nullptr;

        const auto now = std::chrono::steady_clock::now();
        const auto staleBefore = now - PUNCH_KNOCKBACK_CONTACT_GAP;
        std::erase_if(g_punchKnockbackContacts, [staleBefore](const PunchKnockbackContact& contact) {
            return !contact.fist || !contact.target || contact.lastSeen < staleBefore;
        });

        for (auto& contact : g_punchKnockbackContacts) {
            if (contact.fist == fist && contact.target == target && contact.targetBone == targetBone) {
                contact.lastSeen = now;
                return &contact;
            }
        }

        g_punchKnockbackContacts.push_back(
            PunchKnockbackContact{
                .fist = fist,
                .target = target,
                .targetBone = targetBone,
                .lastSeen = now,
                .appliedImpulse = 0.0,
            }
        );
        return &g_punchKnockbackContacts.back();
    }

    template <bool playerScope>
    [[nodiscard]] BoneControl::Settings BoneSettings(const PlayerAbilitiesSection::Config& cfg, bool active) noexcept {
        if (!active) return {};
        if constexpr (playerScope) {
            return {
                .blockDislocation = cfg.blockBoneDislocation,
                .breakStrengthMultiplier = cfg.boneBreakStrengthMultiplier,
                .massMultiplier = cfg.boneMassMultiplier,
            };
        } else {
            return {
                .blockDislocation = cfg.blockEnemyBoneDislocation,
                .breakStrengthMultiplier = cfg.enemyBoneBreakStrengthMultiplier,
                .massMultiplier = cfg.enemyBoneMassMultiplier,
            };
        }
    }

    template <bool playerScope, BoneHookAction action>
    void HandleBoneControlHook(const PlayerAbilitiesSection::Config& cfg, GameHook::ProcessEventContext& context) {
        auto* willie = BoneControl::WillieOwner(context.object);
        if (!BoneControl::MatchesScope(willie, playerScope)) return;

        constexpr bool BLOCK_DISLOCATION = action == BoneHookAction::BlockDislocation;
        const auto settings = BoneSettings<playerScope>(cfg, true);
        const bool cancel =
            BLOCK_DISLOCATION && settings.blockDislocation && BoneControl::ShouldCancelBreak(context, willie);
        BoneControl::Apply(willie, settings, BLOCK_DISLOCATION);

        if constexpr (action == BoneHookAction::MarkSpawned) {
            BoneControl::MarkSpawnedWillie(willie);
        } else if constexpr (action == BoneHookAction::ApplyPendingMass) {
            BoneControl::ApplyPendingSpawnMass(willie, settings);
        }
        if (cancel) context.Cancel();
    }

    template <bool playerScope>
    std::vector<KeybindFunctionHook> MakeBoneControlHooks(const PlayerAbilitiesSection::Config* config) {
        std::vector<KeybindFunctionHook> hooks;
        hooks.reserve(2 + BONE_APPLY_HOOKS.size() + BONE_BLOCK_HOOKS.size());

        hooks.push_back({
            .functionName = "ReceiveBeginPlay",
            .callback = [config](
                            GameHook::ProcessEventContext& context
                        ) { HandleBoneControlHook<playerScope, BoneHookAction::MarkSpawned>(*config, context); },
            .phase = GameHook::HookPhase::After,
        });

        for (const auto* functionName : BONE_APPLY_HOOKS) {
            hooks.push_back({
                .functionName = functionName,
                .callback = [config](GameHook::ProcessEventContext& context) {
                    HandleBoneControlHook<playerScope, BoneHookAction::ApplyPendingMass>(*config, context);
                },
            });
        }
        hooks.push_back({
            .functionName = "Get Damage",
            .callback = [config](
                            GameHook::ProcessEventContext& context
                        ) { HandleBoneControlHook<playerScope, BoneHookAction::ApplyPendingMass>(*config, context); },
            .phase = GameHook::HookPhase::After,
        });

        for (const auto* functionName : BONE_BLOCK_HOOKS) {
            hooks.push_back({
                .functionName = functionName,
                .callback = [config](GameHook::ProcessEventContext& context) {
                    HandleBoneControlHook<playerScope, BoneHookAction::BlockDislocation>(*config, context);
                },
            });
        }
        return hooks;
    }
}

PlayerAbilitiesSection::PlayerAbilitiesSection(ModContext& ctx) : Section(ctx, SECTION) {
    kickWindows.reserve(4);
    g_punchKnockbackContacts.reserve(8);
    InitKeybinds();
}

void PlayerAbilitiesSection::Render() {
    keybinds.Render();
}

template <bool playerScope> void PlayerAbilitiesSection::AddBoneControl() {
    constexpr const auto& DEFINITION = BONE_CONTROL_DEFINITIONS[playerScope ? 0 : 1];
    keybinds.Add({
        .name = DEFINITION.name,
        .tooltip = DEFINITION.tooltip,
        .configSection = DEFINITION.configSection,
        .keyPtr = &(cfg.*DEFINITION.key),
        .callback =
            [this](bool active, const RuntimeContextSnapshot& runtime) {
                if (!active) BoneControl::ClearPendingSpawnMass(playerScope);
                BoneControl::ApplyToScope(runtime, playerScope, BoneSettings<playerScope>(cfg, active));
            },
        .kind = KeybindKind::State,
        .applyOnToggle = true,
        .functionHooks = MakeBoneControlHooks<playerScope>(&cfg),
        .params =
            {KeybindParam(
                 "block_dislocation", "Prevent Dislocations", &(cfg.*DEFINITION.blockDislocation),
                 DEFINITION.blockTooltip
             ),
             KeybindParam(
                 "break_strength_multiplier", "Bone Strength", &(cfg.*DEFINITION.breakStrengthMultiplier), 0.0f, 0.0f,
                 DEFINITION.strengthTooltip
             ),
             KeybindParam(
                 "mass_multiplier", "Body Weight", &(cfg.*DEFINITION.massMultiplier), 0.0f, 0.0f, DEFINITION.massTooltip
             )},
        .group = "Bones & Balance",
    });
}

PlayerAbilitiesSection::KickWindow* PlayerAbilitiesSection::FindKickWindow(SDK::AWeapon_Feet_C* foot) noexcept {
    for (auto& window : kickWindows) {
        if (window.foot == foot) return &window;
    }
    return nullptr;
}

void PlayerAbilitiesSection::ClearPendingKickImpulse(KickWindow& window) noexcept {
    window.pendingImpulseComponent = nullptr;
    window.pendingImpulseStep = 0;
}

void PlayerAbilitiesSection::ApplyPendingKickImpulse(KickWindow& window) {
    if (!window.pendingImpulseComponent) return;
    if (window.pendingImpulseStep >= KICK_IMPULSE_TRANSFER_WEIGHTS.size()) {
        ClearPendingKickImpulse(window);
        return;
    }

    const auto impulse = window.pendingImpulse * KICK_IMPULSE_TRANSFER_WEIGHTS[window.pendingImpulseStep];
    window.pendingImpulseComponent
        ->AddImpulseAtLocation(impulse, window.pendingImpulseLocation, window.pendingImpulseBone);

    if (++window.pendingImpulseStep >= KICK_IMPULSE_TRANSFER_WEIGHTS.size()) {
        ClearPendingKickImpulse(window);
    }
}

void PlayerAbilitiesSection::OpenKickWindow(bool leftKick, GameHook::ProcessEventContext& context) {
    auto* attacker = context.object && context.object->IsA(SDK::AWillie_BP_C::StaticClass())
                         ? static_cast<SDK::AWillie_BP_C*>(context.object)
                         : nullptr;
    if (!attacker || (!BoneControl::MatchesScope(attacker, true) && !cfg.kickMultiplierAffectsEnemies)) return;

    const float multiplier = cfg.kickPowerMultiplier;
    auto* foot = GetKickFoot(attacker, leftKick);
    if (!foot || multiplier <= 1.0f) return;
    if (auto* window = FindKickWindow(foot)) {
        ApplyPendingKickImpulse(*window);
        return;
    }

    auto& window = kickWindows.emplace_back();
    window.foot = foot;
    window.left = leftKick;
    if (leftKick) {
        attacker->Kick_Rate_L *= multiplier;
    } else {
        attacker->Kick_Rate_R *= multiplier;
    }
    BoostWeaponKnockback(foot, multiplier);
}

void PlayerAbilitiesSection::CloseKickWindow(bool leftKick, GameHook::ProcessEventContext& context) {
    auto* attacker = context.object && context.object->IsA(SDK::AWillie_BP_C::StaticClass())
                         ? static_cast<SDK::AWillie_BP_C*>(context.object)
                         : nullptr;
    auto* foot = GetKickFoot(attacker, leftKick);
    std::erase_if(kickWindows, [foot](const KickWindow& window) { return window.foot == foot; });
}

void PlayerAbilitiesSection::HandleKickHit(GameHook::ProcessEventContext& context) {
    auto* params = context.Params<
        SDK::Params::
            Willie_BP_C_BndEvt__BP_ThirdPersonCharacter_Mesh_K2Node_ComponentBoundEvent_0_ComponentHitSignature__DelegateSignature>();
    if (!params || !params->HitComponent) return;

    auto* target = context.object && context.object->IsA(SDK::AWillie_BP_C::StaticClass())
                       ? static_cast<SDK::AWillie_BP_C*>(context.object)
                       : nullptr;
    auto* foot = params->OtherActor && params->OtherActor->IsA(SDK::AWeapon_Feet_C::StaticClass())
                     ? static_cast<SDK::AWeapon_Feet_C*>(params->OtherActor)
                     : nullptr;
    auto* attacker = foot ? foot->Parent_Actor : nullptr;
    if (!foot || !target || !attacker || attacker == target ||
        (!BoneControl::MatchesScope(attacker, true) && !cfg.kickMultiplierAffectsEnemies))
        return;

    auto* window = FindKickWindow(foot);
    if (!window || window->impulseSpent) return;

    const auto targetBone = params->Hit.MyBoneName;
    SDK::FVector impulse{};
    if (!ComputeKnockbackImpulse(
            attacker, target, params->HitComponent, foot->Weapon_Velocity, params->Hit.ImpactPoint, targetBone,
            KickLimbMass(attacker->Mesh, window->left), static_cast<double>(cfg.kickPowerMultiplier), impulse
        ))
        return;

    BoostWeaponKnockback(foot, cfg.kickPowerMultiplier);
    window->pendingImpulseComponent = params->HitComponent;
    window->pendingImpulse = impulse;
    window->pendingImpulseLocation = params->Hit.ImpactPoint;
    window->pendingImpulseBone = targetBone;
    window->pendingImpulseStep = 0;
    ApplyPendingKickImpulse(*window);
    window->impulseSpent = true;
}

void PlayerAbilitiesSection::HandlePunchHit(GameHook::ProcessEventContext& context) {
    auto* params = context.Params<
        SDK::Params::
            Willie_BP_C_BndEvt__BP_ThirdPersonCharacter_Mesh_K2Node_ComponentBoundEvent_0_ComponentHitSignature__DelegateSignature>();
    if (!params || !params->HitComponent) return;

    auto* target = context.object && context.object->IsA(SDK::AWillie_BP_C::StaticClass())
                       ? static_cast<SDK::AWillie_BP_C*>(context.object)
                       : nullptr;
    auto* fist = params->OtherActor && params->OtherActor->IsA(SDK::AWeapon_Fists_C::StaticClass())
                     ? static_cast<SDK::AWeapon_Fists_C*>(params->OtherActor)
                     : nullptr;
    auto* attacker = fist ? fist->Parent_Actor : nullptr;
    if (!fist || !target || !attacker || attacker == target ||
        (!BoneControl::MatchesScope(attacker, true) && !cfg.knockbackAffectsEnemies))
        return;

    const auto targetBone = params->Hit.MyBoneName;
    auto* contact = TouchPunchKnockbackContact(fist, target, targetBone);
    SDK::FVector impulse{};
    if (!ComputeKnockbackImpulse(
            attacker, target, params->HitComponent, fist->Weapon_Velocity, params->Hit.ImpactPoint, targetBone,
            PunchLimbMass(attacker, fist), static_cast<double>(cfg.knockbackMultiplier), impulse
        )) {
        if (contact) contact->appliedImpulse = 0.0;
        return;
    }

    if (contact) {
        const double contactImpulse = impulse.Magnitude();
        if (contact->appliedImpulse > KNOCKBACK_EPSILON &&
            contactImpulse < contact->appliedImpulse * PUNCH_KNOCKBACK_RESET_RATIO) {
            contact->appliedImpulse = contactImpulse;
        }

        const double remainingImpulse = contactImpulse - contact->appliedImpulse;
        if (remainingImpulse <= KNOCKBACK_EPSILON) return;
        if (remainingImpulse < contactImpulse) impulse *= remainingImpulse / contactImpulse;
        contact->appliedImpulse = contactImpulse;
    }

    BoostWeaponKnockback(fist, cfg.knockbackMultiplier);
    params->HitComponent->AddImpulseAtLocation(impulse, params->Hit.ImpactPoint, targetBone);
}

void PlayerAbilitiesSection::TogglePossession(const RuntimeContextSnapshot& runtime) {
    auto* world = runtime.world;
    auto* player = runtime.player;
    auto* controller = runtime.controller;
    if (!player || !controller || !world) return;

    if (PossessState::lastWorld != world) {
        PossessState::Reset();
        PossessState::lastWorld = world;
    }

    auto* currentPawn = controller->K2_GetPawn();
    if (PossessState::possessed && currentPawn != PossessState::possessed) [[unlikely]] {
        PossessState::Reset();
    }

    if (!PossessState::possessed) [[likely]] {
        PossessState::originalPawn = currentPawn;
        auto* nearest = ActorUtils::FindNearestWillie(world, player, player, GameConstants::MAX_DISTANCE);
        if (!nearest) [[unlikely]]
            return;

        PossessState::prevController = static_cast<SDK::AAIController*>(nearest->Controller);
        if (PossessState::prevController) [[likely]] {
            PossessState::prevController->SetActorTickEnabled(false);
        }
        nearest->Player = true;
        controller->Possess(nearest);
        QueuePossessionCamera(controller, nearest);
        PossessState::possessed = nearest;
        return;
    }

    auto* williePawn = static_cast<SDK::AWillie_BP_C*>(currentPawn);
    controller->Possess(PossessState::originalPawn);
    williePawn->Player = false;
    if (PossessState::prevController) [[likely]] {
        PossessState::prevController->Possess(williePawn);
        PossessState::prevController->SetActorTickEnabled(true);
    }
    if (PossessState::originalPawn && PossessState::originalPawn->IsA(SDK::AWillie_BP_C::StaticClass())) {
        QueuePossessionCamera(controller, static_cast<SDK::AWillie_BP_C*>(PossessState::originalPawn));
    }
    PossessState::Reset();
}

void PlayerAbilitiesSection::InitKeybinds() {
    keybinds.Add({
        .name = "Infinite Stamina",
        .tooltip = "Keeps your stamina full",
        .configSection = "InfiniteStamina",
        .keyPtr = &cfg.infiniteStaminaKey,
        .callback =
            [](bool, const RuntimeContextSnapshot& runtime) {
                auto* p = runtime.player;
                if (!p) return;
                p->Stamina = GameConstants::DEFAULT_HEALTH;
            },
        .kind = KeybindKind::State,
        .events = {GameEvent::OffLedge},
        .group = "Health & Recovery",
    });

    keybinds.Add({
        .name = "Enemy Infinite Stamina",
        .tooltip = "Keeps enemy stamina full",
        .configSection = "EnemyInfiniteStamina",
        .keyPtr = &cfg.enemyInfiniteStaminaKey,
        .callback =
            [](bool, const RuntimeContextSnapshot& runtime) {
                auto* world = runtime.world;
                auto* player = runtime.player;
                if (!player || !world) return;
                ActorUtils::ForEachWillie(world, player, [](SDK::AWillie_BP_C* willie) {
                    willie->Stamina = GameConstants::DEFAULT_HEALTH;
                });
            },
        .kind = KeybindKind::State,
        .events = {GameEvent::OffLedge},
        .group = "Health & Recovery",
    });

    keybinds.Add({
        .name = "Knockout Immunity",
        .tooltip = "Keeps you conscious",
        .configSection = "InfiniteConsciousness",
        .keyPtr = &cfg.infiniteConsciousnessKey,
        .callback =
            [](bool, const RuntimeContextSnapshot& runtime) {
                auto* p = runtime.player;
                if (!p) return;
                ActorUtils::SetInfiniteConsciousness(p);
            },
        .kind = KeybindKind::State,
        .events = {GameEvent::OffLedge},
        .group = "Health & Recovery",
    });

    keybinds.Add({
        .name = "Enemy Knockout Immunity",
        .tooltip = "Keeps enemies conscious",
        .configSection = "EnemyInfiniteConsciousness",
        .keyPtr = &cfg.enemyInfiniteConsciousnessKey,
        .callback =
            [](bool, const RuntimeContextSnapshot& runtime) {
                auto* world = runtime.world;
                auto* player = runtime.player;
                if (!player || !world) return;
                ActorUtils::ForEachWillie(world, player, ActorUtils::SetInfiniteConsciousness);
            },
        .kind = KeybindKind::State,
        .events = {GameEvent::OffLedge},
        .group = "Health & Recovery",
    });

    keybinds.Add({
        .name = "Knockout Resistance",
        .tooltip = "Makes you harder to knock out",
        .configSection = "ConsciousnessMultiplier",
        .keyPtr = &cfg.consciousnessMultiplierKey,
        .callback =
            [this](bool active, const RuntimeContextSnapshot& runtime) {
                auto* p = runtime.player;
                if (!p) return;
                float cap = GameConstants::DEFAULT_HEALTH * (active ? cfg.consciousnessMultiplier : 1.0f);
                p->Consciousness_Cap = cap;
            },
        .kind = KeybindKind::State,
        .applyOnToggle = true,
        .events = {GameEvent::OffLedge},
        .params = {KeybindParam(
            "consciousness_multiplier", "Resistance", &cfg.consciousnessMultiplier, 1.0f, 100.0f,
            "Controls how hard you are to knock out"
        )},
        .group = "Health & Recovery",
    });

    keybinds.Add({
        .name = "Enemy Knockout Resistance",
        .tooltip = "Makes enemies harder to knock out",
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
        .kind = KeybindKind::State,
        .applyOnToggle = true,
        .events = {GameEvent::OffLedge},
        .params = {KeybindParam(
            "enemy_consciousness_multiplier", "Resistance", &cfg.enemyConsciousnessMultiplier, 1.0f, 100.0f,
            "Controls how hard enemies are to knock out"
        )},
        .group = "Health & Recovery",
    });

    keybinds.Add({
        .name = "Jump",
        .tooltip = "Jump into the air",
        .configSection = "Jump",
        .keyPtr = &cfg.jumpKey,
        .callback =
            [this](bool, const RuntimeContextSnapshot& runtime) {
                auto* p = runtime.player;
                if (!p) return;
                p->Mesh->AddImpulse(SDK::FVector(0.0f, 0.0f, cfg.jumpForce), SDK::FName(), true);
            },
        .params = {KeybindParam("force", "Height", &cfg.jumpForce, 1000.0f, 10000.0f, "Controls how high you jump")},
        .group = "Movement & Strength",
    });

    keybinds.Add({
        .name = "Movement Speed",
        .tooltip = "Move faster while walking and running",
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
        .kind = KeybindKind::State,
        .applyOnToggle = true,
        .events = {GameEvent::OffLedge},
        .params =
            {KeybindParam(
                 "run_speed_multiplier", "Run Speed", &cfg.playerRunMultiplier, 1.0f, 100.0f,
                 "Controls your running speed"
             ),
             KeybindParam(
                 "walk_speed_multiplier", "Walk Speed", &cfg.playerWalkMultiplier, 1.0f, 100.0f,
                 "Controls your walking speed"
             )},
        .group = "Movement & Strength",
    });

    keybinds.Add({
        .name = "Strength",
        .tooltip = "Become stronger, hold objects more firmly, and hit harder",
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
        .kind = KeybindKind::State,
        .applyOnToggle = true,
        .events = {GameEvent::OffLedge},
        .params =
            {KeybindParam(
                 "strength_multiplier", "Body Strength", &cfg.playerStrengthMultiplier, 1.0f, 10.0f,
                 "Controls how strong and responsive your body feels"
             ),
             KeybindParam(
                 "grab_force_multiplier", "Grip Strength", &cfg.playerGrabForceMultiplier, 1.0f, 10.0f,
                 "Controls how firmly you hold objects"
             ),
             KeybindParam(
                 "hands_rigidity_multiplier", "Punch Power", &cfg.playerHandsRigidityMultiplier, 1.0f, 10.0f,
                 "Controls how hard your punches hit"
             )},
        .group = "Movement & Strength",
    });

    keybinds.Add({
        .name = "Body Stability",
        .tooltip = "Stay upright and resist weakened or limp limbs",
        .configSection = "CustomBodyTonus",
        .keyPtr = &cfg.bodyTonusKey,
        .callback =
            [this](bool active, const RuntimeContextSnapshot& runtime) {
                auto* p = runtime.player;
                if (!p) return;
                p->All_Body_Tonus =
                    GameConstants::DEFAULT_ALL_BODY_TONUS * (active ? cfg.bodyTonusAllBodyMultiplier : 1.0f);
                if (active && cfg.bodyTonusNoBodyWeakening) [[unlikely]] {
                    p->Head_Tonus = GameConstants::FULL_TONUS;
                    p->Arm_L_Tonus = GameConstants::FULL_TONUS;
                    p->Arm_R_Tonus = GameConstants::FULL_TONUS;
                    p->Leg_L_Tonus = GameConstants::FULL_TONUS;
                    p->Leg_R_Tonus = GameConstants::FULL_TONUS;
                }
            },
        .kind = KeybindKind::State,
        .applyOnToggle = true,
        .events = {GameEvent::OffLedge},
        .params =
            {KeybindParam(
                 "all_body", "Stability", &cfg.bodyTonusAllBodyMultiplier, 1.0f, 10.0f,
                 "Controls how firmly you hold your posture"
             ),
             KeybindParam(
                 "no_body_weakening", "Limb Stability", &cfg.bodyTonusNoBodyWeakening,
                 "Keeps injured limbs from going limp"
             )},
        .group = "Movement & Strength",
    });

    AddBoneControl<true>();
    AddBoneControl<false>();

    keybinds.Add({
        .name = "Break Enemy Bones",
        .tooltip = "Breaks every enemy's bones",
        .configSection = "BreakEnemyBones",
        .keyPtr = &cfg.enemyBreakBonesKey,
        .callback = [](bool, const RuntimeContextSnapshot& runtime) { BoneControl::BreakEnemies(runtime); },
        .group = "Bones & Balance",
        .destructive = true,
    });

    keybinds.Add({
        .name = "Ragdoll",
        .tooltip = "Makes you collapse",
        .configSection = "Ragdoll",
        .keyPtr = &cfg.ragdollKey,
        .callback =
            [](bool, const RuntimeContextSnapshot& runtime) {
                auto* p = runtime.player;
                if (!p) return;
                p->All_Body_Tonus = GameConstants::DEFAULT_PAIN;
            },
        .kind = KeybindKind::State,
        .events = {GameEvent::OffLedge},
        .group = "Bones & Balance",
    });

    keybinds.Add({
        .name = "Ragdoll Enemies",
        .tooltip = "Makes enemies collapse",
        .configSection = "EnemyRagdoll",
        .keyPtr = &cfg.enemyRagdollKey,
        .callback =
            [](bool, const RuntimeContextSnapshot& runtime) {
                auto* world = runtime.world;
                auto* player = runtime.player;
                if (!player || !world) return;
                ActorUtils::ForEachWillie(world, player, [](SDK::AWillie_BP_C* willie) {
                    willie->All_Body_Tonus = GameConstants::DEFAULT_PAIN;
                });
            },
        .kind = KeybindKind::State,
        .events = {GameEvent::OffLedge},
        .group = "Bones & Balance",
    });

    keybinds.Add({
        .name = "Drunk Enemies",
        .tooltip = "Makes enemies stumble and lose balance",
        .configSection = "DrunkEnemies",
        .keyPtr = &cfg.enemyDrunkKey,
        .callback =
            [this](bool active, const RuntimeContextSnapshot& runtime) {
                auto* world = runtime.world;
                auto* player = runtime.player;
                if (!player || !world) return;
                const double level = active ? static_cast<double>(cfg.enemyDrunkLevel) : 0.0;
                ActorUtils::ForEachWillie(world, player, [level](SDK::AWillie_BP_C* willie) { willie->Drunk = level; });
            },
        .kind = KeybindKind::State,
        .applyOnToggle = true,
        .events = {GameEvent::OffLedge},
        .params = {KeybindParam(
            "drunk_level", "Intensity", &cfg.enemyDrunkLevel, 0.0f, 1.0f, "Controls how unsteady enemies are"
        )},
        .group = "Bones & Balance",
    });

    keybinds.Add({
        .name = "Unlimited Kicks",
        .tooltip = "Kick repeatedly without waiting",
        .configSection = "NoKickCooldown",
        .keyPtr = &cfg.noKickCooldownKey,
        .callback =
            [](bool, const RuntimeContextSnapshot& runtime) {
                auto* p = runtime.player;
                if (!p) return;
                p->Kick_Cooldown = false;
            },
        .kind = KeybindKind::State,
        .events = {GameEvent::OffLedge},
        .group = "Combat & Survival",
    });

    keybinds.Add({
        .name = "Kick Power",
        .tooltip = "Send targets farther with your kicks",
        .configSection = "KickMultiplier",
        .keyPtr = &cfg.kickMultiplierKey,
        .kind = KeybindKind::State,
        .functionHooks =
            {
                KeybindFunctionHook{
                    .functionName = "Kick L Timeline__UpdateFunc",
                    .callback = [this](GameHook::ProcessEventContext& context) { OpenKickWindow(true, context); },
                    .phase = GameHook::HookPhase::After,
                },
                KeybindFunctionHook{
                    .functionName = "Kick R Timeline__UpdateFunc",
                    .callback = [this](GameHook::ProcessEventContext& context) { OpenKickWindow(false, context); },
                    .phase = GameHook::HookPhase::After,
                },
                KeybindFunctionHook{
                    .functionName = "Kick L Timeline__FinishedFunc",
                    .callback = [this](GameHook::ProcessEventContext& context) { CloseKickWindow(true, context); },
                    .phase = GameHook::HookPhase::After,
                },
                KeybindFunctionHook{
                    .functionName = "Kick R Timeline__FinishedFunc",
                    .callback = [this](GameHook::ProcessEventContext& context) { CloseKickWindow(false, context); },
                    .phase = GameHook::HookPhase::After,
                },
                KeybindFunctionHook{
                    .functionName = "BndEvt__BP_ThirdPersonCharacter_Mesh_K2Node_ComponentBoundEvent_0_"
                                    "ComponentHitSignature__DelegateSignature",
                    .callback = [this](GameHook::ProcessEventContext& context) { HandleKickHit(context); },
                },
            },
        .params =
            {KeybindParam(
                 "kick_power_multiplier", "Power", &cfg.kickPowerMultiplier, 1.0f, 100.0f,
                 "Controls how far your kicks send targets"
             ),
             KeybindParam(
                 "apply_to_enemies", "Strong Enemy Kicks", &cfg.kickMultiplierAffectsEnemies,
                 "Enemy kicks send targets just as far"
             )},
        .group = "Combat & Survival",
    });

    keybinds.Add({
        .name = "Punch Power",
        .tooltip = "Send targets farther with your punches",
        .configSection = "KnockbackMultiplier",
        .keyPtr = &cfg.knockbackMultiplierKey,
        .kind = KeybindKind::State,
        .functionHooks =
            {
                KeybindFunctionHook{
                    .functionName = "BndEvt__BP_ThirdPersonCharacter_Mesh_K2Node_ComponentBoundEvent_0_"
                                    "ComponentHitSignature__DelegateSignature",
                    .callback = [this](GameHook::ProcessEventContext& context) { HandlePunchHit(context); },
                },
            },
        .params =
            {KeybindParam(
                 "knockback_multiplier", "Power", &cfg.knockbackMultiplier, 1.0f, 100.0f,
                 "Controls how far your punches send targets"
             ),
             KeybindParam(
                 "apply_to_enemies", "Strong Enemy Punches", &cfg.knockbackAffectsEnemies,
                 "Enemy punches send targets just as far"
             )},
        .group = "Combat & Survival",
    });

    keybinds.Add({
        .name = "Invulnerability",
        .tooltip = "Take no damage",
        .configSection = "Invulnerability",
        .keyPtr = &cfg.invulnerabilityKey,
        .callback =
            [](bool active, const RuntimeContextSnapshot& runtime) {
                auto* p = runtime.player;
                if (!p) return;
                p->BitPad_5C_0 = active;
                p->Invulnerable = active;
            },
        .kind = KeybindKind::State,
        .applyOnToggle = true,
        .events = {GameEvent::OffLedge},
        .group = "Combat & Survival",
    });

    keybinds.Add({
        .name = "Full Recovery",
        .tooltip = "Keeps you healthy, conscious, and free of pain and broken bones",
        .configSection = "NoPain",
        .keyPtr = &cfg.noPainKey,
        .callback =
            [](bool, const RuntimeContextSnapshot& runtime) {
                auto* p = runtime.player;
                if (!p) return;
                ActorUtils::ApplyNoPainEffect(p);
            },
        .kind = KeybindKind::State,
        .events = {GameEvent::OffLedge},
        .group = "Combat & Survival",
    });

    keybinds.Add({
        .name = "Enemy Full Recovery",
        .tooltip = "Keeps enemies healthy, conscious, and free of pain and broken bones",
        .configSection = "EnemyNoPain",
        .keyPtr = &cfg.enemyNoPainKey,
        .callback =
            [](bool, const RuntimeContextSnapshot& runtime) {
                auto* world = runtime.world;
                auto* player = runtime.player;
                if (!player || !world) return;
                ActorUtils::ForEachWillie(world, player, ActorUtils::ApplyNoPainEffect);
            },
        .kind = KeybindKind::State,
        .events = {GameEvent::OffLedge},
        .group = "Combat & Survival",
    });

    keybinds.Add({
        .name = "Stand Up",
        .tooltip = "Stand up immediately",
        .configSection = "GetUp",
        .keyPtr = &cfg.getUpKey,
        .callback =
            [](bool, const RuntimeContextSnapshot& runtime) {
                auto* p = runtime.player;
                if (!p) return;
                p->Get_Up_Rate = GameConstants::GET_UP_RATE;
            },
        .group = "Special Abilities",
    });

    keybinds.Add({
        .name = "Dash",
        .tooltip = "Surge forward",
        .configSection = "Dash",
        .keyPtr = &cfg.dashKey,
        .callback =
            [this](bool, const RuntimeContextSnapshot& runtime) {
                auto* p = runtime.player;
                if (!p) return;
                SDK::FVector forwardVector = p->GetActorForwardVector();
                p->Mesh->AddImpulse(forwardVector * cfg.dashForce, SDK::FName(), true);
            },
        .params = {KeybindParam("force", "Speed", &cfg.dashForce, 1000.0f, 10000.0f, "Controls how fast you dash")},
        .group = "Special Abilities",
    });

    keybinds.Add({
        .name = "Bite",
        .tooltip = "Bite nearby targets",
        .configSection = "BiteAttack",
        .keyPtr = &cfg.biteAttackKey,
        .callback = [](bool active,
                       const RuntimeContextSnapshot& runtime) { ActorUtils::ApplyBiteState(runtime.player, active); },
        .kind = KeybindKind::State,
        .applyOnToggle = true,
        .events = {GameEvent::OffLedge},
        .group = "Special Abilities",
    });

    keybinds.Add({
        .name = "Nearest Enemy Bites",
        .tooltip = "Makes the nearest enemy bite",
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
        .kind = KeybindKind::State,
        .applyOnToggle = true,
        .events = {GameEvent::OffLedge},
        .params =
            {KeybindParam("range", "Range", &cfg.biteRange, 50.0f, 2000.0f, "Controls how far away the enemy can be")},
        .group = "Special Abilities",
    });

    keybinds.Add({
        .name = "Nearby Enemies Bite",
        .tooltip = "Makes nearby enemies bite",
        .configSection = "EnemyBiteAll",
        .keyPtr = &cfg.enemyBiteAllKey,
        .callback =
            [this](bool active, const RuntimeContextSnapshot& runtime) {
                auto* world = runtime.world;
                auto* player = runtime.player;
                if (!player || !world) return;
                ActorUtils::ForEachWillieInRadius(world, player, cfg.biteAllRange, [active](SDK::AWillie_BP_C* willie) {
                    ActorUtils::ApplyBiteState(willie, active);
                });
            },
        .kind = KeybindKind::State,
        .applyOnToggle = true,
        .events = {GameEvent::OffLedge},
        .params =
            {KeybindParam("range", "Range", &cfg.biteAllRange, 50.0f, 2000.0f, "Controls how far away enemies can be")},
        .group = "Special Abilities",
    });

    keybinds.Add({
        .name = "Control Nearest NPC",
        .tooltip = "Take control of the nearest NPC; use it again to return",
        .configSection = "PossessNearestWillie",
        .keyPtr = &cfg.possessWillieKey,
        .callback = [](bool, const RuntimeContextSnapshot& runtime) { TogglePossession(runtime); },
        .group = "Special Abilities",
    });
}
