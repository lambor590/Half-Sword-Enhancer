#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <atomic>
#include <algorithm>
#include <unordered_set>

#include "Utils/BlueprintRegistry.h"
#include "Hooks/GameHook.h"
#include "SDK/AssetRegistry_classes.hpp"
#include "SDK/AssetRegistry_parameters.hpp"
#include "Logger.h"

struct MapEntry {
    std::string displayName;
    std::string packageName;
    std::string category;
};

class MapRegistry {
    MapRegistry() = default;

    static constexpr std::string_view BASE_GAME_CATEGORY = "Base Game";

    std::atomic<ScanState> state{ScanState::NotStarted};
    std::vector<MapEntry> maps;
    std::vector<std::string> categories;

    static bool HasBadSuffix(std::string_view name) {
        static constexpr std::string_view BAD_SUFFIXES[] = {
            "_BuiltData", "_HLOD", "_Minimap", "_NavData",
            "_Overview", "_Collision", "_LevelMetrics",
        };
        for (auto suffix : BAD_SUFFIXES) {
            if (name.size() >= suffix.size() &&
                name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
                return true;
        }
        return false;
    }

    static bool PathContainsMaps(std::string_view path) {
        return path.find("/Maps/") != std::string_view::npos ||
               (path.size() >= 5 && path.compare(path.size() - 5, 5, "/Maps") == 0);
    }

    static std::string CategorizeByPath(std::string_view packagePath) {
        if (packagePath.find("/Game/Maps") == 0)
            return std::string(BASE_GAME_CATEGORY);

        constexpr std::string_view MOD_PREFIX = "/Game/Mod_";
        if (packagePath.find(MOD_PREFIX) == 0) {
            auto rest = packagePath.substr(MOD_PREFIX.size());
            auto slash = rest.find('/');
            std::string modName(rest.substr(0, slash));
            for (char& c : modName) {
                if (c == '_') c = ' ';
            }
            return modName.empty() ? "Mods" : modName;
        }

        return {};
    }

    static std::string CleanMapName(std::string_view packageName) {
        auto lastSlash = packageName.rfind('/');
        if (lastSlash != std::string_view::npos)
            packageName = packageName.substr(lastSlash + 1);

        std::string name(packageName);
        for (char& c : name) {
            if (c == '_') c = ' ';
        }

        auto start = name.find_first_not_of(' ');
        if (start == std::string::npos) return {};
        auto end = name.find_last_not_of(' ');
        name.erase(end + 1);
        name.erase(0, start);
        return name;
    }

    void BuildCategories() {
        categories.clear();
        for (const auto& m : maps) {
            if (categories.empty() || categories.back() != m.category)
                categories.push_back(m.category);
        }
    }

    void PerformScan() {
        static Logger logger("MapRegistry");
        maps.clear();
        categories.clear();

        try {
            auto si = SDK::UAssetRegistryHelpers::GetAssetRegistry();
            auto* registryObj = si.GetObjectRef();
            if (!registryObj || !registryObj->Class) {
                logger.Log("Asset Registry not available");
                state.store(ScanState::Failed, std::memory_order_release);
                return;
            }

            static SDK::UFunction* getAssetsByPathFn = nullptr;
            if (!getAssetsByPathFn) {
                auto* ifaceClass = SDK::IAssetRegistry::StaticClass();
                if (ifaceClass)
                    getAssetsByPathFn = ifaceClass->GetFunction("AssetRegistry", "GetAssetsByPath");
            }
            if (!getAssetsByPathFn) {
                logger.Log("GetAssetsByPath function not found");
                state.store(ScanState::Failed, std::memory_order_release);
                return;
            }

            SDK::Params::AssetRegistry_GetAssetsByPath params{};
            params.PackagePath = SDK::BasicFilesImpleUtils::StringToName(L"/Game");
            params.bRecursive = true;
            params.bIncludeOnlyOnDiskAssets = false;

            auto flags = getAssetsByPathFn->FunctionFlags;
            getAssetsByPathFn->FunctionFlags |= 0x400;
            registryObj->ProcessEvent(getAssetsByPathFn, &params);
            getAssetsByPathFn->FunctionFlags = flags;

            const int32_t count = params.OutAssetData.Num();
            logger.Log("Scan returned %d total assets under /Game", count);

            static const SDK::FName worldClassName = SDK::BasicFilesImpleUtils::StringToName(L"World");
            std::unordered_set<std::string> seen;
            maps.reserve(32);

            for (int32_t i = 0; i < count; ++i) {
                if (!(params.OutAssetData[i].AssetClassPath.AssetName == worldClassName))
                    continue;

                std::string packagePath = params.OutAssetData[i].PackagePath.GetRawString();
                if (!PathContainsMaps(packagePath))
                    continue;

                std::string packageName = params.OutAssetData[i].PackageName.GetRawString();
                if (HasBadSuffix(packageName))
                    continue;

                if (!seen.insert(packageName).second)
                    continue;

                std::string category = CategorizeByPath(packagePath);
                if (category.empty())
                    continue;

                std::string displayName = CleanMapName(packageName);
                if (displayName.empty())
                    continue;

                maps.push_back({std::move(displayName), std::move(packageName), std::move(category)});
            }

            std::sort(maps.begin(), maps.end(),
                [](const MapEntry& a, const MapEntry& b) {
                    if (a.category != b.category) {
                        bool aBase = (a.category == BASE_GAME_CATEGORY);
                        bool bBase = (b.category == BASE_GAME_CATEGORY);
                        if (aBase != bBase) return aBase;
                        return a.category < b.category;
                    }
                    return a.displayName < b.displayName;
                });

            BuildCategories();
            logger.Log("Map scan complete: %zu maps in %zu categories", maps.size(), categories.size());
        } catch (...) {
            logger.Log("Map scan failed with exception");
        }

        state.store(maps.empty() ? ScanState::Failed : ScanState::Complete, std::memory_order_release);
    }

public:
    static MapRegistry& Get() {
        static MapRegistry instance;
        return instance;
    }

    void RequestScan() {
        ScanState expected = ScanState::NotStarted;
        if (state.compare_exchange_strong(expected, ScanState::Scanning, std::memory_order_acq_rel)) {
            GameHook::QueueAction([this]() { PerformScan(); });
        }
    }

    void RequestRescan() {
        ScanState expected = ScanState::Complete;
        if (!state.compare_exchange_strong(expected, ScanState::Scanning, std::memory_order_acq_rel)) {
            expected = ScanState::Failed;
            if (!state.compare_exchange_strong(expected, ScanState::Scanning, std::memory_order_acq_rel))
                return;
        }
        GameHook::QueueAction([this]() { PerformScan(); });
    }

    ScanState GetState() const { return state.load(std::memory_order_acquire); }
    const std::vector<MapEntry>& GetMaps() const { return maps; }
    const std::vector<std::string>& GetCategories() const { return categories; }

    MapRegistry(const MapRegistry&) = delete;
    MapRegistry& operator=(const MapRegistry&) = delete;
};
