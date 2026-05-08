#pragma once

#include "Menu/Section.h"

class AssetOverridesSection : public Section {
public:
    explicit AssetOverridesSection(ModContext& ctx);

    void OnOpen() override;
    void Render() override;
};
