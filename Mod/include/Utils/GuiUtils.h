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
    
    template<typename T>
    inline bool SliderWithTooltip(const char* label, T* value, T min, T max, const char* tooltip, const char* format = nullptr) {
        bool changed;
        if constexpr (std::is_same_v<T, float>) {
            changed = ImGui::SliderFloat(label, value, min, max, format);
        } else if constexpr (std::is_same_v<T, int>) {
            changed = ImGui::SliderInt(label, value, min, max, format);
        }
        
        if (tooltip && ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", tooltip);
            ImGui::EndTooltip();
        }
        return changed;
    }
    
    inline void TextWithTooltip(const char* text, const char* tooltip) {
        ImGui::Text("%s", text);
        if (tooltip && ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", tooltip);
            ImGui::EndTooltip();
        }
    }
    
    inline bool ButtonWithTooltip(const char* label, const char* tooltip) {
        bool clicked = ImGui::Button(label);
        if (tooltip && ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", tooltip);
            ImGui::EndTooltip();
        }
        return clicked;
    }
}