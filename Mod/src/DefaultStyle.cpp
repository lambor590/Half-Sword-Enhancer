#include "DefaultStyle.h"

void DefaultStyle::ApplyGlobalStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    static constexpr auto precomputedColors = GetColorArray();

    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        style.Colors[i] = precomputedColors[i];
    }

    style.WindowPadding = ImVec2(18, 18);
    style.FramePadding = ImVec2(10, 8);
    style.ItemSpacing = ImVec2(12, 10);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 8.0f;
    style.WindowBorderSize = 1.5f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
    style.DisplayWindowPadding = ImVec2(18, 18);
    style.DisplaySafeAreaPadding = ImVec2(3, 3);
    style.IndentSpacing = 25.0f;
    style.SeparatorTextPadding = ImVec2(15, 6);
}
