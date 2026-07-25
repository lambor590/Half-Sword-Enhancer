#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <atomic>
#include <cstdint>
#include "Utils/CustomizableWeapon.h"

struct BlueprintEntry {
    std::string displayName;
    std::string classPath;
    CustomizableWeapon customizable = CustomizableWeapon::None;
};

enum class ScanState : uint8_t { NotStarted, Scanning, Complete, Failed };

class BlueprintRegistry {
public:
    using ItemIndex = std::size_t;

    struct SubcategoryData {
        std::string name;
        std::vector<ItemIndex> itemIndices;
    };

    struct CategoryData {
        std::string name;
        std::vector<SubcategoryData> subcategories;
    };

    struct ItemLocation {
        uint8_t category = 0;
        uint8_t subcategory = 0;
        ItemIndex item = 0;
    };

private:
    BlueprintRegistry() { LoadCustomPaths(); }

    std::atomic<ScanState> state{ScanState::NotStarted};
    std::atomic<bool> tierScanDone{false};
    std::vector<BlueprintEntry> items;
    std::vector<CategoryData> categories;
    std::vector<ItemLocation> itemLocations;
    std::vector<std::string> customPaths;
    std::vector<std::string> loweredItemNames;

    struct SearchEntry {
        uint16_t key = 0;
        uint32_t item = 0;
    };
    std::vector<SearchEntry> searchIndex;

    void PerformScan();
    void ScanWeaponTiers();
    void InjectCustomizableWeapons();
    void InjectCustomPaths();
    void SortCategories();
    void RebuildItemLocations();
    void RebuildSearchIndex();

    static std::pair<std::string_view, std::string_view> CategorizeByPath(
        const std::string& packagePath, const std::string& assetName
    );

    void AddItem(BlueprintEntry&& entry, std::string_view category, std::string_view subcategory);

public:
    static BlueprintRegistry& Get() {
        static BlueprintRegistry instance;
        return instance;
    }

    void RequestScan();
    void RequestRescan();
    void EnsureTiersScanned();
    [[nodiscard]] ScanState GetState() const { return state.load(std::memory_order_acquire); }

    [[nodiscard]] const std::vector<CategoryData>& GetCategories() const { return categories; }
    const std::vector<BlueprintEntry>& GetAllItems() const { return items; }
    [[nodiscard]] static std::string CleanDisplayName(std::string_view assetName);
    void SearchItems(std::string_view filter, std::vector<ItemIndex>& out) const;

    size_t GetCategoryCount() const { return categories.size(); }
    const CategoryData& GetCategory(size_t idx) const { return categories[idx]; }
    const BlueprintEntry& GetItem(size_t idx) const { return items[idx]; }
    const ItemLocation& GetItemLocation(size_t idx) const { return itemLocations[idx]; }

    void AddCustomPath(const std::string& path);
    void RemoveCustomPath(size_t index);
    const std::vector<std::string>& GetCustomPaths() const { return customPaths; }
    void LoadCustomPaths();
    void SaveCustomPaths();

    BlueprintRegistry(const BlueprintRegistry&) = delete;
    BlueprintRegistry& operator=(const BlueprintRegistry&) = delete;
};
