#pragma once

#include <span>
#include <string_view>
#include <cstdint>

#include "../../ext/SimpleIni.h"

enum class OverrideFieldType : uint8_t { Double, Int, Bool };

/// Type-erased descriptor for a single override field.
/// Points into the owning struct's storage -- no copies, no allocations.
/// Construct via the OverrideField() helpers below.
struct OverrideDescriptor {
    const char* name; ///< INI key / display name
    bool* enabled;
    void* value; ///< Pointer to the value (double*, int*, or bool*)
    OverrideFieldType type;
    double defaultValue; ///< Stored as double for all types
    double minValue;     ///< Optional minimum (0 if unused)
    double maxValue;     ///< Optional maximum (0 if unused)
    float speed;         ///< Drag speed for Double/Int widgets (ignored for Bool)
    const char* tooltip;
};


struct RuntimeOverride;
struct IntOverride;
struct BoolOverride;

constexpr OverrideDescriptor OverrideField(
    const char* name, RuntimeOverride& ovr, double defaultVal = 0.0, double minVal = 0.0, double maxVal = 0.0,
    float speed = 0.1f, const char* tooltip = nullptr
);

constexpr OverrideDescriptor OverrideField(
    const char* name, IntOverride& ovr, int defaultVal = 0, int minVal = 0, int maxVal = 0, float speed = 0.1f,
    const char* tooltip = nullptr
);

constexpr OverrideDescriptor OverrideField(
    const char* name, BoolOverride& ovr, bool defaultVal = false, const char* tooltip = nullptr
);


int CountActive(std::span<const OverrideDescriptor> fields);

template <typename Fn> void ApplyAll(std::span<const OverrideDescriptor> fields, Fn&& applier) {
    for (const auto& f : fields) {
        if (*f.enabled) applier(f);
    }
}


/// Only valid when type == Double.
inline double GetDouble(const OverrideDescriptor& f) {
    return *static_cast<double*>(f.value);
}

/// Only valid when type == Int.
inline int GetInt(const OverrideDescriptor& f) {
    return *static_cast<int*>(f.value);
}

/// Only valid when type == Bool.
inline bool GetBool(const OverrideDescriptor& f) {
    return *static_cast<bool*>(f.value);
}


/// Each setter receives the target actor (as void*) and the descriptor to read the value from.
using OverrideSetter = void (*)(void*, const OverrideDescriptor&);

/// Each fields[i] maps to setters[i]. The setter table must have at least fields.size() entries.
inline void ApplyWithSetters(std::span<const OverrideDescriptor> fields, void* target, const OverrideSetter* setters) {
    for (size_t i = 0; i < fields.size(); ++i)
        if (*fields[i].enabled) setters[i](target, fields[i]);
}


/// When minimalMode is true, only enabled fields are written.
void SerializeAll(
    std::span<const OverrideDescriptor> fields, CSimpleIniA& ini, const char* section, bool minimalMode = false
);

void DeserializeAll(std::span<const OverrideDescriptor> fields, const CSimpleIniA& ini, const char* section);


/// Dispatches to DragFloat, DragInt, or tristate Combo based on field type.
void RenderOverrideField(const OverrideDescriptor& field);

void RenderOverrideGroup(std::span<const OverrideDescriptor> fields);


#include "Utils/OverrideTypes.h"

constexpr OverrideDescriptor OverrideField(
    const char* name, RuntimeOverride& ovr, double defaultVal, double minVal, double maxVal, float speed,
    const char* tooltip
) {
    return {name, &ovr.enabled, &ovr.value, OverrideFieldType::Double, defaultVal, minVal, maxVal, speed, tooltip};
}

constexpr OverrideDescriptor OverrideField(
    const char* name, IntOverride& ovr, int defaultVal, int minVal, int maxVal, float speed, const char* tooltip
) {
    return {
        name,
        &ovr.enabled,
        &ovr.value,
        OverrideFieldType::Int,
        static_cast<double>(defaultVal),
        static_cast<double>(minVal),
        static_cast<double>(maxVal),
        speed,
        tooltip};
}

constexpr OverrideDescriptor OverrideField(const char* name, BoolOverride& ovr, bool defaultVal, const char* tooltip) {
    return {name, &ovr.enabled, &ovr.value, OverrideFieldType::Bool, defaultVal ? 1.0 : 0.0, 0.0, 0.0, 0.0f, tooltip};
}
