#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <atomic>
#include <algorithm>

#include "Utils/BlueprintRegistry.h"
#include "Hooks/GameHook.h"
#include "SDK/AssetRegistry_classes.hpp"
#include "Logger.h"

struct MapEntry {
    std::string displayName;
    std::string packageName;
};

class MapRegistry {
    MapRegistry() = default;

    std::atomic<ScanState> state{ScanState::NotStarted};
    std::vector<MapEntry> maps;

    static std::string CleanMapName(const std::string& packageName) {
        std::string_view sv(packageName);
        auto lastSlash = sv.rfind('/');
        if (lastSlash != std::string_view::npos)
            sv = sv.substr(lastSlash + 1);

        std::string name(sv);
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

    void PerformScan() {
        static Logger logger("MapRegistry");
        maps.clear();

        try {
            auto si = SDK::UAssetRegistryHelpers::GetAssetRegistry();
            auto* registry = reinterpret_cast<SDK::IAssetRegistry*>(si.GetObjectRef());
            if (!registry) {
                logger.Log("Asset Registry not available");
                state.store(ScanState::Failed, std::memory_order_release);
                return;
            }

            SDK::TArray<SDK::FAssetData> results;
            auto mapsPath = SDK::BasicFilesImpleUtils::StringToName(L"/Game/Maps");
            registry->GetAssetsByPath(mapsPath, &results, true, false);

            const int32_t count = results.Num();
            logger.Log("Map scan returned %d assets", count);
            maps.reserve(count);

            for (int32_t i = 0; i < count; ++i) {
                std::string packageName = results[i].PackageName.GetRawString();
                std::string displayName = CleanMapName(packageName);
                if (!displayName.empty())
                    maps.push_back({std::move(displayName), std::move(packageName)});
            }

            std::sort(maps.begin(), maps.end(),
                [](const MapEntry& a, const MapEntry& b) { return a.displayName < b.displayName; });

            logger.Log("Map scan complete: %zu maps found", maps.size());
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

    MapRegistry(const MapRegistry&) = delete;
    MapRegistry& operator=(const MapRegistry&) = delete;
};
