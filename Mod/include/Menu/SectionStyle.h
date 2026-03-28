#pragma once

#include "imgui/imgui.h"

/// Shared style constants and RAII guard for section rendering.
/// Extracted from CollapsibleSection.h so all sections can access it
/// regardless of whether they inherit Section or CollapsibleSection.
namespace SectionStyle {
    constexpr ImVec2 framePadding{8, 6};
    constexpr ImVec2 itemSpacing{10, 8};
    constexpr ImVec2 cellPadding{4, itemSpacing.y * 0.5f};
    constexpr float indentSpacing = 25.0f;

    struct StyleRAII {
        StyleRAII() noexcept {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, framePadding);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, itemSpacing);
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cellPadding);
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, indentSpacing);
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
