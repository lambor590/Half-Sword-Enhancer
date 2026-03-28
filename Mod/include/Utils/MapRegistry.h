#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <atomic>
#include <algorithm>
#include <ranges>
#include <unordered_set>

#include "imgui/imgui.h"
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
    float maxDisplayNameWidth = 0.0f;
    bool displayWidthDirty = true;

    [[nodiscard]] static bool EndsWith(std::string_view str, std::string_view suffix) {
        return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    [[nodiscard]] static bool HasBadSuffix(std::string_view name) {
        static constexpr std::string_view BAD_SUFFIXES[] = {
            "_BuiltData", "_HLOD", "_Minimap", "_NavData", "_Overview", "_Collision", "_LevelMetrics",
        };
        for (auto suffix : BAD_SUFFIXES) {
            if (EndsWith(name, suffix)) return true;
        }
        return false;
    }

    [[nodiscard]] static bool PathContainsMaps(std::string_view path) {
        return path.find("/Maps/") != std::string_view::npos || EndsWith(path, "/Maps");
    }

    [[nodiscard]] static bool StartsWith(std::string_view str, std::string_view prefix) {
        return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
    }

    [[nodiscard]] static std::string CategorizeByPath(std::string_view packagePath) {
        if (StartsWith(packagePath, "/Game/Maps")) return std::string(BASE_GAME_CATEGORY);

        constexpr std::string_view MOD_PREFIX = "/Game/Mod_";
        if (StartsWith(packagePath, MOD_PREFIX)) {
            auto rest = packagePath.substr(MOD_PREFIX.size());
            std::string modName(rest.substr(0, rest.find('/')));
            for (char& c : modName) {
                if (c == '_') c = ' ';
            }
            return modName.empty() ? "Mods" : modName;
        }

        return {};
    }

    [[nodiscard]] static std::string CleanMapName(std::string_view packageName) {
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

    void BuildCategories() {
        categories.clear();
        for (const auto& m : maps) {
            if (categories.empty() || categories.back() != m.category) categories.push_back(m.category);
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

            static SDK::UFunction* getAssetsFn = nullptr;
            if (!getAssetsFn) {
                auto* ifaceClass = SDK::IAssetRegistry::StaticClass();
                if (ifaceClass) getAssetsFn = ifaceClass->GetFunction("AssetRegistry", "GetAssets");
            }
            if (!getAssetsFn) {
                logger.Log("GetAssets function not found");
                state.store(ScanState::Failed, std::memory_order_release);
                return;
            }

            SDK::FName gamePath = SDK::BasicFilesImpleUtils::StringToName(L"/Game");
            SDK::FTopLevelAssetPath worldClassPath{};
            worldClassPath.PackageName = SDK::BasicFilesImpleUtils::StringToName(L"/Script/Engine");
            worldClassPath.AssetName = SDK::BasicFilesImpleUtils::StringToName(L"World");

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
            logger.Log("Scan returned %d World assets under /Game", count);

            std::unordered_set<int32_t> seenIds;
            seenIds.reserve(count);
            maps.reserve(count);

            for (int32_t i = 0; i < count; ++i) {
                const auto& asset = params.OutAssetData[i];

                std::string packagePath = asset.PackagePath.GetRawString();
                if (!PathContainsMaps(packagePath)) continue;

                if (!seenIds.insert(asset.PackageName.ComparisonIndex).second) continue;

                std::string packageName = asset.PackageName.GetRawString();
                if (HasBadSuffix(packageName)) continue;

                std::string category = CategorizeByPath(packagePath);
                if (category.empty()) continue;

                std::string displayName = CleanMapName(packageName);
                if (displayName.empty()) continue;

                maps.push_back({std::move(displayName), std::move(packageName), std::move(category)});
            }

            std::ranges::sort(maps, [](const MapEntry& a, const MapEntry& b) {
                if (a.category != b.category) {
                    bool aBase = (a.category == BASE_GAME_CATEGORY);
                    bool bBase = (b.category == BASE_GAME_CATEGORY);
                    if (aBase != bBase) return aBase;
                    return a.category < b.category;
                }
                return a.displayName < b.displayName;
            });

            BuildCategories();
            displayWidthDirty = true;

            logger.Log("Map scan complete: %zu maps in %zu categories", maps.size(), categories.size());
        } catch (...) {
            logger.Log("Map scan failed with exception");
        }

        state.store(maps.empty() ? ScanState::Failed : ScanState::Complete, std::memory_order_release);
    }

public:
    [[nodiscard]] static MapRegistry& Get() {
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
        auto current = state.load(std::memory_order_acquire);
        if (current != ScanState::Complete && current != ScanState::Failed) return;
        if (state.compare_exchange_strong(current, ScanState::Scanning, std::memory_order_acq_rel))
            GameHook::QueueAction([this]() { PerformScan(); });
    }

    [[nodiscard]] ScanState GetState() const { return state.load(std::memory_order_acquire); }
    [[nodiscard]] const std::vector<MapEntry>& GetMaps() const { return maps; }
    [[nodiscard]] const std::vector<std::string>& GetCategories() const { return categories; }

    [[nodiscard]] float GetMaxDisplayNameWidth() {
        if (displayWidthDirty) {
            maxDisplayNameWidth = 0.0f;
            for (const auto& m : maps) {
                float w = ImGui::CalcTextSize(m.displayName.c_str()).x;
                if (w > maxDisplayNameWidth) maxDisplayNameWidth = w;
            }
            displayWidthDirty = false;
        }
        return maxDisplayNameWidth;
    }

    MapRegistry(const MapRegistry&) = delete;
    MapRegistry& operator=(const MapRegistry&) = delete;
};
