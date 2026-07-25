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
    constexpr const char* INSTALL_SECTION = "Install";
    constexpr const char* INSTALL_MODE_KEY = "install_mode";
    constexpr const char* INSTALL_MODE_STANDALONE = "standalone";
    constexpr const char* INSTALL_MODE_UE4SS = "ue4ss";

    [[nodiscard]] constexpr const char* InstallModeConfigValue(hse::InstallMode mode) noexcept {
        return mode == hse::InstallMode::Ue4ss ? INSTALL_MODE_UE4SS : INSTALL_MODE_STANDALONE;
    }

    [[nodiscard]] constexpr const char* InstallModeName(hse::InstallMode mode) noexcept {
        return mode == hse::InstallMode::Ue4ss ? "UE4SS integration" : "direct installation";
    }

    [[nodiscard]] std::optional<hse::InstallMode> ParseInstallMode(std::string_view value) noexcept {
        if (value == INSTALL_MODE_UE4SS) return hse::InstallMode::Ue4ss;
        if (value == INSTALL_MODE_STANDALONE) return hse::InstallMode::Standalone;
        return std::nullopt;
    }
#endif
}

HSELauncher::HSELauncher() : config(hse::LauncherConfig::Instance()), gameEdition_(hse::GameEdition::FullGame) {}

void HSELauncher::SetupConsole() {
    auto localVersionResult = hse::UpdateManager::GetLocalVersion();
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

    std::cout << R"(
        ______      __
       / ____/___  / /_  ____ _____  ________  _____
       / __/ / __ \/ __ \/ __ `/ __ \/ ___/ _ \/ ___/
      / /___/ / / / / / / /_/ / / / / /__/  __/ /
     /_____/_/ /_/_/ /_/\__,_/_/ /_/\___/\___/_/
    )";
#ifdef EXPERIMENTAL_VERSION
    std::cout << "        [ EXPERIMENTAL BUILD ]\n";
#else
    std::cout << '\n';
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
        "- Install and update Half Sword Enhancer\n\n"
        "Once the mod is installed, just launch the game normally!\n\n"
        "In-game:\n"
        "- Press INSERT to show/hide the mod interface\n"
        "- Use the Settings section to customize interface shortcuts\n"
        "- All mod features are accessible through the interface\n\n"
        "Have fun!",
        "Mod Usage Guide", MB_OK | MB_ICONINFORMATION
    );
}

bool HSELauncher::AskForUpdatePreference() {
    if (config.HasCheckForUpdatesSetting()) return config.GetCheckForUpdates();
    int result = MessageBoxA(
        nullptr,
        "Would you like Half Sword Enhancer to check for new updates automatically?\n\n"
        "- YES: Get notified when new versions are available\n"
        "- NO: Check manually when you want",
        "Update Settings", MB_YESNO | MB_ICONQUESTION
    );
    bool enableUpdates = (result == IDYES);
    [[maybe_unused]] const auto saveResult = config.SetCheckForUpdates(enableUpdates);
    hse::Logger::info(enableUpdates ? "Automatic update checks enabled" : "Automatic update checks disabled");
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
        auto experimentalUpdateResult = hse::UpdateManager::CheckForExperimentalUpdates();
        if (!experimentalUpdateResult) {
            hse::Logger::warn("Could not check for experimental updates. The installed version will be kept.");
            return true;
        }
        cachedExperimentalInfo_ = *experimentalUpdateResult;
    }

    auto& experimentalInfo = *cachedExperimentalInfo_;

    if (experimentalInfo.launcherUpdateAvailable) {
        std::string message = "A new experimental launcher build is available!\n\n"
                              "Do you want to update the launcher now?";
        int result = MessageBoxA(nullptr, message.c_str(), "Launcher Update Available", MB_YESNO | MB_ICONINFORMATION);
        if (result == IDYES) {
            hse::Logger::info("Updating experimental launcher...");
            auto launcherResult = hse::UpdateManager::UpdateLauncher(
                experimentalInfo.downloadUrlBundle, std::nullopt, experimentalInfo.buildId
            );
            return static_cast<bool>(launcherResult);
        }
    }

    return true;
#else
    if (!cachedUpdateInfo_) {
        auto updateInfoResult = hse::UpdateManager::CheckForUpdates();
        if (!updateInfoResult) {
            hse::Logger::warn("Could not check for launcher updates. Installation will continue.");
            return true;
        }
        cachedUpdateInfo_ = *updateInfoResult;
    }

    auto& updateInfo = *cachedUpdateInfo_;
    if (!updateInfo.available) {
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
        hse::Logger::info("Launcher update skipped");
        return true;
    }

    hse::Logger::info("Updating launcher...");
    auto launcherUpdateResult =
        hse::UpdateManager::UpdateLauncher(updateInfo.downloadUrlBundle, updateInfo.remoteVersion);
    return static_cast<bool>(launcherUpdateResult);
#endif
}

bool HSELauncher::LocateGame() {
    if (auto savedPath = config.GetGamePath(); !savedPath.empty()) {
        auto savedLocation = hse::LocateGameAt(savedPath, config.GetGameEdition());
        if (savedLocation) {
            gameBinPath_ = std::move(savedLocation->binariesPath);
            gameEdition_ = savedLocation->edition;
            hse::Logger::info(
                "Half Sword found: %s (%s)", gameBinPath_.string().c_str(),
                hse::DescribeGameEdition(gameEdition_).displayName.data()
            );
            return true;
        }
        hse::Logger::warn("Half Sword is no longer available at its previous location. Searching again...");
    }

    hse::Logger::info("Searching for Half Sword...");
    auto location = hse::LocateGame();
    if (!location) {
        hse::Logger::warn("Half Sword was not found automatically");
        const auto manualPath = AskManualPath();
        if (manualPath.empty()) return false;

        location = hse::LocateGameAt(manualPath);
        if (!location) {
            hse::logAndShowError(
                "Half Sword was not found at: " + manualPath.string(), "That folder does not contain Half Sword.\n\n"
                                                                       "Please provide the path to the game folder, e.g.:\n"
                                                                       "D:\\SteamLibrary\\steamapps\\common\\Half Sword"
            );
            return false;
        }
    }

    gameBinPath_ = std::move(location->binariesPath);
    gameEdition_ = location->edition;
    if (auto saved = config.SetGameLocation(gameBinPath_, gameEdition_); !saved)
        hse::Logger::warn("Could not remember the Half Sword location. You may be asked for it again.");
    hse::Logger::info(
        "Half Sword found: %s (%s)", gameBinPath_.string().c_str(),
        hse::DescribeGameEdition(gameEdition_).displayName.data()
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
    path.erase(last + 1);
    path.erase(0, first);

    return std::filesystem::path(path);
}

#ifdef EXPERIMENTAL_VERSION
hse::InstallMode HSELauncher::GetInstallMode() {
    if (auto stored = ParseInstallMode(config.GetString(INSTALL_SECTION, INSTALL_MODE_KEY, ""))) {
        if (hse::IsInstallModeAvailable(gameBinPath_, *stored)) {
            hse::Logger::info("Installation method: %s", InstallModeName(*stored));
            return *stored;
        }
        hse::Logger::warn("UE4SS was not found, so the previous installation choice cannot be used");
    }

    const auto detectedMode = hse::DetectInstallMode(gameBinPath_);
    std::string message = "Choose how to install this build.\n\n"
                          "YES: Integrate with your existing UE4SS installation.\n\n"
                          "NO: Install directly into Half Sword.\n\n";

    message += detectedMode == hse::InstallMode::Ue4ss
                   ? "An existing UE4SS installation was found, so integration is recommended."
                   : "No UE4SS installation was found, so direct installation is recommended.";

    const UINT defaultButton = detectedMode == hse::InstallMode::Ue4ss ? MB_DEFBUTTON1 : MB_DEFBUTTON2;
    const int result =
        MessageBoxA(nullptr, message.c_str(), "Installation Method", MB_YESNO | MB_ICONQUESTION | defaultButton);
    const auto installMode = result == IDYES ? hse::InstallMode::Ue4ss : hse::InstallMode::Standalone;

    if (auto saveResult = config.SetString(INSTALL_SECTION, INSTALL_MODE_KEY, InstallModeConfigValue(installMode));
        !saveResult) {
        hse::Logger::warn("Could not remember the installation method. You may be asked again.");
    }

    hse::Logger::info("Installation method selected: %s", InstallModeName(installMode));
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
    if (needsInstall && !config.GetCheckForUpdates()) {
        auto prepared = hse::UpdateManager::InstallPreparedPackage(gameBinPath_, installMode);
        if (!prepared) {
            hse::logAndShowError(
                "Could not install the prepared Half Sword Enhancer package",
                "The bundled Half Sword Enhancer files could not be installed."
            );
            return false;
        }
        if (*prepared) return true;
    }

#ifdef EXPERIMENTAL_VERSION
    if (!cachedExperimentalInfo_) {
        auto experimentalUpdateResult = hse::UpdateManager::CheckForExperimentalUpdates();
        if (!experimentalUpdateResult) {
            if (needsInstall) {
                auto prepared = hse::UpdateManager::InstallPreparedPackage(gameBinPath_, installMode);
                if (prepared && *prepared) return true;
                hse::logAndShowError(
                    "Could not check available experimental versions",
                    "Could not connect to the update server. Please check your internet connection."
                );
            } else {
                hse::Logger::warn("Could not check for experimental updates. The installed version will be kept.");
            }
            return !needsInstall;
        }
        cachedExperimentalInfo_ = *experimentalUpdateResult;
    }

    const auto& info = *cachedExperimentalInfo_;
    if (!needsInstall) {
        if (!info.packageUpdateAvailable) {
            hse::Logger::info("Half Sword Enhancer is up to date");
            return true;
        }

        std::string message = "A new experimental Half Sword Enhancer version is available!\n\n"
                              "Do you want to install the update now?";
        if (MessageBoxA(nullptr, message.c_str(), "Half Sword Enhancer Update", MB_YESNO | MB_ICONINFORMATION) !=
            IDYES) {
            hse::Logger::info("Half Sword Enhancer update skipped");
            return true;
        }
    } else {
        hse::Logger::info("Half Sword Enhancer is not ready. Downloading the experimental version...");
    }

    if (auto result = hse::UpdateManager::DownloadAndInstallExperimentalMod(info, gameBinPath_, installMode); !result) {
        hse::logAndShowError(
            "Could not install the experimental version",
            "Half Sword Enhancer could not be installed or updated. Please check your internet connection and try "
            "again."
        );
        return false;
    }

    return true;
#else
    if (!cachedUpdateInfo_) {
        auto updateInfoResult = hse::UpdateManager::CheckForUpdates();
        if (!updateInfoResult) {
            if (needsInstall) {
                auto prepared = hse::UpdateManager::InstallPreparedPackage(gameBinPath_, installMode);
                if (prepared && *prepared) return true;
            } else {
                hse::Logger::warn("Could not check for updates. The installed version will be kept.");
                return true;
            }
            hse::logAndShowError(
                "Could not check the latest Half Sword Enhancer version",
                "Could not connect to the update server. Please check your internet connection."
            );
            return false;
        }
        cachedUpdateInfo_ = *updateInfoResult;
    }

    auto& updateInfo = *cachedUpdateInfo_;

    if (needsInstall) {
        hse::Logger::info("Half Sword Enhancer is not ready. Downloading the latest version...");
        return DownloadAndInstall(updateInfo.remoteVersion, installMode);
    }

    auto installedVersion = hse::UpdateManager::GetInstalledModVersion(gameBinPath_);
    if (!installedVersion) {
        std::string message = "The installed Half Sword Enhancer version could not be identified.\n\n"
                              "Available version: " +
                              updateInfo.remoteVersion.ToString() +
                              "\n\nDo you want to reinstall Half Sword Enhancer to ensure it is current?";
        if (MessageBoxA(nullptr, message.c_str(), "Version Unknown", MB_YESNO | MB_ICONQUESTION) != IDYES) {
            hse::Logger::info("Reinstallation skipped");
            return true;
        }
        return DownloadAndInstall(updateInfo.remoteVersion, installMode);
    }

    if (updateInfo.remoteVersion <= *installedVersion) {
        hse::Logger::info("Half Sword Enhancer is up to date (v%s)", installedVersion->ToString().c_str());
        return true;
    }

    std::string message = "A Half Sword Enhancer update is available!\n\n"
                          "Installed: " +
                          installedVersion->ToString() +
                          "\n"
                          "Available: " +
                          updateInfo.remoteVersion.ToString() +
                          "\n\n"
                          "Do you want to update now?";
    if (MessageBoxA(nullptr, message.c_str(), "Update Available", MB_YESNO | MB_ICONINFORMATION) != IDYES) {
        hse::Logger::info("Half Sword Enhancer update skipped");
        return true;
    }
    return DownloadAndInstall(updateInfo.remoteVersion, installMode);
#endif
}

bool HSELauncher::DownloadAndInstall(const hse::Version& version, hse::InstallMode installMode) {
    auto result = hse::UpdateManager::DownloadAndInstallMod(version, gameBinPath_, installMode);
    if (!result) {
        hse::logAndShowError(
            "Could not install Half Sword Enhancer v" + version.ToString(),
            "Half Sword Enhancer could not be downloaded and installed. Please check your internet connection and "
            "try again."
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
        const auto& edition = hse::DescribeGameEdition(gameEdition_);
        const auto* steamUrl = edition.steamUrl.data();
        hse::Logger::info("Launching %s via Steam...", edition.displayName.data());
        ShellExecuteA(nullptr, "open", steamUrl, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void HSELauncher::ShowExitMessage(bool success) {
    if (success) hse::Logger::info("Half Sword Enhancer is ready");
    hse::Logger::info("Exiting in %d seconds...", EXIT_DELAY_SECONDS);
    std::this_thread::sleep_for(std::chrono::seconds(EXIT_DELAY_SECONDS));
}

int HSELauncher::Run(int /*argc*/, char* /*argv*/[]) {
    try {
        SetupConsole();
        DisplayBanner();
        if (config.IsFirstRun()) ShowFirstRunInstructions();

        if (auto prepared = hse::UpdateManager::PrepareBundledPackage(); !prepared) {
            hse::logAndShowError(
                "Could not prepare the bundled installation files",
                "The bundled Half Sword Enhancer files could not be prepared."
            );
            ShowExitMessage(false);
            return 1;
        }

        if (!PerformSelfUpdate()) {
            ShowExitMessage(false);
            return 1;
        }

        if (!LocateGame()) {
            ShowExitMessage(false);
            return 1;
        }

        if (FindWindowA("UnrealWindow", nullptr) != nullptr) {
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
