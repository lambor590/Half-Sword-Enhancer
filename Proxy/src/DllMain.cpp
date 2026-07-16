#include <Windows.h>

#include <algorithm>
#include <cstddef>
#include <cwchar>
#include <string_view>
#include <vector>

#include "winmm_exports.generated.h"

extern "C" FARPROC originalFuncs[winmm_exports::kCount]{};
extern "C" volatile LONG proxyState = 0;

namespace {
    constexpr DWORD PROXY_READY_WAIT_MS = 250;
    constexpr LONG PROXY_FAILED = -1;
    constexpr LONG PROXY_READY = 1;
    constexpr std::size_t MAX_PATH_CHARACTERS = 32'768;
    constexpr std::wstring_view MOD_FILENAME = L"HSEnhancer.dll";

    HANDLE proxyReadyEvent = nullptr;

    [[nodiscard]] HMODULE LoadOriginalDll() noexcept {
        wchar_t path[MAX_PATH]{};
        const UINT systemDirectoryLength = GetSystemDirectoryW(path, MAX_PATH);
        constexpr wchar_t DLL_SUFFIX[] = L"\\winmm.dll";
        constexpr std::size_t DLL_SUFFIX_CHARACTERS = sizeof(DLL_SUFFIX) / sizeof(wchar_t);
        if (systemDirectoryLength == 0 || systemDirectoryLength + DLL_SUFFIX_CHARACTERS > MAX_PATH) return nullptr;

        std::wmemcpy(path + systemDirectoryLength, DLL_SUFFIX, DLL_SUFFIX_CHARACTERS);
        return LoadLibraryW(path);
    }

    [[nodiscard]] bool ResolveOriginalFunctions(HMODULE originalDll) noexcept {
        for (std::size_t index = 0; index < winmm_exports::kCount; ++index) {
            originalFuncs[index] = GetProcAddress(originalDll, winmm_exports::kNames[index]);
            if (!originalFuncs[index]) return false;
        }
        return true;
    }

    void PublishProxyState(LONG state) noexcept {
        InterlockedExchange(&proxyState, state);
        if (proxyReadyEvent) SetEvent(proxyReadyEvent);
    }

    [[nodiscard]] std::vector<wchar_t> BuildModPath(HMODULE module) {
        std::vector<wchar_t> path(MAX_PATH);
        while (path.size() <= MAX_PATH_CHARACTERS) {
            const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
            if (length == 0) return {};
            if (length < path.size()) {
                const wchar_t* separator = std::wcsrchr(path.data(), L'\\');
                if (!separator) return {};
                const auto prefixLength = static_cast<std::size_t>(separator - path.data()) + 1;
                path.resize(prefixLength + MOD_FILENAME.size() + 1);
                std::wmemcpy(path.data() + prefixLength, MOD_FILENAME.data(), MOD_FILENAME.size());
                path.back() = L'\0';
                return path;
            }
            if (path.size() == MAX_PATH_CHARACTERS) return {};
            path.resize((std::min)(path.size() * 2, MAX_PATH_CHARACTERS));
        }
        return {};
    }

    [[nodiscard]] bool LoadModDll(HMODULE proxyModule) {
        const auto modPath = BuildModPath(proxyModule);
        HMODULE modDll = modPath.empty() ? nullptr : LoadLibraryW(modPath.data());
        if (modDll) {
            using InitFn = void (*)();
            auto init = reinterpret_cast<InitFn>(GetProcAddress(modDll, "HSE_Initialize"));
            if (init) {
                init();
                return true;
            }
            FreeLibrary(modDll);
            MessageBoxA(
                nullptr, "'HSEnhancer.dll' does not export HSE_Initialize and cannot be started.",
                "Half Sword Enhancer", MB_OK | MB_ICONERROR
            );
            return false;
        }

        MessageBoxA(
            nullptr,
            "Could not find 'HSEnhancer.dll'."
            "\n\nPlease make sure the file is named 'HSEnhancer.dll' and is in the same folder as the game.",
            "Half Sword Enhancer", MB_OK | MB_ICONINFORMATION
        );
        return false;
    }

    DWORD WINAPI BootstrapMod(LPVOID context) {
        const HMODULE originalDll = LoadOriginalDll();
        if (!originalDll || !ResolveOriginalFunctions(originalDll)) {
            if (originalDll) FreeLibrary(originalDll);
            PublishProxyState(PROXY_FAILED);
            MessageBoxA(
                nullptr, "Could not load the original System32 'winmm.dll'.", "Half Sword Enhancer",
                MB_OK | MB_ICONERROR
            );
            return 0;
        }
        PublishProxyState(PROXY_READY);

        (void)LoadModDll(static_cast<HMODULE>(context));
        return 0;
    }
}

extern "C" FARPROC WaitForOriginalFunction(std::size_t index) noexcept {
    if (index >= winmm_exports::kCount) return nullptr;

    LONG state = InterlockedCompareExchange(&proxyState, 0, 0);
    if (state == PROXY_READY) return originalFuncs[index];
    if (state == PROXY_FAILED || !proxyReadyEvent) return nullptr;

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
