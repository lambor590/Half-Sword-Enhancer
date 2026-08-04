#include <Windows.h>
#include <atomic>

#include "Logger.h"
#include "Core/ModRuntimeLifecycle.h"
#include "KeybindManager.h"

static Logger logger{"DllMain"};
static std::atomic<bool> lifecycleStarted{false};

extern "C" __declspec(dllexport) void HSE_Initialize() noexcept {
    logger.Log("HSE_Initialize export entered");
    if (lifecycleStarted.exchange(true, std::memory_order_acq_rel)) return;
    KeybindManager::Initialize();
    logger.Log("Keybind manager initialized; starting runtime lifecycle");
    ModRuntimeLifecycle::StartAsync();
}

extern "C" __declspec(dllexport) void HSE_Shutdown() noexcept {
    if (!lifecycleStarted.exchange(false, std::memory_order_acq_rel)) return;
    if (!ModRuntimeLifecycle::Stop()) lifecycleStarted.store(true, std::memory_order_release);
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) noexcept {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        logger.Log("Half Sword Enhancer initializing...");
#ifdef EXPERIMENTAL_VERSION
        logger.Log("This is a public experimental build for testing purposes.");
#endif
    }

    return TRUE;
}
