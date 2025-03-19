#include <iostream>
#include <Windows.h>
#include <string>
#include <thread>
#include <libloaderapi.h>

#include "../resource/resource.h"
#include "Util.h"
#include "Updater.h"
#include "Logger.h"

using namespace Util;

int main() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
    std::cout << R"(
        __  __      ______   _____                        __
       / / / /___ _/ / __/  / ___/      ______  _________/ /
      / /_/ / __ `/ / /_    \__ \ | /| / / __ \/ ___/ __  /
     / __  / /_/ / / __/   ___/ / |/ |/ / /_/ / /  / /_/ /
    /_/ /_/\__,_/_/_/     /____/|__/|__/\____/_/   \__,_/
    )";
    
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    std::cout << R"(
        ______      __
       / ____/___  / /_  ____ _____  ________  _____
      / __/ / __ \/ __ \/ __ `/ __ \/ ___/ _ \/ ___/
     / /___/ / / / / / / /_/ / / / / /__/  __/ /
    /_____/_/ /_/_/ /_/\__,_/_/ /_/\___/\___/_/     
    )" << "\n";
    
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    SetWindowText(GetConsoleWindow(), "Half Sword Enhancer");

    const char* processName = "VersionTest54-Win64-Shipping.exe";
    char tempPath[MAX_PATH], dllPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    GetTempFileNameA(tempPath, "temp", 0, dllPath);

    Logger::info("Made by The Ghost");
    Logger::info("You can change the menu keybinds in the settings");
    
    if (isRunningAsAdmin()) {
        Logger::warn("Detected administrator privileges. Running as administrator can cause permission issues.");
        return 1;
    }
    
    Updater::checkForUpdates();
    
    Logger::info("Searching for Half Sword process...");
    DWORD processId = getProcessIdByName(processName);
    if (processId == 0) {
        Logger::info("Half Sword not found, launching it...");
        ShellExecuteA(0, 0, "steam://rungameid/2527870", 0, 0, SW_SHOW);
        while (!(processId = getProcessIdByName(processName))) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    HANDLE procHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!procHandle) fail("Failed to open handle!");
    Logger::info("Handle opened successfully!");

    HRSRC hResource = FindResourceA(NULL, MAKEINTRESOURCE(IDR_DLL1), RT_RCDATA);
    if (!hResource) fail("Failed to find mod resource!");

    HGLOBAL hLoadedResource = LoadResource(NULL, hResource);
    if (!hLoadedResource) fail("Failed to load mod resource!");

    LPVOID pLockedResource = LockResource(hLoadedResource);
    DWORD dwResourceSize = SizeofResource(NULL, hResource);
    if (!pLockedResource || dwResourceSize == 0) fail("Failed to lock mod resource!");

    HANDLE hFile = CreateFileA(dllPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) fail("Failed to create temporary DLL file.");

    DWORD bytesWritten;
    if (!WriteFile(hFile, pLockedResource, dwResourceSize, &bytesWritten, NULL) || bytesWritten != dwResourceSize) {
        CloseHandle(hFile);
        fail("Failed to write DLL to temporary file.");
    }

#pragma warning(suppress : 6001)
    CloseHandle(hFile);
    Logger::info("DLL written to temporary file successfully.");

    LPVOID remoteMem = VirtualAllocEx(procHandle, nullptr, strlen(dllPath) + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!remoteMem) fail("Failed to allocate memory in Half Sword's process.");
    else if (!WriteProcessMemory(procHandle, remoteMem, dllPath, strlen(dllPath) + 1, NULL)) {
        fail("Failed to write DLL path to Half Sword's process memory.");
    }
    Logger::info("DLL path written to Half Sword's process memory.");

    HANDLE threadHandle = CreateRemoteThread(procHandle, nullptr, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, remoteMem, 0, nullptr);
    if (!threadHandle) {
        fail("Failed to create remote thread.");
    }
    else {
        WaitForSingleObject(threadHandle, INFINITE);
        CloseHandle(threadHandle);
    }
    Logger::info("Remote thread created successfully.");

    DeleteFileA(dllPath);
    Logger::info("Temporary DLL file deleted.");

    if (remoteMem) VirtualFreeEx(procHandle, remoteMem, 0, MEM_RELEASE);
    Logger::info("Memory deallocated successfully.");

    CloseHandle(procHandle);
    Logger::info("Half Sword Enhancer injected successfully! Enjoy!");
    Logger::info("Exiting launcher...");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return 0;
}