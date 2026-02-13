#include "Utils/GameBuildInfo.h"
#include "SDK/Engine_classes.hpp"
#include "Logger.h"
#include "Version.h"

GameBuildInfo& GameBuildInfo::Get() {
    static GameBuildInfo instance;
    return instance;
}

void GameBuildInfo::Query() {
    static Logger logger{ "Compat" };
    auto& info = Get();
    if (info.queried.load(std::memory_order_acquire)) return;

    info.buildVersion = SDK::UKismetSystemLibrary::GetBuildVersion().ToString();
    info.engineVersion = SDK::UKismetSystemLibrary::GetEngineVersion().ToString();
    info.buildConfig = SDK::UKismetSystemLibrary::GetBuildConfiguration().ToString();

    logger.Log("Game Build: %s", info.buildVersion.c_str());
    logger.Log("Engine: %s", info.engineVersion.c_str());
    logger.Log("Config: %s", info.buildConfig.c_str());

    if (info.buildVersion != HSE_TARGET_BUILD) {
        logger.Log("WARNING: Build mismatch! Expected: %s", HSE_TARGET_BUILD);
        info.mismatchDetected.store(true, std::memory_order_release);
    }

    info.queried.store(true, std::memory_order_release);
}
