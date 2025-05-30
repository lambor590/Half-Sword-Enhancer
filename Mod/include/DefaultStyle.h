#pragma once

#include "imgui/imgui.h"

class DefaultStyle {
public:
    static inline const ImVec4 parchment = ImVec4(0.95f, 0.92f, 0.85f, 1.00f);
    static inline const ImVec4 parchmentDark = ImVec4(0.89f, 0.85f, 0.75f, 1.00f);
    static inline const ImVec4 darkWood = ImVec4(0.25f, 0.16f, 0.09f, 1.00f);
    static inline const ImVec4 mediumWood = ImVec4(0.32f, 0.21f, 0.12f, 1.00f);
    static inline const ImVec4 lightWood = ImVec4(0.44f, 0.30f, 0.18f, 1.00f);
    static inline const ImVec4 oldBrass = ImVec4(0.71f, 0.57f, 0.25f, 1.00f);
    static inline const ImVec4 brightBrass = ImVec4(0.82f, 0.67f, 0.35f, 1.00f);
    static inline const ImVec4 darkLeather = ImVec4(0.36f, 0.24f, 0.14f, 1.00f);
    static inline const ImVec4 black = ImVec4(0.10f, 0.08f, 0.06f, 0.95f);
    static inline const ImVec4 darkInk = ImVec4(0.15f, 0.12f, 0.09f, 1.00f);
    static inline const ImVec4 shadow = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
    
    static void ApplyGlobalStyle();
}; 