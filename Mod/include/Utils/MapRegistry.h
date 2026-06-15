#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <atomic>
#include "Utils/BlueprintRegistry.h"

struct MapEntry {
    std::string displayName;
    std::string packageName;
    std::string category;
};

class MapRegistry {
    MapRegistry() = default;

    static constexpr std::string_view BASE_GAME_CATEGORY = "Base Game";
    static constexpr std::string_view MODDED_CATEGORY = "Modded";

    std::atomic<ScanState> state{ScanState::NotStarted};
    std::vector<MapEntry> maps;
    std::vector<std::string> categories;
    float maxDisplayNameWidth = 0.0f;
    bool displayWidthDirty = true;

    [[nodiscard]] static bool IsInternalMapAsset(std::string_view packageName, std::string_view packagePath);
    [[nodiscard]] static bool IsBaseGameMapPath(std::string_view path);
    [[nodiscard]] static std::string CategorizeByPath(std::string_view packagePath);
    [[nodiscard]] static std::string CleanMapName(std::string_view packageName);

    void BuildCategories();
    void PerformScan();

public:
    [[nodiscard]] static MapRegistry& Get() {
        static MapRegistry instance;
        return instance;
    }

    void RequestScan();
    void RequestRescan();

    [[nodiscard]] ScanState GetState() const { return state.load(std::memory_order_acquire); }
    [[nodiscard]] const std::vector<MapEntry>& GetMaps() const { return maps; }
    [[nodiscard]] const std::vector<std::string>& GetCategories() const { return categories; }
    [[nodiscard]] float GetMaxDisplayNameWidth();

    MapRegistry(const MapRegistry&) = delete;
    MapRegistry& operator=(const MapRegistry&) = delete;
};
