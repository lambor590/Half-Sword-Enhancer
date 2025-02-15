#pragma once

#include <memory>
#include <vector>
#include <string>
#include <Windows.h>

#include "IRenderCallback.h"
#include "Action.h"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "Logger.h"
#include "Menu/MenuManager.h"
#include "Menu/Sections/Gameplay/CombatSection.h"
#include "Menu/Sections/Gameplay/MovementSection.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class Gui : public IRenderCallback {
public:
    Gui();
    ~Gui();
    
    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;

    void Setup();
    void Render();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void SetWaitingForKey(bool waiting, const std::string& actionId) {
        waitingForKey = waiting;
        pendingKeybindActionId = actionId;
    }

private:
    static Gui* s_instance;
    static WNDPROC originalWndProc;
    static bool isVisible;
};