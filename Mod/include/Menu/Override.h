#pragma once

#include <span>
#include <cstdint>

#include "../../ext/SimpleIni.h"
#include "Utils/OverrideTypes.h"

enum class OverrideFieldType : uint8_t { Double, Int, Bool };

/// Type-erased descriptor for a single override field.
/// Points into the owning struct's storage -- no copies, no allocations.
/// Construct via the OverrideField() helpers below.
struct OverrideDescriptor {
    const char* name; ///< INI key / display name
    bool* enabled;
    void* value; ///< Pointer to the value (double*, int*, or bool*)
    OverrideFieldType type;
    float speed;         ///< Drag speed for Double/Int widgets (ignored for Bool)
    const char* tooltip;
};

constexpr OverrideDescriptor OverrideField(
    const char* name, RuntimeOverride& ovr, float speed = 0.1f, const char* tooltip = nullptr
);

constexpr OverrideDescriptor OverrideField(
    const char* name, IntOverride& ovr, float speed = 0.1f, const char* tooltip = nullptr
);

constexpr OverrideDescriptor OverrideField(const char* name, BoolOverride& ovr, const char* tooltip = nullptr);


int CountActive(std::span<const OverrideDescriptor> fields);

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

constexpr OverrideDescriptor OverrideField(const char* name, RuntimeOverride& ovr, float speed, const char* tooltip) {
    return {name, &ovr.enabled, &ovr.value, OverrideFieldType::Double, speed, tooltip};
}

constexpr OverrideDescriptor OverrideField(const char* name, IntOverride& ovr, float speed, const char* tooltip) {
    return {name, &ovr.enabled, &ovr.value, OverrideFieldType::Int, speed, tooltip};
}

constexpr OverrideDescriptor OverrideField(const char* name, BoolOverride& ovr, const char* tooltip) {
    return {name, &ovr.enabled, &ovr.value, OverrideFieldType::Bool, 0.0f, tooltip};
}
