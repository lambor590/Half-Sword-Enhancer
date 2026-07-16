#pragma once

#include <optional>
#include <cstdint>
#include <filesystem>

#include "UpdateManager.h"

namespace hse {
    enum class GameEdition : std::uint8_t;
    enum class InstallMode : std::uint8_t;
    class LauncherConfig;
}

class HSELauncher {
    hse::LauncherConfig& config;

    std::filesystem::path gameBinPath_;
    hse::GameEdition gameEdition_;

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
    std::filesystem::path AskManualPath();
#ifdef EXPERIMENTAL_VERSION
    hse::InstallMode GetInstallMode();
#endif
    bool CheckAndInstallMod();
    bool DownloadAndInstall(const hse::Version& version, hse::InstallMode installMode);
    void OfferGameLaunch();
    void ShowExitMessage(bool success);

public:
    HSELauncher();
    ~HSELauncher() = default;
    HSELauncher(const HSELauncher&) = delete;
    HSELauncher& operator=(const HSELauncher&) = delete;

    int Run(int argc, char* argv[]);
};
