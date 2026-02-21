#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>

#include "../include/Launcher.h"

HSELauncher::HSELauncher()
    : updateManager(hse::UpdateManager::Instance()),
    processManager(hse::ProcessManager::Instance()),
    config(hse::LauncherConfig::Instance()) {
}

void HSELauncher::SetupConsole() {
    auto localVersionResult = updateManager.GetLocalVersion();
    if (localVersionResult) {
        const auto versionStr = localVersionResult->ToString();
#ifdef EXPERIMENTAL_VERSION
        SetWindowTextA(GetConsoleWindow(), ("Half Sword Enhancer - Experimental Build " + versionStr).c_str());
#else
        SetWindowTextA(GetConsoleWindow(), ("Half Sword Enhancer " + versionStr).c_str());
#endif
    }
}

void HSELauncher::DisplayBanner() {
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

#ifdef EXPERIMENTAL_VERSION
    std::cout << R"(
        ______      __
       / ____/___  / /_  ____ _____  ________  _____  [ EXPERIMENTAL BUILD ]
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

#ifdef EXPERIMENTAL_VERSION
    hse::Logger::info("Made by The Ghost - Experimental Build");
    hse::Logger::warn("This is a public experimental build for testing purposes.");
    hse::Logger::info("This build will automatically update to the final release when available.");
    hse::Logger::info("Tip: You can drag & drop the mod DLL onto this launcher to install it!");
#else
    hse::Logger::info("Made by The Ghost");
#endif
}

bool HSELauncher::HandleDraggedDLL(int argc, char* argv[]) {
    std::string droppedFile;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg.ends_with(".dll")) {
            droppedFile = arg;
            break;
        }
    }
    if (droppedFile.empty()) return false;

    if (!std::filesystem::exists(droppedFile)) {
        hse::logAndShowError("Dropped file does not exist: " + droppedFile, "The dropped file does not exist.");
        return false;
    }

    const auto cachePath = hse::LauncherConfig::GetCachedModPath(currentGameMode_);
    try {
        std::filesystem::copy_file(droppedFile, cachePath, std::filesystem::copy_options::overwrite_existing);
        hse::Logger::info("Successfully installed mod DLL to cache from: " + droppedFile);

        if (!CopyToActive(cachePath)) return false;

        MessageBoxA(nullptr, ("Mod DLL successfully installed!\n\nFile: " + std::filesystem::path(droppedFile).filename().string()).c_str(), "Mod Installed", MB_OK | MB_ICONINFORMATION);
        return true;
    }
    catch (const std::exception& e) {
        hse::logAndShowError("Failed to install dropped DLL: " + std::string(e.what()), "Failed to install the mod DLL: " + std::string(e.what()));
        return false;
    }
}

void HSELauncher::ShowFirstRunInstructions() {
    MessageBoxA(nullptr,
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

bool HSELauncher::AskForUpdatePreference() {
    if (config.HasCheckForUpdatesSetting()) return config.GetCheckForUpdates();
    hse::Logger::info("Asking user for update preference...");
    int result = MessageBoxA(nullptr,
        "Would you like the launcher to check for new updates automatically?\n\n"
        "- YES: Get notified when new versions are available\n"
        "- NO: Check manually when you want\n\n"
        "Tip: You can drag & drop any mod DLL file onto this launcher to install it instantly.\n\n"
        "You can change this setting later by editing the configuration file.",
        "Update Settings", MB_YESNO | MB_ICONQUESTION);
    bool enableUpdates = (result == IDYES);
    [[maybe_unused]] auto result_unused = config.SetCheckForUpdates(enableUpdates);
    hse::Logger::info(enableUpdates ? "User enabled update checking" : "User disabled update checking");
    return enableUpdates;
}

void HSELauncher::SelectGameMode(int argc, char* argv[]) {
#ifdef EXPERIMENTAL_VERSION
    currentGameMode_ = hse::GameMode::FullGame;
    return;
#else
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--demo" || arg == "--full") {
            currentGameMode_ = (arg == "--demo") ? hse::GameMode::Demo : hse::GameMode::FullGame;
            hse::Logger::info("Using %s mode (override via %.*s)", hse::GameModeName(currentGameMode_), static_cast<int>(arg.size()), arg.data());
            return;
        }
    }

    if (config.HasGameModeSetting()) {
        currentGameMode_ = config.GetGameMode();
    }
    else {
        int result = MessageBoxA(nullptr,
            "Do you want to play the Demo instead of the full game?\n\n"
            "YES = Half Sword Demo\n"
            "NO = Half Sword (Full Game)\n\n"
            "You can change this later using --demo or --full arguments.",
            "Game Mode", MB_YESNO | MB_ICONQUESTION);
        currentGameMode_ = (result == IDYES) ? hse::GameMode::Demo : hse::GameMode::FullGame;
        [[maybe_unused]] auto _ = config.SetGameMode(currentGameMode_);
    }

    hse::Logger::info("Game mode: %s", hse::GameModeName(currentGameMode_));
#endif
}

bool HSELauncher::CopyToActive(const std::filesystem::path& cachePath) {
    try {
        const auto activePath = hse::LauncherConfig::GetModFilePath();
        std::filesystem::copy_file(cachePath, activePath, std::filesystem::copy_options::overwrite_existing);
        return true;
    }
    catch (const std::exception& e) {
        hse::logAndShowError("Failed to activate mod from cache: " + std::string(e.what()),
            "Failed to activate mod. Please try again.");
        return false;
    }
}

bool HSELauncher::EnsureModExists() {
#ifdef EXPERIMENTAL_VERSION
    const auto modPath = hse::LauncherConfig::GetModFilePath();

    if (std::filesystem::exists(modPath)) {
        hse::Logger::info("Mod DLL found at: " + modPath.string());
        return true;
    }

    hse::Logger::info("No mod found - downloading latest version");

    if (!cachedExperimentalInfo_) {
        auto experimentalUpdateResult = updateManager.CheckForExperimentalUpdates();
        if (!experimentalUpdateResult) {
            hse::logAndShowError("Failed to get experimental release information", "Could not connect to update server. Please check your internet connection.");
            return false;
        }
        cachedExperimentalInfo_ = *experimentalUpdateResult;
    }

    auto& experimentalInfo = *cachedExperimentalInfo_;
    if (experimentalInfo.downloadUrlMod.empty()) {
        hse::logAndShowError("No experimental mod available", "Could not find experimental mod in the release.");
        return false;
    }

    auto updateResult = updateManager.UpdateExperimentalMod(experimentalInfo.downloadUrlMod, experimentalInfo.modTimestamp);
    if (updateResult) {
        hse::Logger::info("Experimental mod download completed");
        return true;
    }

    hse::logAndShowError("Failed to download experimental mod", "Failed to download experimental mod files. Please check your internet connection and try again.");
    return false;
#else
    const auto cachePath = hse::LauncherConfig::GetCachedModPath(currentGameMode_);

    if (std::filesystem::exists(cachePath)) {
        hse::Logger::info("Cached mod found for %s, activating...", hse::GameModeName(currentGameMode_));
        return CopyToActive(cachePath);
    }

    hse::Logger::info("No cached mod found - downloading...");

    if (currentGameMode_ == hse::GameMode::Demo) {
        auto result = updateManager.DownloadModToPath(
            hse::UpdateManager::DEMO_MOD_DOWNLOAD_URL, cachePath);
        if (!result) {
            hse::logAndShowError("Failed to download demo mod", "Failed to download demo mod (v0.5.2). Please check your internet connection.");
            return false;
        }
        hse::Logger::info("Demo mod downloaded (v0.5.2)");
        return CopyToActive(cachePath);
    }

    auto updateInfoResult = updateManager.CheckForUpdates();
    if (!updateInfoResult) {
        hse::logAndShowError("Failed to get remote version information", "Could not connect to update server. Please check your internet connection.");
        return false;
    }

    auto& updateInfo = *updateInfoResult;
    if (!updateInfo.remoteVersion.IsValid()) {
        hse::logAndShowError("Invalid version information", "Received invalid version data from server.");
        return false;
    }

    auto result = updateManager.UpdateMod(updateInfo.remoteVersion);
    if (!result) {
        hse::logAndShowError("Failed to download mod", "Failed to download mod files. Please check your internet connection.");
        return false;
    }
    hse::Logger::info("Mod downloaded (version: " + updateInfo.remoteVersion.ToString() + ")");
    return true;
#endif
}

bool HSELauncher::PerformUpdatesIfNeeded() {
    if (!AskForUpdatePreference()) {
        hse::Logger::info("Checking for updates is disabled");
        return true;
    }

    hse::Logger::info("Checking for updates...");

#ifdef EXPERIMENTAL_VERSION
    if (!cachedExperimentalInfo_) {
        auto experimentalUpdateResult = updateManager.CheckForExperimentalUpdates();
        if (!experimentalUpdateResult) {
            hse::Logger::error("Failed to check for experimental updates");
            return true;
        }
        cachedExperimentalInfo_ = *experimentalUpdateResult;
    }

    auto& experimentalInfo = *cachedExperimentalInfo_;

    if (experimentalInfo.stableRelease && experimentalInfo.stableRelease->available) {
        auto& stable = *experimentalInfo.stableRelease;
        std::string message = "A stable release is available!\n\n"
            "Current experimental version: " + stable.currentVersion.ToString() + "\n"
            "Stable release: " + stable.remoteVersion.ToString() + "\n\n"
            "Do you want to update to the stable release?";

        int result = MessageBoxA(nullptr, message.c_str(), "Stable Release Available", MB_YESNO | MB_ICONINFORMATION);

        if (result == IDYES) {
            hse::Logger::info("Updating to stable release...");
            auto modResult = updateManager.UpdateMod(stable.remoteVersion);
            if (!modResult) {
                hse::showError("Failed to update mod files.");
            }
            auto launcherResult = updateManager.UpdateLauncher(stable.downloadUrlLauncher);
            return static_cast<bool>(launcherResult);
        }
        hse::Logger::info("User declined stable release migration");
        return true;
    }

    if (!experimentalInfo.modUpdateAvailable && !experimentalInfo.launcherUpdateAvailable) {
        hse::Logger::info("Experimental build is up to date");
        return true;
    }

    std::string message = "A new experimental build is available!\n\n";
    if (experimentalInfo.modUpdateAvailable) {
        message += "- Mod update available\n";
    }
    if (experimentalInfo.launcherUpdateAvailable) {
        message += "- Launcher update available\n";
    }
    message += "\nDo you want to download and install the update now?";

    int result = MessageBoxA(nullptr, message.c_str(), "Experimental Update Available", MB_YESNO | MB_ICONINFORMATION);

    if (result != IDYES) {
        hse::Logger::info("User declined experimental update");
        return true;
    }

    if (experimentalInfo.modUpdateAvailable && !experimentalInfo.downloadUrlMod.empty()) {
        hse::Logger::info("Updating experimental mod...");
        auto modResult = updateManager.UpdateExperimentalMod(experimentalInfo.downloadUrlMod, experimentalInfo.modTimestamp);
        if (!modResult) {
            hse::showError("Failed to update experimental mod files.");
        }
    }

    if (experimentalInfo.launcherUpdateAvailable && !experimentalInfo.downloadUrlLauncher.empty()) {
        hse::Logger::info("Updating experimental launcher...");
        auto launcherResult = updateManager.UpdateLauncher(experimentalInfo.downloadUrlLauncher, experimentalInfo.launcherTimestamp);
        return static_cast<bool>(launcherResult);
    }

    return true;

#else
    auto updateInfoResult = updateManager.CheckForUpdates();
    if (!updateInfoResult) {
        hse::Logger::error("Failed to check for updates");
        return true;
    }

    auto& updateInfo = *updateInfoResult;
    if (!updateInfo.available) {
        hse::Logger::info("No updates available");
        return true;
    }

    if (currentGameMode_ == hse::GameMode::Demo) {
        hse::Logger::info("Demo mode - skipping mod update, checking launcher only");
        if (!updateInfo.downloadUrlLauncher.empty()) {
            std::string message = "A new launcher version is available!\n\n"
                "Current version: " + updateInfo.currentVersion.ToString() + "\n"
                "New version: " + updateInfo.remoteVersion.ToString() + "\n\n"
                "Do you want to update the launcher now?\n"
                "(The Demo mod will not be changed)";
            int result = MessageBoxA(nullptr, message.c_str(), "Launcher Update Available", MB_YESNO | MB_ICONINFORMATION);
            if (result == IDYES) {
                hse::Logger::info("Updating launcher...");
                auto launcherUpdateResult = updateManager.UpdateLauncher(updateInfo.downloadUrlLauncher);
                return static_cast<bool>(launcherUpdateResult);
            }
        }
        return true;
    }

    std::string message = "A new version of Half Sword Enhancer is available!\n\n"
        "Current version: " + updateInfo.currentVersion.ToString() + "\n"
        "New version: " + updateInfo.remoteVersion.ToString() + "\n\n"
        "Do you want to download and install the update now?";
    int result = MessageBoxA(nullptr, message.c_str(), "Update Available", MB_YESNO | MB_ICONINFORMATION);

    if (result != IDYES) {
        hse::Logger::info("User declined update");
        return true;
    }

    hse::Logger::info("Updating mod first...");

    auto modUpdateResult = updateManager.UpdateMod(updateInfo.remoteVersion);
    if (!modUpdateResult) {
        hse::showError("Failed to update mod files. The launcher update will continue anyway.");
    }

    hse::Logger::info("Now updating launcher...");
    auto launcherUpdateResult = updateManager.UpdateLauncher(updateInfo.downloadUrlLauncher);
    return static_cast<bool>(launcherUpdateResult);
#endif
}

bool HSELauncher::InjectMod() {
    const auto modPath = hse::LauncherConfig::GetModFilePath();
    hse::Logger::info("Starting mod injection process...");
    hse::Logger::warn("Make sure only the selected game is running. Close the other version if open.");

    auto processResult = processManager.LocateOrStartGame(hse::SteamUrl(currentGameMode_));
    if (!processResult) {
        hse::logAndShowError("Could not find or start the game", "Could not find Half Sword window. Please make sure the game starts correctly.");
        return false;
    }

    hse::Logger::info("Game found, injecting mod...");
    auto injectionResult = processManager.InjectDLL(*processResult, modPath.string());
    if (!injectionResult) {
        std::string errorMsg = "Mod injection failed. ";
        if (injectionResult.error() == hse::ProcessError::DllLoadFailed) {
            errorMsg += "The DLL could not be loaded by the game process. This is usually caused by:\n\n"
                       "- Antivirus software blocking the launcher\n"
                       "- Corrupted mod file\n"
                       "- Windows Defender real-time protection\n\n"
                       "Try:\n"
                       "1. Adding an antivirus exception for the launcher\n"
                       "2. Running as administrator\n"
                       "3. Temporarily disabling real-time protection";
        } else {
            errorMsg += "Please try running as administrator or check your antivirus settings.";
        }
        hse::logAndShowError("Mod injection failed", errorMsg);
        return false;
    }

    hse::Logger::info("Half Sword Enhancer loaded successfully! Enjoy!");
    return true;
}

void HSELauncher::ShowExitMessage(bool success) {
    if (success) hse::Logger::info("Launcher completed successfully");
    hse::Logger::info("Exiting in " + std::to_string(EXIT_DELAY_SECONDS) + " seconds...");
    std::this_thread::sleep_for(std::chrono::seconds(EXIT_DELAY_SECONDS));
}

int HSELauncher::Run(int argc, char* argv[]) {
    try {
        SetupConsole();
        DisplayBanner();
        if (config.IsFirstRun()) ShowFirstRunInstructions();
        SelectGameMode(argc, argv);
        if (HandleDraggedDLL(argc, argv)) hse::Logger::info("Mod DLL installed successfully. Now starting injection...");
        if (!PerformUpdatesIfNeeded()) {
            ShowExitMessage(false);
            return 1;
        }
        if (!EnsureModExists()) {
            ShowExitMessage(false);
            return 1;
        }
        bool success = InjectMod();
        ShowExitMessage(success);
        return success ? 0 : 1;
    }
    catch (const std::exception& e) {
        hse::Logger::error("Fatal error: " + std::string(e.what()));
        hse::showError("A fatal error occurred: " + std::string(e.what()));
        ShowExitMessage(false);
        return 1;
    }
}