#pragma once

#include <memory>
#include <Windows.h>
#include <unordered_map>
#include <functional>

#include "Render/IRenderCallback.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "Logger.h"
#include "Menu/MenuManager.h"
#include "GlobalDefinitions.h"
#include "Menu/Sections/Gameplay/PlayerSection.h"
#include "Menu/Sections/Gameplay/WorldSection.h"
#include "Menu/Sections/Entity_Spawner/NPCSection.h"
#include "Menu/Sections/Entity_Spawner/ItemSection.h"
#include "Menu/Sections/Settings/GuiSection.h"

class GuiSection;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class MedievalStyle {
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
    
    static void PushButtonStyle();
    static void PopButtonStyle();
    
    static void PushCheckboxStyle();
    static void PopCheckboxStyle();
    
    static void PushInputStyle();
    static void PopInputStyle();
    
    static void PushHeaderStyle();
    static void PopHeaderStyle();
    
    static void PushPopupStyle();
    static void PopPopupStyle();
    
    static void ApplyGlobalStyle();
};

class Gui : public IRenderCallback {
private:
    Gui() = default;
public:
    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;

    static Gui& Get() {
        static Gui instance;
        return instance;
    }

    void Setup();
    void Render();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static bool IsVisible() { return isVisible; }

private:
    void SetupStyle();
    static WNDPROC originalWndProc;
    static bool isVisible;
};