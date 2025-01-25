#include "Gui.h"

Gui* Gui::s_instance = nullptr;
WNDPROC Gui::originalWndProc = nullptr;
bool Gui::isVisible = true;

Logger logger("Gui");

LRESULT CALLBACK Gui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (GetAsyncKeyState(VK_INSERT) & 1)
        isVisible = !isVisible;

    if (&isVisible && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
}

void Gui::Setup() {
    originalWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
    logger.Log("WndProc hooked successfully");

    ImGui::CreateContext();
    IMGUI_CHECKVERSION();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = "HS-Enhancer-imgui.ini";

    ImGui::StyleColorsDark();

    s_instance = this;
}

void Gui::Render() {
    if (!isVisible)
        return;

    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Half Sword Enhancer", &isVisible,
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoCollapse);

    if (ImGui::CollapsingHeader("Debug")) {
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Delta Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
    }

    ImGui::End();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}