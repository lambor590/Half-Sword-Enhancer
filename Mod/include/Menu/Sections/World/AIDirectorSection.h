#pragma once

#include <cstdint>

#include "Menu/Section.h"
#include "Utils/AIDirector.h"
#include "Utils/GuiUtils.h"

class AIDirectorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{MenuTab::World, "AI Director", "Environment"};

    using Scope = AIDirector::Scope;
    using Profile = AIDirector::Profile;
    using Directive = AIDirector::Directive;

private:
    Scope scope = Scope::AllNPCs;
    Profile profile = Profile::Aggressive;
    Directive activeDirective = Directive::None;
    AIDirector::StatusSummary summary;
    uint64_t lastDirectorResultSequence = 0;

    int team = 0;
    int newTeam = 0;
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
    bool fearless = false;

    GuiUtils::StatusMessage status;

    AIDirector::TargetFilter SelectedTargets() const noexcept;
    AIDirector::BehaviorSettings SelectedBehavior() const noexcept;
    void SyncDirectorSnapshot();

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

    void RenderScope();
    void RenderStatus();
    void RenderAI();
    void RenderBehavior();
    void RenderAdvanced();
    void RenderTactics();

public:
    explicit AIDirectorSection(ModContext& ctx);
    void OnOpen() override;
    void Render() override;
};
