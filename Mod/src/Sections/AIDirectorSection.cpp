#include "Menu/Sections/World/AIDirectorSection.h"

#include <chrono>
#include <iterator>

#include "Hooks/GameHook.h"
#include "Menu/EventBus.h"
#include "Menu/GameEvent.h"
#include "Menu/SectionRegistry.h"
#include "Menu/SectionStyle.h"
#include "SDK/AI_BP_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "Utils/ActorUtils.h"
#include "Utils/GameConstants.h"
#include "Utils/GuiUtils.h"

REGISTER_SECTION(AIDirectorSection, MenuTab::World);

namespace {
    constexpr const char* SCOPE_LABELS[] = {
        "All Enemies",  "Nearest Enemy", "Radius",    "Team",        "Targeting Player", "Has AI",
        "Tick Enabled", "Has Target",    "No Target", "Player Team", "Not Player Team",
    };

    constexpr const char* PROFILE_LABELS[] = {
        "Aggressive", "Defensive", "Passive", "Panic",     "Drunk Brawl",    "Bodyguard",
        "Duelist",    "Horde",     "Coward",  "Berserker", "Training Dummy",
    };

    constexpr const char* COMBAT_BEHAVIOR_LABELS[] = {
        "0", "1", "2", "3", "4", "5",
    };

    constexpr const char* STRAFE_LABELS[] = {
        "Default",
        "Alternate",
    };

    constexpr double DIRECTIVE_INTERVAL_SECONDS = 0.25;
    constexpr int DIRECTIVE_ALLY_TEAM = 30;
    constexpr int DIRECTIVE_HOSTILE_TEAM = 31;
    constexpr int DIRECTIVE_HOSTILE_ALT_TEAM = 32;

    double NowSeconds() {
        static const auto start = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }

    const char* DirectiveLabel(AIDirectorSection::Directive directive) {
        switch (directive) {
            case AIDirectorSection::Directive::AttackPlayer: return "Attack Player";
            case AIDirectorSection::Directive::FightEachOther: return "Fight Each Other";
            case AIDirectorSection::Directive::ProtectPlayer: return "Protect Player";
            case AIDirectorSection::Directive::IgnorePlayer: return "Ignore Player";
            case AIDirectorSection::Directive::PanicFlee: return "Panic / Flee";
            case AIDirectorSection::Directive::FreezeAI: return "Freeze AI";
            case AIDirectorSection::Directive::DuelMode: return "Duel Mode";
            case AIDirectorSection::Directive::None:
            default: return "None";
        }
    }

    SDK::AAI_BP_C* AIController(SDK::AWillie_BP_C* willie) {
        if (!willie) return nullptr;
        auto* controller = willie->GetController();
        return controller && controller->IsA(SDK::AAI_BP_C::StaticClass()) ? static_cast<SDK::AAI_BP_C*>(controller)
                                                                           : nullptr;
    }

    bool TargetsPlayer(SDK::AWillie_BP_C* willie, SDK::AWillie_BP_C* player) {
        if (!willie || !player) return false;

        if (auto* ai = AIController(willie); ai && ai->Target == player) return true;
        for (auto* actor : willie->Targeted_By_AI) {
            if (actor == player) return true;
        }
        return false;
    }

    template <typename Func>
    int ForEachTarget(
        const RuntimeContextSnapshot& runtime, AIDirectorSection::Scope scope, float radius, int team, Func&& func
    ) {
        auto* world = runtime.world;
        auto* player = runtime.player;
        if (!world || !player) return 0;

        if (scope == AIDirectorSection::Scope::NearestEnemy) {
            auto* nearest = ActorUtils::FindNearestWillie(world, player, player, GameConstants::MAX_DISTANCE);
            if (!nearest) return 0;
            func(nearest);
            return 1;
        }

        int count = 0;
        const float targetRadius = scope == AIDirectorSection::Scope::Radius ? radius : GameConstants::MAX_DISTANCE;
        ActorUtils::ForEachWillieInRadius(world, player, targetRadius, [&](SDK::AWillie_BP_C* willie) {
            auto* ai = AIController(willie);
            if (scope == AIDirectorSection::Scope::Team && willie->Team_Int != team) return;
            if (scope == AIDirectorSection::Scope::TargetingPlayer && !TargetsPlayer(willie, player)) return;
            if (scope == AIDirectorSection::Scope::HasAI && !ai) return;
            if (scope == AIDirectorSection::Scope::TickEnabled && (!ai || !ai->IsActorTickEnabled())) return;
            if (scope == AIDirectorSection::Scope::HasTarget && (!ai || !ai->Target)) return;
            if (scope == AIDirectorSection::Scope::NoTarget && (!ai || ai->Target)) return;
            if (scope == AIDirectorSection::Scope::PlayerTeam && willie->Team_Int != player->Team_Int) return;
            if (scope == AIDirectorSection::Scope::NotPlayerTeam && willie->Team_Int == player->Team_Int) return;

            func(willie);
            ++count;
        });
        return count;
    }

    void WakeAI(SDK::AAI_BP_C* ai, SDK::AWillie_BP_C* willie, SDK::AWillie_BP_C* target, bool provoke) {
        if (!ai || !willie) return;

        ai->My_Pawn = willie;
        ai->Target = target;
        ai->Target_Found = target != nullptr;
        ai->Lost_Interest = false;
        ai->SetActorTickEnabled(true);
        if (target && provoke) ai->Get_Insulted(target);
    }

    void SetAggression(SDK::AWillie_BP_C* willie, double attack, double defend, double retreat, double strafe) {
        auto* ai = AIController(willie);
        if (!ai) return;

        ai->Attack_Intent = attack;
        ai->Defend_Intent = defend;
        ai->Retreat_Intent = retreat;
        ai->Strafing_Intent = strafe;
        ai->Threat_Level = attack > defend ? attack : defend;
        ai->Being_Threatened = attack > 0.0 || defend > 0.0;
        ai->AI_Threat = attack > 0.0;
        ai->Lost_Interest = attack <= 0.0 && defend <= 0.0;
        ai->Retreat = retreat > attack && retreat > defend;
        if (willie) willie->AI_Immediate_Threat = ai->Being_Threatened;
    }

    void ApplyProfileToWillie(SDK::AWillie_BP_C* willie, AIDirectorSection::Profile profile, int playerTeam) {
        if (!willie) return;

        auto* ai = AIController(willie);
        if (ai) ai->SetActorTickEnabled(true);

        switch (profile) {
            case AIDirectorSection::Profile::Aggressive:
                willie->Fearless = true;
                willie->Drunk = 0.0;
                willie->Body_Skill__Temp_ = 3.0;
                willie->Weapon_Skill__Temp_ = 3.0;
                willie->Dodge_Rate = 2.0;
                willie->Running_Speed_Rate = 1.35;
                SetAggression(willie, 3.0, 0.5, 0.0, 0.5);
                if (ai) {
                    ai->Fearless = true;
                    ai->Berserk_Rate = 1.0;
                    ai->Drunkness = 0.0;
                }
                break;
            case AIDirectorSection::Profile::Defensive:
                willie->Fearless = true;
                willie->Drunk = 0.0;
                willie->Body_Skill__Temp_ = 1.75;
                willie->Weapon_Skill__Temp_ = 2.5;
                willie->Dodge_Rate = 3.0;
                willie->Running_Speed_Rate = 0.9;
                SetAggression(willie, 0.7, 3.0, 0.2, 1.0);
                if (ai) {
                    ai->Fearless = true;
                    ai->Berserk_Rate = 0.1;
                    ai->Drunkness = 0.0;
                }
                break;
            case AIDirectorSection::Profile::Passive:
                willie->Fearless = false;
                willie->Drunk = 0.0;
                willie->Body_Skill__Temp_ = 0.25;
                willie->Weapon_Skill__Temp_ = 0.25;
                willie->Dodge_Rate = 0.0;
                willie->Running_Speed_Rate = 0.35;
                SetAggression(willie, 0.0, 0.0, 1.0, 0.0);
                if (ai) {
                    ai->Target = nullptr;
                    ai->Target_Found = false;
                    ai->Fearless = false;
                    ai->Berserk_Rate = 0.0;
                    ai->Drunkness = 0.0;
                }
                break;
            case AIDirectorSection::Profile::Panic:
                willie->Fearless = false;
                willie->Drunk = 0.0;
                willie->Body_Skill__Temp_ = 0.2;
                willie->Weapon_Skill__Temp_ = 0.1;
                willie->Dodge_Rate = 4.0;
                willie->Running_Speed_Rate = 1.7;
                SetAggression(willie, 0.0, 0.2, 4.0, 0.0);
                if (ai) {
                    ai->Fearless = false;
                    ai->Berserk_Rate = 0.0;
                    ai->Drunkness = 0.0;
                }
                break;
            case AIDirectorSection::Profile::DrunkBrawl:
                willie->Fearless = true;
                willie->Drunk = 1.0;
                willie->Body_Skill__Temp_ = 1.0;
                willie->Weapon_Skill__Temp_ = 0.75;
                willie->Dodge_Rate = 0.3;
                willie->Running_Speed_Rate = 0.85;
                SetAggression(willie, 2.0, 0.2, 0.0, 1.5);
                if (ai) {
                    ai->Fearless = true;
                    ai->Berserk_Rate = 0.8;
                    ai->Drunkness = 1.0;
                }
                break;
            case AIDirectorSection::Profile::Bodyguard:
                willie->Fearless = true;
                willie->Drunk = 0.0;
                willie->Body_Skill__Temp_ = 2.5;
                willie->Weapon_Skill__Temp_ = 2.5;
                willie->Dodge_Rate = 2.0;
                willie->Running_Speed_Rate = 1.2;
                willie->Team_Int = playerTeam;
                SetAggression(willie, 2.0, 2.0, 0.0, 0.5);
                if (ai) {
                    ai->Fearless = true;
                    ai->Team_Int = playerTeam;
                    ai->Berserk_Rate = 0.3;
                    ai->Drunkness = 0.0;
                }
                break;
            case AIDirectorSection::Profile::Duelist:
                willie->Fearless = true;
                willie->Drunk = 0.0;
                willie->Body_Skill__Temp_ = 4.0;
                willie->Weapon_Skill__Temp_ = 4.0;
                willie->Dodge_Rate = 3.0;
                willie->Running_Speed_Rate = 1.05;
                willie->NPC_Dualist = true;
                SetAggression(willie, 2.0, 2.0, 0.0, 0.75);
                if (ai) {
                    ai->Fearless = true;
                    ai->NPC_Dualist = true;
                    ai->Berserk_Rate = 0.2;
                    ai->Drunkness = 0.0;
                }
                break;
            case AIDirectorSection::Profile::Horde:
                willie->Fearless = true;
                willie->Drunk = 0.25;
                willie->Body_Skill__Temp_ = 0.8;
                willie->Weapon_Skill__Temp_ = 0.6;
                willie->Dodge_Rate = 0.2;
                willie->Running_Speed_Rate = 1.45;
                SetAggression(willie, 2.5, 0.0, 0.0, 0.0);
                if (ai) {
                    ai->Fearless = true;
                    ai->Berserk_Rate = 1.5;
                    ai->Drunkness = 0.25;
                }
                break;
            case AIDirectorSection::Profile::Coward:
                willie->Fearless = false;
                willie->Drunk = 0.0;
                willie->Body_Skill__Temp_ = 0.4;
                willie->Weapon_Skill__Temp_ = 0.3;
                willie->Dodge_Rate = 2.0;
                willie->Running_Speed_Rate = 1.4;
                SetAggression(willie, 0.0, 0.2, 3.0, 0.0);
                if (ai) {
                    ai->Fearless = false;
                    ai->Berserk_Rate = 0.0;
                    ai->Drunkness = 0.0;
                }
                break;
            case AIDirectorSection::Profile::Berserker:
                willie->Fearless = true;
                willie->Drunk = 0.0;
                willie->Body_Skill__Temp_ = 4.5;
                willie->Weapon_Skill__Temp_ = 2.0;
                willie->Dodge_Rate = 0.5;
                willie->Running_Speed_Rate = 1.6;
                SetAggression(willie, 5.0, 0.0, 0.0, 0.0);
                if (ai) {
                    ai->Fearless = true;
                    ai->Berserk_Rate = 3.0;
                    ai->Drunkness = 0.0;
                }
                break;
            case AIDirectorSection::Profile::TrainingDummy:
                willie->Fearless = true;
                willie->Drunk = 0.0;
                willie->Body_Skill__Temp_ = 0.0;
                willie->Weapon_Skill__Temp_ = 0.0;
                willie->Dodge_Rate = 0.0;
                willie->Running_Speed_Rate = 0.0;
                SetAggression(willie, 0.0, 0.0, 0.0, 0.0);
                if (ai) {
                    ai->Target = nullptr;
                    ai->Target_Found = false;
                    ai->SetActorTickEnabled(false);
                }
                break;
        }

        if (ai) {
            ai->Combat_Behavior = SDK::EAI_CombatBehavior_Enum::NewEnumerator0;
            ai->Strafe_Enum = SDK::EAI_Strafe_Enum::NewEnumerator0;
            ai->My_Pawn = willie;
            ai->Team_Int = willie->Team_Int;
        }
    }

    void SetStatus(GuiUtils::StatusMessage& status, int changed, const char* message) {
        status.Set(changed > 0 ? message : "No NPCs matched", changed == 0);
    }
}

AIDirectorSection::AIDirectorSection(ModContext& ctx) : Section(ctx, "AI Director") {
    targetsBuffer.reserve(64);
    enemiesBuffer.reserve(64);
}

void AIDirectorSection::OnOpen() {
    RefreshStatus();
}

void AIDirectorSection::RefreshStatus() {
    auto selectedScope = scope;
    float selectedRadius = radius;
    int selectedTeam = team;
    GameHook::QueueAction([this, selectedScope, selectedRadius, selectedTeam](const RuntimeContextSnapshot& runtime) {
        StatusSummary next;
        bool hasTeam = false;

        next.targets =
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [&](SDK::AWillie_BP_C* willie) {
                if (!hasTeam) {
                    next.teamMin = willie->Team_Int;
                    next.teamMax = willie->Team_Int;
                    hasTeam = true;
                } else {
                    if (willie->Team_Int < next.teamMin) next.teamMin = willie->Team_Int;
                    if (willie->Team_Int > next.teamMax) next.teamMax = willie->Team_Int;
                }

                if (TargetsPlayer(willie, runtime.player)) ++next.targetingPlayer;
                auto* ai = AIController(willie);
                if (!ai) return;

                ++next.aiControllers;
                if (ai->IsActorTickEnabled()) ++next.tickEnabled;
                if (ai->Target) {
                    ++next.targetAssigned;
                } else {
                    ++next.noTarget;
                }
                next.attackIntent += ai->Attack_Intent;
                next.defendIntent += ai->Defend_Intent;
                next.retreatIntent += ai->Retreat_Intent;
                next.strafeIntent += ai->Strafing_Intent;
            });

        if (next.aiControllers > 0) {
            next.attackIntent /= next.aiControllers;
            next.defendIntent /= next.aiControllers;
            next.retreatIntent /= next.aiControllers;
            next.strafeIntent /= next.aiControllers;
        }
        summary = next;
        status.Set("Status refreshed");
    });
}

void AIDirectorSection::SetAITick(bool enabled) {
    auto selectedScope = scope;
    float selectedRadius = radius;
    int selectedTeam = team;
    GameHook::QueueAction([this, selectedScope, selectedRadius, selectedTeam,
                           enabled](const RuntimeContextSnapshot& runtime) {
        int changed =
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [enabled](SDK::AWillie_BP_C* willie) {
                if (auto* ai = AIController(willie)) ai->SetActorTickEnabled(enabled);
            });
        SetStatus(status, changed, enabled ? "AI tick enabled" : "AI tick disabled");
    });
}

void AIDirectorSection::StopAI() {
    SetDirective(Directive::None);

    auto selectedScope = scope;
    float selectedRadius = radius;
    int selectedTeam = team;
    GameHook::QueueAction([this, selectedScope, selectedRadius, selectedTeam](const RuntimeContextSnapshot& runtime) {
        int changed =
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [](SDK::AWillie_BP_C* willie) {
                SetAggression(willie, 0.0, 0.0, 0.0, 0.0);
                if (auto* ai = AIController(willie)) {
                    ai->Target = nullptr;
                    ai->Target_Found = false;
                    ai->SetActorTickEnabled(false);
                }
            });
        SetStatus(status, changed, "AI stopped");
    });
}

void AIDirectorSection::AttackPlayer() {
    ToggleDirective(Directive::AttackPlayer);
}

void AIDirectorSection::ApplyBehavior() {
    auto selectedScope = scope;
    float selectedRadius = radius;
    int selectedTeam = team;
    float selectedBodySkill = bodySkill;
    float selectedWeaponSkill = weaponSkill;
    float selectedDodgeRate = dodgeRate;
    float selectedRunningSpeed = runningSpeed;
    float selectedDrunk = drunkLevel;
    float selectedAttack = attackIntent;
    float selectedDefend = defendIntent;
    float selectedRetreat = retreatIntent;
    float selectedStrafe = strafeIntent;
    float selectedBerserk = berserkRate;
    float selectedParry = parryRate;
    float selectedSwing = swingSpeed;
    float selectedChangeAttack = changeAttackRate;
    float selectedApproach = approachDistance;
    int selectedCombat = combatBehavior;
    int selectedStrafeMode = strafeMode;
    double selectedInvincibility = aiInvincibility;
    double selectedArmorInvincibility = aiArmorInvincibility;
    bool selectedFearless = fearless;
    GameHook::QueueAction([this, selectedScope, selectedRadius, selectedTeam, selectedBodySkill, selectedWeaponSkill,
                           selectedDodgeRate, selectedRunningSpeed, selectedDrunk, selectedAttack, selectedDefend,
                           selectedRetreat, selectedStrafe, selectedBerserk, selectedParry, selectedSwing,
                           selectedChangeAttack, selectedApproach, selectedCombat, selectedStrafeMode,
                           selectedInvincibility, selectedArmorInvincibility,
                           selectedFearless](const RuntimeContextSnapshot& runtime) {
        int changed =
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [&](SDK::AWillie_BP_C* willie) {
                willie->Fearless = selectedFearless;
                willie->Drunk = selectedDrunk;
                willie->Body_Skill__Temp_ = selectedBodySkill;
                willie->Weapon_Skill__Temp_ = selectedWeaponSkill;
                willie->Dodge_Rate = selectedDodgeRate;
                willie->Running_Speed_Rate = selectedRunningSpeed;
                willie->AI_Invincibility_Rate = selectedInvincibility;
                willie->AI_Armor_Invincibility_Rate = selectedArmorInvincibility;
                SetAggression(willie, selectedAttack, selectedDefend, selectedRetreat, selectedStrafe);

                if (auto* ai = AIController(willie)) {
                    ai->Fearless = selectedFearless;
                    ai->Drunkness = selectedDrunk;
                    ai->Berserk_Rate = selectedBerserk;
                    ai->Parry_Rate = selectedParry;
                    ai->Swing_Speed = selectedSwing;
                    ai->Change_Attack_Rate = selectedChangeAttack;
                    ai->Approach_Distance = selectedApproach;
                    ai->AI_Armor_Invincibility_Rate = selectedArmorInvincibility;
                    ai->Combat_Behavior = static_cast<SDK::EAI_CombatBehavior_Enum>(selectedCombat);
                    ai->Strafe_Enum = static_cast<SDK::EAI_Strafe_Enum>(selectedStrafeMode);
                    ai->Team_Int = willie->Team_Int;
                }
            });
        SetStatus(status, changed, "Behavior applied");
    });
}

void AIDirectorSection::ApplyTeam() {
    auto selectedScope = scope;
    float selectedRadius = radius;
    int selectedTeam = team;
    int newTeam = teamOverride;
    GameHook::QueueAction([this, selectedScope, selectedRadius, selectedTeam,
                           newTeam](const RuntimeContextSnapshot& runtime) {
        int changed =
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [newTeam](SDK::AWillie_BP_C* willie) {
                willie->Team_Int = newTeam;
                if (auto* ai = AIController(willie)) ai->Team_Int = newTeam;
            });
        SetStatus(status, changed, "Team applied");
    });
}

void AIDirectorSection::ApplyProfile() {
    auto selectedScope = scope;
    auto selectedProfile = profile;
    float selectedRadius = radius;
    int selectedTeam = team;
    GameHook::QueueAction([this, selectedScope, selectedProfile, selectedRadius,
                           selectedTeam](const RuntimeContextSnapshot& runtime) {
        const int playerTeam = runtime.player ? runtime.player->Team_Int : 0;
        int changed =
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [&](SDK::AWillie_BP_C* willie) {
                ApplyProfileToWillie(willie, selectedProfile, playerTeam);
            });
        SetStatus(status, changed, "Profile applied");
    });
}

void AIDirectorSection::FightEachOther() {
    ToggleDirective(Directive::FightEachOther);
}

void AIDirectorSection::ProtectPlayer() {
    ToggleDirective(Directive::ProtectPlayer);
}

void AIDirectorSection::ToggleDirective(Directive directive) {
    SetDirective(static_cast<Directive>(activeDirective.load()) == directive ? Directive::None : directive);
}

void AIDirectorSection::ForceTargetPlayer() {
    auto selectedScope = scope;
    float selectedRadius = radius;
    int selectedTeam = team;
    GameHook::QueueAction([this, selectedScope, selectedRadius, selectedTeam](const RuntimeContextSnapshot& runtime) {
        int changed =
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [&](SDK::AWillie_BP_C* willie) {
                if (auto* ai = AIController(willie)) WakeAI(ai, willie, runtime.player, true);
            });
        SetStatus(status, changed, "Target set to player");
    });
}

void AIDirectorSection::ForceTargetNearest() {
    auto selectedScope = scope;
    float selectedRadius = radius;
    int selectedTeam = team;
    GameHook::QueueAction([this, selectedScope, selectedRadius, selectedTeam](const RuntimeContextSnapshot& runtime) {
        int changed =
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [&](SDK::AWillie_BP_C* willie) {
                auto* ai = AIController(willie);
                if (!ai || !runtime.world || !runtime.player) return;

                auto* nearest = ActorUtils::FindNearestWillie(runtime.world, runtime.player, willie, 3000.0f, willie);
                WakeAI(ai, willie, nearest, true);
            });
        SetStatus(status, changed, "Target set to nearest NPC");
    });
}

void AIDirectorSection::ClearTargets() {
    auto selectedScope = scope;
    float selectedRadius = radius;
    int selectedTeam = team;
    GameHook::QueueAction([this, selectedScope, selectedRadius, selectedTeam](const RuntimeContextSnapshot& runtime) {
        int changed =
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [](SDK::AWillie_BP_C* willie) {
                if (auto* ai = AIController(willie)) {
                    ai->Target = nullptr;
                    ai->Target_Found = false;
                    ai->Lost_Interest = true;
                }
            });
        SetStatus(status, changed, "Targets cleared");
    });
}

void AIDirectorSection::ForceAttack() {
    auto selectedScope = scope;
    float selectedRadius = radius;
    int selectedTeam = team;
    GameHook::QueueAction([this, selectedScope, selectedRadius, selectedTeam](const RuntimeContextSnapshot& runtime) {
        int changed =
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [](SDK::AWillie_BP_C* willie) {
                if (auto* ai = AIController(willie)) ai->Attack();
            });
        SetStatus(status, changed, "Attack forced");
    });
}

void AIDirectorSection::ForceDash() {
    auto selectedScope = scope;
    float selectedRadius = radius;
    int selectedTeam = team;
    GameHook::QueueAction([this, selectedScope, selectedRadius, selectedTeam](const RuntimeContextSnapshot& runtime) {
        int changed =
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [](SDK::AWillie_BP_C* willie) {
                if (auto* ai = AIController(willie)) ai->Dash_Event();
            });
        SetStatus(status, changed, "Dash forced");
    });
}

void AIDirectorSection::StopBlades() {
    auto selectedScope = scope;
    float selectedRadius = radius;
    int selectedTeam = team;
    GameHook::QueueAction([this, selectedScope, selectedRadius, selectedTeam](const RuntimeContextSnapshot& runtime) {
        int changed =
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [](SDK::AWillie_BP_C* willie) {
                if (auto* ai = AIController(willie)) ai->Stop_That_Blade();
            });
        SetStatus(status, changed, "Blade stop forced");
    });
}

void AIDirectorSection::SetDirective(Directive directive) {
    const auto previousDirective = static_cast<Directive>(activeDirective.load());
    if (previousDirective == directive) return;

    const bool disabling = directive == Directive::None;
    const bool wasInactive = previousDirective == Directive::None;
    directiveScope.store(static_cast<int>(scope));
    directiveRadius.store(radius);
    directiveTeam.store(team);
    activeDirective.store(static_cast<int>(directive));

    if (disabling) {
        EventBus::Get().Unsubscribe(GameEvent::OnTick, this);
        GameHook::QueueAction([this](const RuntimeContextSnapshot& runtime) {
            RestoreDirectiveState(runtime);
            status.Set("Directive cleared");
        });
        return;
    }

    if (wasInactive) {
        EventBus::Get().Subscribe(GameEvent::OnTick, this, [this](const RuntimeContextSnapshot& runtime) {
            OnDirectiveTick(runtime);
        });
    }

    const bool switchingDirective = !wasInactive;
    GameHook::QueueAction([this, directive, switchingDirective](const RuntimeContextSnapshot& runtime) {
        if (static_cast<Directive>(activeDirective.load()) != directive) return;
        if (switchingDirective) RestoreDirectiveState(runtime);
        nextDirectiveApplyTime = 0.0;
        ApplyDirective(runtime, true);
        nextDirectiveApplyTime = NowSeconds() + DIRECTIVE_INTERVAL_SECONDS;
    });
}

void AIDirectorSection::ApplyDirective(const RuntimeContextSnapshot& runtime, bool triggerAttack) {
    auto selectedScope = static_cast<Scope>(directiveScope.load());
    float selectedRadius = directiveRadius.load();
    int selectedTeam = directiveTeam.load();

    targetsBuffer.clear();
    enemiesBuffer.clear();

    auto saveState = [this](SDK::AWillie_BP_C* willie) {
        if (!willie) return;

        const auto key = reinterpret_cast<uintptr_t>(willie);
        if (originalStates.find(key) != originalStates.end()) return;

        ActorState state;
        state.willieTeam = willie->Team_Int;
        state.bodySkill = willie->Body_Skill__Temp_;
        state.weaponSkill = willie->Weapon_Skill__Temp_;
        state.dodgeRate = willie->Dodge_Rate;
        state.runningSpeed = willie->Running_Speed_Rate;
        state.drunk = willie->Drunk;
        state.willieFearless = willie->Fearless;
        state.willieImmediateThreat = willie->AI_Immediate_Threat;
        state.willieDualist = willie->NPC_Dualist;
        if (auto* ai = AIController(willie)) {
            state.aiTeam = ai->Team_Int;
            state.target = reinterpret_cast<uintptr_t>(ai->Target);
            state.attackIntent = ai->Attack_Intent;
            state.defendIntent = ai->Defend_Intent;
            state.retreatIntent = ai->Retreat_Intent;
            state.strafeIntent = ai->Strafing_Intent;
            state.threatLevel = ai->Threat_Level;
            state.berserkRate = ai->Berserk_Rate;
            state.drunkness = ai->Drunkness;
            state.combatBehavior = static_cast<int>(ai->Combat_Behavior);
            state.strafeMode = static_cast<int>(ai->Strafe_Enum);
            state.targetFound = ai->Target_Found;
            state.tickEnabled = ai->IsActorTickEnabled();
            state.aiFearless = ai->Fearless;
            state.beingThreatened = ai->Being_Threatened;
            state.aiThreat = ai->AI_Threat;
            state.lostInterest = ai->Lost_Interest;
            state.retreat = ai->Retreat;
            state.aiDualist = ai->NPC_Dualist;
        }
        originalStates.emplace(key, state);
    };

    auto setTeam = [&](SDK::AWillie_BP_C* willie, int newTeam) {
        if (!willie) return;

        saveState(willie);
        willie->Team_Int = newTeam;
        if (auto* ai = AIController(willie)) ai->Team_Int = newTeam;
    };

    if (runtime.world && runtime.player) {
        originalStates.erase(reinterpret_cast<uintptr_t>(runtime.player));
        ActorUtils::ForEachWillieInRadius(
            runtime.world, runtime.player, GameConstants::MAX_DISTANCE,
            [&](auto* willie) { targetsBuffer.push_back(willie); }
        );
        for (auto it = originalStates.begin(); it != originalStates.end();) {
            bool exists = false;
            for (auto* willie : targetsBuffer) {
                if (reinterpret_cast<uintptr_t>(willie) == it->first) {
                    exists = true;
                    break;
                }
            }
            it = exists ? std::next(it) : originalStates.erase(it);
        }
        targetsBuffer.clear();
    }

    switch (static_cast<Directive>(activeDirective.load())) {
        case Directive::AttackPlayer: {
            auto* player = runtime.player;
            int changed =
                ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [&](SDK::AWillie_BP_C* willie) {
                    auto* ai = AIController(willie);
                    if (!ai) return;

                    setTeam(willie, DIRECTIVE_HOSTILE_TEAM);
                    WakeAI(ai, willie, player, triggerAttack);
                    SetAggression(willie, 3.0, 0.5, 0.0, 0.4);
                    if (triggerAttack) ai->Attack();
                });
            SetStatus(status, changed, "Targeting player");
            break;
        }
        case Directive::FightEachOther: {
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [&](SDK::AWillie_BP_C* willie) {
                targetsBuffer.push_back(willie);
            });
            if (targetsBuffer.size() < 2) {
                status.Set("Need at least 2 NPCs", true);
                return;
            }

            int changed = 0;
            for (size_t i = 0; i < targetsBuffer.size(); ++i) {
                auto* willie = targetsBuffer[i];
                auto* enemy = targetsBuffer[(i + targetsBuffer.size() / 2) % targetsBuffer.size()];
                auto* ai = AIController(willie);
                if (!ai || enemy == willie) continue;

                setTeam(willie, (i & 1) ? DIRECTIVE_HOSTILE_ALT_TEAM : DIRECTIVE_HOSTILE_TEAM);
                WakeAI(ai, willie, enemy, triggerAttack);
                SetAggression(willie, 3.0, 0.3, 0.0, 0.8);
                if (triggerAttack) ai->Attack();
                ++changed;
            }
            SetStatus(status, changed, "NPCs fighting each other");
            break;
        }
        case Directive::ProtectPlayer: {
            auto* player = runtime.player;
            if (!player) {
                status.Set("No player", true);
                return;
            }

            const int playerTeam = player->Team_Int;
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [&](SDK::AWillie_BP_C* willie) {
                targetsBuffer.push_back(willie);
            });

            if (runtime.world) {
                ActorUtils::ForEachWillieInRadius(
                    runtime.world, player, GameConstants::MAX_DISTANCE,
                    [&](SDK::AWillie_BP_C* candidate) {
                        for (auto* target : targetsBuffer) {
                            if (candidate == target) return;
                        }
                        setTeam(candidate, DIRECTIVE_HOSTILE_TEAM);
                        enemiesBuffer.push_back(candidate);
                    }
                );
            }

            int changed = 0;
            for (auto* willie : targetsBuffer) {
                auto* ai = AIController(willie);
                if (!ai) continue;

                SDK::AWillie_BP_C* nearestEnemy = nullptr;
                float nearestDistance = GameConstants::MAX_DISTANCE;
                for (auto* candidate : enemiesBuffer) {
                    if (!candidate || candidate == willie) continue;
                    float distance = willie->GetDistanceTo(candidate);
                    if (distance < nearestDistance) {
                        nearestDistance = distance;
                        nearestEnemy = candidate;
                    }
                }

                setTeam(willie, playerTeam);
                ApplyProfileToWillie(willie, Profile::Bodyguard, playerTeam);
                WakeAI(ai, willie, nearestEnemy, triggerAttack);
                if (triggerAttack && nearestEnemy) ai->Attack();
                ++changed;
            }
            SetStatus(status, changed, "Bodyguards assigned");
            break;
        }
        case Directive::IgnorePlayer: {
            int changed =
                ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [&](SDK::AWillie_BP_C* willie) {
                    auto* ai = AIController(willie);
                    if (!ai) return;

                    saveState(willie);
                    if (ai->Target == runtime.player) {
                        ai->Target = nullptr;
                        ai->Target_Found = false;
                    }
                    SetAggression(willie, 0.0, 0.0, 0.4, 0.0);
                });
            SetStatus(status, changed, "Ignoring player");
            break;
        }
        case Directive::PanicFlee: {
            int changed =
                ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [&](SDK::AWillie_BP_C* willie) {
                    auto* ai = AIController(willie);
                    if (!ai) return;

                    saveState(willie);
                    WakeAI(ai, willie, runtime.player, false);
                    SetAggression(willie, 0.0, 0.2, 4.0, 0.0);
                    ai->Fearless = false;
                    willie->Fearless = false;
                });
            SetStatus(status, changed, "NPCs panicking");
            break;
        }
        case Directive::FreezeAI: {
            int changed =
                ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [&](SDK::AWillie_BP_C* willie) {
                    auto* ai = AIController(willie);
                    if (!ai) return;

                    saveState(willie);
                    ai->Target = nullptr;
                    ai->Target_Found = false;
                    ai->SetActorTickEnabled(false);
                    SetAggression(willie, 0.0, 0.0, 0.0, 0.0);
                });
            SetStatus(status, changed, "AI frozen");
            break;
        }
        case Directive::DuelMode: {
            ForEachTarget(runtime, selectedScope, selectedRadius, selectedTeam, [&](SDK::AWillie_BP_C* willie) {
                targetsBuffer.push_back(willie);
            });
            if (targetsBuffer.size() < 2) {
                status.Set("Need at least 2 NPCs", true);
                return;
            }

            auto* first = targetsBuffer[0];
            auto* second = targetsBuffer[1];
            int changed = 0;
            for (size_t i = 0; i < targetsBuffer.size(); ++i) {
                auto* willie = targetsBuffer[i];
                auto* ai = AIController(willie);
                if (!ai) continue;

                saveState(willie);
                ApplyProfileToWillie(willie, Profile::Duelist, 0);
                if (i == 0) {
                    setTeam(willie, DIRECTIVE_HOSTILE_TEAM);
                    WakeAI(ai, willie, second, triggerAttack);
                } else if (i == 1) {
                    setTeam(willie, DIRECTIVE_HOSTILE_ALT_TEAM);
                    WakeAI(ai, willie, first, triggerAttack);
                } else {
                    ai->Target = nullptr;
                    ai->Target_Found = false;
                    ai->SetActorTickEnabled(false);
                }
                ++changed;
            }
            SetStatus(status, changed, "Duel mode active");
            break;
        }
        case Directive::None:
        default: break;
    }
}

void AIDirectorSection::OnDirectiveTick(const RuntimeContextSnapshot& runtime) {
    if (static_cast<Directive>(activeDirective.load()) == Directive::None) return;

    const double now = NowSeconds();
    if (now < nextDirectiveApplyTime) return;

    nextDirectiveApplyTime = now + DIRECTIVE_INTERVAL_SECONDS;
    ApplyDirective(runtime, false);
}

void AIDirectorSection::RestoreDirectiveState(const RuntimeContextSnapshot& runtime) {
    targetsBuffer.clear();
    if (runtime.player) targetsBuffer.push_back(runtime.player);
    if (runtime.world && runtime.player) {
        ActorUtils::ForEachWillieInRadius(
            runtime.world, runtime.player, GameConstants::MAX_DISTANCE,
            [&](auto* willie) { targetsBuffer.push_back(willie); }
        );
    }

    auto isCurrentTarget = [this](uintptr_t target) {
        if (!target) return false;
        for (auto* willie : targetsBuffer) {
            if (reinterpret_cast<uintptr_t>(willie) == target) return true;
        }
        return false;
    };

    for (auto* willie : targetsBuffer) {
        if (!willie) continue;

        const auto key = reinterpret_cast<uintptr_t>(willie);
        auto it = originalStates.find(key);
        if (it == originalStates.end()) continue;

        willie->Team_Int = it->second.willieTeam;
        willie->Body_Skill__Temp_ = it->second.bodySkill;
        willie->Weapon_Skill__Temp_ = it->second.weaponSkill;
        willie->Dodge_Rate = it->second.dodgeRate;
        willie->Running_Speed_Rate = it->second.runningSpeed;
        willie->Drunk = it->second.drunk;
        willie->Fearless = it->second.willieFearless;
        willie->AI_Immediate_Threat = it->second.willieImmediateThreat;
        willie->NPC_Dualist = it->second.willieDualist;
        if (auto* ai = AIController(willie)) {
            ai->Team_Int = it->second.aiTeam;
            ai->Target =
                isCurrentTarget(it->second.target) ? reinterpret_cast<SDK::AWillie_BP_C*>(it->second.target) : nullptr;
            ai->Target_Found = ai->Target ? it->second.targetFound : false;
            ai->Attack_Intent = it->second.attackIntent;
            ai->Defend_Intent = it->second.defendIntent;
            ai->Retreat_Intent = it->second.retreatIntent;
            ai->Strafing_Intent = it->second.strafeIntent;
            ai->Threat_Level = it->second.threatLevel;
            ai->Berserk_Rate = it->second.berserkRate;
            ai->Drunkness = it->second.drunkness;
            ai->Combat_Behavior = static_cast<SDK::EAI_CombatBehavior_Enum>(it->second.combatBehavior);
            ai->Strafe_Enum = static_cast<SDK::EAI_Strafe_Enum>(it->second.strafeMode);
            ai->Fearless = it->second.aiFearless;
            ai->Being_Threatened = ai->Target ? it->second.beingThreatened : false;
            ai->AI_Threat = ai->Target ? it->second.aiThreat : false;
            ai->Lost_Interest = ai->Target ? it->second.lostInterest : false;
            ai->Retreat = ai->Target ? it->second.retreat : false;
            ai->NPC_Dualist = it->second.aiDualist;
            ai->SetActorTickEnabled(it->second.tickEnabled);
        }
    }
    originalStates.clear();
}

void AIDirectorSection::RenderScope() {
    ImGui::SeparatorText("Targets");

    int scopeIdx = static_cast<int>(scope);
    if (GuiUtils::BeginSizedCombo("Scope", SCOPE_LABELS[scopeIdx], 170.0f)) {
        for (int i = 0; i < static_cast<int>(std::size(SCOPE_LABELS)); ++i) {
            if (ImGui::Selectable(SCOPE_LABELS[i], i == scopeIdx)) scope = static_cast<Scope>(i);
            if (i == scopeIdx) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (scope == Scope::Radius) GuiUtils::DebouncedDragFloat("Radius", &radius, 10.0f, 50.0f, 5000.0f, "%.0f");
    if (scope == Scope::Team) GuiUtils::DebouncedDragInt("Team Filter", &team, 0.2f, 0, 32);
    if (ImGui::Button("Refresh Status")) RefreshStatus();
    status.Render();
}

void AIDirectorSection::RenderStatus() {
    ImGui::SeparatorText("Status");

    if (summary.targets < 0) {
        ImGui::TextUnformatted("Not refreshed");
        return;
    }

    ImGui::Text("Targets: %d", summary.targets);
    ImGui::SameLine();
    ImGui::Text("AI BP: %d", summary.aiControllers);
    ImGui::SameLine();
    ImGui::Text("Tick: %d", summary.tickEnabled);
    ImGui::Text("Assigned: %d", summary.targetAssigned);
    ImGui::SameLine();
    ImGui::Text("No Target: %d", summary.noTarget);
    ImGui::SameLine();
    ImGui::Text("Vs Player: %d", summary.targetingPlayer);
    ImGui::Text("Teams: %d - %d", summary.teamMin, summary.teamMax);
    ImGui::Text(
        "Intent A/D/R/S: %.2f / %.2f / %.2f / %.2f", summary.attackIntent, summary.defendIntent, summary.retreatIntent,
        summary.strafeIntent
    );
}

void AIDirectorSection::RenderAI() {
    ImGui::SeparatorText("AI");

    if (ImGui::Button("Enable Tick")) SetAITick(true);
    ImGui::SameLine();
    if (ImGui::Button("Disable Tick")) SetAITick(false);
    ImGui::SameLine();
    if (ImGui::Button("Stop AI")) StopAI();

    if (ImGui::Button("Target Player")) ForceTargetPlayer();
    ImGui::SameLine();
    if (ImGui::Button("Target Nearest")) ForceTargetNearest();
    ImGui::SameLine();
    if (ImGui::Button("Clear Target")) ClearTargets();

    if (ImGui::Button("Force Attack")) ForceAttack();
    ImGui::SameLine();
    if (ImGui::Button("Force Dash")) ForceDash();
    ImGui::SameLine();
    if (ImGui::Button("Stop Blade")) StopBlades();
}

void AIDirectorSection::RenderBehavior() {
    ImGui::SeparatorText("Profiles");

    int profileIdx = static_cast<int>(profile);
    if (GuiUtils::BeginSizedCombo("Profile", PROFILE_LABELS[profileIdx], 160.0f)) {
        for (int i = 0; i < static_cast<int>(std::size(PROFILE_LABELS)); ++i) {
            if (ImGui::Selectable(PROFILE_LABELS[i], i == profileIdx)) profile = static_cast<Profile>(i);
            if (i == profileIdx) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("Apply Profile")) ApplyProfile();

    ImGui::SeparatorText("Core Behavior");
    ImGui::Checkbox("Fearless", &fearless);
    GuiUtils::DebouncedDragFloat("Drunk", &drunkLevel, 0.01f, 0.0f, 1.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Body Skill", &bodySkill, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Weapon Skill", &weaponSkill, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Dodge Rate", &dodgeRate, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Running Speed", &runningSpeed, 0.05f, 0.0f, 10.0f, "%.2f");
}

void AIDirectorSection::RenderAdvanced() {
    ImGui::SeparatorText("Advanced");

    GuiUtils::DebouncedDragFloat("Attack Intent", &attackIntent, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Defend Intent", &defendIntent, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Retreat Intent", &retreatIntent, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Strafe Intent", &strafeIntent, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Berserk Rate", &berserkRate, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Parry Rate", &parryRate, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Swing Speed", &swingSpeed, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Attack Change Rate", &changeAttackRate, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Approach Distance", &approachDistance, 5.0f, 0.0f, 1000.0f, "%.0f");

    if (GuiUtils::BeginSizedCombo("Combat Behavior", COMBAT_BEHAVIOR_LABELS[combatBehavior], 150.0f)) {
        for (int i = 0; i < static_cast<int>(std::size(COMBAT_BEHAVIOR_LABELS)); ++i) {
            if (ImGui::Selectable(COMBAT_BEHAVIOR_LABELS[i], i == combatBehavior)) combatBehavior = i;
            if (i == combatBehavior) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (GuiUtils::BeginSizedCombo("Strafe Mode", STRAFE_LABELS[strafeMode], 150.0f)) {
        for (int i = 0; i < static_cast<int>(std::size(STRAFE_LABELS)); ++i) {
            if (ImGui::Selectable(STRAFE_LABELS[i], i == strafeMode)) strafeMode = i;
            if (i == strafeMode) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    float invincibility = static_cast<float>(aiInvincibility);
    if (GuiUtils::DebouncedDragFloat("AI Invincibility", &invincibility, 0.01f, 0.0f, 10.0f, "%.2f"))
        aiInvincibility = invincibility;

    float armorInvincibility = static_cast<float>(aiArmorInvincibility);
    if (GuiUtils::DebouncedDragFloat("AI Armor Invincibility", &armorInvincibility, 0.01f, 0.0f, 10.0f, "%.2f"))
        aiArmorInvincibility = armorInvincibility;

    if (ImGui::Button("Apply Behavior")) ApplyBehavior();

    ImGui::SeparatorText("Teams");
    GuiUtils::DebouncedDragInt("New Team", &teamOverride, 0.2f, 0, 32);
    if (ImGui::Button("Apply Team")) ApplyTeam();
}

void AIDirectorSection::RenderTactics() {
    ImGui::SeparatorText("Tactics");

    const auto currentDirective = static_cast<Directive>(activeDirective.load());
    ImGui::Text("Directive: %s", DirectiveLabel(currentDirective));

    if (ImGui::Button("Attack Player")) AttackPlayer();
    ImGui::SameLine();
    if (ImGui::Button("Fight Each Other")) FightEachOther();
    ImGui::SameLine();
    if (ImGui::Button("Protect Player")) ProtectPlayer();

    if (ImGui::Button("Ignore Player")) ToggleDirective(Directive::IgnorePlayer);
    ImGui::SameLine();
    if (ImGui::Button("Panic / Flee")) ToggleDirective(Directive::PanicFlee);
    ImGui::SameLine();
    if (ImGui::Button("Freeze AI")) ToggleDirective(Directive::FreezeAI);
    ImGui::SameLine();
    if (ImGui::Button("Duel Mode")) ToggleDirective(Directive::DuelMode);

    if (currentDirective != Directive::None) {
        if (ImGui::Button("Clear Directive")) SetDirective(Directive::None);
    }
}

void AIDirectorSection::Render() {
    const SectionStyle::StyleRAII style;
    RenderScope();
    RenderStatus();
    RenderTactics();
    RenderAI();
    RenderBehavior();
    RenderAdvanced();
}
