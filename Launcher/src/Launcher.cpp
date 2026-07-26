#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <Windows.h>
#include <ShObjIdl.h>
#include <wrl/client.h>

#include "../include/Launcher.h"
#include "../include/InstallManager.h"
#include "../include/LauncherConfig.h"
#include "../include/SteamLocator.h"
#include "../include/Util.h"

namespace {
    constexpr int EXIT_DELAY_SECONDS = 3;
    constexpr int CONSOLE_RED = FOREGROUND_RED | FOREGROUND_INTENSITY;
    constexpr int CONSOLE_YELLOW = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    constexpr int CONSOLE_WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

    class ComApartment {
    public:
        ComApartment() : result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}
        ~ComApartment() {
            if (SUCCEEDED(result)) CoUninitialize();
        }

        [[nodiscard]] bool Available() const noexcept { return SUCCEEDED(result); }

    private:
        HRESULT result;
    };

    [[nodiscard]] std::filesystem::path BrowseForGameFolder() {
        ComApartment apartment;
        if (!apartment.Available()) return {};

        Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
            return {};

        FILEOPENDIALOGOPTIONS options = 0;
        if (FAILED(dialog->GetOptions(&options)) ||
            FAILED(dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST)))
            return {};

        (void)dialog->SetTitle(L"Select your Half Sword installation");
        (void)dialog->SetOkButtonLabel(L"Select folder");
        const HRESULT shown = dialog->Show(GetConsoleWindow());
        if (FAILED(shown)) return {};

        Microsoft::WRL::ComPtr<IShellItem> item;
        if (FAILED(dialog->GetResult(&item))) return {};

        PWSTR selectedPath = nullptr;
        if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath))) return {};
        std::filesystem::path result(selectedPath);
        CoTaskMemFree(selectedPath);
        return result;
    }

}

HSELauncher::HSELauncher() : config(hse::LauncherConfig::Instance()), gameEdition_(hse::GameEdition::FullGame) {}

void HSELauncher::SetupConsole() {
    SetConsoleOutputCP(CP_UTF8);

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
    /_/ /_/\__,_/_/_/     /____/|__/|__/\____/_/   \__,_/)"
              << "\n\n";
    SetConsoleTextAttribute(hConsole, CONSOLE_YELLOW);

    std::cout << "        ______      __";
#ifdef EXPERIMENTAL_VERSION
    std::cout << "                         [ EXPERIMENTAL BUILD ]";
#endif
    std::cout << R"(
       / ____/___  / /_  ____ _____  ________  _____
      / __/ / __ \/ __ \/ __ `/ __ \/ ___/ _ \/ ___/
     / /___/ / / / / / / /_/ / / / / /__/  __/ /
    /_____/_/ /_/_/ /_/\__,_/_/ /_/\___/\___/_/)"
              << "\n\n";

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

    hse::Logger::info("Checking for launcher updates...");

#ifdef EXPERIMENTAL_VERSION
    if (!cachedExperimentalInfo_) {
        auto experimentalUpdateResult = hse::UpdateManager::CheckForExperimentalUpdates();
        if (!experimentalUpdateResult) {
            hse::Logger::warn("Could not check for experimental launcher updates. Continuing with this launcher.");
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
                "Half Sword found: %s (%s)", hse::PathToUtf8(gameBinPath_).c_str(),
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
            hse::Logger::error("Half Sword was not found at: %s", hse::PathToUtf8(manualPath).c_str());
            std::wstring message = L"That selection does not contain Half Sword.\n\n"
                                   L"Select the Half Sword or Half Sword Demo installation folder.\n\n"
                                   L"Selected path:\n";
            message.append(manualPath.native());
            MessageBoxW(nullptr, message.c_str(), L"Half Sword Not Found", MB_OK | MB_ICONWARNING);
            return false;
        }
    }

    gameBinPath_ = std::move(location->binariesPath);
    gameEdition_ = location->edition;
    if (auto saved = config.SetGameLocation(gameBinPath_, gameEdition_); !saved)
        hse::Logger::warn("Could not remember the Half Sword location. You may be asked for it again.");
    hse::Logger::info(
        "Half Sword found: %s (%s)", hse::PathToUtf8(gameBinPath_).c_str(),
        hse::DescribeGameEdition(gameEdition_).displayName.data()
    );
    return true;
}

std::filesystem::path HSELauncher::AskManualPath() {
    hse::Logger::info("Select your Half Sword installation folder.");
    return BrowseForGameFolder();
}

#ifdef EXPERIMENTAL_VERSION
hse::InstallMode HSELauncher::GetInstallMode() {
    const auto installMode = hse::DetectInstallMode(gameBinPath_);
    hse::Logger::info(
        "Installation method: %s",
        installMode == hse::InstallMode::Ue4ss ? "detected UE4SS integration" : "direct installation"
    );
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
                hse::Logger::warn("Could not check for experimental mod updates. The installed mod will be kept.");
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
    hse::Logger::info("Would you like to launch the game through Steam? [Y/n]");
    std::cout << "  > ";

    std::string input;
    std::getline(std::cin, input);

    if (input.empty() || input[0] == 'Y' || input[0] == 'y') {
        const auto& edition = hse::DescribeGameEdition(gameEdition_);
        hse::Logger::info("Launching %s via Steam...", edition.displayName.data());
        if (auto launched = hse::LaunchGameThroughSteam(gameEdition_); !launched) {
            hse::logAndShowError(
                "Steam did not accept the game launch request",
                "Half Sword could not be started through Steam.\n\n"
                "The mod is installed and ready. Please start Half Sword from your Steam Library."
            );
        }
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
            hse::Logger::error("Cannot write to game folder: %s", hse::PathToUtf8(gameBinPath_).c_str());
            std::wstring message = L"Cannot write to the game folder.\n\n"
                                   L"Please run the installer as administrator or check the folder permissions.\n\n"
                                   L"Path:\n";
            message.append(gameBinPath_.native());
            MessageBoxW(nullptr, message.c_str(), L"Game Folder Is Not Writable", MB_OK | MB_ICONERROR);
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
