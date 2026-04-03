#pragma once

#include <array>

#include "imgui/imgui.h"

constexpr ImVec4 MakeColor(float r, float g, float b, float a = 1.0f) noexcept {
    return ImVec4(r, g, b, a);
}

class DefaultStyle {
public:
    static constexpr ImVec4 PARCHMENT = MakeColor(0.95f, 0.92f, 0.85f);
    static constexpr ImVec4 PARCHMENT_DARK = MakeColor(0.89f, 0.85f, 0.75f);
    static constexpr ImVec4 DARK_WOOD = MakeColor(0.25f, 0.16f, 0.09f);
    static constexpr ImVec4 MEDIUM_WOOD = MakeColor(0.32f, 0.21f, 0.12f);
    static constexpr ImVec4 LIGHT_WOOD = MakeColor(0.44f, 0.30f, 0.18f);
    static constexpr ImVec4 OLD_BRASS = MakeColor(0.71f, 0.57f, 0.25f);
    static constexpr ImVec4 BRIGHT_BRASS = MakeColor(0.82f, 0.67f, 0.35f);
    static constexpr ImVec4 DARK_LEATHER = MakeColor(0.36f, 0.24f, 0.14f);
    static constexpr ImVec4 BLACK = MakeColor(0.10f, 0.08f, 0.06f, 0.95f);
    static constexpr ImVec4 DARK_INK = MakeColor(0.15f, 0.12f, 0.09f);
    static constexpr ImVec4 SHADOW = MakeColor(0.00f, 0.00f, 0.00f, 0.60f);
    static constexpr ImVec4 TEXT_DISABLED = MakeColor(0.65f, 0.60f, 0.55f);
    static constexpr ImVec4 HEADER = MakeColor(0.28f, 0.19f, 0.11f);
    static constexpr ImVec4 HEADER_HOVERED = MakeColor(0.38f, 0.26f, 0.15f);
    static constexpr ImVec4 HEADER_ACTIVE = MakeColor(0.44f, 0.30f, 0.18f);
    static constexpr ImVec4 TEXT_SELECTED_BG = MakeColor(0.70f, 0.55f, 0.30f, 0.35f);
    static constexpr ImVec4 POPUP_BG = MakeColor(0.12f, 0.09f, 0.06f, 0.98f);
    static constexpr ImVec4 TRANSPARENT = MakeColor(0, 0, 0, 0);
    static constexpr ImVec4 MODAL_DIM_BG = MakeColor(0.80f, 0.80f, 0.80f, 0.35f);
    static constexpr ImVec4 WINDOWING_HIGHLIGHT = MakeColor(1, 1, 1, 0.70f);
    static constexpr ImVec4 WINDOWING_DIM_BG = MakeColor(0.80f, 0.80f, 0.80f, 0.20f);
    static constexpr ImVec4 TABLE_ROW_BG_ALT = MakeColor(1, 1, 1, 0.06f);

    static constexpr auto GetColorArray() noexcept {
        std::array<ImVec4, ImGuiCol_COUNT> colors{};

        colors[ImGuiCol_Text] = PARCHMENT;
        colors[ImGuiCol_TextDisabled] = TEXT_DISABLED;
        colors[ImGuiCol_WindowBg] = BLACK;
        colors[ImGuiCol_ChildBg] = TRANSPARENT;
        colors[ImGuiCol_PopupBg] = POPUP_BG;
        colors[ImGuiCol_Border] = OLD_BRASS;
        colors[ImGuiCol_BorderShadow] = SHADOW;
        colors[ImGuiCol_FrameBg] = DARK_WOOD;
        colors[ImGuiCol_FrameBgHovered] = MEDIUM_WOOD;
        colors[ImGuiCol_FrameBgActive] = LIGHT_WOOD;
        colors[ImGuiCol_TitleBg] = DARK_WOOD;
        colors[ImGuiCol_TitleBgActive] = MEDIUM_WOOD;
        colors[ImGuiCol_TitleBgCollapsed] = DARK_INK;
        colors[ImGuiCol_MenuBarBg] = DARK_WOOD;
        colors[ImGuiCol_ScrollbarBg] = DARK_INK;
        colors[ImGuiCol_ScrollbarGrab] = DARK_LEATHER;
        colors[ImGuiCol_ScrollbarGrabHovered] = MEDIUM_WOOD;
        colors[ImGuiCol_ScrollbarGrabActive] = OLD_BRASS;
        colors[ImGuiCol_CheckMark] = BRIGHT_BRASS;
        colors[ImGuiCol_SliderGrab] = DARK_LEATHER;
        colors[ImGuiCol_SliderGrabActive] = OLD_BRASS;
        colors[ImGuiCol_Button] = DARK_WOOD;
        colors[ImGuiCol_ButtonHovered] = LIGHT_WOOD;
        colors[ImGuiCol_ButtonActive] = OLD_BRASS;
        colors[ImGuiCol_Header] = HEADER;
        colors[ImGuiCol_HeaderHovered] = HEADER_HOVERED;
        colors[ImGuiCol_HeaderActive] = HEADER_ACTIVE;
        colors[ImGuiCol_Separator] = OLD_BRASS;
        colors[ImGuiCol_SeparatorHovered] = MEDIUM_WOOD;
        colors[ImGuiCol_SeparatorActive] = BRIGHT_BRASS;
        colors[ImGuiCol_ResizeGrip] = DARK_LEATHER;
        colors[ImGuiCol_ResizeGripHovered] = MEDIUM_WOOD;
        colors[ImGuiCol_ResizeGripActive] = OLD_BRASS;
        colors[ImGuiCol_Tab] = DARK_WOOD;
        colors[ImGuiCol_TabHovered] = MEDIUM_WOOD;
        colors[ImGuiCol_TabActive] = LIGHT_WOOD;
        colors[ImGuiCol_TabUnfocused] = DARK_INK;
        colors[ImGuiCol_TabUnfocusedActive] = DARK_WOOD;
        colors[ImGuiCol_PlotLines] = PARCHMENT;
        colors[ImGuiCol_PlotLinesHovered] = OLD_BRASS;
        colors[ImGuiCol_PlotHistogram] = OLD_BRASS;
        colors[ImGuiCol_PlotHistogramHovered] = BRIGHT_BRASS;
        colors[ImGuiCol_TableHeaderBg] = DARK_WOOD;
        colors[ImGuiCol_TableBorderStrong] = OLD_BRASS;
        colors[ImGuiCol_TableBorderLight] = MEDIUM_WOOD;
        colors[ImGuiCol_TableRowBg] = TRANSPARENT;
        colors[ImGuiCol_TableRowBgAlt] = TABLE_ROW_BG_ALT;
        colors[ImGuiCol_TextSelectedBg] = TEXT_SELECTED_BG;
        colors[ImGuiCol_DragDropTarget] = BRIGHT_BRASS;
        colors[ImGuiCol_NavHighlight] = OLD_BRASS;
        colors[ImGuiCol_NavWindowingHighlight] = WINDOWING_HIGHLIGHT;
        colors[ImGuiCol_NavWindowingDimBg] = WINDOWING_DIM_BG;
        colors[ImGuiCol_ModalWindowDimBg] = MODAL_DIM_BG;
        colors[ImGuiCol_InputTextCursor] = BRIGHT_BRASS;

        return colors;
    }

    static void ApplyGlobalStyle();
};
