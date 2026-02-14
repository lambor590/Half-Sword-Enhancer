#pragma once

#include <array>
#include <cstdint>

namespace TierValidation {

    static constexpr std::array<uint16_t, 20> VALID_TIER_MASKS = {{
        0x000,  // None
        0x1EC,  // SwordArming: 2,3,5,6,7,8
        0x1FC,  // SwordShort: 2,3,4,5,6,7,8
        0x1CC,  // SwordLong: 2,3,6,7,8
        0x1F4,  // MaceShort: 2,4,5,6,7,8
        0x1E8,  // Mace: 3,5,6,7,8
        0x1F8,  // MaceLong: 3,4,5,6,7,8
        0x1FC,  // HaftedShort: 2,3,4,5,6,7,8
        0x1CC,  // Hafted: 2,3,6,7,8
        0x1EC,  // HaftedLong: 2,3,5,6,7,8
        0x1FC,  // PolearmShort: 2,3,4,5,6,7,8
        0x1F0,  // Polearm: 4,5,6,7,8
        0x1F0,  // PolearmLong: 4,5,6,7,8
        0x1FC,  // PollaxeShort: 2,3,4,5,6,7,8
        0x1EC,  // Pollaxe: 2,3,5,6,7,8
        0x1D4,  // PollaxeLong: 2,4,6,7,8
        0x1FC,  // CastedShort: 2,3,4,5,6,7,8
        0x1F4,  // Casted: 2,4,5,6,7,8
        0x1F8,  // CastedLong: 3,4,5,6,7,8
        0x1EC,  // Messer: 2,3,5,6,7,8
    }};

    static constexpr std::array<uint16_t, 15> VALID_ARMOR_TIER_MASKS = {{
        0x0FE, 0x0F8, 0x0F0, 0x0F0, 0x0E0, 0x0F0, 0x0C0,
        0x0F8, 0x0F0, 0x0E0, 0x0FF, 0x0C0, 0x0F8, 0x0FE, 0x0FF,
    }};

    inline int NearestValidTier(uint16_t mask, int tier) noexcept {
        if (mask & (1 << tier)) return tier;
        for (int d = 1; d <= 8; ++d) {
            if (tier + d <= 8 && (mask & (1 << (tier + d)))) return tier + d;
            if (tier - d >= 0 && (mask & (1 << (tier - d)))) return tier - d;
        }
        return 4;
    }

}
