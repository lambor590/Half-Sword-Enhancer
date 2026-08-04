#pragma once

#include <atomic>
#include <cstdint>
#include <Windows.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT
ImGui_ImplWin32_WndProcHandlerEx(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, ImGuiIO& io);

class Gui {
private:
    Gui() = default;

public:
    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;

    static Gui& Get() {
        static Gui instance;
        return instance;
    }

    void Init(HWND newWindow) noexcept;
    void Setup();
    void Shutdown() noexcept;
    void PollDiagnostics() noexcept;
    void Render();
    [[nodiscard]] bool IsInCallback() const noexcept { return currentWndProcDepth > 0; }
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static bool IsVisible() noexcept { return isVisible.load(std::memory_order_relaxed); }
    static void ToggleVisibility(const char* reason) noexcept;
    static bool NeedsRendering() noexcept;

private:
    class WndProcLease {
    public:
        WndProcLease() noexcept : dispatchGui(BeginWndProc()) { ++currentWndProcDepth; }
        ~WndProcLease() {
            --currentWndProcDepth;
            EndWndProc();
        }
        [[nodiscard]] bool DispatchGui() const noexcept { return dispatchGui; }

    private:
        bool dispatchGui = false;
    };

    static constexpr std::uint64_t WNDPROC_PHASE_SHIFT = 61;
    static constexpr std::uint64_t WNDPROC_COUNT_MASK = (std::uint64_t{1} << WNDPROC_PHASE_SHIFT) - 1;
    enum class WndProcPhase : std::uint64_t { Running, Draining, Exclusive, Inactive, Installing, Retiring };
    static constexpr std::uint64_t WndProcState(WndProcPhase phase, std::uint64_t count = 0) noexcept {
        return (static_cast<std::uint64_t>(phase) << WNDPROC_PHASE_SHIFT) | count;
    }
    static constexpr WndProcPhase WndProcPhaseOf(std::uint64_t state) noexcept {
        return static_cast<WndProcPhase>(state >> WNDPROC_PHASE_SHIFT);
    }
    static bool BeginWndProc() noexcept;
    static void EndWndProc() noexcept;
    static void TransitionWndProcPhase(WndProcPhase phase) noexcept;
    static void BeginWndProcInstall() noexcept;
    static void QuiesceWndProc() noexcept;
    static void CompleteWndProcUnhook() noexcept;

    HWND window = nullptr;
    bool menuBuilt = false;
    ULONGLONG nextWndProcDiagnosticAt = 0;
    WNDPROC lastObservedWndProc = nullptr;

    static WNDPROC originalWndProc;
    static std::atomic<std::uint64_t> wndProcState;
    static std::atomic<bool> isVisible;
    static std::atomic<bool> pendingParamFlush;
    static thread_local std::uint32_t currentWndProcDepth;
};
