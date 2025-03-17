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
#include "Menu/Sections/Gameplay/GeneralSection.h"
#include "Menu/Sections/Settings/KeybindsSection.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class Gui : public IRenderCallback {
public:
    Gui() = default;
    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;

    void Setup();
    void Render();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    static void RegisterKeybind(int* key, std::function<void()> callback) {
        s_keybinds[key] = callback;
    }

    static void UnregisterKeybind(int* key) {
        s_keybinds.erase(key);
    }

private:
    void SetupStyle();
    static Gui* s_instance;
    static WNDPROC originalWndProc;
    static bool isVisible;
    static std::unordered_map<int*, std::function<void()>> s_keybinds;
    static inline int s_toggleGuiKey = VK_INSERT;
    static inline int s_unbindKey = VK_DELETE;
};