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
#include "Menu/Sections/Settings/GraphicsSection.h"
#include "Menu/Sections/Settings/GuiSection.h"
#include "DefaultStyle.h"
#include "Hooks/GameHook.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
    static void ToggleVisibility() { 
        isVisible = !isVisible; 
        GameHook::SetInputEnabled(!isVisible);
    }

private:
    static WNDPROC originalWndProc;
    static bool isVisible;
};