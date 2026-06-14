#pragma once

#include "Menu/Section.h"

class AssetOverridesSection : public Section {
public:
    static constexpr SectionDefinition SECTION{MenuTab::Settings, "Asset Overrides"};

    explicit AssetOverridesSection(ModContext& ctx);

    void OnOpen() override;
    void Render() override;
};
