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

    inline float ComboWidthFromText(float maxTextWidth) {
        return maxTextWidth + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2;
    }

    inline float CalcComboWidth(const char* widestItem) {
        return ComboWidthFromText(ImGui::CalcTextSize(widestItem).x);
    }

    inline float CalcComboWidth(const char* const* items, int count) {
        float maxW = 0;
        for (int i = 0; i < count; ++i) {
            float w = ImGui::CalcTextSize(items[i]).x;
            if (w > maxW) maxW = w;
        }
        return ComboWidthFromText(maxW);
    }

    inline float CalcComboWidth(const char* (*getter)(void* data, int idx), void* data, int count) {
        float maxW = 0;
        for (int i = 0; i < count; ++i) {
            float w = ImGui::CalcTextSize(getter(data, i)).x;
            if (w > maxW) maxW = w;
        }
        return ComboWidthFromText(maxW);
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