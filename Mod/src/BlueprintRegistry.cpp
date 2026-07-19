#include <ranges>
#include <string_view>
#include <array>
#include <cctype>
#include <utility>
#include <unordered_set>
#include "Utils/BlueprintRegistry.h"
#include "Utils/GameConstants.h"
#include "Utils/TierValidation.h"
#include "Utils/EquipmentGenerator.h"
#include "Hooks/GameHook.h"
#include "SDK/BP_GameWeapon_Customizable_Master_classes.hpp"
#include "SDK/AssetRegistry_classes.hpp"
#include "SDK/AssetRegistry_parameters.hpp"
#include "ConfigManager.h"
#include "Logger.h"

static Logger g_logger("BlueprintRegistry");

static constexpr std::string_view VALID_ASSET_PREFIXES[] = {
    "BP_GameWeapon_", "BP_Weapon_", "BP_Armor_", "BP_Container_", "BP_Prop_", "BP_Fence_",
    "BP_Quiver_",     "BP_Candle",  "Shield_",   "Chain_BP",      "Trap_",
};

static constexpr std::pair<std::string_view, std::string_view> WEAPON_PATH_CATEGORIES[] = {
    {"Tools", "Tools"},   {"Reforged", "Reforged"}, {"Improvized", "Improvised"},
    {"Ranged", "Ranged"}, {"Treasure", "Treasure"}, {"Unique", "Unique"},
};

static std::string DisplayNameFromClassPath(const std::string& path) {
    std::string_view name = path;
    const size_t dotPos = name.rfind('.');
    if (dotPos == std::string_view::npos) return path;
    name.remove_prefix(dotPos + 1);
    if (name.size() > 2 && name.ends_with("_C")) name.remove_suffix(2);
    return BlueprintRegistry::CleanDisplayName(name);
}

static bool HasValidAssetPrefix(std::string_view assetName) {
    for (auto prefix : VALID_ASSET_PREFIXES) {
        if (assetName.starts_with(prefix)) return true;
    }
    return false;
}

static bool HasPathSegment(std::string_view path, std::string_view segment) {
    size_t pos = path.find(segment);
    while (pos != std::string::npos) {
        const size_t after = pos + segment.size();
        if (pos > 0 && path[pos - 1] == '/' && (after == path.size() || path[after] == '/')) return true;
        pos = path.find(segment, pos + 1);
    }
    return false;
}

namespace {
    constexpr std::array<char, 256> BuildLowerTable() {
        std::array<char, 256> table{};
        for (size_t i = 0; i < table.size(); ++i)
            table[i] = static_cast<char>(i);
        for (char c = 'A'; c <= 'Z'; ++c)
            table[static_cast<unsigned char>(c)] = static_cast<char>(c + 32);
        return table;
    }

    constexpr auto LOWER_TABLE = BuildLowerTable();

    constexpr uint16_t BigramKey(char a, char b) noexcept {
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(static_cast<unsigned char>(a)) << 8u) |
            static_cast<uint16_t>(static_cast<unsigned char>(b))
        );
    }

    std::string ToLowerAscii(std::string_view value) {
        std::string lowered;
        lowered.reserve(value.size());
        for (char c : value)
            lowered.push_back(LOWER_TABLE[static_cast<unsigned char>(c)]);
        return lowered;
    }

    bool IsBlueprintAsset(const SDK::FAssetData& asset) {
        const std::string assetClass = asset.AssetClass.ToString();
        if (assetClass == "Blueprint" || assetClass == "BlueprintGeneratedClass") return true;

        return asset.AssetClassPath.PackageName.GetRawString() == "/Script/Engine" &&
               (asset.AssetClassPath.AssetName.ToString() == "Blueprint" ||
                asset.AssetClassPath.AssetName.ToString() == "BlueprintGeneratedClass");
    }

    void GetGameAssets(SDK::UObject* registryObj, SDK::TArray<SDK::FAssetData>& results) {
        if (!registryObj) return;

        static SDK::UFunction* getAssetsFn = nullptr;
        if (!getAssetsFn) getAssetsFn = SDK::IAssetRegistry::StaticClass()->GetFunction("AssetRegistry", "GetAssets");
        if (!getAssetsFn) return;

        SDK::FName gamePath = SDK::BasicFilesImplUtils::StringToName(L"/Game");

        SDK::Params::AssetRegistry_GetAssets params{};
        params.Filter.PackagePaths = SDK::TArray<SDK::FName>(&gamePath, 1, 1);
        params.Filter.bRecursivePaths = true;
        params.bSkipARFilteredAssets = false;

        auto flags = getAssetsFn->FunctionFlags;
        getAssetsFn->FunctionFlags |= 0x400;
        registryObj->ProcessEvent(getAssetsFn, &params);
        getAssetsFn->FunctionFlags = flags;

        params.Filter.PackagePaths = SDK::TArray<SDK::FName>(nullptr, 0, 0);

        results = params.OutAssetData;
    }
}

void BlueprintRegistry::RequestScan() {
    ScanState expected = ScanState::NotStarted;
    if (state.compare_exchange_strong(expected, ScanState::Scanning, std::memory_order_acq_rel)) {
        if (!GameHook::QueueAction([this](const RuntimeContextSnapshot&) { PerformScan(); }))
            state.store(ScanState::NotStarted, std::memory_order_release);
    }
}

void BlueprintRegistry::RequestRescan() {
    auto previous = state.load(std::memory_order_acquire);
    if ((previous != ScanState::Complete && previous != ScanState::Failed) ||
        !state.compare_exchange_strong(previous, ScanState::Scanning, std::memory_order_acq_rel))
        return;
    tierScanDone = false;
    if (!GameHook::QueueAction([this](const RuntimeContextSnapshot&) { PerformScan(); }))
        state.store(previous, std::memory_order_release);
}

void BlueprintRegistry::PerformScan() {
    items.clear();
    categories.clear();
    itemLocations.clear();
    items.reserve(512);

    bool scanSuccess = false;

    try {
        auto si = SDK::UAssetRegistryHelpers::GetAssetRegistry();
        auto* registryObj = si.GetObjectRef();

        if (registryObj) {
            SDK::TArray<SDK::FAssetData> results;
            GetGameAssets(registryObj, results);

            items.reserve(static_cast<size_t>(results.Num()) + GameConstants::WEAPON_TYPE_COUNT + customPaths.size());

            std::unordered_set<uint64_t> seenIds;
            seenIds.reserve(static_cast<size_t>(results.Num()));
            for (int32_t i = 0; i < results.Num(); ++i) {
                auto& asset = results[i];
                if (!IsBlueprintAsset(asset)) continue;

                std::string packagePath = asset.PackagePath.GetRawString();
                if (!packagePath.starts_with("/Game/")) continue;

                if (packagePath.starts_with("/Game/Maps") || packagePath.starts_with("/Game/UI") ||
                    packagePath.starts_with("/Game/FX") || packagePath.starts_with("/Game/Audio") ||
                    packagePath.starts_with("/Game/Characters") || packagePath.starts_with("/Game/Cinematics")) {
                    continue;
                }

                uint64_t nameKey = (static_cast<uint64_t>(asset.PackageName.ComparisonIndex) << 32) |
                                   static_cast<uint64_t>(asset.PackageName.Number);
                if (!seenIds.insert(nameKey).second) continue;

                std::string assetName = asset.AssetName.ToString();
                if (assetName.ends_with("_C")) assetName.resize(assetName.size() - 2);

                if (assetName.starts_with("BP_GameWeapon_Customizable_")) continue;

                if (assetName.find("_Master") != std::string::npos) continue;
                if (packagePath == "/Game/Assets/Weapons/Blueprints/Built_Weapons" &&
                    assetName.starts_with("ModularWeaponBP_"))
                    continue;

                auto [category, subcategory] = CategorizeByPath(packagePath, assetName);
                if (category.empty()) continue;

                if ((packagePath.starts_with("/Game/Assets/") || packagePath.starts_with("/Game/Blueprints/")) &&
                    category == "Other" && !HasValidAssetPrefix(assetName)) {
                    continue;
                }

                BlueprintEntry entry;
                entry.displayName = CleanDisplayName(assetName);
                entry.classPath = asset.PackageName.GetRawString() + "." + assetName + "_C";

                AddItem(std::move(entry), category, subcategory);
            }

            scanSuccess = !items.empty();
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
    RebuildItemLocations();
    RebuildSearchIndex();

    state.store(scanSuccess ? ScanState::Complete : ScanState::Failed, std::memory_order_release);
}

void BlueprintRegistry::AddItem(
    BlueprintEntry&& entry, std::string_view category, std::string_view subcategory
) {
    const ItemIndex idx = items.size();
    items.push_back(std::move(entry));
    auto categoryIt = std::ranges::find(categories, category, &CategoryData::name);
    if (categoryIt == categories.end()) {
        categories.push_back({std::string(category), {}});
        categoryIt = categories.end() - 1;
    }
    auto& subcategories = categoryIt->subcategories;
    auto subcategoryIt = std::ranges::find(subcategories, subcategory, &SubcategoryData::name);
    if (subcategoryIt == subcategories.end()) {
        subcategories.push_back({std::string(subcategory), {}});
        subcategoryIt = subcategories.end() - 1;
    }
    subcategoryIt->itemIndices.push_back(idx);
}

std::pair<std::string_view, std::string_view> BlueprintRegistry::CategorizeByPath(
    const std::string& path, const std::string& assetName
) {
    if (path.find("/Weapons/") != std::string::npos) {
        for (auto [segment, subcategory] : WEAPON_PATH_CATEGORIES) {
            if (HasPathSegment(path, segment)) return {"Weapons", subcategory};
        }
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
    const auto slash = assetName.find_last_of("/\\");
    if (slash != std::string_view::npos) assetName.remove_prefix(slash + 1);
    const auto dot = assetName.find_last_of('.');
    if (dot != std::string_view::npos) assetName.remove_prefix(dot + 1);

    std::string cleaned{assetName};
    if (cleaned.ends_with("_C")) cleaned.resize(cleaned.size() - 2);
    if (cleaned.size() > 9 && cleaned[cleaned.size() - 9] == '_' &&
        std::ranges::all_of(cleaned.end() - 8, cleaned.end(), [](unsigned char value) {
            return std::isxdigit(value) != 0;
        })) {
        cleaned.resize(cleaned.size() - 9);
    }
    const auto numericSuffix = cleaned.find_last_of('_');
    if (numericSuffix != std::string::npos && cleaned.size() - numericSuffix > 3 &&
        std::ranges::all_of(
            cleaned.begin() + static_cast<std::ptrdiff_t>(numericSuffix + 1), cleaned.end(),
            [](unsigned char value) { return std::isdigit(value) != 0; }
        )) {
        cleaned.resize(numericSuffix);
    }
    assetName = cleaned;

    static constexpr std::string_view PREFIXES[] =
        {"BP_GameWeapon_Customizable_",
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
         "Module_",
         "MODULE_",
         "BPI_",
         "WBP_",
         "ABP_",
         "SK_",
         "SM_",
         "BP_",
         "Shield_"};

    bool removedPrefix = true;
    while (removedPrefix) {
        removedPrefix = false;
        for (auto prefix : PREFIXES) {
            if (assetName.size() <= prefix.size() || !assetName.starts_with(prefix)) continue;
            assetName.remove_prefix(prefix.size());
            removedPrefix = true;
            break;
        }
    }

    std::string name;
    name.reserve(assetName.size() + 8);
    for (size_t index = 0; index < assetName.size(); ++index) {
        const auto current = static_cast<unsigned char>(assetName[index]);
        if (current == '_') {
            if (!name.empty() && name.back() != ' ') name.push_back(' ');
            continue;
        }
        if (!name.empty() && name.back() != ' ' && std::isupper(current)) {
            const auto previous = static_cast<unsigned char>(assetName[index - 1]);
            const bool followsWord = std::islower(previous) || std::isdigit(previous);
            const bool endsAcronym = index + 1 < assetName.size() && std::isupper(previous) &&
                                     std::islower(static_cast<unsigned char>(assetName[index + 1]));
            if (followsWord || endsAcronym) name.push_back(' ');
        }
        name.push_back(static_cast<char>(current));
    }

    auto start = name.find_first_not_of(' ');
    if (start == std::string::npos) {
        name.clear();
    } else if (start > 0)
        name.erase(0, start);
    while (!name.empty() && name.back() == ' ')
        name.pop_back();

    if (name.empty()) name = "Unnamed";
    name.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())));
    return name;
}

void BlueprintRegistry::EnsureTiersScanned() {
    if (tierScanDone) return;
    tierScanDone = true;
    if (!GameHook::QueueAction([this](const RuntimeContextSnapshot&) { ScanWeaponTiers(); })) tierScanDone = false;
}

void BlueprintRegistry::ScanWeaponTiers() {
    auto* world = SDK::UWorld::GetWorld();
    if (!world) {
        tierScanDone = false;
        return;
    }

    std::array<uint16_t, TierValidation::VALID_TIER_MASKS.size()> scannedMasks = {};

    for (int w = 1; w <= GameConstants::WEAPON_TYPE_COUNT; ++w) {
        auto* modulesClass = EquipmentGenerator::GetCustomizableModulesClass(static_cast<CustomizableWeapon>(w));
        if (!modulesClass || !modulesClass->ClassDefaultObject) continue;

        auto* cdo = static_cast<SDK::UBP_GameWeapon_Customizable_Master_C*>(modulesClass->ClassDefaultObject);
        if (cdo->Module_Heads_Array.Num() > 0 && cdo->Module_Grips_Array.Num() > 0) scannedMasks[w] = 0x1FF;
    }

    TierValidation::VALID_TIER_MASKS = scannedMasks;
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
    static const char* categoryOrder[] = {"Weapons", "Modular Armor", "Props"};
    static constexpr size_t ORDER_COUNT = sizeof(categoryOrder) / sizeof(categoryOrder[0]);

    std::ranges::sort(categories, [](const CategoryData& a, const CategoryData& b) {
        auto orderOf = [](const std::string& name) -> int {
            for (size_t i = 0; i < ORDER_COUNT; ++i) {
                if (name == categoryOrder[i]) return static_cast<int>(i);
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

void BlueprintRegistry::RebuildItemLocations() {
    itemLocations.assign(items.size(), {});
    for (size_t ci = 0; ci < categories.size(); ++ci) {
        auto& cat = categories[ci];
        for (size_t si = 0; si < cat.subcategories.size(); ++si) {
            auto& sub = cat.subcategories[si];
            for (size_t ii = 0; ii < sub.itemIndices.size(); ++ii) {
                const ItemIndex itemIdx = sub.itemIndices[ii];
                itemLocations[itemIdx] =
                    {static_cast<uint8_t>(ci), static_cast<uint8_t>(si), static_cast<ItemIndex>(ii)};
            }
        }
    }
}

void BlueprintRegistry::RebuildSearchIndex() {
    loweredItemNames.clear();
    loweredItemNames.reserve(items.size());
    size_t entryCount = 0;
    for (const auto& item : items) {
        loweredItemNames.push_back(ToLowerAscii(item.displayName));
        if (item.displayName.size() > 1) entryCount += item.displayName.size() - 1;
    }

    searchIndex.clear();
    searchIndex.reserve(entryCount);
    std::vector<uint16_t> itemKeys;

    for (ItemIndex itemIdx = 0; itemIdx < loweredItemNames.size(); ++itemIdx) {
        const auto& name = loweredItemNames[itemIdx];
        if (name.size() < 2) continue;

        itemKeys.clear();
        itemKeys.reserve(name.size() - 1);
        for (size_t i = 1; i < name.size(); ++i) {
            itemKeys.push_back(BigramKey(name[i - 1], name[i]));
        }
        std::ranges::sort(itemKeys);
        const auto uniqueEnd = std::ranges::unique(itemKeys).begin();
        for (auto key = itemKeys.begin(); key != uniqueEnd; ++key)
            searchIndex.push_back({*key, static_cast<uint32_t>(itemIdx)});
    }

    std::ranges::sort(searchIndex, {}, &SearchEntry::key);
}

void BlueprintRegistry::SearchItems(std::string_view filter, std::vector<ItemIndex>& out) const {
    out.clear();
    if (filter.empty()) {
        out.reserve(items.size());
        for (ItemIndex i = 0; i < items.size(); ++i)
            out.push_back(i);
        return;
    }

    const std::string loweredFilter = ToLowerAscii(filter);
    if (loweredFilter.size() < 2) {
        for (ItemIndex i = 0; i < loweredItemNames.size(); ++i) {
            if (loweredItemNames[i].find(loweredFilter) != std::string::npos) out.push_back(i);
        }
        return;
    }

    auto bestBegin = searchIndex.end();
    auto bestEnd = searchIndex.end();
    for (size_t i = 1; i < loweredFilter.size(); ++i) {
        const uint16_t key = BigramKey(loweredFilter[i - 1], loweredFilter[i]);
        const auto first =
            std::lower_bound(searchIndex.begin(), searchIndex.end(), key, [](const SearchEntry& entry, uint16_t value) {
                return entry.key < value;
            });
        const auto last = std::upper_bound(first, searchIndex.end(), key, [](uint16_t value, const SearchEntry& entry) {
            return value < entry.key;
        });
        if (first == last) return;
        if (bestBegin == searchIndex.end() || last - first < bestEnd - bestBegin) {
            bestBegin = first;
            bestEnd = last;
        }
    }
    out.reserve(static_cast<size_t>(bestEnd - bestBegin));
    for (auto entry = bestBegin; entry != bestEnd; ++entry) {
        const ItemIndex idx = entry->item;
        if (loweredItemNames[idx].find(loweredFilter) != std::string::npos) out.push_back(idx);
    }
}

void BlueprintRegistry::AddCustomPath(const std::string& path) {
    if (path.empty()) return;
    if (std::ranges::find(customPaths, path) != customPaths.end()) return;
    customPaths.push_back(path);
    SaveCustomPaths();

    if (state.load(std::memory_order_acquire) == ScanState::Complete) {
        BlueprintEntry entry;
        entry.classPath = path;
        entry.displayName = DisplayNameFromClassPath(path);
        AddItem(std::move(entry), "Custom", "Saved");
        SortCategories();
        RebuildItemLocations();
        RebuildSearchIndex();
    }
}

void BlueprintRegistry::RemoveCustomPath(size_t index) {
    if (index >= customPaths.size()) return;
    customPaths.erase(customPaths.begin() + static_cast<std::ptrdiff_t>(index));
    SaveCustomPaths();
}

void BlueprintRegistry::LoadCustomPaths() {
    customPaths.clear();
    auto& cfg = ConfigManager::Get();
    const int count = cfg.GetInt("CustomBlueprints", "count", 0);
    if (count > 0) customPaths.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        char key[16];
        std::snprintf(key, sizeof(key), "path_%d", i);
        std::string path = cfg.GetString("CustomBlueprints", key, "");
        if (!path.empty()) customPaths.push_back(std::move(path));
    }
}

void BlueprintRegistry::SaveCustomPaths() {
    auto& cfg = ConfigManager::Get();
    cfg.BatchSave([&cfg, this] {
        cfg.DeleteSection("CustomBlueprints");
        cfg.SetInt("CustomBlueprints", "count", static_cast<int>(customPaths.size()));
        for (size_t i = 0; i < customPaths.size(); ++i) {
            char key[16];
            std::snprintf(key, sizeof(key), "path_%zu", i);
            cfg.SetString("CustomBlueprints", key, customPaths[i].c_str());
        }
    });
}
