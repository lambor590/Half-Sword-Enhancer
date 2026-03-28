#pragma once

#include <vector>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "Menu/SectionConfig.h"

class PlayerAbilitiesSection : public Section {
private:
    SectionConfig::PlayerConfig& cfg = SectionConfig::player;
    std::vector<KeybindEntry> keybinds;

    void InitKeybinds();

public:
    explicit PlayerAbilitiesSection(ModContext& ctx);
    void Render() override;
};
