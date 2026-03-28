#pragma once

#include <array>
#include <cstdint>

namespace TierValidation {

    inline std::array<uint16_t, 20> VALID_TIER_MASKS = {{}};

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

}
