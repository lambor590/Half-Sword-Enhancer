#include "Gui.h"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "Logger.h"
#include "MenuSection.h"
#include "sections/ISection.h"
#include "sections/GameplaySection.h"
#include "sections/VisualSection.h"
#include "sections/DebugSection.h"
#include "sections/SettingsSection.h"

Gui* Gui::s_instance = nullptr;
WNDPROC Gui::originalWndProc = nullptr;
bool Gui::isVisible = true;

Logger logger("Gui");

Gui::Gui() = default;

Gui::~Gui() {
    if (imguiContext) {
        ImGui::DestroyContext(imguiContext);
    }
}

LRESULT CALLBACK Gui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto& instance = *s_instance;

    if (GetAsyncKeyState(VK_INSERT) & 1)
        isVisible = !isVisible;

    // Capturar teclas para keybindings
    if (instance.waitingForKey && msg == WM_KEYDOWN) {
        instance.waitingForKey = false;
        ActionManager::Get().SetKeyBinding(instance.pendingKeybindActionId, (int)wParam);
        return true;
    }

    if (isVisible && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
}

void Gui::Setup() {
    originalWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
    logger.Log("WndProc hooked successfully");

    imguiContext = ImGui::CreateContext();
    IMGUI_CHECKVERSION();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = "HS-Enhancer-imgui.ini";

    ImGui::StyleColorsDark();
    
    s_instance = this;
    MenuManager::Get().SetGuiInstance(this);
    
    SetupSections();
}

void Gui::SetupSections() {
    auto& menuManager = MenuManager::Get();

    // Inicializar todas las secciones
    sections.emplace_back(std::make_unique<GameplaySection>());
    sections.emplace_back(std::make_unique<VisualSection>());
    sections.emplace_back(std::make_unique<DebugSection>());
    sections.emplace_back(std::make_unique<SettingsSection>());

    // Configurar cada sección
    auto& gameplaySection = menuManager.CreateSection("gameplay", "Gameplay");
    sections[0]->Setup(gameplaySection);

    auto& visualSection = menuManager.CreateSection("visual", "Visual");
    sections[1]->Setup(visualSection);

    auto& debugSection = menuManager.CreateSection("debug", "Debug");
    sections[2]->Setup(debugSection);

    auto& settingsSection = menuManager.CreateSection("settings", "Settings");
    sections[3]->Setup(settingsSection);

    logger.Log("Menu sections initialized successfully");
}

void Gui::Render() {
    if (!isVisible)
        return;

    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Half Sword Enhancer", &isVisible, ImGuiWindowFlags_NoCollapse);
    
    MenuManager::Get().RenderAll();

    ImGui::End();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}