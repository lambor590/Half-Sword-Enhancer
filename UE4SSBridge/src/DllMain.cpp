#include <Windows.h>

namespace {
    using HseFn = void (*)();

    HMODULE hseModule = nullptr;
    bool loadedByBridge = false;

    HMODULE LoadHse() noexcept {
        if (hseModule) return hseModule;

        hseModule = LoadLibraryA("HSEnhancer.dll");
        loadedByBridge = hseModule != nullptr;
        return hseModule;
    }

    void CallHseExport(const char* name) noexcept {
        if (!hseModule) return;

        auto fn = reinterpret_cast<HseFn>(GetProcAddress(hseModule, name));
        if (fn) fn();
    }
}

extern "C" __declspec(dllexport) void start_mod() noexcept {
    if (!LoadHse()) return;
    CallHseExport("HSE_Initialize");
}

extern "C" __declspec(dllexport) void uninstall_mod() noexcept {
    if (!hseModule) hseModule = GetModuleHandleA("HSEnhancer.dll");
    if (!hseModule) return;

    CallHseExport("HSE_Shutdown");

    if (loadedByBridge) FreeLibrary(hseModule);
    hseModule = nullptr;
    loadedByBridge = false;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) noexcept {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    return TRUE;
}
