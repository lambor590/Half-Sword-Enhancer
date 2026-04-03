#pragma once

struct RuntimeOverride {
    bool enabled = false;
    double value = 0.0;
    bool operator==(const RuntimeOverride&) const = default;
};
static_assert(sizeof(RuntimeOverride) <= 16);

struct BoolOverride {
    bool enabled = false;
    bool value = false;
    bool operator==(const BoolOverride&) const = default;
};
struct IntOverride {
    bool enabled = false;
    int value = 0;
    bool operator==(const IntOverride&) const = default;
};
