#include "../resource/resource.h"
#include "Util.h"
#include "Updater.h"
#include "Logger.h"
#include "LauncherConfig.h"

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
        return logAndShowError("Dropped file does not exist: " + droppedFile, "The dropped file does not exist.");
    }
    
    const std::string targetPath = LauncherConfig::GetModFilePath();
    
    try {
        if (std::filesystem::exists(targetPath)) {
            std::filesystem::remove(targetPath);
            Logger::info("Removed existing mod DLL");
        }
        
        std::filesystem::copy_file(droppedFile, targetPath);
        Logger::info("Successfully installed mod DLL from: " + droppedFile);
        
        LauncherConfig& config = LauncherConfig::Get();
        config.SetDownloadedModVersionAsManualInstall();
        
        MessageBoxA(NULL, 
            ("Mod DLL successfully installed!\n\nFile: " + 
            std::filesystem::path(droppedFile).filename().string()).c_str(), 
            "Mod Installed", MB_OK | MB_ICONINFORMATION);
        
        return true;
    }
    catch (const std::exception& e) {
        return logAndShowError("Failed to install dropped DLL: " + std::string(e.what()), 
                              "Failed to install the mod DLL: " + std::string(e.what()));
    }
}

static bool performModInjection(const std::string& dllPath) noexcept {
    try {
        Logger::info("Starting mod injection process...");

        if (!std::filesystem::exists(dllPath)) {
            return logAndShowError("Mod DLL not found at expected location", 
                                  "Mod DLL not found. This should not happen - please restart the launcher.");
        }

        Logger::info("Mod DLL confirmed, locating or starting game...");

        DWORD processId = locateOrStartApplication();
        if (processId == 0) {
            return logAndShowError("Could not find application window", 
                                  "Could not find Half Sword window. Please make sure the game launches correctly.");
        }
        
        Logger::info("Application window found, ready for mod injection...");

        Logger::info("Attempting to inject mod...");
        bool success = initializeModInjection(processId, dllPath);

        if (!success) {
            return logAndShowError("Mod injection failed", 
                                  "Mod injection failed. Please try again with the game freshly launched or check your antivirus settings.");
        }

        Logger::info("Half Sword Enhancer loaded successfully! Enjoy!");
        return true;
    }
    catch (const std::exception& e) {
        return logAndShowError("Error during mod injection: " + std::string(e.what()), 
                              "Error during mod injection: " + std::string(e.what()));
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

        #ifdef DEV_VERSION
            std::cout << R"(
        ______      __
       / ____/___  / /_  ____ _____  ________  _____  [ INTERNAL BUILD ]
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

        const std::string localVersion = Updater::getLocalVersion();
        #ifdef DEV_VERSION
            SetWindowText(GetConsoleWindow(),
                ("Half Sword Enhancer - Internal Build " + localVersion).c_str());
        #else
            SetWindowText(GetConsoleWindow(),
                ("Half Sword Enhancer " + localVersion).c_str());
        #endif

        if (handleDraggedDll(argc, argv)) {
            Logger::info("Mod DLL installed successfully. Now starting injection...");
        }

        const std::string dllPath = LauncherConfig::GetModFilePath();

        #ifdef DEV_VERSION
            Logger::info("Made by The Ghost - Internal Build");
            Logger::warn("This is an internal development build for testing purposes.");
            Logger::info("This build will automatically update to the final release when available.");
            Logger::info("Tip: You can drag & drop DLL files onto this launcher to install them!");
        #else
            Logger::info("Made by The Ghost");
        #endif

        LauncherConfig& config = LauncherConfig::Get();
        
        if (config.IsFirstRun()) {
            MessageBoxA(NULL,
                "The launcher will automatically:\n"
                "- Open Half Sword if it's not running\n"
                "- Load the mod into the game\n\n"
                "Once the mod is loaded in-game:\n\n"
                "- Press INSERT to show/hide the mod interface\n"
                "- Use the Settings section to customize interface keybinds\n"
                "- All mod features are accessible through the interface\n\n"
                "Have fun!",
                "Mod Usage Guide",
                MB_OK | MB_ICONINFORMATION);
        }
        
        bool shouldCheckForUpdates = config.GetCheckForUpdates();
        
        if (!config.HasCheckForUpdatesSetting()) {
            Logger::info("Check for updates setting not found - asking user for preference...");
            
            int result = MessageBoxA(NULL,
                "Would you like the launcher to check for new updates automatically?\n\n"
                "- YES: Get notified when new versions are available\n"
                "- NO: Check manually when you want\n\n"
                "Tip: You can drag & drop any mod DLL file onto this launcher to install it instantly.\n\n"
                "You can change this setting later by editing the configuration file.",
                "Update Settings", 
                MB_YESNO | MB_ICONQUESTION);
            
            bool enableUpdates = (result == IDYES);
            config.SetCheckForUpdates(enableUpdates);
            shouldCheckForUpdates = enableUpdates;
            
            if (enableUpdates) {
                Logger::info("User enabled checking for updates");
            } else {
                Logger::info("User disabled checking for updates");
            }
        }
        
        if (config.IsFirstRun()) {
            Logger::info("First run detected - checking if mod download is needed...");
            bool modExists = std::filesystem::exists(dllPath);
            
            if (!modExists) {
                Logger::info("No mod found - downloading latest version");
                std::string remoteVersion = Updater::getRemoteVersion();
                std::string versionToDownload = (remoteVersion != "0.0.0") ? remoteVersion : Updater::getLocalVersion();
                
                if (downloadDllFromGitHub(dllPath, versionToDownload)) {
                    config.SetDownloadedModVersion(versionToDownload);
                    Logger::info("First-run mod download completed (version: " + versionToDownload + ")");
                } else {
                    return logAndShowError("Failed to download mod on first run and no existing mod available", 
                                          "Failed to download mod files and no existing mod found. Please check your internet connection and try again.");
                }
            } else {
                Logger::info("Existing mod found - will use existing version");
                if (!config.HasVersionInfo()) {
                    config.SetDownloadedModVersionAsUnknown();
                }
            }
        }
        
        if (shouldCheckForUpdates) {
            Updater::checkForUpdates();
        } else {
            Logger::info("Checking for updates disabled in configuration");
        }
        
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