#pragma once

#include "SDK/AssetRegistry_classes.hpp"
#include "SDK/AssetRegistry_parameters.hpp"

namespace AssetRegistryUtils {
    // The rescan blocks the game thread for a few hundred milliseconds, so the Item Spawner and Map Setup share one
    // per session; their Rescan buttons force another.
    inline bool RefreshGameAssets(SDK::UObject* registry, bool force) {
        static bool refreshed = false;
        if (!registry) return false;
        if (refreshed && !force) return true;
        static auto* function = SDK::IAssetRegistry::StaticClass()->GetFunction("AssetRegistry", "ScanPathsSynchronous");
        if (!function) return false;

        // A mod's cooked registry can hide base-game entries; scan the mounted packages together.
        SDK::FString path(L"/Game");
        SDK::Params::AssetRegistry_ScanPathsSynchronous params{};
        params.InPaths = SDK::TArray<SDK::FString>(&path, 1, 1);
        params.bForceRescan = true;
        params.bIgnoreDenyListScanFilters = true;
        registry->ProcessEvent(function, &params);
        refreshed = true;
        return true;
    }
}
