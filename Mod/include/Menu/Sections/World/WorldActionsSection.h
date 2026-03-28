#pragma once

#include "Menu/CollapsibleSection.h"
#include "Menu/SectionConfig.h"

class WorldActionsSection : public CollapsibleSection {
private:
    SectionConfig::WorldConfig& cfg = SectionConfig::world;

public:
    explicit WorldActionsSection(ModContext& ctx);
};
