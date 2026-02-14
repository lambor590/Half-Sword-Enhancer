#include "Gui.h"
#include "Menu/Sections/Gameplay/PlayerSection.h"
#include "Menu/Sections/Gameplay/WorldSection.h"
#include "Menu/Sections/Entity_Spawner/NPCSection.h"
#include "Menu/Sections/Entity_Spawner/ItemSection.h"
#include "Menu/Sections/Entity_Spawner/WeaponEditorSection.h"
#include "Menu/Sections/Settings/GraphicsSection.h"
#include "Menu/Sections/Settings/GuiSection.h"
#include "KeybindManager.h"
#include "NotificationManager.h"
#include "Version.h"
#include "Utils/GameBuildInfo.h"

WNDPROC Gui::originalWndProc = nullptr;
bool Gui::isVisible = true;

Logger logger("Gui");

namespace {
    bool s_showMismatchPopup = false;
    bool s_mismatchDismissed = false;
    bool s_popupOpened = false;

    void RenderMismatchPopup() {
        if (!s_mismatchDismissed && !s_showMismatchPopup &&
            GameBuildInfo::Get().mismatchDetected.load(std::memory_order_relaxed)) {
            s_showMismatchPopup = true;
            s_popupOpened = false;
        }

        if (!s_showMismatchPopup) return;

        if (!s_popupOpened) {
            ImGui::OpenPopup("##version_mismatch");
            s_popupOpened = true;
        }

        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0.6f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 16.0f));
        if (ImGui::BeginPopupModal("##version_mismatch", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
            auto& info = GameBuildInfo::Get();
            ImGui::Text("Game version mismatch detected");
            ImGui::Spacing();
            ImGui::Text("Expected: %s", HSE_TARGET_BUILD);
            ImGui::Text("Detected: %s", info.buildVersion.c_str());
            ImGui::Spacing();
            ImGui::Text("The mod may not work correctly with this game version.");
            ImGui::Spacing();
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
                s_showMismatchPopup = false;
                s_mismatchDismissed = true;
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}

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

    ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

    if (io.WantCaptureMouse && (msg == WM_SETCURSOR || (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)))
        return true;
    if ((io.WantTextInput || io.WantCaptureKeyboard) && msg >= WM_KEYFIRST && msg <= WM_KEYLAST)
        return true;

    return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
}

void Gui::Setup() {
    IMGUI_CHECKVERSION();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    static const std::string iniPath = (ConfigManager::GetAppDataPath() / "imgui.ini").string();
    io.IniFilename = iniPath.c_str();

    DefaultStyle::ApplyGlobalStyle();

    ImGui::SetNextWindowSize(ImVec2(640, 389), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(544, 331), ImGuiCond_Once, ImVec2(0.5f, 0.5f));

    NotificationManager::Initialize();

    MenuManager::Get().AddSection<PlayerSection>(MenuTab::Gameplay);
    MenuManager::Get().AddSection<WorldSection>(MenuTab::Gameplay);
    MenuManager::Get().AddSection<NPCSection>(MenuTab::Entity_Spawner);
    MenuManager::Get().AddSection<ItemSection>(MenuTab::Entity_Spawner);
    MenuManager::Get().AddSection<WeaponEditorSection>(MenuTab::Entity_Spawner);
    MenuManager::Get().AddSection<GuiSection>(MenuTab::Settings);
    MenuManager::Get().AddSection<GraphicsSection>(MenuTab::Settings);

    originalWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
    logger.Log("WndProc hooked successfully");
}

void Gui::Render() {
    static bool previousVisibility = isVisible;

    if (previousVisibility != isVisible) {
        GameHook::SetInputEnabled(!isVisible);
        previousVisibility = isVisible;
    }

    NotificationManager::Update();

    const bool hasNotifications = NotificationManager::IsEnabled() && NotificationManager::HasNotifications();

    if (!isVisible && !hasNotifications && !s_showMismatchPopup) [[likely]] {
        return;
    }

    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = isVisible;

    if (!isVisible) {
        io.WantCaptureMouse = io.WantCaptureKeyboard = io.WantTextInput = false;
    }

    if (isVisible) {
        constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse;
        #ifdef EXPERIMENTAL_VERSION
            constexpr const char* windowTitle = "Half Sword Enhancer v" HSE_VERSION " - Experimental Build";
        #else
            constexpr const char* windowTitle = "Half Sword Enhancer v" HSE_VERSION;
        #endif

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
        if (ImGui::Begin(windowTitle, &isVisible, windowFlags)) {
            MenuManager::Get().RenderMenu();
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    RenderMismatchPopup();

    NotificationManager::Render();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}