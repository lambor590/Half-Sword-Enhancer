#pragma once

#include <array>
#include <cstring>

#include "imgui/imgui.h"
#include "ConfigManager.h"

struct RuntimeOverride { bool enabled = false; double value = 0.0; };
struct BoolOverride    { bool enabled = false; bool value = false; };
struct IntOverride     { bool enabled = false; int value = 0; };

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

    inline constexpr auto LOWER_TABLE = [] {
        std::array<char, 256> t{};
        for (int i = 0; i < 256; ++i) t[i] = static_cast<char>(i);
        for (int i = 'A'; i <= 'Z'; ++i) t[i] = static_cast<char>(i + 32);
        return t;
    }();

    inline bool MatchesFilter(const char* name, size_t nameLen, const char* filter, size_t filterLen) {
        if (filterLen == 0) return true;
        if (filterLen > nameLen) return false;
        const char firstLower = LOWER_TABLE[static_cast<unsigned char>(filter[0])];
        const size_t limit = nameLen - filterLen;
        for (size_t i = 0; i <= limit; ++i) {
            if (LOWER_TABLE[static_cast<unsigned char>(name[i])] != firstLower) continue;
            size_t j = 1;
            while (j < filterLen &&
                   LOWER_TABLE[static_cast<unsigned char>(name[i + j])] ==
                   LOWER_TABLE[static_cast<unsigned char>(filter[j])])
                ++j;
            if (j == filterLen) return true;
        }
        return false;
    }

    inline constexpr const char* TIER_LABELS[] = {
        "Tier 0", "Tier 1", "Tier 2", "Tier 3", "Tier 4",
        "Tier 5", "Tier 6", "Tier 7", "Tier 8"
    };

    inline float CachedTierComboWidth() {
        static float w = CalcComboWidth(TIER_LABELS, 9);
        return w;
    }

    inline void RenderFreeTierCombo(const char* label, int& tier) {
        ImGui::SetNextItemWidth(CachedTierComboWidth());
        if (ImGui::BeginCombo(label, TIER_LABELS[tier])) {
            for (int t = 0; t <= 8; ++t) {
                if (ImGui::Selectable(TIER_LABELS[t], t == tier))
                    tier = t;
                if (t == tier) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    inline void RenderPriceDrag(const char* label, double& price, float speed = 1.0f) {
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.75f);
        float val = static_cast<float>(price);
        if (ImGui::DragFloat(label, &val, speed, 0.0f, 0.0f, "%.1f"))
            price = val;
        ImGui::PopItemWidth();
    }

    inline void RenderOverrideDrag(const char* label, RuntimeOverride& ovr,
                                   float speed = 0.1f, float min = 0.0f, float max = 0.0f) {
        ImGui::PushID(label);
        ImGui::Checkbox("##en", &ovr.enabled);
        ImGui::SameLine();
        if (!ovr.enabled) ImGui::BeginDisabled();
        float val = static_cast<float>(ovr.value);
        if (ImGui::DragFloat(label, &val, speed, min, max, "%.3f"))
            ovr.value = val;
        if (!ovr.enabled) ImGui::EndDisabled();
        ImGui::PopID();
    }

    inline void RenderOverrideInt(const char* label, IntOverride& ovr, int min = 0, int max = 10) {
        ImGui::PushID(label);
        ImGui::Checkbox("##en", &ovr.enabled);
        ImGui::SameLine();
        if (!ovr.enabled) ImGui::BeginDisabled();
        ImGui::SliderInt(label, &ovr.value, min, max);
        if (!ovr.enabled) ImGui::EndDisabled();
        ImGui::PopID();
    }

    inline void RenderOverrideBool(const char* label, BoolOverride& ovr) {
        ImGui::PushID(label);
        ImGui::Checkbox("##en", &ovr.enabled);
        ImGui::SameLine();
        if (!ovr.enabled) ImGui::BeginDisabled();
        ImGui::Checkbox(label, &ovr.value);
        if (!ovr.enabled) ImGui::EndDisabled();
        ImGui::PopID();
    }

}