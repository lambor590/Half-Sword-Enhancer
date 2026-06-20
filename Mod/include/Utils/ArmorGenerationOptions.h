#pragma once

#include "SDK/SecondaryMetal_Type_structs.hpp"
#include "SDK/Steel_Type_structs.hpp"

namespace EquipmentGenerator {
    inline constexpr int ARMOR_STEEL_TYPE_COUNT = 12;
    inline constexpr int ARMOR_SECONDARY_METAL_TYPE_COUNT = 8;

    struct ArmorGenerationOptions {
        float moduleChance = 0.5f;
        bool forceMetalMaterial = false;
        SDK::ESteel_Type steelType = SDK::ESteel_Type::NewEnumerator0;
        SDK::ESecondaryMetal_Type metalPiecesType = SDK::ESecondaryMetal_Type::NewEnumerator0;
    };

    inline int SteelTypeIndex(SDK::ESteel_Type type) noexcept {
        int index = static_cast<int>(type);
        return index >= 0 && index < ARMOR_STEEL_TYPE_COUNT ? index : 0;
    }

    inline int SecondaryMetalTypeIndex(SDK::ESecondaryMetal_Type type) noexcept {
        int index = static_cast<int>(type);
        return index >= 0 && index < ARMOR_SECONDARY_METAL_TYPE_COUNT ? index : 0;
    }

    inline SDK::ESteel_Type SteelTypeFromIndex(int index) noexcept {
        return static_cast<SDK::ESteel_Type>(
            index >= 0 && index < ARMOR_STEEL_TYPE_COUNT ? index : 0
        );
    }

    inline SDK::ESecondaryMetal_Type SecondaryMetalTypeFromIndex(int index) noexcept {
        return static_cast<SDK::ESecondaryMetal_Type>(
            index >= 0 && index < ARMOR_SECONDARY_METAL_TYPE_COUNT ? index : 0
        );
    }
}
