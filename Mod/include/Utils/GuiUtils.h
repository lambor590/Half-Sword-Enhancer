#pragma once

#include "imgui/imgui.h"
#include "ConfigManager.h"

namespace GuiUtils {
    inline constexpr ImVec2 kTooltipPadding{8.0f, 6.0f};
    inline constexpr ImVec2 kPopupPadding{10.0f, 8.0f};

    inline void BeginStyledTooltip() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, kTooltipPadding);
        ImGui::BeginTooltip();
    }

    inline void EndStyledTooltip() {
        ImGui::EndTooltip();
        ImGui::PopStyleVar();
    }

    inline bool CheckboxWithTooltip(const char* label, bool* value, const char* tooltip) {
        bool changed = ImGui::Checkbox(label, value);
        if (ImGui::IsItemHovered()) {
            BeginStyledTooltip();
            ImGui::Text("%s", tooltip);
            EndStyledTooltip();
        }
        return changed;
    }

    inline bool CheckboxWithConfig(const char* label, const char* section, const char* key,
                                   bool defaultValue, const char* tooltip = nullptr) {
        static bool value = ConfigManager::Get().GetBool(section, key, defaultValue);
        bool changed = ImGui::Checkbox(label, &value);
        if (changed) {
            ConfigManager::Get().SetBool(section, key, value);
        }
        if (tooltip && ImGui::IsItemHovered()) {
            BeginStyledTooltip();
            ImGui::Text("%s", tooltip);
            EndStyledTooltip();
        }
        return changed;
    }

}