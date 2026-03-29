#include "Menu/Override.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "SimpleIni.h"

// ── CountActive ───────────────────────────────────────────────────────

int CountActive(std::span<const OverrideDescriptor> fields) {
    int count = 0;
    for (const auto& f : fields)
        count += *f.enabled;
    return count;
}

// ── INI helpers (local) ───────────────────────────────────────────────

namespace {

    void SerializeDouble(CSimpleIniA& ini, const char* section, const char* key, bool enabled, double value) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d,%.6g", enabled ? 1 : 0, value);
        ini.SetValue(section, key, buf);
    }

    void SerializeInt(CSimpleIniA& ini, const char* section, const char* key, bool enabled, int value) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d,%d", enabled ? 1 : 0, value);
        ini.SetValue(section, key, buf);
    }

    void DeserializeDouble(const char* str, bool& enabled, double& value) {
        if (!str || !str[0]) {
            enabled = false;
            value = 0.0;
            return;
        }
        int en = 0;
        std::sscanf(str, "%d,%lf", &en, &value);
        enabled = en != 0;
    }

    void DeserializeInt(const char* str, bool& enabled, int& value) {
        if (!str || !str[0]) {
            enabled = false;
            value = 0;
            return;
        }
        int en = 0;
        std::sscanf(str, "%d,%d", &en, &value);
        enabled = en != 0;
    }

    void DeserializeBool(const char* str, bool& enabled, bool& value) {
        if (!str || !str[0]) {
            enabled = false;
            value = false;
            return;
        }
        int en = 0, val = 0;
        std::sscanf(str, "%d,%d", &en, &val);
        enabled = en != 0;
        value = val != 0;
    }

} // namespace

// ── SerializeAll ──────────────────────────────────────────────────────

void SerializeAll(std::span<const OverrideDescriptor> fields, CSimpleIniA& ini, const char* section, bool minimalMode) {
    for (const auto& f : fields) {
        if (minimalMode && !*f.enabled) continue;

        switch (f.type) {
            case OverrideFieldType::Double:
                SerializeDouble(ini, section, f.name, *f.enabled, *static_cast<double*>(f.value));
                break;
            case OverrideFieldType::Int:
                SerializeInt(ini, section, f.name, *f.enabled, *static_cast<int*>(f.value));
                break;
            case OverrideFieldType::Bool:
                SerializeInt(ini, section, f.name, *f.enabled, *static_cast<bool*>(f.value) ? 1 : 0);
                break;
        }
    }
}

// ── DeserializeAll ────────────────────────────────────────────────────

void DeserializeAll(std::span<const OverrideDescriptor> fields, const CSimpleIniA& ini, const char* section) {
    for (const auto& f : fields) {
        const char* raw = ini.GetValue(section, f.name, "");

        switch (f.type) {
            case OverrideFieldType::Double: DeserializeDouble(raw, *f.enabled, *static_cast<double*>(f.value)); break;
            case OverrideFieldType::Int: DeserializeInt(raw, *f.enabled, *static_cast<int*>(f.value)); break;
            case OverrideFieldType::Bool: DeserializeBool(raw, *f.enabled, *static_cast<bool*>(f.value)); break;
        }
    }
}
