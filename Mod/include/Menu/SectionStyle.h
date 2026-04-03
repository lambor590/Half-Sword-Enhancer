#pragma once

#include "imgui/imgui.h"

namespace SectionStyle {
    constexpr ImVec2 FRAME_PADDING{8, 6};
    constexpr ImVec2 ITEM_SPACING{10, 8};
    constexpr ImVec2 CELL_PADDING{4, ITEM_SPACING.y * 0.5f};
    constexpr float INDENT_SPACING = 25.0f;

    struct StyleRAII {
        StyleRAII() noexcept {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, FRAME_PADDING);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING);
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, CELL_PADDING);
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, INDENT_SPACING);
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.25f);
        }
        ~StyleRAII() noexcept {
            ImGui::PopItemWidth();
            ImGui::PopStyleVar(4);
        }
        StyleRAII(const StyleRAII&) = delete;
        StyleRAII& operator=(const StyleRAII&) = delete;
    };
}
