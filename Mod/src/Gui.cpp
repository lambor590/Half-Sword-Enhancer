#include "Gui.h"
#include "KeybindManager.h"
#include "NotificationManager.h"

WNDPROC Gui::originalWndProc = nullptr;
bool Gui::isVisible = true;

Logger logger("Gui");

LRESULT CALLBACK Gui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (KeybindManager::ProcessRebindEvent(msg, wParam))
        return true;

    if (KeybindManager::ProcessKeyEvent(msg, wParam))
        return true;

    if (isVisible && (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam) || 
        (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST && ImGui::GetIO().WantCaptureMouse))) {
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
    static const std::string iniPath = (ConfigManager::GetAppDataPath() / "imgui.ini").string();
    io.IniFilename = iniPath.c_str();

    DefaultStyle::ApplyGlobalStyle();

    ImGui::SetNextWindowSize(ImVec2(699, 389), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(544, 331), ImGuiCond_Once, ImVec2(0.5f, 0.5f));

    NotificationManager::Initialize();

    MenuManager::Get().AddSection<PlayerSection>(MenuTab::Gameplay);
    MenuManager::Get().AddSection<WorldSection>(MenuTab::Gameplay);
    MenuManager::Get().AddSection<NPCSection>(MenuTab::Entity_Spawner);
    MenuManager::Get().AddSection<ItemSection>(MenuTab::Entity_Spawner);
    MenuManager::Get().AddSection<GuiSection>(MenuTab::Settings);
    MenuManager::Get().AddSection<GraphicsSection>(MenuTab::Settings);
}

void Gui::Render() {
    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    static bool previousVisibility = isVisible;

    if (previousVisibility != isVisible) {
        GameHook::SetInputEnabled(!isVisible);
        KeybindManager::ResetKeyStates();
        previousVisibility = isVisible;
    }

    if (!isVisible) {
        ImGuiIO& io = ImGui::GetIO();
        io.WantCaptureMouse = io.WantCaptureKeyboard = io.WantTextInput = false;
    }

    NotificationManager::Update();

    if (isVisible) {
        constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;
        #ifdef DEV_VERSION
            constexpr const char* windowTitle = "Half Sword Enhancer - Dev Build";
        #else
            constexpr const char* windowTitle = "Half Sword Enhancer";
        #endif

        if (ImGui::Begin(windowTitle, &isVisible, windowFlags)) {
            MenuManager::Get().RenderMenu();
        }
        ImGui::End();
    }

    NotificationManager::Render();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}