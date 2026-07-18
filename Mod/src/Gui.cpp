#include "Gui.h"
#include "ConfigManager.h"
#include "Core/ModContext.h"
#include "DefaultStyle.h"
#include "Hooks/GameHook.h"
#include "Menu/Keybind.h"
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

#include <algorithm>
#include <bit>

WNDPROC Gui::originalWndProc = nullptr;
std::atomic<std::uint64_t> Gui::wndProcState{Gui::WndProcState(Gui::WndProcPhase::Inactive)};
std::atomic<bool> Gui::isVisible = true;
std::atomic<bool> Gui::pendingParamFlush = false;
thread_local std::uint32_t Gui::currentWndProcDepth = 0;

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
    std::atomic<bool> s_mismatchNeedsInput{false};

    void RenderMismatchPopup() {
        if (!s_mismatchDismissed && !s_showMismatchPopup &&
            GameBuildInfo::Get().mismatchDetected.load(std::memory_order_relaxed)) {
            s_showMismatchPopup = true;
            s_popupOpened = false;
            s_mismatchNeedsInput.store(true, std::memory_order_release);
            if (!Gui::IsVisible()) Gui::ToggleVisibility();
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
            ImGui::Text("This game version may not be supported");
            ImGui::Spacing();
            ImGui::Text("Supported game: %s", HSE_TARGET_BUILD);
            ImGui::Text("Your game: %s", info.buildVersion.c_str());
            ImGui::Spacing();
            ImGui::Text("Some features may be unavailable until Half Sword Enhancer is updated.");
            ImGui::Spacing();
            if (ImGui::Button("Continue", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
                s_showMismatchPopup = false;
                s_mismatchDismissed = true;
                s_mismatchNeedsInput.store(false, std::memory_order_release);
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}

void Gui::ToggleVisibility() noexcept {
    const bool wasVisible = isVisible.load(std::memory_order_relaxed);
    if (wasVisible && s_mismatchNeedsInput.load(std::memory_order_acquire)) return;
    if (wasVisible) {
        pendingParamFlush.store(true, std::memory_order_release);
        KeybindManager::CancelRebind();
    } else {
        (void)GameHook::QueueAction([](const RuntimeContextSnapshot&) {});
    }
    isVisible.store(!wasVisible, std::memory_order_relaxed);
}

bool Gui::BeginWndProc() noexcept {
    auto callback = wndProcState.load(std::memory_order_acquire);
    for (;;) {
        if ((callback & WNDPROC_COUNT_MASK) == WNDPROC_COUNT_MASK) {
            wndProcState.wait(callback, std::memory_order_acquire);
            callback = wndProcState.load(std::memory_order_acquire);
            continue;
        }
        if (wndProcState
                .compare_exchange_weak(callback, callback + 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
            callback += 1;
            auto phase = WndProcPhaseOf(callback);
            while (phase == WndProcPhase::Exclusive || phase == WndProcPhase::Installing) {
                wndProcState.wait(callback, std::memory_order_acquire);
                callback = wndProcState.load(std::memory_order_acquire);
                phase = WndProcPhaseOf(callback);
            }
            return phase == WndProcPhase::Running;
        }
    }
}

void Gui::EndWndProc() noexcept {
    wndProcState.fetch_sub(1, std::memory_order_acq_rel);
    wndProcState.notify_all();
}

void Gui::TransitionWndProcPhase(WndProcPhase phase) noexcept {
    auto callback = wndProcState.load(std::memory_order_acquire);
    for (;;) {
        const auto updated = WndProcState(phase, callback & WNDPROC_COUNT_MASK);
        if (wndProcState
                .compare_exchange_weak(callback, updated, std::memory_order_acq_rel, std::memory_order_acquire)) {
            wndProcState.notify_all();
            return;
        }
    }
}

void Gui::BeginWndProcInstall() noexcept {
    auto callback = wndProcState.load(std::memory_order_acquire);
    for (;;) {
        if (WndProcPhaseOf(callback) == WndProcPhase::Inactive && (callback & WNDPROC_COUNT_MASK) == 0 &&
            wndProcState.compare_exchange_weak(
                callback, WndProcState(WndProcPhase::Installing), std::memory_order_acq_rel, std::memory_order_acquire
            )) {
            return;
        }
        wndProcState.wait(callback, std::memory_order_acquire);
        callback = wndProcState.load(std::memory_order_acquire);
    }
}

void Gui::CompleteWndProcInstall() noexcept {
    TransitionWndProcPhase(WndProcPhase::Running);
}

void Gui::QuiesceWndProc() noexcept {
    auto callback = wndProcState.load(std::memory_order_acquire);
    for (;;) {
        auto phase = WndProcPhaseOf(callback);
        if (phase == WndProcPhase::Exclusive || phase == WndProcPhase::Inactive) return;
        if (phase == WndProcPhase::Running) {
            const auto draining = WndProcState(WndProcPhase::Draining, callback & WNDPROC_COUNT_MASK);
            if (!wndProcState
                     .compare_exchange_weak(callback, draining, std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            callback = draining;
            phase = WndProcPhase::Draining;
        }
        if (phase == WndProcPhase::Installing) {
            const auto exclusive = WndProcState(WndProcPhase::Exclusive, callback & WNDPROC_COUNT_MASK);
            if (wndProcState
                    .compare_exchange_weak(callback, exclusive, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return;
            }
            continue;
        }
        if (phase == WndProcPhase::Draining && (callback & WNDPROC_COUNT_MASK) == 0) {
            if (wndProcState.compare_exchange_weak(
                    callback, WndProcState(WndProcPhase::Exclusive), std::memory_order_acq_rel,
                    std::memory_order_acquire
                )) {
                return;
            }
            continue;
        }
        wndProcState.wait(callback, std::memory_order_acquire);
        callback = wndProcState.load(std::memory_order_acquire);
    }
}

void Gui::CompleteWndProcUnhook() noexcept {
    TransitionWndProcPhase(WndProcPhase::Retiring);

    auto callback = wndProcState.load(std::memory_order_acquire);
    for (;;) {
        if (WndProcPhaseOf(callback) == WndProcPhase::Retiring && (callback & WNDPROC_COUNT_MASK) == 0 &&
            wndProcState.compare_exchange_weak(
                callback, WndProcState(WndProcPhase::Inactive), std::memory_order_acq_rel, std::memory_order_acquire
            )) {
            wndProcState.notify_all();
            return;
        }
        wndProcState.wait(callback, std::memory_order_acquire);
        callback = wndProcState.load(std::memory_order_acquire);
    }
}

LRESULT CALLBACK Gui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    const WndProcLease callback;
    const auto original = originalWndProc;
    if (!callback.DispatchGui()) {
        return original ? CallWindowProc(original, hWnd, msg, wParam, lParam)
                        : DefWindowProc(hWnd, msg, wParam, lParam);
    }

    if (KeybindManager::ProcessRebindEvent(msg, wParam, lParam)) return true;

    if (!isVisible) [[likely]] {
        if (KeybindManager::ProcessKeyEvent(msg, wParam, lParam)) return true;
        return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
    }

    if (msg == WM_INPUT) [[likely]]
        return true;

    if (KeybindManager::ProcessToggleGuiEvent(msg, wParam, lParam)) return true;

    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplWin32_WndProcHandlerEx(hWnd, msg, wParam, lParam, io);

    const bool keyboardPress = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
    const bool searchShortcut = keyboardPress && wParam == 'K' && (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if ((!keyboardPress || (!io.WantTextInput && !searchShortcut)) &&
        KeybindManager::ProcessKeyEvent(msg, wParam, lParam))
        return true;

    if (io.WantCaptureMouse && (msg == WM_SETCURSOR || (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST))) return true;
    if ((io.WantTextInput || io.WantCaptureKeyboard) && msg >= WM_KEYFIRST && msg <= WM_KEYLAST) return true;

    return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
}

void Gui::Setup() {
    if (setupComplete) return;
    const bool rehydratingRuntime = menuBuilt;

    IMGUI_CHECKVERSION();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    static const std::string INI_PATH = (ConfigManager::GetAppDataPath() / "imgui.ini").string();
    io.IniFilename = INI_PATH.c_str();

    DefaultStyle::ApplyGlobalStyle();

    NotificationManager::Initialize();

    if (!menuBuilt) {
        auto& ctx = ModContext::Get();
        auto& menu = MenuManager::Get();
        menu.AddSection<PlayerAbilitiesSection>(ctx);
        menu.AddSection<PlayerEditorSection>(ctx);
        menu.AddSection<SaveEditorSection>(ctx);
        menu.AddSection<WorldActionsSection>(ctx);
        menu.AddSection<MapLoaderSection>(ctx);
        menu.AddSection<AIDirectorSection>(ctx);
        menu.AddSection<FreeCameraSection>(ctx);
        menu.AddSection<WorldEditorSection>(ctx);
        menu.AddSection<SkyEditorSection>(ctx);
        menu.AddSection<ItemSpawnerSection>(ctx);
        menu.AddSection<NPCEditorSection>(ctx);
        menu.AddSection<LoadoutManagerSection>(ctx);
        menu.AddSection<WeaponEditorSection>(ctx);
        menu.AddSection<ArmorEditorSection>(ctx);
        menu.AddSection<GuiSection>(ctx);
        menu.AddSection<GraphicsSection>(ctx);
        menu.AddSection<AssetOverridesSection>(ctx);
        menu.LoadNavigationState();
        menuBuilt = true;
    }
    if (rehydratingRuntime) {
        KeybindRuntime::OnRuntimeStart();
        MapLoaderSection::OnRuntimeStart();
    }

    BeginWndProcInstall();
    const auto wndProc = static_cast<WNDPROC>(WndProc);
    originalWndProc = std::bit_cast<WNDPROC>(GetWindowLongPtr(window, GWLP_WNDPROC));
    SetWindowLongPtr(window, GWLP_WNDPROC, std::bit_cast<LONG_PTR>(wndProc));
    CompleteWndProcInstall();
    setupComplete = true;
}

void Gui::Shutdown() noexcept {
    QuiesceWndProc();
    if (setupComplete && window && originalWndProc) {
        const auto installedWndProc = std::bit_cast<WNDPROC>(GetWindowLongPtr(window, GWLP_WNDPROC));
        if (installedWndProc == static_cast<WNDPROC>(WndProc)) {
            SetWindowLongPtr(window, GWLP_WNDPROC, std::bit_cast<LONG_PTR>(originalWndProc));
        }
    }
    CompleteWndProcUnhook();
    window = nullptr;
    setupComplete = false;
}

bool Gui::NeedsRendering() noexcept {
    ConfigManager::Get().FlushIfDue();

    if (pendingParamFlush.load(std::memory_order_acquire)) return true;

    const bool hasNotifications = NotificationManager::Update();
    if (isVisible.load(std::memory_order_relaxed) || hasNotifications) [[unlikely]] return true;

    if (s_showMismatchPopup) [[unlikely]]
        return true;
    if (!s_mismatchDismissed && GameBuildInfo::Get().mismatchDetected.load(std::memory_order_relaxed)) [[unlikely]]
        return true;

    return false;
}

void Gui::Render() {
    if (pendingParamFlush.exchange(false, std::memory_order_acq_rel)) {
        KeybindRuntime::FlushPendingParamChanges();
    }
    const bool visible = isVisible.load(std::memory_order_relaxed);

    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = visible;

    if (visible) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        constexpr float VIEWPORT_EDGE_MARGIN = 12.0f;
        const ImVec2 maximumWindowSize{
            (std::max)(1.0f, viewport->WorkSize.x - VIEWPORT_EDGE_MARGIN * 2.0f),
            (std::max)(1.0f, viewport->WorkSize.y - VIEWPORT_EDGE_MARGIN * 2.0f),
        };
        const ImVec2 defaultWindowSize{
            (std::min)(760.0f, maximumWindowSize.x), (std::min)(620.0f, maximumWindowSize.y)
        };
        const ImVec2 minimumWindowSize{
            (std::min)(560.0f, maximumWindowSize.x), (std::min)(420.0f, maximumWindowSize.y)
        };

        ImGui::SetNextWindowSize(defaultWindowSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSizeConstraints(minimumWindowSize, maximumWindowSize);

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

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
        bool showWindow = true;
        const bool windowExpanded = ImGui::Begin(WINDOW_TITLE, &showWindow, WINDOW_FLAGS);

        if (windowExpanded) {
            MenuManager::Get().RenderMenu();
        }
        ImGui::End();
        ImGui::PopStyleVar();
        if (!showWindow) ToggleVisibility();
    } else {
        io.WantCaptureMouse = io.WantCaptureKeyboard = io.WantTextInput = false;
    }

    RenderMismatchPopup();

    NotificationManager::Render();

    ImGui::Render();
}
