#include "Menu/Sections/World/AIDirectorSection.h"

#include <iterator>

#include "Utils/GuiUtils.h"

namespace {
    constexpr const char* SCOPE_LABELS[] = {
        "All NPCs",      "Nearest NPC",  "Within Distance", "Matching Alliance", "Fighting Player", "Controllable NPCs",
        "Unfrozen NPCs", "Has Opponent", "No Opponent",     "Player Alliance",   "Other Alliances",
    };

    constexpr const char* PROFILE_LABELS[] = {
        "Aggressive", "Defensive", "Passive", "Panic",     "Drunk Brawl",    "Bodyguard",
        "Duelist",    "Horde",     "Coward",  "Berserker", "Training Dummy",
    };

    constexpr const char* COMBAT_BEHAVIOR_LABELS[] = {
        "Style 1", "Style 2", "Style 3", "Style 4", "Style 5", "Style 6",
    };

    constexpr const char* STRAFE_LABELS[] = {
        "Default",
        "Alternate",
    };

    constexpr const char* DirectiveLabel(AIDirectorSection::Directive directive) noexcept {
        switch (directive) {
            case AIDirectorSection::Directive::AttackPlayer: return "Attack Player";
            case AIDirectorSection::Directive::FightEachOther: return "Fight Each Other";
            case AIDirectorSection::Directive::ProtectPlayer: return "Protect Player";
            case AIDirectorSection::Directive::IgnorePlayer: return "Ignore Player";
            case AIDirectorSection::Directive::PanicFlee: return "Panic and Flee";
            case AIDirectorSection::Directive::FreezeAI: return "Freeze NPCs";
            case AIDirectorSection::Directive::DuelMode: return "Duel";
            case AIDirectorSection::Directive::None:
            default: return "Normal";
        }
    }

}

AIDirectorSection::AIDirectorSection(ModContext& ctx) : Section(ctx, SECTION) {}

void AIDirectorSection::OnOpen() {
    AIDirector::Get().RefreshStatus(SelectedTargets());
}

AIDirector::TargetFilter AIDirectorSection::SelectedTargets() const noexcept {
    return {.scope = scope, .radius = radius, .team = team};
}

void AIDirectorSection::SyncDirectorSnapshot() {
    auto& director = AIDirector::Get();
    auto next = director.GetSnapshot();
    activeDirective = director.ActiveDirective();
    if (next.resultSequence == 0 || next.resultSequence == lastDirectorResultSequence) return;

    summary = next.summary;
    lastDirectorResultSequence = next.resultSequence;
    if (next.lastError)
        status.SetError(next.lastError);
    else
        status.Clear();
}

void AIDirectorSection::RenderScope() {
    ImGui::SeparatorText("Affected NPCs");

    int scopeIdx = static_cast<int>(scope);
    if (GuiUtils::BeginSizedCombo("Apply To", SCOPE_LABELS[scopeIdx], 170.0f)) {
        for (int i = 0; i < static_cast<int>(std::size(SCOPE_LABELS)); ++i) {
            if (ImGui::Selectable(SCOPE_LABELS[i], i == scopeIdx)) scope = static_cast<Scope>(i);
            if (i == scopeIdx) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (scope == Scope::Radius) GuiUtils::DebouncedDragFloat("Distance", &radius, 10.0f, 50.0f, 5000.0f, "%.0f");
    if (scope == Scope::Team) GuiUtils::DebouncedDragInt("Alliance", &team, 0.2f, 0, 32);
    if (ImGui::Button("Update Overview")) AIDirector::Get().RefreshStatus(SelectedTargets());
    status.Render();
}

void AIDirectorSection::RenderStatus() {
    ImGui::SeparatorText("Overview");

    if (summary.targets < 0) {
        ImGui::TextUnformatted("Not checked yet");
        return;
    }

    constexpr ImGuiTableFlags STATUS_FLAGS = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings;
    if (ImGui::BeginTable("##AIStatus", 3, STATUS_FLAGS)) {
        ImGui::TableNextColumn();
        ImGui::Text("Affected: %d", summary.targets);
        ImGui::TableNextColumn();
        ImGui::Text("Controllable: %d", summary.aiControllers);
        ImGui::TableNextColumn();
        ImGui::Text("Unfrozen: %d", summary.tickEnabled);
        ImGui::TableNextColumn();
        ImGui::Text("In Combat: %d", summary.targetAssigned);
        ImGui::TableNextColumn();
        ImGui::Text("No Opponent: %d", summary.noTarget);
        ImGui::TableNextColumn();
        ImGui::Text("Fighting Player: %d", summary.targetingPlayer);
        ImGui::EndTable();
    }
    ImGui::Text("Alliances: %d - %d", summary.teamMin, summary.teamMax);
    ImGui::Text(
        "Aggression / Defense / Retreat / Footwork: %.2f / %.2f / %.2f / %.2f", summary.attackIntent,
        summary.defendIntent, summary.retreatIntent, summary.strafeIntent
    );
}

void AIDirectorSection::RenderAI() {
    ImGui::SeparatorText("NPC Actions");

    if (GuiUtils::Button("Resume NPCs")) AIDirector::Get().SetAITick(SelectedTargets(), true);
    (void)GuiUtils::SameLineIfFitsButton("Freeze NPCs");
    if (GuiUtils::Button("Freeze NPCs")) AIDirector::Get().SetAITick(SelectedTargets(), false);
    (void)GuiUtils::SameLineIfFitsButton("End Combat");
    if (GuiUtils::Button("End Combat")) AIDirector::Get().StopAI(SelectedTargets());

    if (GuiUtils::Button("Focus on Player"))
        AIDirector::Get().SetTarget(SelectedTargets(), AIDirector::TargetMode::Player);
    (void)GuiUtils::SameLineIfFitsButton("Focus on Nearest NPC");
    if (GuiUtils::Button("Focus on Nearest NPC"))
        AIDirector::Get().SetTarget(SelectedTargets(), AIDirector::TargetMode::NearestNpc);
    (void)GuiUtils::SameLineIfFitsButton("Forget Opponents");
    if (GuiUtils::Button("Forget Opponents"))
        AIDirector::Get().SetTarget(SelectedTargets(), AIDirector::TargetMode::Clear);

    if (GuiUtils::Button("Attack")) AIDirector::Get().TriggerImpulse(SelectedTargets(), AIDirector::Impulse::Attack);
    (void)GuiUtils::SameLineIfFitsButton("Dash");
    if (GuiUtils::Button("Dash")) AIDirector::Get().TriggerImpulse(SelectedTargets(), AIDirector::Impulse::Dash);
    (void)GuiUtils::SameLineIfFitsButton("Stop Weapon Swing");
    if (GuiUtils::Button("Stop Weapon Swing"))
        AIDirector::Get().TriggerImpulse(SelectedTargets(), AIDirector::Impulse::StopBlade);
}

void AIDirectorSection::RenderBehavior() {
    ImGui::SeparatorText("Combat Personality");

    int profileIdx = static_cast<int>(profile);
    if (GuiUtils::BeginSizedCombo("Personality", PROFILE_LABELS[profileIdx], 160.0f)) {
        for (int i = 0; i < static_cast<int>(std::size(PROFILE_LABELS)); ++i) {
            if (ImGui::Selectable(PROFILE_LABELS[i], i == profileIdx)) profile = static_cast<Profile>(i);
            if (i == profileIdx) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("Use Personality")) AIDirector::Get().ApplyProfile(SelectedTargets(), profile);

    ImGui::SeparatorText("Custom Behavior");
    ImGui::Checkbox("Fearless", &fearless);
    GuiUtils::DebouncedDragFloat("Drunkenness", &drunkLevel, 0.01f, 0.0f, 1.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Physical Skill", &bodySkill, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Weapon Skill", &weaponSkill, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Dodging", &dodgeRate, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Running Speed", &runningSpeed, 0.05f, 0.0f, 10.0f, "%.2f");
}

void AIDirectorSection::RenderAdvanced() {
    ImGui::SeparatorText("Fine Tuning");

    GuiUtils::DebouncedDragFloat("Aggression", &attackIntent, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Defense", &defendIntent, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Retreat Tendency", &retreatIntent, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Footwork", &strafeIntent, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Berserk Tendency", &berserkRate, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Parrying", &parryRate, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Swing Speed", &swingSpeed, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Attack Variety", &changeAttackRate, 0.05f, 0.0f, 10.0f, "%.2f");
    GuiUtils::DebouncedDragFloat("Preferred Distance", &approachDistance, 5.0f, 0.0f, 1000.0f, "%.0f");

    if (GuiUtils::BeginSizedCombo("Fighting Style", COMBAT_BEHAVIOR_LABELS[combatBehavior], 150.0f)) {
        for (int i = 0; i < static_cast<int>(std::size(COMBAT_BEHAVIOR_LABELS)); ++i) {
            if (ImGui::Selectable(COMBAT_BEHAVIOR_LABELS[i], i == combatBehavior)) combatBehavior = i;
            if (i == combatBehavior) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (GuiUtils::BeginSizedCombo("Footwork Style", STRAFE_LABELS[strafeMode], 150.0f)) {
        for (int i = 0; i < static_cast<int>(std::size(STRAFE_LABELS)); ++i) {
            if (ImGui::Selectable(STRAFE_LABELS[i], i == strafeMode)) strafeMode = i;
            if (i == strafeMode) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    auto invincibility = static_cast<float>(aiInvincibility);
    if (GuiUtils::DebouncedDragFloat("Damage Resistance", &invincibility, 0.01f, 0.0f, 10.0f, "%.2f"))
        aiInvincibility = invincibility;

    auto armorInvincibility = static_cast<float>(aiArmorInvincibility);
    if (GuiUtils::DebouncedDragFloat("Armor Resistance", &armorInvincibility, 0.01f, 0.0f, 10.0f, "%.2f"))
        aiArmorInvincibility = armorInvincibility;

    if (ImGui::Button("Use Custom Behavior")) {
        AIDirector::Get().ApplyBehavior(
            SelectedTargets(),
            {
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
            }
        );
    }

    ImGui::SeparatorText("Alliances");
    GuiUtils::DebouncedDragInt("Alliance", &newTeam, 0.2f, 0, 32);
    if (ImGui::Button("Change Alliance")) AIDirector::Get().ApplyTeam(SelectedTargets(), newTeam);
}

void AIDirectorSection::RenderTactics() {
    ImGui::SeparatorText("Tactics");

    const auto currentDirective = activeDirective;
    ImGui::Text("Current Behavior: %s", DirectiveLabel(currentDirective));

    struct TacticAction {
        const char* label;
        Directive directive;
    };
    static constexpr TacticAction TACTICS[] = {
        {"Attack Player", Directive::AttackPlayer},
        {"Fight Each Other", Directive::FightEachOther},
        {"Protect Player", Directive::ProtectPlayer},
        {"Ignore Player", Directive::IgnorePlayer},
        {"Panic and Flee", Directive::PanicFlee},
        {"Freeze NPCs", Directive::FreezeAI},
        {"Duel", Directive::DuelMode},
    };
    for (size_t i = 0; i < std::size(TACTICS); ++i) {
        const auto& tactic = TACTICS[i];
        if (i > 0) (void)GuiUtils::SameLineIfFitsButton(tactic.label);
        const auto tone =
            tactic.directive == currentDirective ? GuiUtils::ButtonTone::Primary : GuiUtils::ButtonTone::Default;
        if (GuiUtils::Button(tactic.label, tone))
            AIDirector::Get().ToggleDirective(SelectedTargets(), tactic.directive);
    }

    if (currentDirective != Directive::None) {
        if (GuiUtils::Button("Restore Normal Behavior", GuiUtils::ButtonTone::Quiet))
            AIDirector::Get().SetDirective(SelectedTargets(), Directive::None);
    }
}

void AIDirectorSection::Render() {
    SyncDirectorSnapshot();
    RenderScope();
    RenderStatus();
    RenderTactics();
    RenderAI();
    RenderBehavior();
    RenderAdvanced();
}
