#pragma once

#include <span>
#include <string_view>
#include <cstdint>

class CSimpleIniA;

/// Type tag for override field discriminated union.
enum class OverrideFieldType : uint8_t { Double, Int, Bool };

/// Type-erased descriptor for a single override field.
/// Points into the owning struct's storage -- no copies, no allocations.
/// Construct via the OverrideField() helpers below.
struct OverrideDescriptor {
    const char* name;       ///< INI key / display name
    bool* enabled;          ///< Pointer to the enabled flag
    void* value;            ///< Pointer to the value (double*, int*, or bool*)
    OverrideFieldType type; ///< Which type the value pointer refers to
    double defaultValue;    ///< Default value (stored as double for all types)
    double minValue;        ///< Optional minimum (0 if unused)
    double maxValue;        ///< Optional maximum (0 if unused)
    float speed;            ///< Drag speed for Double/Int widgets (ignored for Bool)
};

// ── Construction helpers ──────────────────────────────────────────────

struct RuntimeOverride;
struct IntOverride;
struct BoolOverride;

/// Create a descriptor for a RuntimeOverride (double value).
constexpr OverrideDescriptor OverrideField(
    const char* name, RuntimeOverride& ovr, double defaultVal = 0.0, double minVal = 0.0, double maxVal = 0.0,
    float speed = 0.1f
);

/// Create a descriptor for an IntOverride.
constexpr OverrideDescriptor OverrideField(
    const char* name, IntOverride& ovr, int defaultVal = 0, int minVal = 0, int maxVal = 0, float speed = 0.1f
);

/// Create a descriptor for a BoolOverride.
constexpr OverrideDescriptor OverrideField(const char* name, BoolOverride& ovr, bool defaultVal = false);

// ── Iteration utilities ───────────────────────────────────────────────

/// Count how many fields in the span have enabled == true.
int CountActive(std::span<const OverrideDescriptor> fields);

/// Apply every enabled override through a caller-provided functor.
/// The applier signature: void(const OverrideDescriptor& field).
/// Typical usage: iterate and assign to a game object.
template <typename Fn> void ApplyAll(std::span<const OverrideDescriptor> fields, Fn&& applier) {
    for (const auto& f : fields) {
        if (*f.enabled) applier(f);
    }
}

// ── Value accessors (for use inside applier lambdas) ──────────────────

/// Read the value as double. Only valid when type == Double.
inline double GetDouble(const OverrideDescriptor& f) {
    return *static_cast<double*>(f.value);
}

/// Read the value as int. Only valid when type == Int.
inline int GetInt(const OverrideDescriptor& f) {
    return *static_cast<int*>(f.value);
}

/// Read the value as bool. Only valid when type == Bool.
inline bool GetBool(const OverrideDescriptor& f) {
    return *static_cast<bool*>(f.value);
}

// ── INI persistence ───────────────────────────────────────────────────

/// Serialize all override fields into an INI object under the given section.
/// When minimalMode is true, only enabled fields are written.
void SerializeAll(
    std::span<const OverrideDescriptor> fields, CSimpleIniA& ini, const char* section, bool minimalMode = false
);

/// Deserialize all override fields from an INI object under the given section.
void DeserializeAll(std::span<const OverrideDescriptor> fields, const CSimpleIniA& ini, const char* section);

// ── ImGui rendering ───────────────────────────────────────────────────

/// Render a single override field with its enabled toggle.
/// Dispatches to DragFloat, DragInt, or tristate Combo based on field type.
void RenderOverrideField(const OverrideDescriptor& field);

/// Render all override fields in a group.
void RenderOverrideGroup(std::span<const OverrideDescriptor> fields);

// ── Inline constexpr construction (needs OverrideTypes.h included by caller) ──

#include "Utils/OverrideTypes.h"

constexpr OverrideDescriptor OverrideField(
    const char* name, RuntimeOverride& ovr, double defaultVal, double minVal, double maxVal, float speed
) {
    return {name, &ovr.enabled, &ovr.value, OverrideFieldType::Double, defaultVal, minVal, maxVal, speed};
}

constexpr OverrideDescriptor OverrideField(
    const char* name, IntOverride& ovr, int defaultVal, int minVal, int maxVal, float speed
) {
    return {
        name,
        &ovr.enabled,
        &ovr.value,
        OverrideFieldType::Int,
        static_cast<double>(defaultVal),
        static_cast<double>(minVal),
        static_cast<double>(maxVal),
        speed};
}

constexpr OverrideDescriptor OverrideField(const char* name, BoolOverride& ovr, bool defaultVal) {
    return {name, &ovr.enabled, &ovr.value, OverrideFieldType::Bool, defaultVal ? 1.0 : 0.0, 0.0, 0.0, 0.0f};
}
