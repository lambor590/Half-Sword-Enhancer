#pragma once

#include <string>
#include <atomic>

struct GameBuildInfo {
    std::string buildVersion;
    std::atomic_flag queryStarted = ATOMIC_FLAG_INIT;
    std::atomic<bool> mismatchDetected = false;

    static GameBuildInfo& Get();
    static void Query();
};
