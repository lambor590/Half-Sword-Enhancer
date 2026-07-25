#pragma once

#include <array>
#include <mutex>
#include <span>

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
    bool overridesDirty = true;
    SDK::AWillie_BP_C* bodyOverridesPlayer = nullptr;
    KeybindList keybinds;

    PresetSectionState<PlayerPresetSerializer> presets;
    int activeTab = 0;

    std::array<OverrideDescriptor, 3> physicalFields;
    std::array<OverrideDescriptor, 12> healthFields;
    std::array<OverrideDescriptor, 10> physicsFields;
    std::array<OverrideDescriptor, 7> movementFields;
    std::array<OverrideDescriptor, 12> combatFields;
    std::array<OverrideDescriptor, 9> skillFields;
    std::array<OverrideDescriptor, 4> stateFields;

    void InitKeybinds();
    void BuildDescriptors();
    void RenderTrackedField(const OverrideDescriptor& field);
    void RenderTrackedGroup(std::span<const OverrideDescriptor> fields);
    void ApplyToPlayer(SDK::AWillie_BP_C* p);
    void PublishOverrides();
    void ReadFromPlayer();
    void ClonePlayer(SDK::AWillie_BP_C* player);
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
