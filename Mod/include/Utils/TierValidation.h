#pragma once

#include <array>
#include <cstdint>

#include "Utils/GameConstants.h"

namespace TierValidation {

    inline std::array<uint16_t, GameConstants::WEAPON_TYPE_COUNT + 1> VALID_TIER_MASKS = {{}};

    static constexpr std::array<uint16_t, 15> VALID_ARMOR_TIER_MASKS = {{
        0x0FE,
        0x0F8,
        0x0F0,
        0x0F0,
        0x0E0,
        0x0F0,
        0x0C0,
        0x0F8,
        0x0F0,
        0x0E0,
        0x0FF,
        0x0C0,
        0x0F8,
        0x0FE,
        0x0FF,
    }};

    inline int NearestValidTier(uint16_t mask, int tier) noexcept {
        if (mask & (1 << tier)) return tier;
        for (int d = 1; d <= 8; ++d) {
            if (tier + d <= 8 && (mask & (1 << (tier + d)))) return tier + d;
            if (tier - d >= 0 && (mask & (1 << (tier - d)))) return tier - d;
        }
        return 4;
    }

    inline int RandomValidTier(uint16_t mask) noexcept {
        int validTiers[9];
        int count = 0;
        for (int t = 0; t <= 8; ++t)
            if (mask & (1 << t)) validTiers[count++] = t;
        return count > 0 ? validTiers[GameConstants::RandomInt(0, count - 1)] : 4;
    }

}
