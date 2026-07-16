#pragma once

#include <algorithm>

#include "imgui/imgui.h"

namespace SectionStyle {
    constexpr ImVec2 FRAME_PADDING{8, 6};
    constexpr ImVec2 ITEM_SPACING{10, 8};
    constexpr ImVec2 CELL_PADDING{4, ITEM_SPACING.y * 0.5f};
    constexpr float INDENT_SPACING = 25.0f;
    constexpr float FIELD_MIN_WIDTH = 120.0f;
    constexpr float FIELD_MAX_WIDTH = 280.0f;
    constexpr float FIELD_WIDTH_RATIO = 0.4f;

    [[nodiscard]] inline float ResolveFieldWidth(float available) noexcept {
        const float safeAvailable = (std::max)(1.0f, available);
        return (std::min)(
            safeAvailable,
            (std::clamp)(safeAvailable * FIELD_WIDTH_RATIO, FIELD_MIN_WIDTH, FIELD_MAX_WIDTH)
        );
    }

    struct StyleRAII {
        StyleRAII() noexcept {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, FRAME_PADDING);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING);
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, CELL_PADDING);
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, INDENT_SPACING);
            const float available = ImGui::GetContentRegionAvail().x;
            ImGui::PushItemWidth(ResolveFieldWidth(available));
        }
        ~StyleRAII() noexcept {
            ImGui::PopItemWidth();
            ImGui::PopStyleVar(4);
        }
        StyleRAII(const StyleRAII&) = delete;
        StyleRAII& operator=(const StyleRAII&) = delete;
    };
}
