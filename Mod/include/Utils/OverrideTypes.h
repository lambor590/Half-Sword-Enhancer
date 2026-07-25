#pragma once

template <typename ValueT> struct ValueOverride {
    bool enabled = false;
    ValueT value{};
    bool operator==(const ValueOverride&) const = default;
};

using RuntimeOverride = ValueOverride<double>;
using BoolOverride = ValueOverride<bool>;
using IntOverride = ValueOverride<int>;

static_assert(sizeof(RuntimeOverride) <= 16);
