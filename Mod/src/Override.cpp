#include "Menu/Override.h"

#include "imgui/imgui.h"
#include "Utils/GuiUtils.h"

int CountActive(std::span<const OverrideDescriptor> fields) {
    int count = 0;
    for (const auto& f : fields)
        count += *f.enabled;
    return count;
}

namespace {

    void RenderDoubleDrag(const OverrideDescriptor& field) {
        ImGui::Checkbox("##en", field.enabled);
        ImGui::SameLine();
        if (!*field.enabled) ImGui::BeginDisabled();
        auto val = static_cast<float>(*static_cast<double*>(field.value));
        ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
        GuiUtils::DebouncedDragFloat(field.name, &val, field.speed, 0.0f, 0.0f, "%.3f");
        GuiUtils::StoreEdited(*static_cast<double*>(field.value), val);
        if (!*field.enabled) ImGui::EndDisabled();
    }

    void RenderIntDrag(const OverrideDescriptor& field) {
        ImGui::Checkbox("##en", field.enabled);
        ImGui::SameLine();
        if (!*field.enabled) ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
        GuiUtils::DebouncedDragInt(field.name, static_cast<int*>(field.value), field.speed);
        if (!*field.enabled) ImGui::EndDisabled();
    }

    void RenderBoolCombo(const OverrideDescriptor& field) {
        static constexpr const char* TRISTATE[] = {"Default", "No", "Yes"};
        static float tristateW = GuiUtils::CalcComboWidth(TRISTATE, 3);

        bool& enabled = *field.enabled;
        bool& value = *static_cast<bool*>(field.value);
        int current = enabled ? (value ? 2 : 1) : 0;
        GuiUtils::PrepareNextCombo(tristateW);
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
    if (field.tooltip) GuiUtils::HelpTooltip(field.tooltip);
    ImGui::PopID();
}

void RenderOverrideGroup(std::span<const OverrideDescriptor> fields) {
    for (const auto& f : fields)
        RenderOverrideField(f);
}
