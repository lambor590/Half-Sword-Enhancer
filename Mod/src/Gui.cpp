#include "Gui.h"
#include "Menu/Sections/Gameplay/PlayerSection.h"
#include "Menu/Sections/Gameplay/WorldSection.h"
#include "Menu/Sections/Entity_Spawner/NPCEditorSection.h"
#include "Menu/Sections/Entity_Spawner/ItemSection.h"
#include "Menu/Sections/Entity_Spawner/WeaponEditorSection.h"
#include "Menu/Sections/Entity_Spawner/ArmorEditorSection.h"
#include "Menu/Sections/Loadout_Manager/EquipmentManagerSection.h"
#include "Menu/Sections/Settings/GraphicsSection.h"
#include "Menu/Sections/Settings/GuiSection.h"
#include "KeybindManager.h"
#include "NotificationManager.h"
#include "Version.h"
#include "Utils/GameBuildInfo.h"

WNDPROC Gui::originalWndProc = nullptr;
std::atomic<bool> Gui::isVisible = true;

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
        if (ImGui::BeginPopupModal("##version_mismatch", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings)) {
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

    if (msg == WM_INPUT) [[likely]]
        return true;

    static thread_local ImGuiIO* cachedIO = nullptr;
    if (!cachedIO) [[unlikely]] {
        cachedIO = &ImGui::GetIO();
    }
    ImGuiIO& io = *cachedIO;

    if (!io.WantTextInput && KeybindManager::ProcessKeyEvent(msg, wParam))
        return true;

    ImGui_ImplWin32_WndProcHandlerEx(hWnd, msg, wParam, lParam, io);

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
    MenuManager::Get().AddSection<ItemSection>(MenuTab::Entity_Spawner);
    MenuManager::Get().AddSection<NPCEditorSection>(MenuTab::Entity_Spawner);
    MenuManager::Get().AddSection<WeaponEditorSection>(MenuTab::Entity_Spawner);
    MenuManager::Get().AddSection<ArmorEditorSection>(MenuTab::Entity_Spawner);
    MenuManager::Get().AddSection<EquipmentManagerSection>(MenuTab::Loadout_Manager);
    MenuManager::Get().AddSection<GuiSection>(MenuTab::Settings);
    MenuManager::Get().AddSection<GraphicsSection>(MenuTab::Settings);

    originalWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
    logger.Log("WndProc hooked successfully");
}

bool Gui::NeedsRendering() noexcept {
    if (isVisible.load(std::memory_order_relaxed)) [[unlikely]] {
        NotificationManager::Update();
        return true;
    }

    const bool hasNotifications = NotificationManager::IsEnabled() && NotificationManager::HasNotifications();
    if (hasNotifications) [[unlikely]] {
        NotificationManager::Update();
        return true;
    }

    if (s_showMismatchPopup) [[unlikely]] return true;
    if (!s_mismatchDismissed && GameBuildInfo::Get().mismatchDetected.load(std::memory_order_relaxed)) [[unlikely]]
        return true;

    return false;
}

void Gui::Render() {
    const bool visible = isVisible.load(std::memory_order_relaxed);

    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = visible;

    if (visible) {
        constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse;
        #ifdef EXPERIMENTAL_VERSION
            constexpr const char* windowTitle = "Half Sword Enhancer v" HSE_VERSION " - Experimental Build###HSEMain";
        #else
            constexpr const char* windowTitle = "Half Sword Enhancer v" HSE_VERSION "###HSEMain";
        #endif

        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
        bool showWindow = true;
        if (ImGui::Begin(windowTitle, &showWindow, windowFlags)) {
            MenuManager::Get().RenderMenu();
        }
        ImGui::End();
        ImGui::PopStyleVar();
        if (!showWindow) isVisible.store(false, std::memory_order_relaxed);
    } else {
        io.WantCaptureMouse = io.WantCaptureKeyboard = io.WantTextInput = false;
    }

    RenderMismatchPopup();

    NotificationManager::Render();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}