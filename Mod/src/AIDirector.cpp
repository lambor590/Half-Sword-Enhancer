#include "Utils/AIDirector.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <iterator>
#include <string>
#include <utility>

#include "Hooks/GameHook.h"
#include "Menu/GameEvent.h"
#include "SDK/AI_BP_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "Utils/ActorUtils.h"
#include "Utils/GameConstants.h"

namespace {
    constexpr double DIRECTIVE_INTERVAL_SECONDS = 0.25;
    constexpr int DIRECTIVE_HOSTILE_TEAM = 31;
    constexpr int DIRECTIVE_HOSTILE_ALT_TEAM = 32;

    double ElapsedSeconds() {
        static const auto START = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - START).count();
    }

    bool PointerLess(SDK::AWillie_BP_C* lhs, SDK::AWillie_BP_C* rhs) noexcept {
        return reinterpret_cast<uintptr_t>(lhs) < reinterpret_cast<uintptr_t>(rhs);
    }

    bool TargetsPlayer(SDK::AWillie_BP_C* willie, SDK::AWillie_BP_C* player) {
        if (!willie || !player) return false;

        if (auto* ai = ActorUtils::GetAIController(willie); ai && ai->Target == player) return true;
        for (auto* actor : willie->Targeted_By_AI) {
            if (actor == player) return true;
        }
        return false;
    }

    template <typename Func>
    int ForEachTarget(
        const RuntimeContextSnapshot& runtime, AIDirector::TargetFilter query, Func&& func
    ) {
        auto* world = runtime.world;
        auto* player = runtime.player;
        if (!world || !player) return 0;

        if (query.scope == AIDirector::Scope::NearestNpc) {
            auto* nearest = ActorUtils::FindNearestWillie(world, player, player, GameConstants::MAX_DISTANCE);
            if (!nearest) return 0;
            func(nearest);
            return 1;
        }

        int count = 0;
        const float targetRadius =
            query.scope == AIDirector::Scope::Radius ? query.radius : GameConstants::MAX_DISTANCE;
        ActorUtils::ForEachWillieInRadius(world, player, targetRadius, [&](SDK::AWillie_BP_C* willie) {
            auto* ai = ActorUtils::GetAIController(willie);
            if (query.scope == AIDirector::Scope::Team && willie->Team_Int != query.team) return;
            if (query.scope == AIDirector::Scope::TargetingPlayer && !TargetsPlayer(willie, player)) return;
            if (query.scope == AIDirector::Scope::HasAI && !ai) return;
            if (query.scope == AIDirector::Scope::TickEnabled && (!ai || !ai->IsActorTickEnabled())) return;
            if (query.scope == AIDirector::Scope::HasTarget && (!ai || !ai->Target)) return;
            if (query.scope == AIDirector::Scope::NoTarget && (!ai || ai->Target)) return;
            if (query.scope == AIDirector::Scope::PlayerTeam && willie->Team_Int != player->Team_Int) return;
            if (query.scope == AIDirector::Scope::NotPlayerTeam && willie->Team_Int == player->Team_Int) return;

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
        auto* ai = ActorUtils::GetAIController(willie);
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

    void ApplyProfileToWillie(SDK::AWillie_BP_C* willie, AIDirector::Profile profile, int playerTeam) {
        if (!willie) return;

        auto* ai = ActorUtils::GetAIController(willie);
        if (ai) ai->SetActorTickEnabled(true);

        switch (profile) {
            case AIDirector::Profile::Aggressive:
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
            case AIDirector::Profile::Defensive:
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
            case AIDirector::Profile::Passive:
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
            case AIDirector::Profile::Panic:
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
            case AIDirector::Profile::DrunkBrawl:
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
            case AIDirector::Profile::Bodyguard:
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
            case AIDirector::Profile::Duelist:
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
            case AIDirector::Profile::Horde:
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
            case AIDirector::Profile::Coward:
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
            case AIDirector::Profile::Berserker:
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
            case AIDirector::Profile::TrainingDummy:
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

        if (willie->Fearless || (ai && ai->Fearless)) ActorUtils::ApplyFearlessEffect(willie);

        if (ai) {
            ai->Combat_Behavior = SDK::EAI_CombatBehavior_Enum::NewEnumerator0;
            ai->Strafe_Enum = SDK::EAI_Strafe_Enum::NewEnumerator0;
            ai->My_Pawn = willie;
            ai->Team_Int = willie->Team_Int;
        }
    }

}

AIDirector& AIDirector::Get() {
    static AIDirector instance;
    return instance;
}

AIDirector::AIDirector() {
    targetsBuffer.reserve(64);
    enemiesBuffer.reserve(64);
    originalStates.reserve(64);
}

void AIDirector::RefreshStatus(TargetFilter query) {
    GameHook::QueueAction([query](const RuntimeContextSnapshot& runtime) {
        AIDirector::Get().RefreshStatus(runtime, query);
    });
}

void AIDirector::SetAITick(TargetFilter query, bool enabled) {
    GameHook::QueueAction([query, enabled](const RuntimeContextSnapshot& runtime) {
        AIDirector::Get().SetAITick(runtime, query, enabled);
    });
}

void AIDirector::StopAI(TargetFilter query) {
    ClearDirective();
    GameHook::QueueAction([query](const RuntimeContextSnapshot& runtime) {
        AIDirector::Get().StopAI(runtime, query);
    });
}

void AIDirector::ApplyBehavior(TargetFilter query, BehaviorSettings settings) {
    GameHook::QueueAction([query, settings](const RuntimeContextSnapshot& runtime) {
        AIDirector::Get().ApplyBehavior(runtime, query, settings);
    });
}

void AIDirector::ApplyTeam(TargetFilter query, int newTeam) {
    GameHook::QueueAction([query, newTeam](const RuntimeContextSnapshot& runtime) {
        AIDirector::Get().ApplyTeam(runtime, query, newTeam);
    });
}

void AIDirector::ApplyProfile(TargetFilter query, Profile profile) {
    GameHook::QueueAction([query, profile](const RuntimeContextSnapshot& runtime) {
        AIDirector::Get().ApplyProfile(runtime, query, profile);
    });
}

void AIDirector::SetTarget(TargetFilter query, TargetMode mode) {
    GameHook::QueueAction([query, mode](const RuntimeContextSnapshot& runtime) {
        AIDirector::Get().SetTarget(runtime, query, mode);
    });
}

void AIDirector::TriggerImpulse(TargetFilter query, Impulse impulse) {
    GameHook::QueueAction([query, impulse](const RuntimeContextSnapshot& runtime) {
        AIDirector::Get().TriggerImpulse(runtime, query, impulse);
    });
}

void AIDirector::ToggleDirective(TargetFilter query, Directive directive) {
    SetDirective(query, ActiveDirective() == directive ? Directive::None : directive);
}

void AIDirector::SetDirective(TargetFilter query, Directive directive) {
    if (directive == Directive::None) {
        ClearDirective();
        return;
    }

    const auto previousDirective = ActiveDirective();
    if (previousDirective == directive) return;

    activeDirective.store(static_cast<int>(directive), std::memory_order_release);
    EnsureDirectiveTickSubscription();

    const bool switchingDirective = previousDirective != Directive::None;
    GameHook::QueueAction([query, directive, switchingDirective](const RuntimeContextSnapshot& runtime) {
        AIDirector::Get().StartDirective(runtime, query, directive, switchingDirective);
    });
}

void AIDirector::ClearDirective() {
    bool hasRestoreWork = false;
    {
        std::lock_guard lock(directiveMutex);
        hasRestoreWork = !originalStates.empty() || pendingDirectiveRestore;
    }
    if (ActiveDirective() == Directive::None && !hasRestoreWork) return;

    activeDirective.store(static_cast<int>(Directive::None), std::memory_order_release);
    EnsureDirectiveTickSubscription();

    GameHook::QueueAction([](const RuntimeContextSnapshot& runtime) { AIDirector::Get().ClearDirective(runtime); });
}

AIDirector::Directive AIDirector::ActiveDirective() const noexcept {
    return static_cast<Directive>(activeDirective.load(std::memory_order_acquire));
}

void AIDirector::OnRuntimeShutdown() noexcept {
    try {
        {
            std::lock_guard lock(directiveMutex);
            activeDirective.store(static_cast<int>(Directive::None), std::memory_order_release);
            directiveQuery = {};
            pendingDirectiveRestore = false;
            pendingDirectiveInitialTrigger = false;
            nextDirectiveApplyTime = 0.0;
            directiveTickSubscription = EventBus::INVALID_SUBSCRIPTION;
            originalStates.clear();
            targetsBuffer.clear();
            enemiesBuffer.clear();
        }
    } catch (...) {
        return;
    }
}

AIDirector::Snapshot AIDirector::GetSnapshot() const {
    std::lock_guard lock(snapshotMutex);
    auto current = snapshot;
    current.activeDirective = ActiveDirective();
    return current;
}

bool AIDirector::PublishUnavailable(const RuntimeContextSnapshot& runtime) {
    if (runtime.world && runtime.player) return false;

    CommandResult result;
    result.message = runtime.world ? "Player not available" : "World not available";
    result.isError = true;

    std::lock_guard lock(snapshotMutex);
    result.sequence = ++nextResultSequence;
    snapshot.lastResult = std::move(result);
    return true;
}

void AIDirector::PublishStatus(StatusSummary summary, CommandResult result) {
    std::lock_guard lock(snapshotMutex);
    result.sequence = ++nextResultSequence;
    snapshot.summary = summary;
    snapshot.lastResult = std::move(result);
}

void AIDirector::PublishResult(MutationResult result, const char* successMessage, bool aiOnly) {
    CommandResult published;
    if (result.matched == 0) {
        published.message = "No NPCs matched";
    } else if (aiOnly && result.affected == 0) {
        published.message = "No AI controllers matched";
    } else {
        published.message = successMessage;
        if (result.skippedNoAI > 0) {
            published.message += " (";
            published.message += std::to_string(result.skippedNoAI);
            published.message += " no AI)";
        }
    }
    published.isError = result.matched == 0 || (aiOnly && result.affected == 0);

    std::lock_guard lock(snapshotMutex);
    published.sequence = ++nextResultSequence;
    snapshot.lastResult = std::move(published);
}

void AIDirector::PublishMessage(const char* message, bool isError) {
    CommandResult published;
    published.message = message ? message : "";
    published.isError = isError;

    std::lock_guard lock(snapshotMutex);
    published.sequence = ++nextResultSequence;
    snapshot.lastResult = std::move(published);
}

void AIDirector::PublishDirectiveResult(int changed, const char* successMessage) {
    PublishMessage(changed > 0 ? successMessage : "No NPCs matched", changed == 0);
}

void AIDirector::EnsureDirectiveTickSubscription() {
    std::lock_guard lock(directiveMutex);
    if (directiveTickSubscription != EventBus::INVALID_SUBSCRIPTION) return;

    directiveTickSubscription = EventBus::Get().Subscribe(GameEvent::OnTick, [](EventBus::EventContext& event) {
        AIDirector::Get().OnDirectiveTick(event.Runtime());
    });
}

void AIDirector::UnsubscribeDirectiveTickIfIdle() {
    if (ActiveDirective() != Directive::None || pendingDirectiveRestore) return;
    if (directiveTickSubscription == EventBus::INVALID_SUBSCRIPTION) return;

    EventBus::Get().Unsubscribe(directiveTickSubscription);
    directiveTickSubscription = EventBus::INVALID_SUBSCRIPTION;
}

void AIDirector::StartDirective(
    const RuntimeContextSnapshot& runtime, TargetFilter query, Directive directive, bool switchingDirective
) {
    std::lock_guard lock(directiveMutex);
    if (ActiveDirective() != directive) return;

    directiveQuery = query;
    if ((switchingDirective || pendingDirectiveRestore) && !RestoreDirectiveState(runtime, nullptr)) {
        pendingDirectiveInitialTrigger = true;
        nextDirectiveApplyTime = ElapsedSeconds() + DIRECTIVE_INTERVAL_SECONDS;
        return;
    }

    pendingDirectiveRestore = false;
    pendingDirectiveInitialTrigger = false;
    nextDirectiveApplyTime = 0.0;
    ApplyDirective(runtime, true);
    nextDirectiveApplyTime = ElapsedSeconds() + DIRECTIVE_INTERVAL_SECONDS;
}

void AIDirector::ClearDirective(const RuntimeContextSnapshot& runtime) {
    std::lock_guard lock(directiveMutex);
    pendingDirectiveRestore = pendingDirectiveRestore || !originalStates.empty();
    pendingDirectiveInitialTrigger = false;

    if (RestoreDirectiveState(runtime, "Directive cleared")) {
        UnsubscribeDirectiveTickIfIdle();
    } else {
        nextDirectiveApplyTime = ElapsedSeconds() + DIRECTIVE_INTERVAL_SECONDS;
    }
}

void AIDirector::OnDirectiveTick(const RuntimeContextSnapshot& runtime) {
    std::lock_guard lock(directiveMutex);

    const double now = ElapsedSeconds();
    if (now < nextDirectiveApplyTime) return;

    bool triggerInitialApply = false;
    if (pendingDirectiveRestore) {
        nextDirectiveApplyTime = now + DIRECTIVE_INTERVAL_SECONDS;
        if (!RestoreDirectiveState(runtime, nullptr)) return;

        if (ActiveDirective() == Directive::None) {
            PublishMessage("Directive cleared");
            UnsubscribeDirectiveTickIfIdle();
            return;
        }
        triggerInitialApply = pendingDirectiveInitialTrigger;
        pendingDirectiveInitialTrigger = false;
    }

    if (ActiveDirective() == Directive::None) {
        UnsubscribeDirectiveTickIfIdle();
        return;
    }

    nextDirectiveApplyTime = now + DIRECTIVE_INTERVAL_SECONDS;
    ApplyDirective(runtime, triggerInitialApply);
}

bool AIDirector::RestoreDirectiveState(const RuntimeContextSnapshot& runtime, const char* successMessage) {
    if (originalStates.empty()) {
        pendingDirectiveRestore = false;
        if (successMessage) PublishMessage(successMessage);
        return true;
    }

    if (PublishUnavailable(runtime)) {
        pendingDirectiveRestore = true;
        return false;
    }

    targetsBuffer.clear();
    targetsBuffer.push_back(runtime.player);
    ActorUtils::ForEachWillieInRadius(
        runtime.world, runtime.player, GameConstants::MAX_DISTANCE,
        [&](auto* willie) { targetsBuffer.push_back(willie); }
    );
    std::sort(targetsBuffer.begin(), targetsBuffer.end(), PointerLess);

    auto isCurrentTarget = [this](uintptr_t key) {
        return std::binary_search(
            targetsBuffer.begin(), targetsBuffer.end(), std::bit_cast<SDK::AWillie_BP_C*>(key), PointerLess
        );
    };

    for (auto it = originalStates.begin(); it != originalStates.end();) {
        if (!isCurrentTarget(it->first)) {
            it = originalStates.erase(it);
            continue;
        }

        auto* willie = std::bit_cast<SDK::AWillie_BP_C*>(it->first);
        willie->Team_Int = it->second.willieTeam;
        willie->Body_Skill__Temp_ = it->second.bodySkill;
        willie->Weapon_Skill__Temp_ = it->second.weaponSkill;
        willie->Dodge_Rate = it->second.dodgeRate;
        willie->Running_Speed_Rate = it->second.runningSpeed;
        willie->Drunk = it->second.drunk;
        willie->Fearless = it->second.willieFearless;
        willie->AI_Immediate_Threat = it->second.willieImmediateThreat;
        willie->NPC_Dualist = it->second.willieDualist;
        if (auto* ai = ActorUtils::GetAIController(willie)) {
            ai->Team_Int = it->second.aiTeam;
            ai->Target =
                isCurrentTarget(it->second.target) ? std::bit_cast<SDK::AWillie_BP_C*>(it->second.target) : nullptr;
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
        it = originalStates.erase(it);
    }

    pendingDirectiveRestore = false;
    if (successMessage) PublishMessage(successMessage);
    return true;
}

void AIDirector::ApplyDirective(const RuntimeContextSnapshot& runtime, bool triggerAttack) {
    if (PublishUnavailable(runtime)) return;

    const TargetFilter selectedQuery = directiveQuery;
    const bool publishResult = triggerAttack;

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
        if (auto* ai = ActorUtils::GetAIController(willie)) {
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
        if (auto* ai = ActorUtils::GetAIController(willie)) ai->Team_Int = newTeam;
    };

    originalStates.erase(reinterpret_cast<uintptr_t>(runtime.player));
    if (!originalStates.empty()) {
        ActorUtils::ForEachWillieInRadius(
            runtime.world, runtime.player, GameConstants::MAX_DISTANCE,
            [&](auto* willie) { enemiesBuffer.push_back(willie); }
        );
        std::sort(enemiesBuffer.begin(), enemiesBuffer.end(), PointerLess);
        for (auto it = originalStates.begin(); it != originalStates.end();) {
            const bool current = std::binary_search(
                enemiesBuffer.begin(), enemiesBuffer.end(), std::bit_cast<SDK::AWillie_BP_C*>(it->first), PointerLess
            );
            it = current ? std::next(it) : originalStates.erase(it);
        }
        enemiesBuffer.clear();
    }

    switch (ActiveDirective()) {
        case Directive::AttackPlayer: {
            auto* player = runtime.player;
            int changed =
                ForEachTarget(runtime, selectedQuery, [&](SDK::AWillie_BP_C* willie) {
                    auto* ai = ActorUtils::GetAIController(willie);
                    if (!ai) return;

                    setTeam(willie, DIRECTIVE_HOSTILE_TEAM);
                    WakeAI(ai, willie, player, triggerAttack);
                    SetAggression(willie, 3.0, 0.5, 0.0, 0.4);
                    if (triggerAttack) ai->Attack();
                });
            if (publishResult) PublishDirectiveResult(changed, "Targeting player");
            break;
        }
        case Directive::FightEachOther: {
            ForEachTarget(runtime, selectedQuery, [&](SDK::AWillie_BP_C* willie) {
                targetsBuffer.push_back(willie);
            });
            if (targetsBuffer.size() < 2) {
                if (publishResult) PublishMessage("Need at least 2 NPCs", true);
                return;
            }

            int changed = 0;
            for (size_t i = 0; i < targetsBuffer.size(); ++i) {
                auto* willie = targetsBuffer[i];
                auto* enemy = targetsBuffer[(i + targetsBuffer.size() / 2) % targetsBuffer.size()];
                auto* ai = ActorUtils::GetAIController(willie);
                if (!ai || enemy == willie) continue;

                setTeam(willie, (i & 1) ? DIRECTIVE_HOSTILE_ALT_TEAM : DIRECTIVE_HOSTILE_TEAM);
                WakeAI(ai, willie, enemy, triggerAttack);
                SetAggression(willie, 3.0, 0.3, 0.0, 0.8);
                if (triggerAttack) ai->Attack();
                ++changed;
            }
            if (publishResult) PublishDirectiveResult(changed, "NPCs fighting each other");
            break;
        }
        case Directive::ProtectPlayer: {
            auto* player = runtime.player;
            if (!player) {
                if (publishResult) PublishMessage("No player", true);
                return;
            }

            const int playerTeam = player->Team_Int;
            ForEachTarget(runtime, selectedQuery, [&](SDK::AWillie_BP_C* willie) {
                targetsBuffer.push_back(willie);
            });

            ActorUtils::ForEachWillieInRadius(
                runtime.world, player, GameConstants::MAX_DISTANCE, [&](SDK::AWillie_BP_C* candidate) {
                    const bool selected = std::find(targetsBuffer.begin(), targetsBuffer.end(), candidate) !=
                                          targetsBuffer.end();
                    if (selected) return;
                    setTeam(candidate, DIRECTIVE_HOSTILE_TEAM);
                    enemiesBuffer.push_back(candidate);
                }
            );

            int changed = 0;
            for (auto* willie : targetsBuffer) {
                auto* ai = ActorUtils::GetAIController(willie);
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
            if (publishResult) PublishDirectiveResult(changed, "Bodyguards assigned");
            break;
        }
        case Directive::IgnorePlayer: {
            int changed =
                ForEachTarget(runtime, selectedQuery, [&](SDK::AWillie_BP_C* willie) {
                    auto* ai = ActorUtils::GetAIController(willie);
                    if (!ai) return;

                    saveState(willie);
                    if (ai->Target == runtime.player) {
                        ai->Target = nullptr;
                        ai->Target_Found = false;
                    }
                    SetAggression(willie, 0.0, 0.0, 0.4, 0.0);
                });
            if (publishResult) PublishDirectiveResult(changed, "Ignoring player");
            break;
        }
        case Directive::PanicFlee: {
            int changed =
                ForEachTarget(runtime, selectedQuery, [&](SDK::AWillie_BP_C* willie) {
                    auto* ai = ActorUtils::GetAIController(willie);
                    if (!ai) return;

                    saveState(willie);
                    WakeAI(ai, willie, runtime.player, false);
                    SetAggression(willie, 0.0, 0.2, 4.0, 0.0);
                    ai->Fearless = false;
                    willie->Fearless = false;
                });
            if (publishResult) PublishDirectiveResult(changed, "NPCs panicking");
            break;
        }
        case Directive::FreezeAI: {
            int changed =
                ForEachTarget(runtime, selectedQuery, [&](SDK::AWillie_BP_C* willie) {
                    auto* ai = ActorUtils::GetAIController(willie);
                    if (!ai) return;

                    saveState(willie);
                    ai->Target = nullptr;
                    ai->Target_Found = false;
                    ai->SetActorTickEnabled(false);
                    SetAggression(willie, 0.0, 0.0, 0.0, 0.0);
                });
            if (publishResult) PublishDirectiveResult(changed, "AI frozen");
            break;
        }
        case Directive::DuelMode: {
            ForEachTarget(runtime, selectedQuery, [&](SDK::AWillie_BP_C* willie) {
                targetsBuffer.push_back(willie);
            });
            if (targetsBuffer.size() < 2) {
                if (publishResult) PublishMessage("Need at least 2 NPCs", true);
                return;
            }

            auto* first = targetsBuffer[0];
            auto* second = targetsBuffer[1];
            int changed = 0;
            for (size_t i = 0; i < targetsBuffer.size(); ++i) {
                auto* willie = targetsBuffer[i];
                auto* ai = ActorUtils::GetAIController(willie);
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
            if (publishResult) PublishDirectiveResult(changed, "Duel mode active");
            break;
        }
        case Directive::None:
        default: break;
    }
}

void AIDirector::RefreshStatus(const RuntimeContextSnapshot& runtime, TargetFilter query) {
    if (PublishUnavailable(runtime)) return;

    StatusSummary next;
    bool hasTeam = false;
    next.targets = ForEachTarget(runtime, query, [&](SDK::AWillie_BP_C* willie) {
        if (!hasTeam) {
            next.teamMin = willie->Team_Int;
            next.teamMax = willie->Team_Int;
            hasTeam = true;
        } else {
            if (willie->Team_Int < next.teamMin) next.teamMin = willie->Team_Int;
            if (willie->Team_Int > next.teamMax) next.teamMax = willie->Team_Int;
        }

        if (TargetsPlayer(willie, runtime.player)) ++next.targetingPlayer;
        auto* ai = ActorUtils::GetAIController(willie);
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

    CommandResult result;
    result.message = "Status refreshed";
    PublishStatus(next, std::move(result));
}

void AIDirector::SetAITick(const RuntimeContextSnapshot& runtime, TargetFilter query, bool enabled) {
    if (PublishUnavailable(runtime)) return;

    MutationResult result;
    result.matched = ForEachTarget(runtime, query, [&](SDK::AWillie_BP_C* willie) {
        if (auto* ai = ActorUtils::GetAIController(willie)) {
            ai->SetActorTickEnabled(enabled);
            ++result.affected;
        } else {
            ++result.skippedNoAI;
        }
    });
    PublishResult(result, enabled ? "AI tick enabled" : "AI tick disabled", true);
}

void AIDirector::StopAI(const RuntimeContextSnapshot& runtime, TargetFilter query) {
    if (PublishUnavailable(runtime)) return;

    MutationResult result;
    result.matched = ForEachTarget(runtime, query, [&](SDK::AWillie_BP_C* willie) {
        auto* ai = ActorUtils::GetAIController(willie);
        if (!ai) {
            ++result.skippedNoAI;
            return;
        }
        SetAggression(willie, 0.0, 0.0, 0.0, 0.0);
        ai->Target = nullptr;
        ai->Target_Found = false;
        ai->SetActorTickEnabled(false);
        ++result.affected;
    });
    PublishResult(result, "AI stopped", true);
}

void AIDirector::ApplyBehavior(
    const RuntimeContextSnapshot& runtime, TargetFilter query, const BehaviorSettings& settings
) {
    if (PublishUnavailable(runtime)) return;

    MutationResult result;
    result.matched = ForEachTarget(runtime, query, [&](SDK::AWillie_BP_C* willie) {
        willie->Drunk = settings.drunkLevel;
        willie->Body_Skill__Temp_ = settings.bodySkill;
        willie->Weapon_Skill__Temp_ = settings.weaponSkill;
        willie->Dodge_Rate = settings.dodgeRate;
        willie->Running_Speed_Rate = settings.runningSpeed;
        willie->AI_Invincibility_Rate = settings.aiInvincibility;
        willie->AI_Armor_Invincibility_Rate = settings.aiArmorInvincibility;
        SetAggression(willie, settings.attackIntent, settings.defendIntent, settings.retreatIntent, settings.strafeIntent);
        ActorUtils::SetFearlessReinforced(willie, settings.fearless);
        if (settings.fearless) {
            ActorUtils::ApplyFearlessEffect(willie);
        } else {
            willie->Fearless = false;
        }

        if (auto* ai = ActorUtils::GetAIController(willie)) {
            ai->Drunkness = settings.drunkLevel;
            ai->Berserk_Rate = settings.berserkRate;
            ai->Parry_Rate = settings.parryRate;
            ai->Swing_Speed = settings.swingSpeed;
            ai->Change_Attack_Rate = settings.changeAttackRate;
            ai->Approach_Distance = settings.approachDistance;
            ai->AI_Armor_Invincibility_Rate = settings.aiArmorInvincibility;
            ai->Combat_Behavior = static_cast<SDK::EAI_CombatBehavior_Enum>(settings.combatBehavior);
            ai->Strafe_Enum = static_cast<SDK::EAI_Strafe_Enum>(settings.strafeMode);
            ai->Team_Int = willie->Team_Int;
            if (!settings.fearless) ai->Fearless = false;
        } else {
            ++result.skippedNoAI;
        }
        ++result.affected;
    });
    ActorUtils::SetFearlessReinforcementHooksEnabled(
        ActorUtils::PruneFearlessReinforcementTargets(runtime.world, runtime.player)
    );
    PublishResult(result, "Behavior applied", false);
}

void AIDirector::ApplyTeam(const RuntimeContextSnapshot& runtime, TargetFilter query, int newTeam) {
    if (PublishUnavailable(runtime)) return;

    MutationResult result;
    result.matched = ForEachTarget(runtime, query, [&](SDK::AWillie_BP_C* willie) {
        willie->Team_Int = newTeam;
        if (auto* ai = ActorUtils::GetAIController(willie)) {
            ai->Team_Int = newTeam;
        } else {
            ++result.skippedNoAI;
        }
        ++result.affected;
    });
    PublishResult(result, "Team applied", false);
}

void AIDirector::ApplyProfile(const RuntimeContextSnapshot& runtime, TargetFilter query, Profile profile) {
    if (PublishUnavailable(runtime)) return;

    const int playerTeam = runtime.player ? runtime.player->Team_Int : 0;
    MutationResult result;
    result.matched = ForEachTarget(runtime, query, [&](SDK::AWillie_BP_C* willie) {
        if (!ActorUtils::GetAIController(willie)) ++result.skippedNoAI;
        ApplyProfileToWillie(willie, profile, playerTeam);
        ++result.affected;
    });
    PublishResult(result, "Profile applied", false);
}

void AIDirector::SetTarget(const RuntimeContextSnapshot& runtime, TargetFilter query, TargetMode mode) {
    if (PublishUnavailable(runtime)) return;

    std::lock_guard lock(directiveMutex);
    targetsBuffer.clear();
    if (mode == TargetMode::NearestNpc) {
        ActorUtils::ForEachWillieInRadius(
            runtime.world, runtime.player, GameConstants::MAX_DISTANCE,
            [&](SDK::AWillie_BP_C* willie) { targetsBuffer.push_back(willie); }
        );
    }

    MutationResult result;
    result.matched = ForEachTarget(runtime, query, [&](SDK::AWillie_BP_C* willie) {
        auto* ai = ActorUtils::GetAIController(willie);
        if (!ai) {
            ++result.skippedNoAI;
            return;
        }

        if (mode == TargetMode::Clear) {
            ai->Target = nullptr;
            ai->Target_Found = false;
            ai->Lost_Interest = true;
            ++result.affected;
            return;
        }
        if (mode == TargetMode::Player) {
            WakeAI(ai, willie, runtime.player, true);
            ++result.affected;
            return;
        }

        SDK::AWillie_BP_C* nearest = nullptr;
        float nearestDistance = 3000.0f;
        for (auto* candidate : targetsBuffer) {
            if (!candidate || candidate == willie) continue;
            const float distance = willie->GetDistanceTo(candidate);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = candidate;
            }
        }
        WakeAI(ai, willie, nearest, true);
        ++result.affected;
    });

    const char* message = "Targets cleared";
    if (mode == TargetMode::Player) message = "Target set to player";
    if (mode == TargetMode::NearestNpc) message = "Target set to nearest NPC";
    PublishResult(result, message, true);
}

void AIDirector::TriggerImpulse(const RuntimeContextSnapshot& runtime, TargetFilter query, Impulse impulse) {
    if (PublishUnavailable(runtime)) return;

    MutationResult result;
    result.matched = ForEachTarget(runtime, query, [&](SDK::AWillie_BP_C* willie) {
        auto* ai = ActorUtils::GetAIController(willie);
        if (!ai) {
            ++result.skippedNoAI;
            return;
        }
        if (impulse == Impulse::Attack) ai->Attack();
        if (impulse == Impulse::Dash) ai->Dash_Event();
        if (impulse == Impulse::StopBlade) ai->Stop_That_Blade();
        ++result.affected;
    });

    const char* message = "Attack forced";
    if (impulse == Impulse::Dash) message = "Dash forced";
    if (impulse == Impulse::StopBlade) message = "Blade stop forced";
    PublishResult(result, message, true);
}
