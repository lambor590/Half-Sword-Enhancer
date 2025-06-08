#include "Gui.h"
#include "KeybindManager.h"
#include "NotificationManager.h"

WNDPROC Gui::originalWndProc = nullptr;
bool Gui::isVisible = true;

Logger logger("Gui");

LRESULT CALLBACK Gui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (KeybindManager::ProcessKeyEvent(msg, wParam))
        return true;
    
    if (msg == WM_KEYDOWN && wParam == KeybindManager::GetToggleGuiKey()) {
        isVisible = !isVisible;
        return true;
    }

    if (isVisible && (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam) || 
        (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST))) {
        return true;
    }

    return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
}

void Gui::Setup() {    
    originalWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
    logger.Log("WndProc hooked successfully");

    ImGui::CreateContext();
    IMGUI_CHECKVERSION();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = _strdup((ConfigManager::GetAppDataPath() / "imgui.ini").string().c_str());

    DefaultStyle::ApplyGlobalStyle();

    ImGui::SetNextWindowSize(ImVec2(699, 389), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(544, 331), ImGuiCond_Once, ImVec2(0.5f, 0.5f));

    NotificationManager::Initialize();

    MenuManager::Get().AddSection<PlayerSection>(MenuTab::Gameplay);
    MenuManager::Get().AddSection<WorldSection>(MenuTab::Gameplay);
    MenuManager::Get().AddSection<NPCSection>(MenuTab::Entity_Spawner);
    MenuManager::Get().AddSection<ItemSection>(MenuTab::Entity_Spawner);
    MenuManager::Get().AddSection<GuiSection>(MenuTab::Settings);
}

void Gui::Render() {
    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    if (!isVisible) {
        ImGuiIO& io = ImGui::GetIO();
        io.WantCaptureMouse = false;
        io.WantCaptureKeyboard = false;
        io.WantTextInput = false;
    }

    NotificationManager::Update();

    if (isVisible) {
        ImGui::Begin("Half Sword Enhancer", &isVisible, ImGuiWindowFlags_NoCollapse);
        MenuManager::Get().RenderMenu();
        ImGui::End();
    }

    NotificationManager::Render();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}