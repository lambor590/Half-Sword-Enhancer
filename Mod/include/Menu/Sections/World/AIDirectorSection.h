#pragma once

#include <cstdint>

#include "Menu/Section.h"
#include "Utils/AIDirector.h"
#include "Utils/GuiUtils.h"

class AIDirectorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::World, "NPC Control", "Choose who NPCs fight and how they behave in combat."
    };

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
    AIDirector::BehaviorSettings behavior;
    float radius = 1000.0f;

    GuiUtils::StatusMessage status;

    AIDirector::TargetFilter SelectedTargets() const noexcept;
    void SyncDirectorSnapshot();

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
