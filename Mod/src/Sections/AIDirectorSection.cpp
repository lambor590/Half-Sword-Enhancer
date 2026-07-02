#include "Menu/Sections/World/AIDirectorSection.h"

#include <iterator>

#include "Menu/SectionStyle.h"
#include "Utils/GuiUtils.h"

namespace {
    constexpr const char* SCOPE_LABELS[] = {
        "All NPCs",  "Nearest NPC", "Radius",    "Team",        "Targeting Player", "Has AI",
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

}

AIDirectorSection::AIDirectorSection(ModContext& ctx) : Section(ctx, SECTION) {}

void AIDirectorSection::OnOpen() {
    RefreshStatus();
}

AIDirector::TargetFilter AIDirectorSection::SelectedTargets() const noexcept {
    return {.scope = scope, .radius = radius, .team = team};
}

AIDirector::BehaviorSettings AIDirectorSection::SelectedBehavior() const noexcept {
    return {
        .bodySkill = bodySkill,
        .weaponSkill = weaponSkill,
        .dodgeRate = dodgeRate,
        .runningSpeed = runningSpeed,
        .drunkLevel = drunkLevel,
        .attackIntent = attackIntent,
        .defendIntent = defendIntent,
        .retreatIntent = retreatIntent,
        .strafeIntent = strafeIntent,
        .berserkRate = berserkRate,
        .parryRate = parryRate,
        .swingSpeed = swingSpeed,
        .changeAttackRate = changeAttackRate,
        .approachDistance = approachDistance,
        .aiInvincibility = aiInvincibility,
        .aiArmorInvincibility = aiArmorInvincibility,
        .combatBehavior = combatBehavior,
        .strafeMode = strafeMode,
        .fearless = fearless,
    };
}

void AIDirectorSection::SyncDirectorSnapshot() {
    auto next = AIDirector::Get().GetSnapshot();
    summary = next.summary;
    activeDirective = next.activeDirective;
    if (next.lastResult.sequence == 0 || next.lastResult.sequence == lastDirectorResultSequence) return;

    lastDirectorResultSequence = next.lastResult.sequence;
    status.Set(next.lastResult.message, next.lastResult.isError);
}

void AIDirectorSection::RefreshStatus() {
    AIDirector::Get().RefreshStatus(SelectedTargets());
}

void AIDirectorSection::SetAITick(bool enabled) {
    AIDirector::Get().SetAITick(SelectedTargets(), enabled);
}

void AIDirectorSection::StopAI() {
    AIDirector::Get().StopAI(SelectedTargets());
}

void AIDirectorSection::AttackPlayer() {
    ToggleDirective(Directive::AttackPlayer);
}

void AIDirectorSection::ApplyBehavior() {
    AIDirector::Get().ApplyBehavior(SelectedTargets(), SelectedBehavior());
}

void AIDirectorSection::ApplyTeam() {
    AIDirector::Get().ApplyTeam(SelectedTargets(), newTeam);
}

void AIDirectorSection::ApplyProfile() {
    AIDirector::Get().ApplyProfile(SelectedTargets(), profile);
}

void AIDirectorSection::FightEachOther() {
    ToggleDirective(Directive::FightEachOther);
}

void AIDirectorSection::ProtectPlayer() {
    ToggleDirective(Directive::ProtectPlayer);
}

void AIDirectorSection::ToggleDirective(Directive directive) {
    AIDirector::Get().ToggleDirective(SelectedTargets(), directive);
}

void AIDirectorSection::ForceTargetPlayer() {
    AIDirector::Get().SetTarget(SelectedTargets(), AIDirector::TargetMode::Player);
}

void AIDirectorSection::ForceTargetNearest() {
    AIDirector::Get().SetTarget(SelectedTargets(), AIDirector::TargetMode::NearestNpc);
}

void AIDirectorSection::ClearTargets() {
    AIDirector::Get().SetTarget(SelectedTargets(), AIDirector::TargetMode::Clear);
}

void AIDirectorSection::ForceAttack() {
    AIDirector::Get().TriggerImpulse(SelectedTargets(), AIDirector::Impulse::Attack);
}

void AIDirectorSection::ForceDash() {
    AIDirector::Get().TriggerImpulse(SelectedTargets(), AIDirector::Impulse::Dash);
}

void AIDirectorSection::StopBlades() {
    AIDirector::Get().TriggerImpulse(SelectedTargets(), AIDirector::Impulse::StopBlade);
}

void AIDirectorSection::SetDirective(Directive directive) {
    AIDirector::Get().SetDirective(SelectedTargets(), directive);
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

    auto invincibility = static_cast<float>(aiInvincibility);
    if (GuiUtils::DebouncedDragFloat("AI Invincibility", &invincibility, 0.01f, 0.0f, 10.0f, "%.2f"))
        aiInvincibility = invincibility;

    auto armorInvincibility = static_cast<float>(aiArmorInvincibility);
    if (GuiUtils::DebouncedDragFloat("AI Armor Invincibility", &armorInvincibility, 0.01f, 0.0f, 10.0f, "%.2f"))
        aiArmorInvincibility = armorInvincibility;

    if (ImGui::Button("Apply Behavior")) ApplyBehavior();

    ImGui::SeparatorText("Teams");
    GuiUtils::DebouncedDragInt("New Team", &newTeam, 0.2f, 0, 32);
    if (ImGui::Button("Apply Team")) ApplyTeam();
}

void AIDirectorSection::RenderTactics() {
    ImGui::SeparatorText("Tactics");

    const auto currentDirective = activeDirective;
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
    SyncDirectorSnapshot();
    RenderScope();
    RenderStatus();
    RenderTactics();
    RenderAI();
    RenderBehavior();
    RenderAdvanced();
}
