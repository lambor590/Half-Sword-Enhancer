#pragma once

#include <array>
#include <cstring>
#include <filesystem>
#include <vector>

#include "imgui/imgui.h"
#include "ConfigManager.h"
#include "DefaultStyle.h"
#include "Utils/PresetUtils.h"
#include "Utils/OverrideTypes.h"

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

    inline void RenderOverrideIndicator(bool active) {
        if (!active) return;
        auto* drawList = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float radius = 3.0f;
        float yCenter = pos.y + ImGui::GetFrameHeight() * 0.5f;
        drawList->AddCircleFilled(ImVec2(pos.x + radius, yCenter), radius, IM_COL32(209, 171, 89, 255));
        ImGui::Dummy(ImVec2(radius * 2 + 2.0f, 0));
        ImGui::SameLine(0, 0);
    }

    inline void RenderOverrideDrag(const char* label, RuntimeOverride& ovr,
                                   float speed = 0.1f, float min = 0.0f, float max = 0.0f) {
        ImGui::PushID(label);
        RenderOverrideIndicator(ovr.enabled);
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
        RenderOverrideIndicator(ovr.enabled);
        ImGui::Checkbox("##en", &ovr.enabled);
        ImGui::SameLine();
        if (!ovr.enabled) ImGui::BeginDisabled();
        ImGui::SliderInt(label, &ovr.value, min, max);
        if (!ovr.enabled) ImGui::EndDisabled();
        ImGui::PopID();
    }

    inline void RenderOverrideBool(const char* label, BoolOverride& ovr) {
        static constexpr const char* TRISTATE[] = { "Default", "No", "Yes" };
        static float tristateW = CalcComboWidth(TRISTATE, 3);

        int current = ovr.enabled ? (ovr.value ? 2 : 1) : 0;
        ImGui::PushID(label);
        RenderOverrideIndicator(ovr.enabled);
        ImGui::SetNextItemWidth(tristateW);
        if (ImGui::Combo(label, &current, TRISTATE, 3)) {
            ovr.enabled = (current != 0);
            ovr.value = (current == 2);
        }
        ImGui::PopID();
    }

    inline constexpr float UNDERLINE_TAB_HPAD = 12.0f;
    inline constexpr float UNDERLINE_TAB_VPAD = 4.0f;
    inline constexpr float UNDERLINE_THICKNESS = 2.0f;
    inline constexpr float UNDERLINE_GAP = 6.0f;
    inline constexpr float TAB_SPACING = 4.0f;

    inline void RenderUnderlineTabs(const char* id, int& activeTab,
        const char* const* labels, int count)
    {
        ImGui::PushID(id);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float textH = ImGui::GetTextLineHeight();
        float rowH = textH + UNDERLINE_TAB_VPAD * 2;
        float availWidth = ImGui::GetContentRegionAvail().x;

        static const ImU32 activeCol = ImGui::ColorConvertFloat4ToU32(DefaultStyle::oldBrass);
        static const ImU32 inactiveCol = ImGui::ColorConvertFloat4ToU32(DefaultStyle::textDisabled);
        static const ImU32 hoverCol = ImGui::ColorConvertFloat4ToU32(DefaultStyle::parchmentDark);
        static const ImU32 baselineCol = ImGui::ColorConvertFloat4ToU32(
            ImVec4(DefaultStyle::mediumWood.x, DefaultStyle::mediumWood.y,
                   DefaultStyle::mediumWood.z, 0.35f));

        float x = cursor.x;
        for (int i = 0; i < count; ++i) {
            ImVec2 textSize = ImGui::CalcTextSize(labels[i]);
            float tabW = textSize.x + UNDERLINE_TAB_HPAD * 2;

            char btnId[32];
            std::snprintf(btnId, sizeof(btnId), "##tab%d", i);
            ImGui::SetCursorScreenPos(ImVec2(x, cursor.y));
            if (ImGui::InvisibleButton(btnId, ImVec2(tabW, rowH)))
                activeTab = i;
            bool hovered = ImGui::IsItemHovered();
            if (i < count - 1) ImGui::SameLine(0, TAB_SPACING);

            bool isActive = (i == activeTab);
            ImU32 textCol = isActive ? activeCol : (hovered ? hoverCol : inactiveCol);
            ImVec2 textPos(x + UNDERLINE_TAB_HPAD, cursor.y + UNDERLINE_TAB_VPAD);
            dl->AddText(textPos, textCol, labels[i]);

            if (isActive) {
                float lineY = cursor.y + rowH;
                dl->AddLine(ImVec2(x, lineY), ImVec2(x + tabW, lineY),
                            activeCol, UNDERLINE_THICKNESS);
            }

            x += tabW + TAB_SPACING;
        }

        float baselineY = cursor.y + rowH + 1.0f;
        dl->AddLine(ImVec2(cursor.x, baselineY),
                    ImVec2(cursor.x + availWidth, baselineY),
                    baselineCol, 1.0f);

        ImGui::SetCursorScreenPos(ImVec2(cursor.x, baselineY + UNDERLINE_GAP));
        ImGui::PopID();
    }

    struct StatusMessage {
        std::string text;
        double time = 0.0;
        bool isError = false;

        void Set(std::string msg, bool error = false) {
            text = std::move(msg);
            time = ImGui::GetTime();
            isError = error;
        }

        void Render() {
            if (text.empty()) return;
            if (ImGui::GetTime() - time > 3.0) { text.clear(); return; }
            ImVec4 color = isError ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
                                   : ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
            ImGui::TextColored(color, "%s", text.c_str());
        }
    };

    struct PresetTreeAction {
        enum Type { None, Load, Delete } type = None;
        std::filesystem::path path;
    };

    inline PresetTreeAction RenderPresetTree(const PresetUtils::PresetTreeNode& node) {
        PresetTreeAction action;
        static const float loadW = ImGui::CalcTextSize("Load").x + ImGui::GetStyle().FramePadding.x * 2;
        static const float delW = ImGui::CalcTextSize("Del").x + ImGui::GetStyle().FramePadding.x * 2;
        static const float buttonsWidth = loadW + delW + ImGui::GetStyle().ItemSpacing.x * 2;

        for (const auto& child : node.children) {
            if (ImGui::TreeNode(child.name.c_str())) {
                auto childAction = RenderPresetTree(child);
                if (childAction.type != PresetTreeAction::None)
                    action = std::move(childAction);
                ImGui::TreePop();
            }
        }

        for (size_t i = 0; i < node.presets.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            float textW = ImGui::GetContentRegionAvail().x - buttonsWidth;

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(node.presets[i].name.c_str());
            if (textW > 0)
                ImGui::SameLine(textW);
            if (ImGui::Button("Load"))
                action = {PresetTreeAction::Load, node.presets[i].path};
            ImGui::SameLine();
            if (ImGui::Button("Del"))
                action = {PresetTreeAction::Delete, node.presets[i].path};
            ImGui::PopID();
        }

        return action;
    }

}
