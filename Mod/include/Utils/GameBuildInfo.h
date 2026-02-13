#pragma once

#include <string>
#include <atomic>

struct GameBuildInfo {
    std::string buildVersion;
    std::string engineVersion;
    std::string buildConfig;
    std::atomic<bool> queried = false;
    std::atomic<bool> mismatchDetected = false;

    static GameBuildInfo& Get();
    static void Query();
};
