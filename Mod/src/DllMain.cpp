#include <Windows.h>

#include "DirectXHook.h"
#include "Logger.h"
#include "MemoryUtils.h"
#include "GameHook.h"

static Logger logger{ "DllMain" };

static void OpenDebugTerminal()
{
    std::fstream terminalEnableFile;
    terminalEnableFile.open("enhancer_enable_terminal.txt", std::fstream::in);
    if (terminalEnableFile.is_open())
    {
        if (AllocConsole())
        {
            freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
            SetWindowText(GetConsoleWindow(), "Half Sword Enhancer");
        }
        terminalEnableFile.close();
    }
}

static DWORD WINAPI DXHookThread(LPVOID lpParam)
{
    static Renderer renderer;
    static DirectXHook dxHook(&renderer);
    dxHook.Hook();
    return 0;
}

static DWORD WINAPI GameHookThread(LPVOID lpParam)
{
    static GameHook gameHook;
    while (!gameHook.Hook())
    {
        Sleep(1000);
    }
    return 0;
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID)
{

    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        OpenDebugTerminal();
        CreateThread(0, 0, &DXHookThread, 0, 0, NULL);
        CreateThread(0, 0, &GameHookThread, 0, 0, NULL);
    }

    return 1;
}