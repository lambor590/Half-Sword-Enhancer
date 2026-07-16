#pragma once

#include "imgui/imgui.h"
#include "SDK/CoreUObject_classes.hpp"
#include "Utils/ArmorGenerationOptions.h"
#include "Utils/GuiUtils.h"
#include "Utils/PropertyBrowser.h"

namespace ArmorGenerationUi {
    inline SDK::UEnum* SteelTypeEnum() {
        static SDK::UEnum* enumPtr = nullptr;
        if (!enumPtr) enumPtr = SDK::UObject::FindObjectFast<SDK::UEnum>("Steel_Type", SDK::EClassCastFlags::Enum);
        return enumPtr;
    }

    inline SDK::UEnum* SecondaryMetalTypeEnum() {
        static SDK::UEnum* enumPtr = nullptr;
        if (!enumPtr)
            enumPtr = SDK::UObject::FindObjectFast<SDK::UEnum>("SecondaryMetal_Type", SDK::EClassCastFlags::Enum);
        return enumPtr;
    }

    inline void RenderOptions(EquipmentGenerator::ArmorGenerationOptions& options) {
        ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
        GuiUtils::DebouncedDragFloat("Armor Coverage", &options.moduleChance, 0.01f, 0.0f, 1.0f, "%.2f");

        ImGui::Checkbox("All Metal", &options.forceMetalMaterial);
        if (!options.forceMetalMaterial) ImGui::BeginDisabled();

        int steel = EquipmentGenerator::SteelTypeIndex(options.steelType);
        const auto& steelInfo = PropertyBrowser::GetEnumInfo(SteelTypeEnum());
        if (!steelInfo.names.empty() &&
            GuiUtils::RenderEnumCombo("Main Metal", steel, steelInfo.names, steelInfo.maxTextWidthEm))
            options.steelType = EquipmentGenerator::SteelTypeFromIndex(steel);

        int secondary = EquipmentGenerator::SecondaryMetalTypeIndex(options.metalPiecesType);
        const auto& secondaryInfo = PropertyBrowser::GetEnumInfo(SecondaryMetalTypeEnum());
        if (!secondaryInfo.names.empty() &&
            GuiUtils::RenderEnumCombo("Accent Metal", secondary, secondaryInfo.names, secondaryInfo.maxTextWidthEm))
            options.metalPiecesType = EquipmentGenerator::SecondaryMetalTypeFromIndex(secondary);

        if (!options.forceMetalMaterial) ImGui::EndDisabled();
    }
}
