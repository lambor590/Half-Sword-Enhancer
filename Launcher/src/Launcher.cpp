#include <iostream>
#include <filesystem>
#include <optional>
#include <string_view>
#include <thread>
#include <chrono>
#include <Windows.h>

#include "../include/Launcher.h"
#include "../include/InstallManager.h"
#include "../include/LauncherConfig.h"
#include "../include/SteamLocator.h"
#include "../include/Util.h"

#include <shellapi.h>

namespace {
    constexpr int EXIT_DELAY_SECONDS = 3;
    constexpr int CONSOLE_RED = FOREGROUND_RED | FOREGROUND_INTENSITY;
    constexpr int CONSOLE_YELLOW = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    constexpr int CONSOLE_WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

#ifdef EXPERIMENTAL_VERSION
    constexpr std::string_view INSTALL_SECTION = "Install";
    constexpr std::string_view INSTALL_MODE_KEY = "install_mode";
    constexpr std::string_view INSTALL_MODE_STANDALONE = "standalone";
    constexpr std::string_view INSTALL_MODE_UE4SS = "ue4ss";

    [[nodiscard]] constexpr std::string_view InstallModeConfigValue(hse::InstallMode mode) noexcept {
        return mode == hse::InstallMode::Ue4ss ? INSTALL_MODE_UE4SS : INSTALL_MODE_STANDALONE;
    }

    [[nodiscard]] constexpr const char* InstallModeName(hse::InstallMode mode) noexcept {
        return mode == hse::InstallMode::Ue4ss ? "UE4SS" : "standalone";
    }

    [[nodiscard]] std::optional<hse::InstallMode> ParseInstallMode(std::string_view value) noexcept {
        if (value == INSTALL_MODE_UE4SS) return hse::InstallMode::Ue4ss;
        if (value == INSTALL_MODE_STANDALONE) return hse::InstallMode::Standalone;
        return std::nullopt;
    }
#endif
}

HSELauncher::HSELauncher()
    : config(hse::LauncherConfig::Instance()),
      gameEdition_(hse::GameEdition::FullGame) {}

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
    )"
              << "\n";
#else
    std::cout << R"(
        ______      __
       / ____/___  / /_  ____ _____  ________  _____
      / __/ / __ \/ __ \/ __ `/ __ \/ ___/ _ \/ ___/
     / /___/ / / / / / / /_/ / / / / /__/  __/ /
    /_____/_/ /_/_/ /_/\__,_/_/ /_/\___/\___/_/
    )"
              << "\n";
#endif

    SetConsoleTextAttribute(hConsole, CONSOLE_WHITE);

#ifdef EXPERIMENTAL_VERSION
    hse::Logger::info("Made by The Ghost - Experimental Build");
    hse::Logger::warn("This is a public experimental build for testing purposes.");
    hse::Logger::info("This build will automatically update to the final release when available.");
#else
    hse::Logger::info("Made by The Ghost");
#endif
}

void HSELauncher::ShowFirstRunInstructions() {
    MessageBoxA(
        nullptr,
        "The installer will automatically:\n"
        "- Find your Half Sword installation\n"
        "- Install and update the mod files\n\n"
        "Once the mod is installed, just launch the game normally!\n\n"
        "In-game:\n"
        "- Press INSERT to show/hide the mod interface\n"
        "- Use the Settings section to customize interface keybinds\n"
        "- All mod features are accessible through the interface\n\n"
        "Have fun!",
        "Mod Usage Guide", MB_OK | MB_ICONINFORMATION
    );
}

bool HSELauncher::AskForUpdatePreference() {
    if (config.HasCheckForUpdatesSetting()) return config.GetCheckForUpdates();
    hse::Logger::info("Asking user for update preference...");
    int result = MessageBoxA(
        nullptr,
        "Would you like the installer to check for new updates automatically?\n\n"
        "- YES: Get notified when new versions are available\n"
        "- NO: Check manually when you want\n\n"
        "You can change this setting later by editing the configuration file.",
        "Update Settings", MB_YESNO | MB_ICONQUESTION
    );
    bool enableUpdates = (result == IDYES);
    (void)config.SetCheckForUpdates(enableUpdates);
    hse::Logger::info(enableUpdates ? "User enabled update checking" : "User disabled update checking");
    return enableUpdates;
}

bool HSELauncher::PerformSelfUpdate() {
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
                              "Current experimental version: " +
                              stable.currentVersion.ToString() +
                              "\n"
                              "Stable release: " +
                              stable.remoteVersion.ToString() +
                              "\n\n"
                              "Do you want to update to the stable release?";

        int result = MessageBoxA(nullptr, message.c_str(), "Stable Release Available", MB_YESNO | MB_ICONINFORMATION);

        if (result == IDYES) {
            hse::Logger::info("Updating launcher to stable release...");
            auto launcherResult = updateManager.UpdateLauncher(stable.downloadUrlLauncher);
            return static_cast<bool>(launcherResult);
        }
        hse::Logger::info("User declined stable release migration");
        return true;
    }

    if (experimentalInfo.launcherUpdateAvailable && !experimentalInfo.downloadUrlLauncher.empty()) {
        std::string message = "A new experimental launcher build is available!\n\n"
                              "Do you want to update the launcher now?";
        int result = MessageBoxA(nullptr, message.c_str(), "Launcher Update Available", MB_YESNO | MB_ICONINFORMATION);
        if (result == IDYES) {
            hse::Logger::info("Updating experimental launcher...");
            auto launcherResult =
                updateManager.UpdateLauncher(experimentalInfo.downloadUrlLauncher, experimentalInfo.launcherTimestamp);
            return static_cast<bool>(launcherResult);
        }
    }

    return true;
#else
    if (!cachedUpdateInfo_) {
        auto updateInfoResult = updateManager.CheckForUpdates();
        if (!updateInfoResult) {
            hse::Logger::error("Failed to check for updates");
            return true;
        }
        cachedUpdateInfo_ = *updateInfoResult;
    }

    auto& updateInfo = *cachedUpdateInfo_;
    if (!updateInfo.available || updateInfo.downloadUrlLauncher.empty()) {
        hse::Logger::info("Launcher is up to date");
        return true;
    }

    std::string message = "A new version of Half Sword Enhancer is available!\n\n"
                          "Current version: " +
                          updateInfo.currentVersion.ToString() +
                          "\n"
                          "New version: " +
                          updateInfo.remoteVersion.ToString() +
                          "\n\n"
                          "Do you want to update the launcher now?";
    int result = MessageBoxA(nullptr, message.c_str(), "Launcher Update Available", MB_YESNO | MB_ICONINFORMATION);

    if (result != IDYES) {
        hse::Logger::info("User declined launcher update");
        return true;
    }

    hse::Logger::info("Updating launcher...");
    auto launcherUpdateResult = updateManager.UpdateLauncher(updateInfo.downloadUrlLauncher);
    return static_cast<bool>(launcherUpdateResult);
#endif
}

bool HSELauncher::LocateGame() {
    if (config.HasGamePath()) {
        auto savedPath = config.GetGamePath();
        if (std::filesystem::exists(savedPath)) {
            gameBinPath_ = savedPath;
            gameEdition_ = config.GetGameEdition();
            hse::Logger::info(
                "Game found (saved): %s (%s)", gameBinPath_.string().c_str(), hse::GameEditionName(gameEdition_)
            );
            return true;
        }
        hse::Logger::warn("Saved game path no longer valid, re-detecting...");
    }

    hse::Logger::info("Searching for Half Sword installation...");
    auto locateResult = hse::LocateGame();
    if (locateResult) {
        gameBinPath_ = locateResult->binariesPath;
        gameEdition_ = locateResult->edition;
        (void)config.SetGamePath(gameBinPath_);
        (void)config.SetGameEdition(gameEdition_);
        hse::Logger::info("Game found: %s (%s)", gameBinPath_.string().c_str(), hse::GameEditionName(gameEdition_));
        return true;
    }

    hse::Logger::warn("Could not auto-detect game installation");
    std::filesystem::path manualPath = AskManualPath();
    if (manualPath.empty()) return false;

    auto manualResult = hse::LocateGameAt(manualPath);
    if (!manualResult) {
        hse::logAndShowError(
            "Invalid game path: " + manualPath.string(),
            "The specified path does not appear to contain a valid Half Sword installation.\n\n"
            "Please provide the path to the game folder, e.g.:\n"
            "D:\\SteamLibrary\\steamapps\\common\\Half Sword"
        );
        return false;
    }

    gameBinPath_ = manualResult->binariesPath;
    gameEdition_ = manualResult->edition;
    (void)config.SetGamePath(gameBinPath_);
    (void)config.SetGameEdition(gameEdition_);
    hse::Logger::info(
        "Game found (manual): %s (%s)", gameBinPath_.string().c_str(), hse::GameEditionName(gameEdition_)
    );
    return true;
}

std::filesystem::path HSELauncher::AskManualPath() {
    hse::Logger::info("Please enter the path to your Half Sword installation:");
    hse::Logger::info("  Example: D:\\SteamLibrary\\steamapps\\common\\Half Sword");
    std::cout << "\n  > ";

    std::string path;
    std::getline(std::cin, path);

    const auto first = path.find_first_not_of("\"'");
    if (first == std::string::npos) return {};
    const auto last = path.find_last_not_of("\"'");
    path = path.substr(first, last - first + 1);

    return std::filesystem::path(path);
}

#ifdef EXPERIMENTAL_VERSION
hse::InstallMode HSELauncher::GetInstallMode() {
    if (auto stored = ParseInstallMode(config.GetString(INSTALL_SECTION, INSTALL_MODE_KEY, ""))) {
        hse::Logger::info("Install mode: %s", InstallModeName(*stored));
        return *stored;
    }

    const auto detectedMode = hse::DetectInstallMode(gameBinPath_);
    std::string message =
        "Choose how to install this closed-test build.\n\n"
        "YES: UE4SS mode. Use this if UE4SS is installed; HSE will be installed as "
        "ue4ss\\Mods\\HSEnhancer\\dlls\\main.dll.\n\n"
        "NO: Standalone mode. HSE will use the winmm.dll proxy.\n\n";

    message += detectedMode == hse::InstallMode::Ue4ss
                   ? "UE4SS files were detected, so UE4SS mode is recommended."
                   : "UE4SS was not detected, so standalone mode is recommended unless you install UE4SS yourself.";

    const UINT defaultButton =
        detectedMode == hse::InstallMode::Ue4ss ? MB_DEFBUTTON1 : MB_DEFBUTTON2;
    const int result =
        MessageBoxA(nullptr, message.c_str(), "Install Mode", MB_YESNO | MB_ICONQUESTION | defaultButton);
    const auto installMode = result == IDYES ? hse::InstallMode::Ue4ss : hse::InstallMode::Standalone;

    if (auto saveResult = config.SetString(INSTALL_SECTION, INSTALL_MODE_KEY, InstallModeConfigValue(installMode));
        !saveResult) {
        hse::Logger::warn("Failed to save install mode preference");
    }

    hse::Logger::info("Install mode selected: %s", InstallModeName(installMode));
    return installMode;
}
#endif

bool HSELauncher::CheckAndInstallMod() {
#ifdef EXPERIMENTAL_VERSION
    const auto installMode = GetInstallMode();
#else
    const auto installMode = hse::DetectInstallMode(gameBinPath_);
#endif
    const bool needsInstall = !hse::IsInstallationComplete(gameBinPath_, installMode);

#ifdef EXPERIMENTAL_VERSION
    if (needsInstall) {
        hse::Logger::info("Mod files incomplete. Downloading experimental build...");

        if (!cachedExperimentalInfo_) {
            auto experimentalUpdateResult = updateManager.CheckForExperimentalUpdates();
            if (!experimentalUpdateResult) {
                hse::logAndShowError(
                    "Failed to get experimental release information",
                    "Could not connect to update server. Please check your internet connection."
                );
                return false;
            }
            cachedExperimentalInfo_ = *experimentalUpdateResult;
        }

        auto& info = *cachedExperimentalInfo_;
        if (info.downloadUrlMod.empty()) {
            hse::Logger::info("No experimental build available, downloading stable release...");
            auto stableInfo = updateManager.CheckForUpdates();
            if (!stableInfo || !stableInfo->remoteVersion.IsValid()) {
                hse::logAndShowError(
                    "No mod available",
                    "Could not find any mod version to install. Please check your internet connection."
                );
                return false;
            }
            return DownloadAndInstall(stableInfo->remoteVersion, installMode);
        }

        auto result = updateManager.DownloadAndInstallExperimentalMod(info, gameBinPath_, installMode);
        if (!result) {
            hse::logAndShowError(
                "Failed to install experimental mod",
                "Failed to download and install the mod. Please check your internet connection."
            );
            return false;
        }
        return true;
    }

    if (!cachedExperimentalInfo_) {
        auto experimentalUpdateResult = updateManager.CheckForExperimentalUpdates();
        if (!experimentalUpdateResult) {
            hse::Logger::error("Failed to check for experimental updates");
            return true;
        }
        cachedExperimentalInfo_ = *experimentalUpdateResult;
    }

    auto& info = *cachedExperimentalInfo_;

    if (info.stableRelease && info.stableRelease->available) {
        auto& stable = *info.stableRelease;
        std::string message = "A stable mod release is available!\n\n"
                              "Stable release: " +
                              stable.remoteVersion.ToString() +
                              "\n\n"
                              "Do you want to install the stable version?";
        int result = MessageBoxA(nullptr, message.c_str(), "Stable Release Available", MB_YESNO | MB_ICONINFORMATION);
        if (result == IDYES) {
            return DownloadAndInstall(stable.remoteVersion, installMode);
        }
    }

    if (info.modUpdateAvailable && !info.downloadUrlMod.empty()) {
        std::string message = "A new experimental mod build is available!\n\n"
                              "Do you want to install the update now?";
        int result = MessageBoxA(nullptr, message.c_str(), "Mod Update Available", MB_YESNO | MB_ICONINFORMATION);
        if (result == IDYES) {
            auto updateResult = updateManager.DownloadAndInstallExperimentalMod(info, gameBinPath_, installMode);
            if (!updateResult) {
                hse::showError("Failed to update experimental mod files.");
                return false;
            }
        }
    } else {
        hse::Logger::info("Mod is up to date");
    }

    return true;
#else
    if (!cachedUpdateInfo_) {
        auto updateInfoResult = updateManager.CheckForUpdates();
        if (!updateInfoResult) {
            hse::logAndShowError(
                "Failed to get remote version information",
                "Could not connect to update server. Please check your internet connection."
            );
            return false;
        }
        cachedUpdateInfo_ = *updateInfoResult;
    }

    auto& updateInfo = *cachedUpdateInfo_;

    if (needsInstall) {
        hse::Logger::info("Mod files incomplete. Downloading...");

        if (!updateInfo.remoteVersion.IsValid()) {
            hse::logAndShowError("Invalid version information", "Received invalid version data from server.");
            return false;
        }

        return DownloadAndInstall(updateInfo.remoteVersion, installMode);
    }

    auto installedVersion = updateManager.GetInstalledModVersion(gameBinPath_);
    bool needsUpdate = false;

    if (installedVersion) {
        needsUpdate = updateInfo.remoteVersion > *installedVersion;
        if (needsUpdate) {
            std::string message = "A mod update is available!\n\n"
                                  "Installed: " +
                                  installedVersion->ToString() +
                                  "\n"
                                  "Available: " +
                                  updateInfo.remoteVersion.ToString() +
                                  "\n\n"
                                  "Do you want to update now?";
            int result = MessageBoxA(nullptr, message.c_str(), "Update Available", MB_YESNO | MB_ICONINFORMATION);
            if (result != IDYES) {
                hse::Logger::info("User declined mod update");
                return true;
            }
        }
    } else {
        needsUpdate = updateInfo.available;
        if (needsUpdate) {
            std::string message = "An update may be available (v" + updateInfo.remoteVersion.ToString() +
                                  ").\n\n"
                                  "Do you want to reinstall the mod?";
            int result = MessageBoxA(nullptr, message.c_str(), "Update Available", MB_YESNO | MB_ICONQUESTION);
            if (result != IDYES) {
                return true;
            }
        }
    }

    if (needsUpdate) {
        return DownloadAndInstall(updateInfo.remoteVersion, installMode);
    }

    hse::Logger::info("Mod is up to date (v%s)", installedVersion->ToString().c_str());
    return true;
#endif
}

bool HSELauncher::DownloadAndInstall(const hse::Version& version, hse::InstallMode installMode) {
    auto result = updateManager.DownloadAndInstallMod(version, gameBinPath_, installMode);
    if (!result) {
        hse::logAndShowError(
            "Failed to install mod v" + version.ToString(),
            "Failed to download and install the mod. Please check your internet connection and try again."
        );
        return false;
    }
    return true;
}

void HSELauncher::OfferGameLaunch() {
    hse::Logger::info("Would you like to launch the game? [Y/n]");
    std::cout << "  > ";

    std::string input;
    std::getline(std::cin, input);

    if (input.empty() || input[0] == 'Y' || input[0] == 'y') {
        const auto* steamUrl = hse::SteamUrl(gameEdition_);
        hse::Logger::info("Launching %s via Steam...", hse::GameEditionName(gameEdition_));
        ShellExecuteA(nullptr, "open", steamUrl, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void HSELauncher::ShowExitMessage(bool success) {
    if (success) hse::Logger::info("Installer completed successfully");
    hse::Logger::info("Exiting in %d seconds...", EXIT_DELAY_SECONDS);
    std::this_thread::sleep_for(std::chrono::seconds(EXIT_DELAY_SECONDS));
}

int HSELauncher::Run(int /*argc*/, char* /*argv*/[]) {
    try {
        SetupConsole();
        DisplayBanner();
        if (config.IsFirstRun()) ShowFirstRunInstructions();

        if (!PerformSelfUpdate()) {
            ShowExitMessage(false);
            return 1;
        }

        if (!LocateGame()) {
            ShowExitMessage(false);
            return 1;
        }

        if (hse::IsGameRunning()) {
            hse::Logger::warn("Half Sword is currently running.");
            hse::Logger::warn("Please close the game before installing or updating the mod.");
            ShowExitMessage(false);
            return 1;
        }

        auto permResult = hse::TestWritePermissions(gameBinPath_);
        if (!permResult || !*permResult) {
            hse::logAndShowError(
                "Cannot write to game folder: " + gameBinPath_.string(),
                "Cannot write to the game folder.\n\n"
                "Please run the installer as administrator or check the folder permissions.\n\n"
                "Path: " +
                    gameBinPath_.string()
            );
            ShowExitMessage(false);
            return 1;
        }

        if (!CheckAndInstallMod()) {
            ShowExitMessage(false);
            return 1;
        }

        OfferGameLaunch();
        ShowExitMessage(true);
        return 0;
    } catch (const std::exception& e) {
        hse::Logger::error("Fatal error: %s", e.what());
        hse::showError(std::string("A fatal error occurred: ") + e.what());
        ShowExitMessage(false);
        return 1;
    }
}
