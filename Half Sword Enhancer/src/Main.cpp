#include "../resource/resource.h"
#include "Util.h"
#include "Updater.h"
#include "Logger.h"

using namespace Util;

int main() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    
    showBanner(hConsole);
    
    std::string windowTitle = "Half Sword Enhancer ";
    windowTitle += Updater::getLocalVersion();
    SetWindowText(GetConsoleWindow(), windowTitle.c_str());
    
    const std::string& appDataPath = getAppDataPath();
    const char* processName = "VersionTest54-Win64-Shipping.exe";
    std::string dllPath = appDataPath + "\\temp_enhancer.dll";
    
    Logger::info("Made by The Ghost");
    
    if (isRunningAsAdmin())
        Logger::warn("Detected administrator privileges. Running as administrator can cause permission issues.");
    
    Updater::checkForUpdates();
    
    DWORD processId = findOrLaunchGame(processName);
    
    if (!WaitForGameWindow(processId)) {
        fail("Could not find game window. Aborting injection.");
    }
    
    ScopedHandle procHandle(OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD, FALSE, processId));
    if (!procHandle.isValid()) {
        fail("Failed to open handle! Please run the launcher as administrator or check your antivirus.");
    }
    Logger::info("Handle opened successfully!");
    
    extractDllToTempFile(dllPath, IDR_DLL1);
    injectDll(procHandle.get(), dllPath);
    
    DeleteFileA(dllPath.c_str());
    Logger::info("Temporary DLL file deleted.");
    
    Logger::info("Half Sword Enhancer injected successfully! Enjoy!");
    Logger::info("Exiting launcher in 3 seconds...");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    return 0;
}