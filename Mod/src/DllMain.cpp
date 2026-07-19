#include <Windows.h>
#include <atomic>
#include <cstdio>

#include "Logger.h"
#include "Core/ModRuntimeLifecycle.h"
#include "KeybindManager.h"

static Logger logger{"DllMain"};
static std::atomic<bool> lifecycleStarted{false};

extern "C" __declspec(dllexport) void HSE_Initialize() noexcept {
    if (lifecycleStarted.exchange(true, std::memory_order_acq_rel)) return;
    KeybindManager::Initialize();
    ModRuntimeLifecycle::StartAsync();
}

extern "C" __declspec(dllexport) void HSE_Shutdown() noexcept {
    if (!lifecycleStarted.exchange(false, std::memory_order_acq_rel)) return;
    if (!ModRuntimeLifecycle::Stop()) lifecycleStarted.store(true, std::memory_order_release);
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) noexcept {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
#ifdef EXPERIMENTAL_VERSION
        AllocConsole();
        freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        SetWindowText(GetConsoleWindow(), "Half Sword Enhancer - Experimental Build");
        logger.Log("Half Sword Enhancer - Experimental Build initializing...");
        logger.Log("This is a public experimental build for testing purposes.");
#else
        logger.Log("Half Sword Enhancer initializing...");
#endif
    }

    return TRUE;
}
