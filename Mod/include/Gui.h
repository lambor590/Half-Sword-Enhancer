#pragma once

#include <atomic>
#include <Windows.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern IMGUI_IMPL_API LRESULT
ImGui_ImplWin32_WndProcHandlerEx(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, ImGuiIO& io);

class Gui {
private:
    Gui() = default;

public:
    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;

    static Gui& Get() {
        static Gui instance;
        return instance;
    }

    void Init(HWND newWindow) noexcept;
    bool IsInitialized() const noexcept;
    void Setup();
    void Render();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static bool IsVisible() noexcept { return isVisible.load(std::memory_order_relaxed); }
    static void ToggleVisibility() noexcept {
        isVisible.store(!isVisible.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    static bool NeedsRendering() noexcept;

private:
    HWND window = nullptr;
    bool setupComplete = false;

    static WNDPROC originalWndProc;
    static std::atomic<bool> isVisible;
};
