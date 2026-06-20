#pragma once

#include <string>
#include <vector>

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

    inline bool RenderEnumCombo(const char* label, int& value, const std::vector<std::string>& names) {
        if (names.empty()) return false;

        if (value < 0 || value >= static_cast<int>(names.size())) value = 0;

        float maxW = ImGui::CalcTextSize("Unknown").x;
        for (const auto& name : names) {
            const float width = ImGui::CalcTextSize(name.c_str()).x;
            if (width > maxW) maxW = width;
        }

        bool changed = false;
        if (GuiUtils::BeginSizedCombo(label, names[value].c_str(), GuiUtils::ComboWidthFromText(maxW))) {
            for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                if (ImGui::Selectable(names[i].c_str(), value == i)) {
                    value = i;
                    changed = true;
                }
                if (value == i) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    inline void RenderOptions(EquipmentGenerator::ArmorGenerationOptions& options) {
        ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
        GuiUtils::DebouncedDragFloat("Module Chance", &options.moduleChance, 0.01f, 0.0f, 1.0f, "%.2f");

        ImGui::Checkbox("Force Metal Material", &options.forceMetalMaterial);
        if (!options.forceMetalMaterial) ImGui::BeginDisabled();

        int steel = EquipmentGenerator::SteelTypeIndex(options.steelType);
        auto steelNames = PropertyBrowser::BuildEnumNames(SteelTypeEnum());
        if (!steelNames.empty() && RenderEnumCombo("Steel Type", steel, steelNames))
            options.steelType = EquipmentGenerator::SteelTypeFromIndex(steel);

        int secondary = EquipmentGenerator::SecondaryMetalTypeIndex(options.metalPiecesType);
        auto secondaryNames = PropertyBrowser::BuildEnumNames(SecondaryMetalTypeEnum());
        if (!secondaryNames.empty() && RenderEnumCombo("Secondary Metal", secondary, secondaryNames))
            options.metalPiecesType = EquipmentGenerator::SecondaryMetalTypeFromIndex(secondary);

        if (!options.forceMetalMaterial) ImGui::EndDisabled();
    }
}
