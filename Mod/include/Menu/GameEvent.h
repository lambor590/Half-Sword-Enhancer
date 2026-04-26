#pragma once

#include <cstdint>

enum class GameEvent : uint8_t {
    BeginFight,
    InAbyss,
    OffLedge,
    OnTick,
};
