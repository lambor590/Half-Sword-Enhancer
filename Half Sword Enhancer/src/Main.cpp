#include "../resource/resource.h"
#include "Util.h"
#include "Updater.h"
#include "Logger.h"

using namespace Util;

constexpr int CONSOLE_RED = FOREGROUND_RED | FOREGROUND_INTENSITY;
constexpr int CONSOLE_YELLOW = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr int CONSOLE_WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr int EXIT_DELAY_SECONDS = 3;

static bool performDllInjection(const std::string& dllPath, const char* processName = PROCESS_NAME) noexcept {
    try {
        Logger::info("Starting injection process...");

        const std::string& appDataPath = getAppDataPath();

        DWORD processId = findOrLaunchGame(processName);
        if (processId == 0) {
            Logger::error("Failed to find or launch game");
            showError("Could not launch Half Sword. Please make sure the game is installed correctly on Steam.");
            return false;
        }

        if (!WaitForGameWindow(processId)) {
            Logger::error("Game window not found after timeout");
            showError("Could not find game window. Please make sure the game launches correctly.");
            return false;
        }

        try {
            Logger::info("Extracting DLL to temporary file...");
            extractDllToTempFile(dllPath, IDR_DLL1);
        } catch (const std::exception& e) {
            Logger::error(std::string("Failed to extract DLL: ") + e.what());
            showError("Failed to extract mod files. This might be due to antivirus blocking the mod.");
            return false;
        }

        Logger::info("Attempting to inject DLL...");
        bool success = injectDll(processId, dllPath);

        if (!success) {
            Logger::error("DLL injection failed");
            showError("DLL injection failed. Please try again with the game freshly launched or check your antivirus settings.");
            return false;
        }

        Logger::info("Half Sword Enhancer injected successfully! Enjoy!");
        return true;
    }
    catch (const std::exception& e) {
        Logger::error(std::string("Error during injection: ") + e.what());
        showError(std::string("Error during injection: ") + e.what());
        return false;
    }
}

int main() {
    try {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

        SetConsoleTextAttribute(hConsole, CONSOLE_RED);
        std::cout << R"(
        __  __      ______   _____                        __
       / / / /___ _/ / __/  / ___/      ______  _________/ /
      / /_/ / __ `/ / /_    \__ \ | /| / / __ \/ ___/ __  /
     / __  / /_/ / / __/   ___/ / |/ |/ / /_/ / /  / /_/ /
    /_/ /_/\__,_/_/_/     /____/|__/|__/\____/_/   \__,_/
    )";

        SetConsoleTextAttribute(hConsole, CONSOLE_YELLOW);
        std::cout << R"(
        ______      __
       / ____/___  / /_  ____ _____  ________  _____
      / __/ / __ \/ __ \/ __ `/ __ \/ ___/ _ \/ ___/
     / /___/ / / / / / / /_/ / / / / /__/  __/ /
    /_____/_/ /_/_/ /_/\__,_/_/ /_/\___/\___/_/
    )" << "\n";

        SetConsoleTextAttribute(hConsole, CONSOLE_WHITE);

        SetWindowText(GetConsoleWindow(),
            ("Half Sword Enhancer " + Updater::getLocalVersion()).c_str());

        const std::string dllPath = getAppDataPath() + "\\" + DLL_FILENAME;

        Logger::info("Made by The Ghost");

        Updater::checkForUpdates();
        bool success = performDllInjection(dllPath);

        Logger::info("Exiting launcher in " + std::to_string(EXIT_DELAY_SECONDS) + " seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(EXIT_DELAY_SECONDS));

        return success ? 0 : 1;
    }
    catch (const std::exception& e) {
        Logger::error(std::string("Fatal error: ") + e.what());
        showError(e.what());
        return 1;
    }
}