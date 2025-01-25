#pragma once

#include <memory>
#include <Windows.h>

#include "IRenderCallback.h"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "Logger.h"
#include "core/Action.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class Gui : public IRenderCallback {
public:
    Gui() = default;
    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;

    void Setup();
    void Render();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void RegisterActions();
    void RenderGameplayTab();
    void RenderVisualTab();
    void RenderDebugTab();
    void RenderSettingsTab();
    
    void RenderActionToggle(const std::string& id);
    void RenderKeybind(const std::string& id);
    
    static Gui* s_instance;
    static WNDPROC originalWndProc;
    static bool isVisible;
    bool waitingForKey = false;
    std::string pendingKeybindActionId;
};