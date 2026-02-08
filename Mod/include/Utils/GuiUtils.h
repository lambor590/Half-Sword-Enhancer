#pragma once

#include "imgui/imgui.h"
#include "ConfigManager.h"

namespace GuiUtils {
    inline bool CheckboxWithTooltip(const char* label, bool* value, const char* tooltip) {
        bool changed = ImGui::Checkbox(label, value);
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", tooltip);
            ImGui::EndTooltip();
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
            ImGui::BeginTooltip();
            ImGui::Text("%s", tooltip);
            ImGui::EndTooltip();
        }
        return changed;
    }

}