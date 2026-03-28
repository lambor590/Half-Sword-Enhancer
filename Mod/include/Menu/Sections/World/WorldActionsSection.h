#pragma once

#include <vector>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "Menu/SectionConfig.h"

class WorldActionsSection : public Section {
private:
    SectionConfig::WorldConfig& cfg = SectionConfig::world;
    std::vector<KeybindEntry> keybinds;

    void InitKeybinds();

public:
    explicit WorldActionsSection(ModContext& ctx);
    void Render() override;
};
