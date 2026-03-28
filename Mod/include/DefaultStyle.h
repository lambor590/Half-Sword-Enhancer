#pragma once

#include <array>

#include "imgui/imgui.h"

constexpr ImVec4 makeColor(float r, float g, float b, float a = 1.0f) noexcept {
    return ImVec4(r, g, b, a);
}

class DefaultStyle {
public:
    static constexpr ImVec4 parchment = makeColor(0.95f, 0.92f, 0.85f);
    static constexpr ImVec4 parchmentDark = makeColor(0.89f, 0.85f, 0.75f);
    static constexpr ImVec4 darkWood = makeColor(0.25f, 0.16f, 0.09f);
    static constexpr ImVec4 mediumWood = makeColor(0.32f, 0.21f, 0.12f);
    static constexpr ImVec4 lightWood = makeColor(0.44f, 0.30f, 0.18f);
    static constexpr ImVec4 oldBrass = makeColor(0.71f, 0.57f, 0.25f);
    static constexpr ImVec4 brightBrass = makeColor(0.82f, 0.67f, 0.35f);
    static constexpr ImVec4 darkLeather = makeColor(0.36f, 0.24f, 0.14f);
    static constexpr ImVec4 black = makeColor(0.10f, 0.08f, 0.06f, 0.95f);
    static constexpr ImVec4 darkInk = makeColor(0.15f, 0.12f, 0.09f);
    static constexpr ImVec4 shadow = makeColor(0.00f, 0.00f, 0.00f, 0.60f);
    static constexpr ImVec4 textDisabled = makeColor(0.65f, 0.60f, 0.55f);
    static constexpr ImVec4 header = makeColor(0.28f, 0.19f, 0.11f);
    static constexpr ImVec4 headerHovered = makeColor(0.38f, 0.26f, 0.15f);
    static constexpr ImVec4 headerActive = makeColor(0.44f, 0.30f, 0.18f);
    static constexpr ImVec4 textSelectedBg = makeColor(0.70f, 0.55f, 0.30f, 0.35f);
    static constexpr ImVec4 popupBg = makeColor(0.12f, 0.09f, 0.06f, 0.98f);
    static constexpr ImVec4 transparent = makeColor(0, 0, 0, 0);
    static constexpr ImVec4 modalDimBg = makeColor(0.80f, 0.80f, 0.80f, 0.35f);
    static constexpr ImVec4 windowingHighlight = makeColor(1, 1, 1, 0.70f);
    static constexpr ImVec4 windowingDimBg = makeColor(0.80f, 0.80f, 0.80f, 0.20f);
    static constexpr ImVec4 tableRowBgAlt = makeColor(1, 1, 1, 0.06f);

    static constexpr auto GetColorArray() noexcept {
        std::array<ImVec4, ImGuiCol_COUNT> colors{};

        colors[ImGuiCol_Text] = parchment;
        colors[ImGuiCol_TextDisabled] = textDisabled;
        colors[ImGuiCol_WindowBg] = black;
        colors[ImGuiCol_ChildBg] = transparent;
        colors[ImGuiCol_PopupBg] = popupBg;
        colors[ImGuiCol_Border] = oldBrass;
        colors[ImGuiCol_BorderShadow] = shadow;
        colors[ImGuiCol_FrameBg] = darkWood;
        colors[ImGuiCol_FrameBgHovered] = mediumWood;
        colors[ImGuiCol_FrameBgActive] = lightWood;
        colors[ImGuiCol_TitleBg] = darkWood;
        colors[ImGuiCol_TitleBgActive] = mediumWood;
        colors[ImGuiCol_TitleBgCollapsed] = darkInk;
        colors[ImGuiCol_MenuBarBg] = darkWood;
        colors[ImGuiCol_ScrollbarBg] = darkInk;
        colors[ImGuiCol_ScrollbarGrab] = darkLeather;
        colors[ImGuiCol_ScrollbarGrabHovered] = mediumWood;
        colors[ImGuiCol_ScrollbarGrabActive] = oldBrass;
        colors[ImGuiCol_CheckMark] = brightBrass;
        colors[ImGuiCol_SliderGrab] = darkLeather;
        colors[ImGuiCol_SliderGrabActive] = oldBrass;
        colors[ImGuiCol_Button] = darkWood;
        colors[ImGuiCol_ButtonHovered] = lightWood;
        colors[ImGuiCol_ButtonActive] = oldBrass;
        colors[ImGuiCol_Header] = header;
        colors[ImGuiCol_HeaderHovered] = headerHovered;
        colors[ImGuiCol_HeaderActive] = headerActive;
        colors[ImGuiCol_Separator] = oldBrass;
        colors[ImGuiCol_SeparatorHovered] = mediumWood;
        colors[ImGuiCol_SeparatorActive] = brightBrass;
        colors[ImGuiCol_ResizeGrip] = darkLeather;
        colors[ImGuiCol_ResizeGripHovered] = mediumWood;
        colors[ImGuiCol_ResizeGripActive] = oldBrass;
        colors[ImGuiCol_Tab] = darkWood;
        colors[ImGuiCol_TabHovered] = mediumWood;
        colors[ImGuiCol_TabActive] = lightWood;
        colors[ImGuiCol_TabUnfocused] = darkInk;
        colors[ImGuiCol_TabUnfocusedActive] = darkWood;
        colors[ImGuiCol_PlotLines] = parchment;
        colors[ImGuiCol_PlotLinesHovered] = oldBrass;
        colors[ImGuiCol_PlotHistogram] = oldBrass;
        colors[ImGuiCol_PlotHistogramHovered] = brightBrass;
        colors[ImGuiCol_TableHeaderBg] = darkWood;
        colors[ImGuiCol_TableBorderStrong] = oldBrass;
        colors[ImGuiCol_TableBorderLight] = mediumWood;
        colors[ImGuiCol_TableRowBg] = transparent;
        colors[ImGuiCol_TableRowBgAlt] = tableRowBgAlt;
        colors[ImGuiCol_TextSelectedBg] = textSelectedBg;
        colors[ImGuiCol_DragDropTarget] = brightBrass;
        colors[ImGuiCol_NavHighlight] = oldBrass;
        colors[ImGuiCol_NavWindowingHighlight] = windowingHighlight;
        colors[ImGuiCol_NavWindowingDimBg] = windowingDimBg;
        colors[ImGuiCol_ModalWindowDimBg] = modalDimBg;
        colors[ImGuiCol_InputTextCursor] = brightBrass;

        return colors;
    }

    static void ApplyGlobalStyle();
};