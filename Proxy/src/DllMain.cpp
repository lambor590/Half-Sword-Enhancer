#include <Windows.h>
#include <KnownFolders.h>
#include <ShlObj.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cwchar>
#include <string>
#include <string_view>
#include <vector>

#include "winmm_exports.generated.h"

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")

extern "C" FARPROC originalFuncs[winmm_exports::kCount]{};
extern "C" volatile LONG proxyState = 0;

namespace {
    constexpr DWORD PROXY_READY_WAIT_MS = 250;
    constexpr LONG PROXY_READY = 1;
    constexpr std::size_t MAX_PATH_CHARACTERS = 32'768;
    constexpr std::wstring_view MOD_FILENAME = L"HSEnhancer.dll";
    constexpr std::wstring_view APP_DATA_DIRECTORY = L"\\Half Sword Enhancer";
    constexpr std::wstring_view LOG_FILENAME = L"\\logs.log";

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

    [[nodiscard]] std::vector<wchar_t> BuildModPath(HMODULE module) {
        std::vector<wchar_t> path(MAX_PATH);
        while (true) {
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
    }

    [[nodiscard]] std::wstring BuildLogPath() {
        PWSTR roamingAppData = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roamingAppData))) return {};

        std::wstring path{roamingAppData};
        CoTaskMemFree(roamingAppData);
        path.append(APP_DATA_DIRECTORY);
        if (!CreateDirectoryW(path.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return {};
        path.append(LOG_FILENAME);
        return path;
    }

    class BootstrapLogger {
    public:
        BootstrapLogger() {
            const auto path = BuildLogPath();
            if (path.empty()) return;
            file = CreateFileW(
                path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
            );
        }

        ~BootstrapLogger() {
            if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        }

        BootstrapLogger(const BootstrapLogger&) = delete;
        BootstrapLogger& operator=(const BootstrapLogger&) = delete;

        template <typename... Args> void Write(const char* format, Args... args) const noexcept {
            char line[514]{};
            const int messageLength = sprintf_s(line, sizeof(line) - 2, format, args...);
            if (messageLength <= 0) return;
            int lineLength = messageLength;
            line[lineLength++] = '\r';
            line[lineLength++] = '\n';
            line[lineLength] = '\0';
            OutputDebugStringA(line);
            if (file == INVALID_HANDLE_VALUE) return;

            DWORD written = 0;
            (void)WriteFile(file, line, static_cast<DWORD>(lineLength), &written, nullptr);
            (void)FlushFileBuffers(file);
        }

    private:
        HANDLE file = INVALID_HANDLE_VALUE;
    };

    void LoadModDll(HMODULE proxyModule, const BootstrapLogger& bootstrapLogger) {
        const auto modPath = BuildModPath(proxyModule);
        if (modPath.empty()) bootstrapLogger.Write("Failed to resolve the sibling HSEnhancer.dll path");

        SetLastError(ERROR_SUCCESS);
        HMODULE modDll = modPath.empty() ? nullptr : LoadLibraryW(modPath.data());
        const DWORD loadError = GetLastError();
        if (modDll) {
            bootstrapLogger.Write("HSEnhancer.dll loaded");
            using InitFn = void (*)();
            SetLastError(ERROR_SUCCESS);
            auto init = reinterpret_cast<InitFn>(GetProcAddress(modDll, "HSE_Initialize"));
            const DWORD exportError = GetLastError();
            if (init) {
                bootstrapLogger.Write("Calling HSE_Initialize");
                init();
                bootstrapLogger.Write("HSE_Initialize returned successfully");
                return;
            }
            bootstrapLogger.Write("HSE_Initialize export lookup failed (error=%lu)", exportError);
            FreeLibrary(modDll);
            MessageBoxA(
                nullptr, "'HSEnhancer.dll' does not export HSE_Initialize and cannot be started.",
                "Half Sword Enhancer", MB_OK | MB_ICONERROR
            );
            return;
        }

        bootstrapLogger.Write("HSEnhancer.dll load failed (error=%lu)", loadError);

        MessageBoxA(
            nullptr,
            "Could not find 'HSEnhancer.dll'."
            "\n\nPlease make sure the file is named 'HSEnhancer.dll' and is in the same folder as the game.",
            "Half Sword Enhancer", MB_OK | MB_ICONINFORMATION
        );
    }

    DWORD WINAPI BootstrapMod(LPVOID context) {
        try {
            const auto proxyModule = static_cast<HMODULE>(context);
            const BootstrapLogger bootstrapLogger;
            bootstrapLogger.Write("Proxy bootstrap worker entered");

            const HMODULE originalDll = LoadOriginalDll();
            // Some Wine prefixes cannot load their built-in winmm while the native
            // override selects this proxy. Missing functions fail through the safe
            // trampolines, so they do not need to block HSE startup.
            if (originalDll) CacheOriginalFunctions(originalDll);
            PublishProxyState(PROXY_READY);

            LoadModDll(proxyModule, bootstrapLogger);
        } catch (...) {
            OutputDebugStringA("Half Sword Enhancer proxy bootstrap terminated with an exception\n");
            PublishProxyState(PROXY_READY);
        }
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
