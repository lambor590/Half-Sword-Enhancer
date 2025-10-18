#include "Gui.h"
#include "KeybindManager.h"
#include "NotificationManager.h"

WNDPROC Gui::originalWndProc = nullptr;
bool Gui::isVisible = true;

Logger logger("Gui");

LRESULT CALLBACK Gui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (KeybindManager::ProcessRebindEvent(msg, wParam))
        return true;

    if (!isVisible) [[likely]] {
        if (KeybindManager::ProcessKeyEvent(msg, wParam))
            return true;
        return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
    }

    static thread_local ImGuiIO* cachedIO = nullptr;
    if (!cachedIO) [[unlikely]] {
        cachedIO = &ImGui::GetIO();
    }
    ImGuiIO& io = *cachedIO;

    if (!io.WantTextInput && KeybindManager::ProcessKeyEvent(msg, wParam))
        return true;

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    if (io.WantCaptureMouse || io.WantTextInput)
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
    static bool previousVisibility = isVisible;

    if (previousVisibility != isVisible) {
        GameHook::SetInputEnabled(!isVisible);
        previousVisibility = isVisible;
    }

    NotificationManager::Update();

    const bool hasNotifications = NotificationManager::IsEnabled() && NotificationManager::HasNotifications();

    if (!isVisible && !hasNotifications) [[likely]] {
        return;
    }

    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    if (!isVisible) {
        ImGuiIO& io = ImGui::GetIO();
        io.WantCaptureMouse = io.WantCaptureKeyboard = io.WantTextInput = false;
    }

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