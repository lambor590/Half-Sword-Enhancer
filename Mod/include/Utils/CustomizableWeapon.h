#pragma once

#include <cstdint>

enum class CustomizableWeapon : uint8_t {
    None = 0,
    SwordArming, SwordShort, SwordLong,
    MaceShort, Mace, MaceLong,
    HaftedShort, Hafted, HaftedLong,
    PolearmShort, Polearm, PolearmLong,
    PollaxeShort, Pollaxe, PollaxeLong,
    CastedShort, Casted, CastedLong,
    Messer
};
