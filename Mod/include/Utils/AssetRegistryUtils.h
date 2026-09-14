#pragma once

#include "SDK/AssetRegistry_classes.hpp"
#include "SDK/AssetRegistry_parameters.hpp"

namespace AssetRegistryUtils {
    inline bool RefreshGameAssets(SDK::UObject* registry) {
        if (!registry) return false;
        static auto* function = SDK::IAssetRegistry::StaticClass()->GetFunction("AssetRegistry", "ScanPathsSynchronous");
        if (!function) return false;

        // A mod's cooked registry can hide base-game entries; scan the mounted packages together.
        SDK::FString path(L"/Game");
        SDK::Params::AssetRegistry_ScanPathsSynchronous params{};
        params.InPaths = SDK::TArray<SDK::FString>(&path, 1, 1);
        params.bForceRescan = true;
        params.bIgnoreDenyListScanFilters = true;
        registry->ProcessEvent(function, &params);
        return true;
    }
}
