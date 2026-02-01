#pragma once

#include <string>
#include <optional>
#include <Windows.h>

#include "UpdateManager.h"
#include "ProcessManager.h"
#include "LauncherConfig.h"

class HSELauncher {
    static constexpr int EXIT_DELAY_SECONDS = 3;
    static constexpr int CONSOLE_RED = FOREGROUND_RED | FOREGROUND_INTENSITY;
    static constexpr int CONSOLE_YELLOW = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    static constexpr int CONSOLE_WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

    hse::UpdateManager& updateManager;
    hse::ProcessManager& processManager;
    hse::LauncherConfig& config;

#ifdef DEV_VERSION
    std::optional<hse::DevUpdateInfo> cachedDevInfo_;
#endif

    void DisplayBanner();
    void SetupConsole();
    bool HandleDraggedDLL(int argc, char* argv[]);
    void ShowFirstRunInstructions();
    bool AskForUpdatePreference();
    bool EnsureModExists();
    bool PerformUpdatesIfNeeded();
    bool InjectMod();
    void ShowExitMessage(bool success);

public:
    HSELauncher();
    ~HSELauncher() = default;
    HSELauncher(const HSELauncher&) = delete;
    HSELauncher& operator=(const HSELauncher&) = delete;

    int Run(int argc, char* argv[]);
};