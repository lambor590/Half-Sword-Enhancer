#pragma once

#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Enum_WeaponType_Specific_structs.hpp"
#include "Utils/GuiUtils.h"
#include "Utils/PropertyBrowser.h"

namespace WeaponGenerationUi {
    inline constexpr const char* SPECIFIC_TYPE_CONFIG_KEY = "weapon_specific_type";
    inline constexpr int GREATSWORD_INDEX = static_cast<int>(SDK::Enum_WeaponType_Specific::Enum_WeaponType_MAX);
    inline constexpr auto TWO_HANDED_SWORDS = SDK::Enum_WeaponType_Specific::NewEnumerator1;

    inline SDK::UEnum* SpecificTypeEnum() {
        static SDK::UEnum* enumPtr = nullptr;
        if (!enumPtr)
            enumPtr = SDK::UObject::FindObjectFast<SDK::UEnum>(
                "Enum_WeaponType_Specific", SDK::EClassCastFlags::Enum
            );
        return enumPtr;
    }

    inline SDK::Enum_WeaponType_Specific SpecificTypeFromIndex(int value) {
        const int maxValue = static_cast<int>(SDK::Enum_WeaponType_Specific::Enum_WeaponType_MAX);
        if (value < 0 || value >= maxValue) value = 0;
        return static_cast<SDK::Enum_WeaponType_Specific>(value);
    }

    inline bool IsGreatswordIndex(int value) noexcept { return value == GREATSWORD_INDEX; }

    inline bool RenderSpecificTypeCombo(const char* label, int& value, bool includeGreatsword = false) {
        auto names = PropertyBrowser::BuildEnumNames(SpecificTypeEnum());
        if (includeGreatsword) names.emplace_back("Greatsword");
        return GuiUtils::RenderEnumCombo(label, value, names);
    }
}
