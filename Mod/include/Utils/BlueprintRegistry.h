#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <atomic>
#include <algorithm>
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

private:
    BlueprintRegistry() { LoadCustomPaths(); }

    std::atomic<ScanState> state{ScanState::NotStarted};
    std::atomic<bool> tierScanDone{false};
    std::vector<BlueprintEntry> items;
    std::vector<CategoryData> categories;
    std::vector<std::string> customPaths;

    void PerformScan();
    void ScanWeaponTiers();
    void InjectCustomizableWeapons();
    void InjectCustomPaths();
    void SortCategories();

    static std::pair<std::string_view, std::string_view> CategorizeByPath(
        const std::string& packagePath, const std::string& assetName
    );

    size_t FindOrCreateCategory(std::string_view name);
    size_t FindOrCreateSubcategory(size_t catIdx, std::string_view name);

    ItemIndex AddItem(const BlueprintEntry& entry, std::string_view category, std::string_view subcategory);

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

    size_t GetCategoryCount() const { return categories.size(); }
    const CategoryData& GetCategory(size_t idx) const { return categories[idx]; }
    const BlueprintEntry& GetItem(size_t idx) const { return items[idx]; }

    void AddCustomPath(const std::string& path);
    void RemoveCustomPath(size_t index);
    const std::vector<std::string>& GetCustomPaths() const { return customPaths; }
    void LoadCustomPaths();
    void SaveCustomPaths();

    BlueprintRegistry(const BlueprintRegistry&) = delete;
    BlueprintRegistry& operator=(const BlueprintRegistry&) = delete;
};
