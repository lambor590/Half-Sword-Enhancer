#include <Windows.h>

namespace {
    using HseFn = void (*)();

    HMODULE hseModule = nullptr;

    void CallHseExport(HMODULE module, const char* name) noexcept {
        auto fn = reinterpret_cast<HseFn>(GetProcAddress(module, name));
        if (fn) fn();
    }
}

extern "C" __declspec(dllexport) void start_mod() noexcept {
    if (!hseModule) hseModule = LoadLibraryA("HSEnhancer.dll");
    if (hseModule) CallHseExport(hseModule, "HSE_Initialize");
}

extern "C" __declspec(dllexport) void uninstall_mod() noexcept {
    HMODULE module = hseModule ? hseModule : GetModuleHandleA("HSEnhancer.dll");
    if (!module) return;

    CallHseExport(module, "HSE_Shutdown");

    if (hseModule) FreeLibrary(hseModule);
    hseModule = nullptr;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) noexcept {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    return TRUE;
}
