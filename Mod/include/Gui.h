#pragma once

#include <atomic>
#include <d3d11.h>
#include <Windows.h>
#include <wrl/client.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "Logger.h"
#include "Menu/MenuManager.h"
#include "GlobalDefinitions.h"
#include "DefaultStyle.h"

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

    void Init(
        Microsoft::WRL::ComPtr<ID3D11Device> newDevice, Microsoft::WRL::ComPtr<ID3D11DeviceContext> newContext,
        HWND newWindow
    ) noexcept;
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
    Microsoft::WRL::ComPtr<ID3D11Device> device = nullptr;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context = nullptr;
    HWND window = nullptr;

    static WNDPROC originalWndProc;
    static std::atomic<bool> isVisible;
};
