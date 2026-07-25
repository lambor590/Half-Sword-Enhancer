#pragma once

#include "imgui/imgui.h"
#include "Utils/ArmorGenerationOptions.h"
#include "Utils/GuiUtils.h"
#include "Utils/PropertyBrowser.h"

namespace ArmorGenerationUi {
    inline void RenderOptions(EquipmentGenerator::ArmorGenerationOptions& options) {
        ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
        GuiUtils::DebouncedDragFloat("Armor Coverage", &options.moduleChance, 0.01f, 0.0f, 1.0f, "%.2f");

        ImGui::Checkbox("All Metal", &options.forceMetalMaterial);
        if (!options.forceMetalMaterial) ImGui::BeginDisabled();

        int steel = EquipmentGenerator::SteelTypeIndex(options.steelType);
        const auto& steelInfo = PropertyBrowser::GetEnumInfo("Steel_Type");
        if (!steelInfo.names.empty() &&
            GuiUtils::RenderEnumCombo("Main Metal", steel, steelInfo.names, steelInfo.maxTextWidthEm))
            options.steelType = EquipmentGenerator::SteelTypeFromIndex(steel);

        int secondary = EquipmentGenerator::SecondaryMetalTypeIndex(options.metalPiecesType);
        const auto& secondaryInfo = PropertyBrowser::GetEnumInfo("SecondaryMetal_Type");
        if (!secondaryInfo.names.empty() &&
            GuiUtils::RenderEnumCombo("Accent Metal", secondary, secondaryInfo.names, secondaryInfo.maxTextWidthEm))
            options.metalPiecesType = EquipmentGenerator::SecondaryMetalTypeFromIndex(secondary);

        if (!options.forceMetalMaterial) ImGui::EndDisabled();
    }
}
