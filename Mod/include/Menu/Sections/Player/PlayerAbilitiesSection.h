#pragma once

#include "Menu/CollapsibleSection.h"
#include "Menu/SectionConfig.h"

class PlayerAbilitiesSection : public CollapsibleSection {
private:
    SectionConfig::PlayerConfig& cfg = SectionConfig::player;

public:
    explicit PlayerAbilitiesSection(ModContext& ctx);
};
