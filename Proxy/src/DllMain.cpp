#include <Windows.h>

#include <algorithm>
#include <cstddef>
#include <cwchar>
#include <string>
#include <string_view>

#include "winmm_exports.generated.h"

extern "C" FARPROC originalFuncs[winmm_exports::kCount]{};
extern "C" volatile LONG proxyState = 0;

namespace {
    constexpr DWORD PROXY_READY_WAIT_MS = 250;
    constexpr LONG PROXY_READY = 1;
    constexpr std::size_t MAX_PATH_CHARACTERS = 32'768;
    constexpr std::wstring_view MOD_FILENAME = L"HSEnhancer.dll";
    constexpr wchar_t GAME_FILENAME[] = L"HalfSwordUE5-Win64-Shipping.exe";

    HANDLE proxyReadyEvent = nullptr;

    [[nodiscard]] HMODULE LoadOriginalDll() noexcept {
        wchar_t path[MAX_PATH]{};
        const UINT systemDirectoryLength = GetSystemDirectoryW(path, MAX_PATH);
        constexpr wchar_t DLL_SUFFIX[] = L"\\winmm.dll";
        constexpr std::size_t DLL_SUFFIX_CHARACTERS = sizeof(DLL_SUFFIX) / sizeof(wchar_t);
        if (systemDirectoryLength == 0 || systemDirectoryLength + DLL_SUFFIX_CHARACTERS > MAX_PATH) return nullptr;

        std::wmemcpy(path + systemDirectoryLength, DLL_SUFFIX, DLL_SUFFIX_CHARACTERS);
        return LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    }

    void CacheOriginalFunctions(HMODULE originalDll) noexcept {
        // Wine omits some legacy Windows exports. Their trampolines return a
        // function-appropriate failure value instead of blocking mod startup.
        for (std::size_t index = 0; index < winmm_exports::kCount; ++index) {
            originalFuncs[index] = GetProcAddress(originalDll, winmm_exports::kNames[index]);
        }
    }

    void PublishProxyState(LONG state) noexcept {
        InterlockedExchange(&proxyState, state);
        if (proxyReadyEvent) SetEvent(proxyReadyEvent);
    }

    [[nodiscard]] std::wstring GetModulePath(HMODULE module) {
        std::wstring path(MAX_PATH, L'\0');
        while (true) {
            const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
            if (length == 0) return {};
            if (length < path.size()) {
                path.resize(length);
                return path;
            }
            if (path.size() == MAX_PATH_CHARACTERS) return {};
            path.resize((std::min)(path.size() * 2, MAX_PATH_CHARACTERS));
        }
    }

    void LoadModDll(HMODULE proxyModule) {
        const auto gamePath = GetModulePath(nullptr);
        auto modPath = GetModulePath(proxyModule);
        const auto gameSeparator = gamePath.find_last_of(L'\\');
        const auto proxySeparator = modPath.find_last_of(L'\\');
        // This loader belongs to Half Sword. Other hosts still receive winmm
        // forwarding, but must not initialize the mod or display game dialogs.
        if (gameSeparator == std::wstring::npos || proxySeparator == std::wstring::npos ||
            CompareStringOrdinal(gamePath.c_str() + gameSeparator + 1, -1, GAME_FILENAME, -1, TRUE) != CSTR_EQUAL ||
            CompareStringOrdinal(
                gamePath.c_str(), static_cast<int>(gameSeparator), modPath.c_str(), static_cast<int>(proxySeparator),
                TRUE
            ) != CSTR_EQUAL)
            return;
        modPath.resize(proxySeparator + 1);
        modPath.append(MOD_FILENAME);
        HMODULE modDll = LoadLibraryExW(
            modPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32
        );
        if (modDll) {
            using InitFn = void (*)();
            auto init = reinterpret_cast<InitFn>(GetProcAddress(modDll, "HSE_Initialize"));
            if (init) {
                init();
                return;
            }
            FreeLibrary(modDll);
            MessageBoxA(
                nullptr, "'HSEnhancer.dll' does not export HSE_Initialize and cannot be started.",
                "Half Sword Enhancer", MB_OK | MB_ICONERROR
            );
            return;
        }

        MessageBoxA(
            nullptr,
            "Could not find 'HSEnhancer.dll'."
            "\n\nPlease make sure the file is named 'HSEnhancer.dll' and is in the same folder as the game.",
            "Half Sword Enhancer", MB_OK | MB_ICONINFORMATION
        );
    }

    DWORD WINAPI BootstrapMod(LPVOID context) {
        const HMODULE originalDll = LoadOriginalDll();
        // Some Wine prefixes cannot load their built-in winmm while the native
        // override selects this proxy. Missing functions fail through the safe
        // trampolines, so they do not need to block HSE startup.
        if (originalDll) CacheOriginalFunctions(originalDll);
        PublishProxyState(PROXY_READY);

        LoadModDll(static_cast<HMODULE>(context));
        return 0;
    }
}

extern "C" FARPROC WaitForOriginalFunction(std::size_t index) noexcept {
    LONG state = InterlockedCompareExchange(&proxyState, 0, 0);
    if (state == PROXY_READY) return originalFuncs[index];
    if (!proxyReadyEvent) return nullptr;

    (void)WaitForSingleObject(proxyReadyEvent, PROXY_READY_WAIT_MS);
    state = InterlockedCompareExchange(&proxyState, 0, 0);
    return state == PROXY_READY ? originalFuncs[index] : nullptr;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reasonForCall, LPVOID /*reserved*/) {
    if (reasonForCall != DLL_PROCESS_ATTACH) return TRUE;

    DisableThreadLibraryCalls(module);

    // Loading another DLL from DllMain runs under the loader lock and can deadlock.
    // Bootstrap after attach instead; an unusually early winmm call fails closed in
    // the assembly trampoline if the post-attach worker cannot publish in time.
    proxyReadyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!proxyReadyEvent) return FALSE;

    HANDLE bootstrapThread = CreateThread(nullptr, 0, BootstrapMod, module, 0, nullptr);
    if (!bootstrapThread) {
        CloseHandle(proxyReadyEvent);
        proxyReadyEvent = nullptr;
        return FALSE;
    }
    CloseHandle(bootstrapThread);
    return TRUE;
}
