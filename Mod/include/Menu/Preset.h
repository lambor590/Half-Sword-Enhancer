#pragma once

#include <concepts>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include "SimpleIni.h"
#include "Menu/Override.h"
#include "Utils/PresetDataBase.h"
#include "Utils/PresetSerializerBase.h"

// ── Field descriptor types ────────────────────────────────────────────

/// Type tag for preset field discriminated union.
enum class PresetFieldType : uint8_t { String, Int, Double, Bool, Vec3, Rotator, Color };

/// Type-erased descriptor for a single preset data field.
/// Points into the owning struct's storage -- no copies, no allocations.
struct PresetFieldDescriptor {
    const char* section;    ///< INI section name
    const char* key;        ///< INI key name
    PresetFieldType type;   ///< Which type the value pointer refers to
    void* value;            ///< Pointer to the field in the data struct
    const char* defaultStr; ///< Default string value for deserialization fallback
};

/// Construct helpers for building field descriptors.
namespace PresetField {

    constexpr PresetFieldDescriptor String(
        const char* section, const char* key, std::string* value, const char* def = ""
    ) {
        return {section, key, PresetFieldType::String, value, def};
    }

    constexpr PresetFieldDescriptor Int(const char* section, const char* key, int* value, const char* def = "0") {
        return {section, key, PresetFieldType::Int, value, def};
    }

    constexpr PresetFieldDescriptor Double(
        const char* section, const char* key, double* value, const char* def = "0.0"
    ) {
        return {section, key, PresetFieldType::Double, value, def};
    }

    constexpr PresetFieldDescriptor Bool(const char* section, const char* key, bool* value, const char* def = "0") {
        return {section, key, PresetFieldType::Bool, value, def};
    }

    constexpr PresetFieldDescriptor Vec3(const char* section, const char* key, void* value, const char* def = nullptr) {
        return {section, key, PresetFieldType::Vec3, value, def};
    }

    constexpr PresetFieldDescriptor Rotator(
        const char* section, const char* key, void* value, const char* def = nullptr
    ) {
        return {section, key, PresetFieldType::Rotator, value, def};
    }

    constexpr PresetFieldDescriptor Color(
        const char* section, const char* key, void* value, const char* def = nullptr
    ) {
        return {section, key, PresetFieldType::Color, value, def};
    }

} // namespace PresetField

// ── Override field group descriptor ───────────────────────────────────

/// Pairs a span of override descriptors with the INI section they serialize under.
struct OverrideGroupDescriptor {
    const char* section;
    std::span<const OverrideDescriptor> fields;
};

// ── Unified serialize/deserialize (free functions) ────────────────────

void SerializePresetFields(std::span<const PresetFieldDescriptor> fields, CSimpleIniA& ini);
void DeserializePresetFields(std::span<const PresetFieldDescriptor> fields, const CSimpleIniA& ini);

// ── Concept detection for optional DataType hooks ─────────────────────

/// DataType must provide GetPresetFields().
template <typename T>
concept HasPresetFields = requires(T& t) {
    { T::GetPresetFields(t) } -> std::convertible_to<std::vector<PresetFieldDescriptor>>;
};

/// DataType may provide override groups.
template <typename T>
concept HasOverrideGroups = requires(T& t) {
    { T::GetOverrideGroups(t) } -> std::convertible_to<std::vector<OverrideGroupDescriptor>>;
};

/// DataType may provide custom serialization hooks.
template <typename T>
concept HasCustomSerialize = requires(const T& t, CSimpleIniA& ini) {
    {T::SerializeCustom(t, ini)};
};

template <typename T>
concept HasCustomDeserialize = requires(T& t, const CSimpleIniA& ini) {
    {T::DeserializeCustom(t, ini)};
};

// ── PresetSerializer<DataType> ────────────────────────────────────────

/// Unified preset serializer template. DataType provides field descriptors;
/// this class provides the single SerializeToIni / DeserializeFromIni code path.
/// Inherits PresetSerializerBase CRTP for file I/O (LoadFromFile, SaveToFile, etc.).
template <typename DataType>
class PresetSerializer : public PresetSerializerBase<PresetSerializer<DataType>, DataType> {
public:
    static constexpr const char* kPresetsSubdir = DataType::kPresetsSubdir;

    static std::string SerializeToIni(const DataType& data) {
        CSimpleIniA ini;
        ini.SetUnicode(false);

        ini.SetValue("Preset", "name", data.name.c_str());
        ini.SetValue("Preset", "version", "1");

        // Serialize typed fields via descriptors
        if constexpr (HasPresetFields<DataType>) {
            auto fields = DataType::GetPresetFields(const_cast<DataType&>(data));
            SerializePresetFields(fields, ini);
        }

        // Serialize override groups
        if constexpr (HasOverrideGroups<DataType>) {
            auto groups = DataType::GetOverrideGroups(const_cast<DataType&>(data));
            for (const auto& group : groups)
                SerializeAll(group.fields, ini, group.section);
        }

        // Optional custom serialization
        if constexpr (HasCustomSerialize<DataType>) {
            DataType::SerializeCustom(data, ini);
        }

        std::string output;
        ini.Save(output);
        return output;
    }

    static DataType DeserializeFromIni(const std::string& iniContent) {
        DataType result;
        CSimpleIniA ini;
        ini.SetUnicode(false);

        if (ini.LoadData(iniContent) < 0) {
            result.error = "Failed to parse INI data";
            return result;
        }

        const char* ver = ini.GetValue("Preset", "version", "0");
        if (std::strcmp(ver, "1") != 0) {
            result.error = "Unsupported preset version: " + std::string(ver);
            return result;
        }

        result.name = ini.GetValue("Preset", "name", "Unnamed");

        // Deserialize typed fields
        if constexpr (HasPresetFields<DataType>) {
            auto fields = DataType::GetPresetFields(result);
            DeserializePresetFields(fields, ini);
        }

        // Deserialize override groups
        if constexpr (HasOverrideGroups<DataType>) {
            auto groups = DataType::GetOverrideGroups(result);
            for (const auto& group : groups)
                DeserializeAll(group.fields, ini, group.section);
        }

        // Optional custom deserialization
        if constexpr (HasCustomDeserialize<DataType>) {
            DataType::DeserializeCustom(result, ini);
        }

        result.success = true;
        return result;
    }
};
