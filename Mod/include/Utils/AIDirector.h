#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/ModContext.h"
#include "Menu/EventBus.h"

namespace SDK {
    class AWillie_BP_C;
}

class AIDirector {
public:
    static AIDirector& Get();

    enum class Scope : int {
        AllNPCs,
        NearestNpc,
        Radius,
        Team,
        TargetingPlayer,
        HasAI,
        TickEnabled,
        HasTarget,
        NoTarget,
        PlayerTeam,
        NotPlayerTeam
    };

    enum class Profile : int {
        Aggressive,
        Defensive,
        Passive,
        Panic,
        DrunkBrawl,
        Bodyguard,
        Duelist,
        Horde,
        Coward,
        Berserker,
        TrainingDummy
    };

    enum class Directive : int {
        None,
        AttackPlayer,
        FightEachOther,
        ProtectPlayer,
        IgnorePlayer,
        PanicFlee,
        FreezeAI,
        DuelMode
    };

    enum class TargetMode : int { Player, NearestNpc, Clear };
    enum class Impulse : int { Attack, Dash, StopBlade };

    struct TargetFilter {
        Scope scope = Scope::AllNPCs;
        float radius = 1000.0f;
        int team = 0;
    };

    struct BehaviorSettings {
        float bodySkill = 1.0f;
        float weaponSkill = 1.0f;
        float dodgeRate = 1.0f;
        float runningSpeed = 1.0f;
        float drunkLevel = 0.0f;
        float attackIntent = 1.0f;
        float defendIntent = 1.0f;
        float retreatIntent = 0.0f;
        float strafeIntent = 0.0f;
        float berserkRate = 0.0f;
        float parryRate = 1.0f;
        float swingSpeed = 1.0f;
        float changeAttackRate = 1.0f;
        float approachDistance = 180.0f;
        double aiInvincibility = 0.0;
        double aiArmorInvincibility = 0.0;
        int combatBehavior = 0;
        int strafeMode = 0;
        bool fearless = false;
    };

    struct StatusSummary {
        int targets = -1;
        int aiControllers = 0;
        int tickEnabled = 0;
        int targetAssigned = 0;
        int targetingPlayer = 0;
        int noTarget = 0;
        int teamMin = 0;
        int teamMax = 0;
        double attackIntent = 0.0;
        double defendIntent = 0.0;
        double retreatIntent = 0.0;
        double strafeIntent = 0.0;
    };

    struct CommandResult {
        std::uint64_t sequence = 0;
        std::string message;
        bool isError = false;
    };

    struct Snapshot {
        StatusSummary summary;
        CommandResult lastResult;
        Directive activeDirective = Directive::None;
    };

    void RefreshStatus(TargetFilter query);
    void SetAITick(TargetFilter query, bool enabled);
    void StopAI(TargetFilter query);
    void ApplyBehavior(TargetFilter query, BehaviorSettings settings);
    void ApplyTeam(TargetFilter query, int newTeam);
    void ApplyProfile(TargetFilter query, Profile profile);
    void SetTarget(TargetFilter query, TargetMode mode);
    void TriggerImpulse(TargetFilter query, Impulse impulse);
    void ToggleDirective(TargetFilter query, Directive directive);
    void SetDirective(TargetFilter query, Directive directive);
    void ClearDirective();
    [[nodiscard]] Directive ActiveDirective() const noexcept;
    void PrepareForRuntimeShutdown() noexcept;
    void OnRuntimeShutdown() noexcept;
    Snapshot GetSnapshot() const;

    AIDirector(const AIDirector&) = delete;
    AIDirector& operator=(const AIDirector&) = delete;

private:
    AIDirector();

    struct MutationResult {
        int matched = 0;
        int affected = 0;
        int skippedNoAI = 0;
    };

    struct ActorState {
        int willieTeam = 0;
        int aiTeam = 0;
        SDK::AWillie_BP_C* target = nullptr;
        double bodySkill = 0.0;
        double weaponSkill = 0.0;
        double dodgeRate = 0.0;
        double runningSpeed = 0.0;
        double drunk = 0.0;
        double attackIntent = 0.0;
        double defendIntent = 0.0;
        double retreatIntent = 0.0;
        double strafeIntent = 0.0;
        double threatLevel = 0.0;
        double berserkRate = 0.0;
        double drunkness = 0.0;
        int combatBehavior = 0;
        int strafeMode = 0;
        bool targetFound = false;
        bool tickEnabled = false;
        bool willieFearless = false;
        bool aiFearless = false;
        bool willieImmediateThreat = false;
        bool beingThreatened = false;
        bool aiThreat = false;
        bool lostInterest = false;
        bool retreat = false;
        bool willieDualist = false;
        bool aiDualist = false;
    };

    void RefreshStatus(const RuntimeContextSnapshot& runtime, TargetFilter query);
    void SetAITick(const RuntimeContextSnapshot& runtime, TargetFilter query, bool enabled);
    void StopAI(const RuntimeContextSnapshot& runtime, TargetFilter query);
    void ApplyBehavior(const RuntimeContextSnapshot& runtime, TargetFilter query, const BehaviorSettings& settings);
    void ApplyTeam(const RuntimeContextSnapshot& runtime, TargetFilter query, int newTeam);
    void ApplyProfile(const RuntimeContextSnapshot& runtime, TargetFilter query, Profile profile);
    void SetTarget(const RuntimeContextSnapshot& runtime, TargetFilter query, TargetMode mode);
    void TriggerImpulse(const RuntimeContextSnapshot& runtime, TargetFilter query, Impulse impulse);
    void StartDirective(
        const RuntimeContextSnapshot& runtime, TargetFilter query, Directive directive, bool switchingDirective
    );
    void ClearDirective(const RuntimeContextSnapshot& runtime);
    void ApplyDirective(const RuntimeContextSnapshot& runtime, bool triggerAttack);
    void OnDirectiveTick(const RuntimeContextSnapshot& runtime);
    bool RestoreDirectiveState(const RuntimeContextSnapshot& runtime, const char* successMessage);
    void EnsureDirectiveTickSubscription();
    void UnsubscribeDirectiveTickIfIdle();

    bool PublishUnavailable(const RuntimeContextSnapshot& runtime);
    void PublishStatus(StatusSummary summary, CommandResult result);
    void PublishResult(MutationResult result, const char* successMessage, bool aiOnly);
    void PublishMessage(const char* message, bool isError = false);
    void PublishDirectiveResult(int changed, const char* successMessage);

    mutable std::mutex snapshotMutex;
    mutable std::mutex directiveMutex;
    Snapshot snapshot;
    std::uint64_t nextResultSequence = 0;
    std::atomic<Directive> activeDirective{Directive::None};
    TargetFilter directiveQuery;
    bool pendingDirectiveRestore = false;
    bool pendingDirectiveInitialTrigger = false;
    double nextDirectiveApplyTime = 0.0;
    EventBus::SubscriptionHandle directiveTickSubscription = EventBus::INVALID_SUBSCRIPTION;
    std::unordered_map<SDK::AWillie_BP_C*, ActorState> originalStates;
    std::vector<SDK::AWillie_BP_C*> targetsBuffer;
    std::vector<SDK::AWillie_BP_C*> enemiesBuffer;
};
