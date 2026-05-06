#pragma once

#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Menu/Section.h"
#include "Utils/GuiUtils.h"

namespace SDK {
    class AWillie_BP_C;
}

class AIDirectorSection : public Section {
public:
    enum class Scope : int {
        AllEnemies,
        NearestEnemy,
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

private:
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

    struct ActorState {
        int willieTeam = 0;
        int aiTeam = 0;
        uintptr_t target = 0;
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

    Scope scope = Scope::AllEnemies;
    Profile profile = Profile::Aggressive;
    std::atomic<int> activeDirective{static_cast<int>(Directive::None)};
    std::atomic<int> directiveScope{static_cast<int>(Scope::AllEnemies)};
    std::atomic<int> directiveTeam{0};
    std::atomic<float> directiveRadius{1000.0f};
    std::unordered_map<uintptr_t, ActorState> originalStates;
    std::vector<SDK::AWillie_BP_C*> targetsBuffer;
    std::vector<SDK::AWillie_BP_C*> enemiesBuffer;
    StatusSummary summary;

    int team = 0;
    int teamOverride = 0;
    int combatBehavior = 0;
    int strafeMode = 0;
    float radius = 1000.0f;
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
    double nextDirectiveApplyTime = 0.0;
    bool fearless = false;

    GuiUtils::StatusMessage status;

    void RefreshStatus();
    void SetAITick(bool enabled);
    void StopAI();
    void AttackPlayer();
    void ApplyBehavior();
    void ApplyTeam();
    void ApplyProfile();
    void FightEachOther();
    void ProtectPlayer();
    void ToggleDirective(Directive directive);
    void ForceTargetPlayer();
    void ForceTargetNearest();
    void ClearTargets();
    void ForceAttack();
    void ForceDash();
    void StopBlades();
    void SetDirective(Directive directive);
    void ApplyDirective(const RuntimeContextSnapshot& runtime, bool triggerAttack);
    void OnDirectiveTick(const RuntimeContextSnapshot& runtime);
    void RestoreDirectiveState(const RuntimeContextSnapshot& runtime);

    void RenderScope();
    void RenderStatus();
    void RenderAI();
    void RenderBehavior();
    void RenderAdvanced();
    void RenderTactics();

public:
    const char* GetGroup() const noexcept override { return "Environment"; }

    explicit AIDirectorSection(ModContext& ctx);
    void OnOpen() override;
    void Render() override;
};
