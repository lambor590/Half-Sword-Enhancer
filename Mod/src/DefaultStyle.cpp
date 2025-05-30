#include "DefaultStyle.h"

void DefaultStyle::ApplyGlobalStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    colors[ImGuiCol_WindowBg] = black;
    colors[ImGuiCol_Border] = oldBrass;
    colors[ImGuiCol_BorderShadow] = shadow;
    colors[ImGuiCol_Text] = parchment;
    colors[ImGuiCol_TextDisabled] = ImVec4(0.65f, 0.60f, 0.55f, 1.00f);
    
    colors[ImGuiCol_Header] = ImVec4(0.28f, 0.19f, 0.11f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.38f, 0.26f, 0.15f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.44f, 0.30f, 0.18f, 1.00f);
    
    colors[ImGuiCol_Tab] = darkWood;
    colors[ImGuiCol_TabHovered] = mediumWood;
    colors[ImGuiCol_TabActive] = lightWood;
    colors[ImGuiCol_TabUnfocused] = darkInk;
    colors[ImGuiCol_TabUnfocusedActive] = darkWood;
    
    colors[ImGuiCol_Button] = darkWood;
    colors[ImGuiCol_ButtonHovered] = lightWood;
    colors[ImGuiCol_ButtonActive] = oldBrass;
    
    colors[ImGuiCol_FrameBg] = darkWood;
    colors[ImGuiCol_FrameBgHovered] = mediumWood;
    colors[ImGuiCol_FrameBgActive] = lightWood;
    
    colors[ImGuiCol_CheckMark] = brightBrass;
    
    colors[ImGuiCol_ScrollbarBg] = darkInk;
    colors[ImGuiCol_ScrollbarGrab] = darkLeather;
    colors[ImGuiCol_ScrollbarGrabHovered] = mediumWood;
    colors[ImGuiCol_ScrollbarGrabActive] = oldBrass;
    
    colors[ImGuiCol_TitleBg] = darkWood;
    colors[ImGuiCol_TitleBgActive] = mediumWood;
    colors[ImGuiCol_TitleBgCollapsed] = darkInk;
    
    colors[ImGuiCol_SliderGrab] = darkLeather;
    colors[ImGuiCol_SliderGrabActive] = oldBrass;
    
    colors[ImGuiCol_ResizeGrip] = darkLeather;
    colors[ImGuiCol_ResizeGripHovered] = mediumWood;
    colors[ImGuiCol_ResizeGripActive] = oldBrass;
    
    colors[ImGuiCol_Separator] = oldBrass;
    colors[ImGuiCol_SeparatorHovered] = mediumWood;
    colors[ImGuiCol_SeparatorActive] = brightBrass;
    
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.70f, 0.55f, 0.30f, 0.35f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.09f, 0.06f, 0.98f);
    
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
    style.SelectableTextAlign = ImVec2(0.5f, 0.5f);
    style.DisplayWindowPadding = ImVec2(18, 18);
    style.DisplaySafeAreaPadding = ImVec2(3, 3);
    style.IndentSpacing = 25.0f;
    style.SeparatorTextPadding = ImVec2(15, 6);
} 