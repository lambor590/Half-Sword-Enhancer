#pragma once

#include "SDK/Enum_WeaponType_Specific_structs.hpp"
#include "Utils/GuiUtils.h"
#include "Utils/PropertyBrowser.h"

namespace WeaponGenerationUi {
    inline constexpr const char* SPECIFIC_TYPE_CONFIG_KEY = "weapon_specific_type";
    inline constexpr int GREATSWORD_INDEX = static_cast<int>(SDK::Enum_WeaponType_Specific::Enum_WeaponType_MAX);
    inline constexpr auto TWO_HANDED_SWORDS = SDK::Enum_WeaponType_Specific::NewEnumerator1;

    inline SDK::Enum_WeaponType_Specific SpecificTypeFromIndex(int value) {
        const int maxValue = static_cast<int>(SDK::Enum_WeaponType_Specific::Enum_WeaponType_MAX);
        if (value < 0 || value >= maxValue) value = 0;
        return static_cast<SDK::Enum_WeaponType_Specific>(value);
    }

    inline bool IsGreatswordIndex(int value) noexcept {
        return value == GREATSWORD_INDEX;
    }

    inline bool RenderSpecificTypeCombo(const char* label, int& value, bool includeGreatsword = false) {
        const auto& enumInfo = PropertyBrowser::GetEnumInfo("Enum_WeaponType_Specific");
        if (!includeGreatsword) return GuiUtils::RenderEnumCombo(label, value, enumInfo.names, enumInfo.maxTextWidthEm);

        static const PropertyBrowser::EnumInfo* cachedInfo = nullptr;
        static std::uint64_t cachedRevision = 0;
        static std::vector<std::string> namesWithGreatsword;
        static float maxTextWidthEm = 0.0f;
        if (cachedInfo != &enumInfo || cachedRevision != enumInfo.revision || namesWithGreatsword.empty()) {
            namesWithGreatsword = enumInfo.names;
            namesWithGreatsword.emplace_back("Greatsword");
            maxTextWidthEm = (std::max)(enumInfo.maxTextWidthEm,
                                        ImGui::CalcTextSize("Greatsword").x / (std::max)(1.0f, ImGui::GetFontSize()));
            cachedInfo = &enumInfo;
            cachedRevision = enumInfo.revision;
        }
        return GuiUtils::RenderEnumCombo(label, value, namesWithGreatsword, maxTextWidthEm);
    }
}
