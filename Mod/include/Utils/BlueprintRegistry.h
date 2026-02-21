#pragma once

#include <string>
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
    struct SubcategoryData {
        std::string name;
        std::vector<uint16_t> itemIndices;
    };

    struct CategoryData {
        std::string name;
        std::vector<SubcategoryData> subcategories;
    };

private:
    BlueprintRegistry() { LoadCustomPaths(); }

    std::atomic<ScanState> state{ScanState::NotStarted};
    bool tierScanDone = false;
    std::vector<BlueprintEntry> items;
    std::vector<CategoryData> categories;
    std::vector<std::string> customPaths;

    void PerformScan();
    void ScanWeaponTiers();
    void InjectCustomizableWeapons();
    void InjectCustomPaths();
    void SortCategories();

    static std::pair<std::string, std::string> CategorizeByPath(const std::string& packagePath, const std::string& assetName);
    static std::string CleanDisplayName(const std::string& assetName);

    size_t FindOrCreateCategory(const std::string& name);
    size_t FindOrCreateSubcategory(size_t catIdx, const std::string& name);

    uint16_t AddItem(BlueprintEntry entry, const std::string& category, const std::string& subcategory);

public:
    static BlueprintRegistry& Get() {
        static BlueprintRegistry instance;
        return instance;
    }

    void RequestScan();
    void RequestRescan();
    void EnsureTiersScanned();
    ScanState GetState() const { return state.load(std::memory_order_acquire); }

    const std::vector<CategoryData>& GetCategories() const { return categories; }
    const std::vector<BlueprintEntry>& GetAllItems() const { return items; }

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
