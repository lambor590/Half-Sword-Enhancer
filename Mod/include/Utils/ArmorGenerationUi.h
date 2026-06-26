#pragma once

#include "imgui/imgui.h"
#include "SDK/CoreUObject_classes.hpp"
#include "Utils/ArmorGenerationOptions.h"
#include "Utils/GuiUtils.h"
#include "Utils/PropertyBrowser.h"

namespace ArmorGenerationUi {
    inline SDK::UEnum* SteelTypeEnum() {
        static SDK::UEnum* enumPtr = nullptr;
        if (!enumPtr)
            enumPtr = SDK::UObject::FindObjectFast<SDK::UEnum>("Steel_Type", SDK::EClassCastFlags::Enum);
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
        GuiUtils::DebouncedDragFloat("Module Chance", &options.moduleChance, 0.01f, 0.0f, 1.0f, "%.2f");

        ImGui::Checkbox("Force Metal Material", &options.forceMetalMaterial);
        if (!options.forceMetalMaterial) ImGui::BeginDisabled();

        int steel = EquipmentGenerator::SteelTypeIndex(options.steelType);
        auto steelNames = PropertyBrowser::BuildEnumNames(SteelTypeEnum());
        if (!steelNames.empty() && GuiUtils::RenderEnumCombo("Steel Type", steel, steelNames))
            options.steelType = EquipmentGenerator::SteelTypeFromIndex(steel);

        int secondary = EquipmentGenerator::SecondaryMetalTypeIndex(options.metalPiecesType);
        auto secondaryNames = PropertyBrowser::BuildEnumNames(SecondaryMetalTypeEnum());
        if (!secondaryNames.empty() && GuiUtils::RenderEnumCombo("Secondary Metal", secondary, secondaryNames))
            options.metalPiecesType = EquipmentGenerator::SecondaryMetalTypeFromIndex(secondary);

        if (!options.forceMetalMaterial) ImGui::EndDisabled();
    }
}
