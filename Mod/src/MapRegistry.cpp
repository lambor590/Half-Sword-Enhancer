#include <algorithm>
#include <ranges>
#include <string_view>
#include <unordered_set>
#include "Utils/MapRegistry.h"
#include "Hooks/GameHook.h"
#include "SDK/AssetRegistry_classes.hpp"
#include "SDK/AssetRegistry_parameters.hpp"
#include "imgui/imgui.h"
#include "Logger.h"

static Logger g_logger("MapRegistry");

namespace {
    constexpr std::string_view BASE_GAME_CATEGORY = "Base Game";
    constexpr std::string_view MODDED_CATEGORY = "Modded";

    bool IsInternalMapAsset(std::string_view packageName, std::string_view packagePath) {
        static constexpr std::string_view BAD_SUFFIXES[] = {
            "_BuiltData", "_HLOD", "_Minimap", "_NavData", "_Overview", "_Collision", "_LevelMetrics",
            "_Details", "_Lighting", "_PhysicsProps", "_VFX",
        };
        for (auto suffix : BAD_SUFFIXES) {
            if (packageName.ends_with(suffix)) return true;
        }

        static constexpr std::string_view INTERNAL_TOKENS[] = {
            "LevelInstance",
            "levelinstance",
            "__ExternalActors__",
            "__ExternalObjects__",
        };
        for (auto token : INTERNAL_TOKENS) {
            if (packageName.find(token) != std::string_view::npos || packagePath.find(token) != std::string_view::npos)
                return true;
        }

        static constexpr std::string_view SUBMAP_PATHS[] = {
            "/Game/Maps/LevelInstances",
            "/Game/Maps/Lighting",
            "/Game/Maps/VFX_Level_Layer",
            "/Game/Maps/VFX_arena_cutting_volumetric_particles",
            "/Game/Maps/Arena_Cellar",
            "/Game/Maps/Arena_LordsHall",
            "/Game/Maps/Arenas/Arena_Alley",
            "/Game/Maps/Arenas/Arena_Ambush",
            "/Game/Maps/Arenas/Arena_Pit",
            "/Game/Maps/Arenas/Arena_Slums",
        };
        for (auto submapPath : SUBMAP_PATHS) {
            if (packagePath == submapPath ||
                (packagePath.starts_with(submapPath) && packagePath[submapPath.size()] == '/'))
                return true;
        }

        return false;
    }

    bool IsKnownBaseGameMap(std::string_view packageName) {
        static constexpr std::string_view BASE_GAME_MAPS[] = {
            "/Game/Maps/Abyss/Abyss_Map_Open_EA",
            "/Game/Maps/Arena_Cutting_Map",
            "/Game/Maps/Arenas/Map_Arena_Alley",
            "/Game/Maps/Arenas/Map_Arena_Ambush_Test",
            "/Game/Maps/Arenas/Map_Arena_Cellar",
            "/Game/Maps/Arenas/Map_Arena_EastTower",
            "/Game/Maps/Arenas/Map_Arena_LordsHall",
            "/Game/Maps/Arenas/Map_Arena_Pit",
            "/Game/Maps/Arenas/Map_Arena_Slums",
            "/Game/Maps/Arenas/Map_Arena_Yard",
            "/Game/Maps/Map_Hub_Tavern_Frank",
            "/Game/Maps/Menus/Map_Menu_SplashScreens",
            "/Game/Maps/Menus/Map_Menu_Startup",
            "/Game/Maps/Test/Map_Test_Empty",
            "/Game/Maps/Workshop_Smithery_Map",
        };

        return std::ranges::binary_search(BASE_GAME_MAPS, packageName);
    }

    std::string CleanMapName(std::string_view packageName) {
        auto lastSlash = packageName.rfind('/');
        if (lastSlash != std::string_view::npos) packageName = packageName.substr(lastSlash + 1);

        std::string name(packageName);
        for (char& c : name) {
            if (c == '_') c = ' ';
        }

        auto start = name.find_first_not_of(' ');
        if (start == std::string::npos) return {};
        auto end = name.find_last_not_of(' ');
        if (start == 0 && end == name.size() - 1) return name;
        name.erase(end + 1);
        name.erase(0, start);
        return name;
    }
}

void MapRegistry::PerformScan() {
    maps.clear();
    categories.clear();

    try {
        auto si = SDK::UAssetRegistryHelpers::GetAssetRegistry();
        auto* registryObj = si.GetObjectRef();
        if (!registryObj || !registryObj->Class) {
            g_logger.Log("Asset Registry not available");
            state.store(ScanState::Failed, std::memory_order_release);
            return;
        }

        auto* ifaceClass = SDK::IAssetRegistry::StaticClass();

        static SDK::UFunction* scanPathsFn = nullptr;
        if (!scanPathsFn) {
            if (ifaceClass) scanPathsFn = ifaceClass->GetFunction("AssetRegistry", "ScanPathsSynchronous");
        }
        if (scanPathsFn) {
            SDK::FString gamePathString(L"/Game");
            SDK::Params::AssetRegistry_ScanPathsSynchronous scanParams{};
            scanParams.InPaths = SDK::TArray<SDK::FString>(&gamePathString, 1, 1);
            scanParams.bForceRescan = true;
            scanParams.bIgnoreDenyListScanFilters = true;

            auto flags = scanPathsFn->FunctionFlags;
            scanPathsFn->FunctionFlags |= 0x400;
            registryObj->ProcessEvent(scanPathsFn, &scanParams);
            scanPathsFn->FunctionFlags = flags;

            scanParams.InPaths = SDK::TArray<SDK::FString>(nullptr, 0, 0);
        } else {
            g_logger.Log("ScanPathsSynchronous function not found");
        }

        static SDK::UFunction* getAssetsFn = nullptr;
        if (!getAssetsFn) {
            if (ifaceClass) getAssetsFn = ifaceClass->GetFunction("AssetRegistry", "GetAssets");
        }
        if (!getAssetsFn) {
            g_logger.Log("GetAssets function not found");
            state.store(ScanState::Failed, std::memory_order_release);
            return;
        }

        SDK::FName gamePath = SDK::BasicFilesImplUtils::StringToName(L"/Game");
        SDK::FTopLevelAssetPath worldClassPath{};
        worldClassPath.PackageName = SDK::BasicFilesImplUtils::StringToName(L"/Script/Engine");
        worldClassPath.AssetName = SDK::BasicFilesImplUtils::StringToName(L"World");

        SDK::Params::AssetRegistry_GetAssets params{};
        params.Filter.PackagePaths = SDK::TArray<SDK::FName>(&gamePath, 1, 1);
        params.Filter.ClassPaths = SDK::TArray<SDK::FTopLevelAssetPath>(&worldClassPath, 1, 1);
        params.Filter.bRecursivePaths = true;
        params.Filter.bRecursiveClasses = false;
        params.bSkipARFilteredAssets = false;

        auto flags = getAssetsFn->FunctionFlags;
        getAssetsFn->FunctionFlags |= 0x400;
        registryObj->ProcessEvent(getAssetsFn, &params);
        getAssetsFn->FunctionFlags = flags;

        params.Filter.PackagePaths = SDK::TArray<SDK::FName>(nullptr, 0, 0);
        params.Filter.ClassPaths = SDK::TArray<SDK::FTopLevelAssetPath>(nullptr, 0, 0);

        const int32_t count = params.OutAssetData.Num();

        std::unordered_set<uint64_t> seenPackages;
        seenPackages.reserve(count);
        maps.reserve(count);

        for (int32_t i = 0; i < count; ++i) {
            const auto& asset = params.OutAssetData[i];

            const uint64_t packageId = (static_cast<uint64_t>(asset.PackageName.ComparisonIndex) << 32) |
                                       static_cast<uint64_t>(asset.PackageName.Number);
            if (!seenPackages.insert(packageId).second) continue;

            std::string packageName = asset.PackageName.GetRawString();
            std::string packagePath = asset.PackagePath.GetRawString();
            if (IsInternalMapAsset(packageName, packagePath)) continue;

            std::string displayName = CleanMapName(packageName);
            if (displayName.empty()) continue;

            const std::string_view category = IsKnownBaseGameMap(packageName) ? BASE_GAME_CATEGORY : MODDED_CATEGORY;
            maps.push_back({std::move(displayName), std::move(packageName), category});
        }

        std::ranges::sort(maps, [](const MapEntry& a, const MapEntry& b) {
            if (a.category != b.category) return a.category == BASE_GAME_CATEGORY;
            return a.displayName < b.displayName;
        });

        for (const auto& map : maps) {
            if (categories.empty() || categories.back() != map.category) categories.emplace_back(map.category);
        }
        displayWidthDirty = true;
    } catch (...) {
        g_logger.Log("Map scan failed with exception");
    }

    state.store(maps.empty() ? ScanState::Failed : ScanState::Complete, std::memory_order_release);
}

void MapRegistry::RequestScan() {
    ScanState expected = ScanState::NotStarted;
    if (state.compare_exchange_strong(expected, ScanState::Scanning, std::memory_order_acq_rel)) {
        if (!GameHook::QueueAction([this](const RuntimeContextSnapshot&) { PerformScan(); }))
            state.store(ScanState::NotStarted, std::memory_order_release);
    }
}

void MapRegistry::RequestRescan() {
    auto current = state.load(std::memory_order_acquire);
    if (current != ScanState::Complete && current != ScanState::Failed) return;
    if (state.compare_exchange_strong(current, ScanState::Scanning, std::memory_order_acq_rel) &&
        !GameHook::QueueAction([this](const RuntimeContextSnapshot&) { PerformScan(); }))
        state.store(current, std::memory_order_release);
}

float MapRegistry::GetMaxDisplayNameWidth() {
    if (displayWidthDirty) {
        maxDisplayNameWidth = 0.0f;
        for (const auto& m : maps) {
            maxDisplayNameWidth = (std::max)(maxDisplayNameWidth, ImGui::CalcTextSize(m.displayName.c_str()).x);
        }
        displayWidthDirty = false;
    }
    return maxDisplayNameWidth;
}
