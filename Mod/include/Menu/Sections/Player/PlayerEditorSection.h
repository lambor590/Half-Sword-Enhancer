#pragma once

#include <vector>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "Utils/PlayerPresetSerializer.h"
#include "Utils/PresetSectionState.h"

class PlayerEditorSection : public Section {
private:
    int enforceKey = -1;
    PlayerEditorOverrides overrides{};
    std::vector<KeybindEntry> keybinds;

    PresetSectionState<PlayerPresetSerializer> presets;
    int activeTab = 0;

    void InitKeybinds();
    int CountActiveOverrides() const;
    static void ApplyActiveOverrides(SDK::AWillie_BP_C* p, PlayerEditorOverrides& ovr);
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
};
