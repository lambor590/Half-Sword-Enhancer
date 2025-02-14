#include "Gui.h"
#include "Menu/MenuManager.h"
#include "Menu/Sections/Gameplay/CombatSection.h"
#include "Menu/Sections/Gameplay/MovementSection.h"

Gui* Gui::s_instance = nullptr;
WNDPROC Gui::originalWndProc = nullptr;
bool Gui::isVisible = true;

Logger logger("Gui");

LRESULT CALLBACK Gui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto& instance = *s_instance;

    if (GetAsyncKeyState(VK_INSERT) & 1)
        isVisible = !isVisible;

    if (isVisible && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
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
    io.IniFilename = "HS-Enhancer-config.ini";

    ImGui::StyleColorsDark();

    // Inicializar secciones
    MenuManager::Get().AddSection<CombatSection>(MenuTab::Gameplay);
    MenuManager::Get().AddSection<MovementSection>(MenuTab::Gameplay);

    s_instance = this;
}

void Gui::Render() {
    if (!isVisible)
        return;

    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Half Sword Enhancer", &isVisible, ImGuiWindowFlags_NoCollapse);

    if (ImGui::BeginTabBar("MainTabBar")) {
        if (ImGui::BeginTabItem("Gameplay")) {
            MenuManager::Get().RenderSections(MenuTab::Gameplay);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Debug")) {
            MenuManager::Get().RenderSections(MenuTab::Debug);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Settings")) {
            MenuManager::Get().RenderSections(MenuTab::Settings);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}