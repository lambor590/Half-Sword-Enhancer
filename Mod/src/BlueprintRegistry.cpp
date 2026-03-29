#include <ranges>
#include <string_view>
#include <unordered_set>
#include "Utils/BlueprintRegistry.h"
#include "Utils/GameConstants.h"
#include "Utils/TierValidation.h"
#include "Utils/EquipmentGenerator.h"
#include "Hooks/GameHook.h"
#include "SDK/AssetRegistry_classes.hpp"
#include "ConfigManager.h"
#include "Logger.h"

static Logger g_logger("BlueprintRegistry");

static constexpr std::string_view VALID_ASSET_PREFIXES[] = {
    "BP_GameWeapon_", "BP_Weapon_", "BP_Armor_", "BP_Container_", "BP_Prop_", "BP_Fence_",
    "BP_Quiver_",     "BP_Candle",  "Shield_",   "Chain_BP",      "Trap_",
};

static std::string DisplayNameFromClassPath(const std::string& path) {
    size_t dotPos = path.rfind('.');
    if (dotPos == std::string::npos) return path;
    std::string name = path.substr(dotPos + 1);
    if (name.size() > 2 && name.ends_with("_C")) name.resize(name.size() - 2);
    return BlueprintRegistry::CleanDisplayName(name);
}

static bool HasValidAssetPrefix(const std::string& assetName) {
    for (auto prefix : VALID_ASSET_PREFIXES) {
        if (assetName.compare(0, prefix.size(), prefix.data()) == 0) return true;
    }
    return false;
}

void BlueprintRegistry::RequestScan() {
    ScanState expected = ScanState::NotStarted;
    if (state.compare_exchange_strong(expected, ScanState::Scanning, std::memory_order_acq_rel)) {
        GameHook::QueueAction([this]() { PerformScan(); });
    }
}

void BlueprintRegistry::RequestRescan() {
    ScanState expected = ScanState::Complete;
    if (!state.compare_exchange_strong(expected, ScanState::Scanning, std::memory_order_acq_rel)) {
        expected = ScanState::Failed;
        if (!state.compare_exchange_strong(expected, ScanState::Scanning, std::memory_order_acq_rel)) return;
    }
    tierScanDone = false;
    GameHook::QueueAction([this]() { PerformScan(); });
}

void BlueprintRegistry::PerformScan() {
    items.clear();
    categories.clear();
    items.reserve(512);

    bool scanSuccess = false;

    try {
        auto si = SDK::UAssetRegistryHelpers::GetAssetRegistry();
        auto* registry = reinterpret_cast<SDK::IAssetRegistry*>(si.GetObjectRef());

        if (registry) {
            SDK::FARFilter filter{};
            filter.bRecursivePaths = true;

            SDK::TArray<SDK::FAssetData> results;
            SDK::UAssetRegistryHelpers::GetBlueprintAssets(filter, &results);

            g_logger.Log("Asset Registry scan returned %d blueprints", results.Num());

            std::unordered_set<uint64_t> seenIds;
            for (int32_t i = 0; i < results.Num(); ++i) {
                auto& asset = results[i];

                std::string packagePath = asset.PackagePath.GetRawString();
                if (packagePath.find("/Game/") != 0) continue;

                if (packagePath.find("/Game/Maps") == 0 || packagePath.find("/Game/UI") == 0 ||
                    packagePath.find("/Game/FX") == 0 || packagePath.find("/Game/Audio") == 0 ||
                    packagePath.find("/Game/Characters") == 0 || packagePath.find("/Game/Cinematics") == 0) {
                    continue;
                }

                uint64_t nameKey = (static_cast<uint64_t>(asset.PackageName.ComparisonIndex) << 32) |
                                   static_cast<uint64_t>(asset.PackageName.Number);
                if (!seenIds.insert(nameKey).second) continue;

                std::string packageName = asset.PackageName.GetRawString();
                std::string assetName = asset.AssetName.ToString();

                if (packagePath.find("/Game/Assets/") == 0 || packagePath.find("/Game/Blueprints/") == 0) {
                    if (!HasValidAssetPrefix(assetName)) continue;
                }

                if (assetName.find("BP_GameWeapon_Customizable_") == 0) continue;

                if (assetName.find("_Master") != std::string::npos) continue;

                auto [category, subcategory] = CategorizeByPath(packagePath, assetName);
                if (category.empty()) continue;

                BlueprintEntry entry;
                entry.displayName = CleanDisplayName(assetName);
                entry.classPath = packageName + "." + assetName + "_C";

                AddItem(std::move(entry), category, subcategory);
            }

            scanSuccess = !items.empty();
            if (scanSuccess) {
                g_logger.Log("Scan complete: %zu items in %zu categories", items.size(), categories.size());
            }
        }
    } catch (...) {
        g_logger.Log("Asset Registry scan failed with exception");
        scanSuccess = false;
    }

    if (!scanSuccess) {
        g_logger.Log("Asset Registry scan found no items");
    }

    InjectCustomizableWeapons();
    InjectCustomPaths();
    SortCategories();

    state.store(items.empty() ? ScanState::Failed : ScanState::Complete, std::memory_order_release);
}

size_t BlueprintRegistry::FindOrCreateCategory(std::string_view name) {
    for (size_t i = 0; i < categories.size(); ++i) {
        if (categories[i].name == name) return i;
    }
    categories.push_back({std::string(name), {}});
    return categories.size() - 1;
}

size_t BlueprintRegistry::FindOrCreateSubcategory(size_t catIdx, std::string_view name) {
    auto& subs = categories[catIdx].subcategories;
    for (size_t i = 0; i < subs.size(); ++i) {
        if (subs[i].name == name) return i;
    }
    subs.push_back({std::string(name), {}});
    return subs.size() - 1;
}

uint16_t BlueprintRegistry::AddItem(BlueprintEntry entry, std::string_view category, std::string_view subcategory) {
    uint16_t idx = static_cast<uint16_t>(items.size());
    items.push_back(std::move(entry));
    size_t catIdx = FindOrCreateCategory(category);
    size_t subIdx = FindOrCreateSubcategory(catIdx, subcategory);
    categories[catIdx].subcategories[subIdx].itemIndices.push_back(idx);
    return idx;
}

std::pair<std::string_view, std::string_view> BlueprintRegistry::CategorizeByPath(
    const std::string& path, const std::string& assetName
) {
    if (path.find("/Weapons/") != std::string::npos) {
        if (path.find("/Tools/") != std::string::npos) return {"Weapons", "Tools"};
        if (path.find("/Reforged/") != std::string::npos) return {"Weapons", "Reforged"};
        if (path.find("/Improvized/") != std::string::npos) return {"Weapons", "Improvised"};
        if (path.find("/Ranged/") != std::string::npos) return {"Weapons", "Ranged"};
        if (path.find("/Treasure/") != std::string::npos) return {"Weapons", "Treasure"};
        if (path.find("/Unique/") != std::string::npos) return {"Weapons", "Unique"};
        if (assetName.find("Shield") != std::string::npos) return {"Weapons", "Shields"};
        if (assetName.find("Trap") != std::string::npos) return {"Weapons", "Traps"};
        return {"Weapons", "General"};
    }

    if (path.find("/Modular_Armor") != std::string::npos) {
        if (assetName.find("Module_") != std::string::npos) return {};
        if (assetName.find("_Head_") != std::string::npos) return {"Modular Armor", "Head"};
        if (assetName.find("_Body_") != std::string::npos || assetName.find("_Chest_") != std::string::npos)
            return {"Modular Armor", "Body"};
        if (assetName.find("_Arms_") != std::string::npos) return {"Modular Armor", "Arms"};
        if (assetName.find("_Hands_") != std::string::npos) return {"Modular Armor", "Hands"};
        if (assetName.find("_Feet_") != std::string::npos) return {"Modular Armor", "Feet"};
        if (assetName.find("_Legs_") != std::string::npos) return {"Modular Armor", "Legs"};
        if (assetName.find("_Neck_") != std::string::npos) return {"Modular Armor", "Neck"};
        if (assetName.find("_Shoulders_") != std::string::npos) return {"Modular Armor", "Shoulders"};
        if (assetName.find("_Waist_") != std::string::npos) return {"Modular Armor", "Waist"};
        return {"Modular Armor", "Other"};
    }

    if (path.find("/Armor/") != std::string::npos) return {};

    if (path.find("/Props/") != std::string::npos) {
        if (path.find("/Lights/") != std::string::npos) return {"Props", "Lights"};
        if (path.find("/Furniture/") != std::string::npos) return {"Props", "Furniture"};
        if (path.find("/Smithing/") != std::string::npos) return {"Props", "Smithing"};
        if (path.find("/Fence/") != std::string::npos) return {"Props", "Fences"};
        if (path.find("/Construction/") != std::string::npos) return {"Props", "Construction"};
        if (path.find("/Candle/") != std::string::npos) return {"Props", "Candles"};
        return {"Props", "General"};
    }

    if (path.find("/Traps/") != std::string::npos) return {"Props", "Traps"};

    if (assetName.find("BP_GameWeapon_") == 0) return {"Weapons", "Customizable"};

    if (path.find("/Game/") == 0 && path.size() > 6) {
        size_t start = 6;
        size_t end = path.find('/', start);
        if (end != std::string::npos && path.compare(start, 4, "Mod_") == 0)
            return {"Mods", std::string_view(path).substr(start + 4, end - start - 4)};
    }

    return {"Other", "General"};
}

std::string BlueprintRegistry::CleanDisplayName(std::string_view assetName) {
    std::string name{assetName};

    static constexpr std::string_view prefixes[] = {
        "BP_GameWeapon_Customizable_",
        "BP_GameWeapon_",
        "BP_Weapon_Reforged_",
        "BP_Weapon_Tool_",
        "BP_Weapon_Improv_",
        "BP_Weapon_Ranged_Weapon_",
        "BP_Weapon_Ranged_Projectle_",
        "BP_Weapon_Treasure_",
        "BP_Weapon_",
        "ModularWeaponBP_Tool_",
        "ModularWeaponBP_",
        "BP_Armor_Modular_Core_",
        "BP_Armor_Modular_Module_",
        "BP_Armor_Head_Hat_",
        "BP_Armor_Head_",
        "BP_Armor_Body_Doublet_",
        "BP_Armor_Body_",
        "BP_Armor_Arms_",
        "BP_Armor_Legs_Hosen_",
        "BP_Armor_Legs_",
        "BP_Armor_Hands_",
        "BP_Armor_Feet_",
        "BP_Armor_Neck_",
        "BP_Armor_Shoulders_",
        "BP_Armor_",
        "BP_Container_",
        "BP_Prop_Light_",
        "BP_Prop_Furniture_",
        "BP_Prop_Smithing_",
        "BP_Prop_Construction_",
        "BP_Fence_",
        "BP_Quiver_",
        "BP_Candle",
        "BP_",
        "Shield_"};

    for (auto prefix : prefixes) {
        if (name.size() > prefix.size() && name.compare(0, prefix.size(), prefix.data()) == 0) {
            name = name.substr(prefix.size());
            break;
        }
    }

    for (char& c : name) {
        if (c == '_') c = ' ';
    }

    auto start = name.find_first_not_of(' ');
    if (start == std::string::npos) {
        name.clear();
    } else if (start > 0)
        name.erase(0, start);
    while (!name.empty() && name.back() == ' ')
        name.pop_back();

    if (name.empty()) name = std::string{assetName};
    return name;
}

void BlueprintRegistry::EnsureTiersScanned() {
    if (tierScanDone) return;
    tierScanDone = true;
    GameHook::QueueAction([this]() { ScanWeaponTiers(); });
}

void BlueprintRegistry::ScanWeaponTiers() {
    auto* world = SDK::UWorld::GetWorld();
    if (!world) {
        tierScanDone = false;
        return;
    }

    std::array<uint16_t, TierValidation::VALID_TIER_MASKS.size()> scannedMasks = {};

    SDK::FStr_Passport_Weapon1 passport{};
    for (int w = 1; w <= GameConstants::WEAPON_TYPE_COUNT; ++w) {
        for (int tier = 0; tier <= 8; ++tier) {
            passport = EquipmentGenerator::GenerateCustomizableWeapon(
                world, static_cast<CustomizableWeapon>(w), static_cast<SDK::Enum_Ranks>(tier)
            );

            if (passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 != nullptr) scannedMasks[w] |= (1 << tier);
        }
    }

    TierValidation::VALID_TIER_MASKS = scannedMasks;

    int populated = 0;
    for (int i = 1; i <= GameConstants::WEAPON_TYPE_COUNT; ++i)
        if (scannedMasks[i] != 0) ++populated;
    g_logger.Log("Weapon tier scan: populated %d/%d weapon type masks", populated, GameConstants::WEAPON_TYPE_COUNT);
}

void BlueprintRegistry::InjectCustomizableWeapons() {
    static_assert(
        static_cast<int>(CustomizableWeapon::Messer) == GameConstants::WEAPON_TYPE_COUNT,
        "WEAPON_TYPE_NAMES must match CustomizableWeapon enum range"
    );
    for (int i = 0; i < GameConstants::WEAPON_TYPE_COUNT; ++i) {
        BlueprintEntry entry;
        entry.displayName = GameConstants::WEAPON_TYPE_NAMES[i];
        entry.customizable = static_cast<CustomizableWeapon>(i + 1);
        AddItem(std::move(entry), "Weapons", "Customizable");
    }
}

void BlueprintRegistry::InjectCustomPaths() {
    if (customPaths.empty()) return;

    for (const auto& path : customPaths) {
        BlueprintEntry entry;
        entry.classPath = path;
        entry.displayName = DisplayNameFromClassPath(path);
        AddItem(std::move(entry), "Custom", "Saved");
    }
}

void BlueprintRegistry::SortCategories() {
    static const char* CATEGORY_ORDER[] = {"Weapons", "Modular Armor", "Props"};
    static constexpr size_t ORDER_COUNT = sizeof(CATEGORY_ORDER) / sizeof(CATEGORY_ORDER[0]);

    std::ranges::sort(categories, [](const CategoryData& a, const CategoryData& b) {
        auto orderOf = [](const std::string& name) -> int {
            for (size_t i = 0; i < ORDER_COUNT; ++i) {
                if (name == CATEGORY_ORDER[i]) return static_cast<int>(i);
            }
            return static_cast<int>(ORDER_COUNT);
        };
        int oa = orderOf(a.name), ob = orderOf(b.name);
        if (oa != ob) return oa < ob;
        return a.name < b.name;
    });

    for (auto& cat : categories) {
        std::ranges::sort(cat.subcategories, [](const SubcategoryData& a, const SubcategoryData& b) {
            return a.name < b.name;
        });
    }
}

void BlueprintRegistry::AddCustomPath(const std::string& path) {
    if (path.empty()) return;
    for (const auto& p : customPaths) {
        if (p == path) return;
    }
    customPaths.push_back(path);
    SaveCustomPaths();

    if (state.load(std::memory_order_acquire) == ScanState::Complete) {
        BlueprintEntry entry;
        entry.classPath = path;
        entry.displayName = DisplayNameFromClassPath(path);
        AddItem(std::move(entry), "Custom", "Saved");
    }
}

void BlueprintRegistry::RemoveCustomPath(size_t index) {
    if (index >= customPaths.size()) return;
    customPaths.erase(customPaths.begin() + index);
    SaveCustomPaths();
}

void BlueprintRegistry::LoadCustomPaths() {
    customPaths.clear();
    auto& cfg = ConfigManager::Get();
    int count = cfg.GetInt("CustomBlueprints", "count", 0);
    for (int i = 0; i < count && i < 100; ++i) {
        char key[16];
        std::snprintf(key, sizeof(key), "path_%d", i);
        std::string path = cfg.GetString("CustomBlueprints", key, "");
        if (!path.empty()) customPaths.push_back(std::move(path));
    }
}

void BlueprintRegistry::SaveCustomPaths() {
    auto& cfg = ConfigManager::Get();
    cfg.SetInt("CustomBlueprints", "count", static_cast<int>(customPaths.size()));
    for (size_t i = 0; i < customPaths.size(); ++i) {
        char key[16];
        std::snprintf(key, sizeof(key), "path_%zu", i);
        cfg.SetString("CustomBlueprints", key, customPaths[i]);
    }
    cfg.SaveConfigDeferred();
}
