#include "Utils/GameBuildInfo.h"
#include "SDK/Engine_classes.hpp"
#include "SDK/Engine_parameters.hpp"
#include "Logger.h"
#include "Version.h"

GameBuildInfo& GameBuildInfo::Get() {
    static GameBuildInfo instance;
    return instance;
}

void GameBuildInfo::Query() {
    static Logger logger{"Compat"};
    auto& info = Get();
    if (info.queryStarted.test_and_set(std::memory_order_acq_rel)) return;

    auto* systemLibraryClass = SDK::UKismetSystemLibrary::StaticClass();
    auto* getBuildVersion =
        systemLibraryClass ? systemLibraryClass->GetFunction("KismetSystemLibrary", "GetBuildVersion") : nullptr;
    auto* systemLibrary = getBuildVersion ? SDK::UKismetSystemLibrary::GetDefaultObj() : nullptr;
    if (!systemLibrary) {
        logger.Log("Game build version is unavailable");
        return;
    }

    SDK::Params::KismetSystemLibrary_GetBuildVersion params{};
    const auto flags = getBuildVersion->FunctionFlags;
    getBuildVersion->FunctionFlags |= 0x400;
    systemLibrary->ProcessEvent(getBuildVersion, &params);
    getBuildVersion->FunctionFlags = flags;
    info.buildVersion = params.ReturnValue.ToString();

    logger.Log("Game Build: %s", info.buildVersion.c_str());

    if (info.buildVersion != HSE_TARGET_BUILD) {
        logger.Log("WARNING: Build mismatch! Expected: %s", HSE_TARGET_BUILD);
        info.mismatchDetected.store(true, std::memory_order_release);
    }
}
