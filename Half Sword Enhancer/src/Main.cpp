#include "../resource/resource.h"
#include "Util.h"
#include "Updater.h"
#include "Logger.h"

using namespace Util;

static bool performDllInjection(const std::string& dllPath, const char* processName) {
    try {
        DWORD processId = findOrLaunchGame(processName);
        if (processId == 0) {
            Logger::error("Failed to find or launch game");
            return false;
        }

        if (!WaitForGameWindow(processId))
            fail("Could not find game window. Aborting injection.");

        extractDllToTempFile(dllPath, IDR_DLL1);
        bool success = injectDll(processId, dllPath);

        DeleteFileA(dllPath.c_str());
        Logger::info("Temporary DLL file deleted.");

        if (!success)
            fail("DLL injection failed. Please try again with the game freshly launched.");

        Logger::info("Half Sword Enhancer injected successfully! Enjoy!");
        return true;
    }
    catch (const std::exception& e) {
        Logger::error(std::string("Error during injection: ") + e.what());
        showError(e.what());
        return false;
    }
}

int main() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    const int RED = FOREGROUND_RED | FOREGROUND_INTENSITY;
    const int YELLOW = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    const int WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

    SetConsoleTextAttribute(hConsole, RED);
    std::cout << R"(
        __  __      ______   _____                        __
       / / / /___ _/ / __/  / ___/      ______  _________/ /
      / /_/ / __ `/ / /_    \__ \ | /| / / __ \/ ___/ __  /
     / __  / /_/ / / __/   ___/ / |/ |/ / /_/ / /  / /_/ /
    /_/ /_/\__,_/_/_/     /____/|__/|__/\____/_/   \__,_/
    )";

    SetConsoleTextAttribute(hConsole, YELLOW);
    std::cout << R"(
        ______      __
       / ____/___  / /_  ____ _____  ________  _____
      / __/ / __ \/ __ \/ __ `/ __ \/ ___/ _ \/ ___/
     / /___/ / / / / / / /_/ / / / / /__/  __/ /
    /_____/_/ /_/_/ /_/\__,_/_/ /_/\___/\___/_/     
    )" << "\n";

    SetConsoleTextAttribute(hConsole, WHITE);

    try {
        SetWindowText(GetConsoleWindow(),
            ("Half Sword Enhancer " + Updater::getLocalVersion()).c_str());

        const std::string dllPath = getAppDataPath() + "\\HS-Enhancer.dll";
        const char* processName = "VersionTest54-Win64-Shipping.exe";

        Logger::info("Made by The Ghost");

        if (isRunningAsAdmin())
            Logger::warn("Detected administrator privileges. Running as administrator can cause permission issues.");

        Updater::checkForUpdates();
        bool success = performDllInjection(dllPath, processName);

        Logger::info("Exiting launcher in 3 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(3));

        return success ? 0 : 1;
    }
    catch (const std::exception& e) {
        Logger::error(std::string("Fatal error: ") + e.what());
        showError(e.what());
        return 1;
    }
}