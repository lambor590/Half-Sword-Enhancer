#include "../resource/resource.h"
#include "Util.h"
#include "Updater.h"
#include "Logger.h"

using namespace Util;

constexpr int CONSOLE_RED = FOREGROUND_RED | FOREGROUND_INTENSITY;
constexpr int CONSOLE_YELLOW = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr int CONSOLE_WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr int EXIT_DELAY_SECONDS = 3;

static bool handleDraggedDll(int argc, char* argv[]) noexcept {
    if (argc < 2) return false;
    
    std::string droppedFile = argv[1];
    
    if (droppedFile.length() < 4 || 
        droppedFile.substr(droppedFile.length() - 4) != ".dll") {
        return false;
    }
    
    if (!std::filesystem::exists(droppedFile)) {
        Logger::error("Dropped file does not exist: " + droppedFile);
        showError("The dropped file does not exist.");
        return false;
    }
    
    const std::string& appDataPath = getAppDataPath();
    std::string targetPath = appDataPath + "\\" + DLL_FILENAME;
    
    try {
        if (std::filesystem::exists(targetPath)) {
            std::filesystem::remove(targetPath);
            Logger::info("Removed existing mod DLL");
        }
        
        std::filesystem::copy_file(droppedFile, targetPath);
        Logger::info("Successfully installed mod DLL from: " + droppedFile);
        
        MessageBoxA(NULL, 
            ("Mod DLL successfully installed!\n\nFile: " + 
            std::filesystem::path(droppedFile).filename().string()).c_str(), 
            "Mod Installed", MB_OK | MB_ICONINFORMATION);
        
        return true;
    }
    catch (const std::exception& e) {
        Logger::error(std::string("Failed to install dropped DLL: ") + e.what());
        showError("Failed to install the mod DLL: " + std::string(e.what()));
        return false;
    }
}

static bool performModInjection(const std::string& dllPath) noexcept {
    try {
        Logger::info("Starting mod injection process...");

        const std::string& appDataPath = getAppDataPath();

        DWORD processId = locateOrStartApplication();
        if (processId == 0) {
            Logger::error("Could not find application window");
            showError("Could not find Half Sword window. Please make sure the game launches correctly.");
            return false;
        }
        
        Logger::info("Application window found, ready for mod injection...");

        if (!std::filesystem::exists(dllPath)) {
            std::string localVersion = Updater::getLocalVersion();
            if (!downloadDllFromGitHub(dllPath, localVersion)) {
                Logger::error("Failed to download mod DLL");
                showError("Failed to download mod files. Please check your internet connection.");
                return false;
            }
        }

        Logger::info("Attempting to inject mod...");
        bool success = initializeModInjection(processId, dllPath);

        if (!success) {
            Logger::error("Mod injection failed");
            showError("Mod injection failed. Please try again with the game freshly launched or check your antivirus settings.");
            return false;
        }

        Logger::info("Half Sword Enhancer loaded successfully! Enjoy!");
        return true;
    }
    catch (const std::exception& e) {
        Logger::error(std::string("Error during mod injection: ") + e.what());
        showError(std::string("Error during mod injection: ") + e.what());
        return false;
    }
}


int main(int argc, char* argv[]) {
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

        #ifdef BETA_VERSION
            std::cout << R"(
        ______      __
       / ____/___  / /_  ____ _____  ________  _____  [ B E T A ]
      / __/ / __ \/ __ \/ __ `/ __ \/ ___/ _ \/ ___/
     / /___/ / / / / / / /_/ / / / / /__/  __/ /
    /_____/_/ /_/_/ /_/\__,_/_/ /_/\___/\___/_/
    )" << "\n";
        #else
            std::cout << R"(
        ______      __
       / ____/___  / /_  ____ _____  ________  _____
      / __/ / __ \/ __ \/ __ `/ __ \/ ___/ _ \/ ___/
     / /___/ / / / / / / /_/ / / / / /__/  __/ /
    /_____/_/ /_/_/ /_/\__,_/_/ /_/\___/\___/_/
    )" << "\n";
        #endif

        SetConsoleTextAttribute(hConsole, CONSOLE_WHITE);

        #ifdef BETA_VERSION
            SetWindowText(GetConsoleWindow(),
                ("Half Sword Enhancer BETA " + Updater::getLocalVersion()).c_str());
        #else
            SetWindowText(GetConsoleWindow(),
                ("Half Sword Enhancer " + Updater::getLocalVersion()).c_str());
        #endif

        if (handleDraggedDll(argc, argv)) {
            Logger::info("Mod DLL installed successfully. Now starting injection...");
        }

        const std::string dllPath = getAppDataPath() + "\\" + DLL_FILENAME;

        #ifdef BETA_VERSION
            Logger::info("Made by The Ghost - BETA VERSION");
            Logger::warn("This is a pre-release version for testing only!");
            Logger::info("Tip: You can drag & drop DLL files onto this launcher to install them!");
        #else
            Logger::info("Made by The Ghost");
        #endif

        Updater::checkForUpdates();
        bool success = performModInjection(dllPath);

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