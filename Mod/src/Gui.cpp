#include "Gui.h"
#include "ConfigManager.h"
#include "Core/ModContext.h"
#include "DefaultStyle.h"
#include "Menu/MenuManager.h"
#include "Menu/Sections/Equipment/ArmorEditorSection.h"
#include "Menu/Sections/Equipment/LoadoutManagerSection.h"
#include "Menu/Sections/Equipment/WeaponEditorSection.h"
#include "Menu/Sections/Player/PlayerAbilitiesSection.h"
#include "Menu/Sections/Player/PlayerEditorSection.h"
#include "Menu/Sections/Player/SaveEditorSection.h"
#include "Menu/Sections/Settings/AssetOverridesSection.h"
#include "Menu/Sections/Settings/GraphicsSection.h"
#include "Menu/Sections/Settings/GuiSection.h"
#include "Menu/Sections/Spawner/ItemSpawnerSection.h"
#include "Menu/Sections/Spawner/NPCEditorSection.h"
#include "Menu/Sections/World/AIDirectorSection.h"
#include "Menu/Sections/World/FreeCameraSection.h"
#include "Menu/Sections/World/MapLoaderSection.h"
#include "Menu/Sections/World/SkyEditorSection.h"
#include "Menu/Sections/World/WorldActionsSection.h"
#include "Menu/Sections/World/WorldEditorSection.h"
#include "KeybindManager.h"
#include "NotificationManager.h"
#include "Version.h"
#include "Utils/GameBuildInfo.h"

#include <bit>

WNDPROC Gui::originalWndProc = nullptr;
std::atomic<bool> Gui::isVisible = true;

void Gui::Init(HWND newWindow) noexcept {
    window = newWindow;
}

bool Gui::IsInitialized() const noexcept {
    return window != nullptr;
}

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
        if (ImGui::BeginPopupModal(
                "##version_mismatch", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings
            )) {
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
    if (KeybindManager::ProcessRebindEvent(msg, wParam)) return true;

    if (!isVisible) [[likely]] {
        if (KeybindManager::ProcessKeyEvent(msg, wParam)) return true;
        return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
    }

    if (msg == WM_INPUT) [[likely]]
        return true;

    static thread_local ImGuiIO* cachedIO = nullptr;
    if (!cachedIO) [[unlikely]] {
        cachedIO = &ImGui::GetIO();
    }
    ImGuiIO& io = *cachedIO;

    if (!io.WantTextInput && KeybindManager::ProcessKeyEvent(msg, wParam)) return true;

    ImGui_ImplWin32_WndProcHandlerEx(hWnd, msg, wParam, lParam, io);

    if (io.WantCaptureMouse && (msg == WM_SETCURSOR || (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST))) return true;
    if ((io.WantTextInput || io.WantCaptureKeyboard) && msg >= WM_KEYFIRST && msg <= WM_KEYLAST) return true;

    return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
}

void Gui::Setup() {
    if (setupComplete) return;

    IMGUI_CHECKVERSION();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    static const std::string INI_PATH = (ConfigManager::GetAppDataPath() / "imgui.ini").string();
    io.IniFilename = INI_PATH.c_str();

    DefaultStyle::ApplyGlobalStyle();

    ImGui::SetNextWindowSize(ImVec2(640, 389), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(544, 331), ImGuiCond_Once, ImVec2(0.5f, 0.5f));

    NotificationManager::Initialize();

    auto& ctx = ModContext::Get();
    auto& menu = MenuManager::Get();
    menu.AddSection<GuiSection>(ctx);
    menu.AddSection<AssetOverridesSection>(ctx);
    menu.AddSection<GraphicsSection>(ctx);
    menu.AddSection<AIDirectorSection>(ctx);
    menu.AddSection<WorldActionsSection>(ctx);
    menu.AddSection<FreeCameraSection>(ctx);
    menu.AddSection<WorldEditorSection>(ctx);
    menu.AddSection<SkyEditorSection>(ctx);
    menu.AddSection<MapLoaderSection>(ctx);
    menu.AddSection<PlayerAbilitiesSection>(ctx);
    menu.AddSection<PlayerEditorSection>(ctx);
    menu.AddSection<SaveEditorSection>(ctx);
    menu.AddSection<NPCEditorSection>(ctx);
    menu.AddSection<ItemSpawnerSection>(ctx);
    menu.AddSection<ArmorEditorSection>(ctx);
    menu.AddSection<WeaponEditorSection>(ctx);
    menu.AddSection<LoadoutManagerSection>(ctx);

    const auto wndProc = static_cast<WNDPROC>(WndProc);
    originalWndProc = std::bit_cast<WNDPROC>(SetWindowLongPtr(window, GWLP_WNDPROC, std::bit_cast<LONG_PTR>(wndProc)));
    setupComplete = true;
}

bool Gui::NeedsRendering() noexcept {
    if (isVisible.load(std::memory_order_relaxed)) [[unlikely]] {
        (void)NotificationManager::Update();
        return true;
    }

    if (NotificationManager::HasNotifications() && NotificationManager::Update()) [[unlikely]]
        return true;

    if (s_showMismatchPopup) [[unlikely]]
        return true;
    if (!s_mismatchDismissed && GameBuildInfo::Get().mismatchDetected.load(std::memory_order_relaxed)) [[unlikely]]
        return true;

    return false;
}

void Gui::Render() {
    const bool visible = isVisible.load(std::memory_order_relaxed);

    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = visible;

    if (visible) {
        constexpr ImGuiWindowFlags WINDOW_FLAGS =
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
#ifndef HSE_WINDOW_TITLE
#ifdef EXPERIMENTAL_VERSION
#define HSE_WINDOW_TITLE "Half Sword Enhancer v" HSE_VERSION " - Experimental Build###HSEMain"
#else
#define HSE_WINDOW_TITLE "Half Sword Enhancer v" HSE_VERSION "###HSEMain"
#endif
#endif
        constexpr const char* WINDOW_TITLE = HSE_WINDOW_TITLE;

        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
        bool showWindow = true;
        if (ImGui::Begin(WINDOW_TITLE, &showWindow, WINDOW_FLAGS)) {
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
}
