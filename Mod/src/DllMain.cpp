#include <Windows.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <future>

#include "Logger.h"
#include "MemoryUtils.h"
#include "Hooks/GameHook.h"
#include "Hooks/DirectXHook.h"
#include "Render/Renderer.h"
#include "GlobalDefinitions.h"

static Logger logger{ "DllMain" };
static Renderer renderer;

static void OpenDebugTerminal()
{
    std::fstream terminalEnableFile("enhancer_enable_terminal.txt", std::fstream::in);
    if (terminalEnableFile.is_open())
    {
        AllocConsole();
        freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        SetWindowText(GetConsoleWindow(), "Half Sword Enhancer");
        terminalEnableFile.close();
    }
}

static DWORD WINAPI DXHookThread(LPVOID)
{
    g_DirectXHook = new DirectXHook(&renderer);
    g_DirectXHook->Hook();
    return 0;
}

static DWORD WINAPI GameHookThread(LPVOID)
{
    while (!GameHook::Get().Hook())
    {
        Sleep(1000);
    }
    return 0;
}

static void Cleanup()
{
    logger.Log("Cleaning up resources...");
    std::promise<void> cleanupPromise;
    auto cleanupFuture = cleanupPromise.get_future();
    
    std::thread cleanupThread([&cleanupPromise]() {
        renderer.Cleanup();
        GameHook::Get().Unhook();
        cleanupPromise.set_value();
    });
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        OpenDebugTerminal();
        CreateThread(nullptr, 0, DXHookThread, nullptr, 0, nullptr);
        CreateThread(nullptr, 0, GameHookThread, nullptr, 0, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH) Cleanup();
    
    return TRUE;
}