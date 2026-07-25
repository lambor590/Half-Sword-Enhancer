#pragma once

#include "Menu/Section.h"

class AssetOverridesSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::Settings, "Custom Textures", "Replace supported game textures with your own images."
    };

    explicit AssetOverridesSection(ModContext& ctx);

    void OnOpen() override;
    void Render() override;
};
