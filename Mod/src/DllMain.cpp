#include <Windows.h>
#include <atomic>
#include <cstdio>

#include "Logger.h"
#include "Core/ModRuntimeLifecycle.h"
#include "KeybindManager.h"

static Logger logger{"DllMain"};
static std::atomic<bool> lifecycleStarted{false};

#ifdef EXPERIMENTAL_VERSION
static void OpenDebugTerminal() noexcept {
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    SetWindowText(GetConsoleWindow(), "Half Sword Enhancer - Experimental Build");
}
#endif

extern "C" __declspec(dllexport) void HSE_Initialize() noexcept {
    lifecycleStarted.store(true, std::memory_order_release);
    ModRuntimeLifecycle::StartAsync();
}

extern "C" __declspec(dllexport) void HSE_Shutdown() noexcept {
    if (!lifecycleStarted.exchange(false, std::memory_order_acq_rel)) return;
    ModRuntimeLifecycle::Stop();
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID reserved) noexcept {
    switch (reason) {
        case DLL_PROCESS_ATTACH: DisableThreadLibraryCalls(module);
#ifdef EXPERIMENTAL_VERSION
            OpenDebugTerminal();
            logger.Log("Half Sword Enhancer - Experimental Build initializing...");
            logger.Log("This is a public experimental build for testing purposes.");
#else
            logger.Log("Half Sword Enhancer initializing...");
#endif
            KeybindManager::Initialize();
            break;
        case DLL_PROCESS_DETACH:
            // reserved is non-null during process termination; avoid loader-lock cleanup work there.
            if (!reserved) HSE_Shutdown();
            break;
    }

    return TRUE;
}
