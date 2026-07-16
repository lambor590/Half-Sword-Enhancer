#pragma once

#include <mutex>
#include <vector>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "Menu/Override.h"
#include "Utils/PlayerPresetSerializer.h"
#include "Utils/PresetSectionState.h"

class PlayerEditorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::Player, "Player Editor", "Change player health, body, movement, combat, and skills."
    };

private:
    int enforceKey = -1;
    PlayerEditorOverrides overrides{};
    std::mutex publishedOverridesMutex;
    PlayerEditorOverrides publishedOverrides{};
    KeybindList keybinds;

    PresetSectionState<PlayerPresetSerializer> presets;
    int activeTab = 0;

    std::vector<OverrideDescriptor> physicalFields;
    std::vector<OverrideDescriptor> healthFields;
    std::vector<OverrideDescriptor> physicsFields;
    std::vector<OverrideDescriptor> movementFields;
    std::vector<OverrideDescriptor> combatFields;
    std::vector<OverrideDescriptor> skillFields;
    std::vector<OverrideDescriptor> stateFields;

    void InitKeybinds();
    void BuildDescriptors();
    int CountAllActive() const;
    void ApplyToPlayer(SDK::AWillie_BP_C* p);
    void PublishOverrides();
    void ReadFromPlayer();
    PlayerPresetData BuildPresetData() const;
    void ApplyPresetData(const PlayerPresetData& d);
    void ClonePlayer();
    void RenderPhysicalTab();
    void RenderHealthTab();
    void RenderPhysicsTab();
    void RenderMovementTab();
    void RenderCombatTab();
    void RenderSkillsStateTab();

public:
    explicit PlayerEditorSection(ModContext& ctx);
    void Render() override;
    KeybindList* GetSearchKeybinds() noexcept override { return &keybinds; }
};
