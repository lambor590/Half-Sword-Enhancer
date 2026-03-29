#include "Menu/Override.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "imgui/imgui.h"
#include "SimpleIni.h"
#include "Utils/GuiUtils.h"

int CountActive(std::span<const OverrideDescriptor> fields) {
    int count = 0;
    for (const auto& f : fields)
        count += *f.enabled;
    return count;
}

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


namespace {

    void RenderDoubleDrag(const OverrideDescriptor& field) {
        ImGui::Checkbox("##en", field.enabled);
        ImGui::SameLine();
        if (!*field.enabled) ImGui::BeginDisabled();
        float val = static_cast<float>(*static_cast<double*>(field.value));
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        if (ImGui::DragFloat(field.name, &val, field.speed, 0.0f, 0.0f, "%.3f"))
            *static_cast<double*>(field.value) = val;
        if (!*field.enabled) ImGui::EndDisabled();
    }

    void RenderIntDrag(const OverrideDescriptor& field) {
        ImGui::Checkbox("##en", field.enabled);
        ImGui::SameLine();
        if (!*field.enabled) ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        ImGui::DragInt(field.name, static_cast<int*>(field.value), field.speed, 0, 0);
        if (!*field.enabled) ImGui::EndDisabled();
    }

    void RenderBoolCombo(const OverrideDescriptor& field) {
        static constexpr const char* TRISTATE[] = {"Default", "No", "Yes"};
        static float tristateW = GuiUtils::CalcComboWidth(TRISTATE, 3);

        bool& enabled = *field.enabled;
        bool& value = *static_cast<bool*>(field.value);
        int current = enabled ? (value ? 2 : 1) : 0;
        ImGui::SetNextItemWidth(tristateW);
        if (ImGui::Combo(field.name, &current, TRISTATE, 3)) {
            enabled = (current != 0);
            value = (current == 2);
        }
    }

} // namespace

void RenderOverrideField(const OverrideDescriptor& field) {
    ImGui::PushID(field.name);
    switch (field.type) {
        case OverrideFieldType::Double: RenderDoubleDrag(field); break;
        case OverrideFieldType::Int: RenderIntDrag(field); break;
        case OverrideFieldType::Bool: RenderBoolCombo(field); break;
    }
    if (field.tooltip && ImGui::IsItemHovered()) {
        GuiUtils::BeginStyledTooltip();
        ImGui::TextUnformatted(field.tooltip);
        GuiUtils::EndStyledTooltip();
    }
    ImGui::PopID();
}

void RenderOverrideGroup(std::span<const OverrideDescriptor> fields) {
    for (const auto& f : fields)
        RenderOverrideField(f);
}
