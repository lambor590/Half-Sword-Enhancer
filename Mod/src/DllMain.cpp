#include <Windows.h>
#include <fstream>
#include <future>

#include "Logger.h"
#include "MemoryUtils.h"
#include "Hooks/GameHook.h"
#include "Hooks/DirectXHook.h"
#include "Render/Renderer.h"
#include "GlobalDefinitions.h"
#include "KeybindManager.h"

static Logger logger{ "DllMain" };
static Renderer renderer;
static DirectXHook dxHook(&renderer);

static inline bool CheckTerminalFile() noexcept {
    std::fstream terminalFile("enhancer_enable_terminal.txt", std::fstream::in);
    return terminalFile.is_open();
}

static void OpenDebugTerminal() noexcept
{
    if (CheckTerminalFile()) {
        AllocConsole();
        freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        #ifdef DEV_VERSION
            SetWindowText(GetConsoleWindow(), "Half Sword Enhancer - Internal Build");
        #else
            SetWindowText(GetConsoleWindow(), "Half Sword Enhancer");
        #endif
    }
}

static DWORD WINAPI DXHookThread(LPVOID) noexcept
{
    g_DirectXHook = &dxHook;
    g_DirectXHook->Hook();
    return 0;
}

static DWORD WINAPI GameHookThread(LPVOID) noexcept
{
    GameHook::Get().Hook();
    return 0;
}

static void Cleanup() noexcept
{
    logger.Log("Cleaning up resources...");
    std::promise<void> cleanupPromise;
    auto cleanupFuture = cleanupPromise.get_future();
    
    std::thread cleanupThread([&cleanupPromise]() noexcept {
        renderer.Cleanup();
        GameHook::Get().Unhook();
        cleanupPromise.set_value();
    });
    
    if (cleanupFuture.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
        logger.Log("Cleanup timed out, terminating forcefully");
        cleanupThread.detach();
    } else {
        cleanupThread.join();
    }
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID) noexcept
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(module);
        OpenDebugTerminal();
        #ifdef DEV_VERSION
            logger.Log("Half Sword Enhancer - Internal Build initializing...");
            logger.Log("This is an internal development build for testing purposes.");
        #else
            logger.Log("Half Sword Enhancer initializing...");
        #endif
        KeybindManager::Initialize();
        CreateThread(nullptr, 0, DXHookThread, nullptr, 0, nullptr);
        CreateThread(nullptr, 0, GameHookThread, nullptr, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        Cleanup();
        break;
    }
    
    return TRUE;
}