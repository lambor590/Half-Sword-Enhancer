#pragma once

#include <Windows.h>

static HMODULE LoadOriginalDLL()
{
    char systemPath[MAX_PATH];
    GetSystemDirectoryA(systemPath, MAX_PATH);
    char originalDllPath[MAX_PATH];
    wsprintfA(originalDllPath, "%s\\dwmapi.dll", systemPath);
    return LoadLibraryA(originalDllPath);
}

static bool LoadModDLL()
{
    HMODULE hModDLL = LoadLibraryA("HSEnhancer.dll");
    if (hModDLL) {
        return true;
    }
    MessageBoxA(NULL,
        "Could not find 'HSEnhancer.dll'."
        "\n\nPlease make sure the file is named 'HSEnhancer.dll' and is in the same folder as the game.",
        "Half Sword Enhancer",
        MB_OK | MB_ICONINFORMATION);
    return false;
}