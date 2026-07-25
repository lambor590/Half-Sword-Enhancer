#pragma once

#include <cstdint>
#include <span>

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
) {
    return {name, &ovr.enabled, &ovr.value, OverrideFieldType::Double, speed, tooltip};
}

constexpr OverrideDescriptor OverrideField(
    const char* name, IntOverride& ovr, float speed = 0.1f, const char* tooltip = nullptr
) {
    return {name, &ovr.enabled, &ovr.value, OverrideFieldType::Int, speed, tooltip};
}

constexpr OverrideDescriptor OverrideField(const char* name, BoolOverride& ovr, const char* tooltip = nullptr) {
    return {name, &ovr.enabled, &ovr.value, OverrideFieldType::Bool, 0.0f, tooltip};
}


int CountActive(std::span<const OverrideDescriptor> fields);

/// Dispatches to DragFloat, DragInt, or tristate Combo based on field type.
void RenderOverrideField(const OverrideDescriptor& field);

void RenderOverrideGroup(std::span<const OverrideDescriptor> fields);
