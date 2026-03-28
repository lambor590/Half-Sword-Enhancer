#pragma once

#include <string>
#include <optional>
#include <filesystem>
#include <Windows.h>

#include "UpdateManager.h"
#include "SteamLocator.h"
#include "InstallManager.h"
#include "LauncherConfig.h"
#include "Util.h"

class HSELauncher {
#if __has_include("launcher_ext.h")
    friend struct lext;
#endif
    static constexpr int EXIT_DELAY_SECONDS = 3;
    static constexpr int CONSOLE_RED = FOREGROUND_RED | FOREGROUND_INTENSITY;
    static constexpr int CONSOLE_YELLOW = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    static constexpr int CONSOLE_WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

    hse::UpdateManager& updateManager;
    hse::SteamLocator& steamLocator;
    hse::InstallManager& installManager;
    hse::LauncherConfig& config;

    std::filesystem::path gameBinPath_;
    hse::GameEdition gameEdition_ = hse::GameEdition::FullGame;

#ifdef EXPERIMENTAL_VERSION
    std::optional<hse::ExperimentalUpdateInfo> cachedExperimentalInfo_;
#else
    std::optional<hse::UpdateInfo> cachedUpdateInfo_;
#endif

    void DisplayBanner();
    void SetupConsole();
    void ShowFirstRunInstructions();
    bool AskForUpdatePreference();
    bool PerformSelfUpdate();
    bool LocateGame();
    std::string AskManualPath();
    bool CheckAndInstallMod();
    bool DownloadAndInstall(const hse::Version& version);
    void OfferGameLaunch();
    void ShowExitMessage(bool success);

public:
    HSELauncher();
    ~HSELauncher() = default;
    HSELauncher(const HSELauncher&) = delete;
    HSELauncher& operator=(const HSELauncher&) = delete;

    int Run(int argc, char* argv[]);
};
