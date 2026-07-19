#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "Menu/SectionConfig.h"
#include "Utils/ArmorGenerationOptions.h"
#include "Utils/BlueprintRegistry.h"
#include "Utils/GuiUtils.h"
#include "Utils/ItemSpawnPresetSerializer.h"
#include "Utils/PresetLinkPickerState.h"
#include "Utils/PresetPickerState.h"
#include "Utils/PresetSectionState.h"
#include "Utils/SpawnWorkflow.h"
#include "Utils/WeaponPresetSerializer.h"
#include "Utils/ArmorPresetSerializer.h"

class ItemSpawnerSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::Spawner, "Item Spawner", "Add items, armor, and weapons to the current map, including saved designs."
    };

    struct Config {
        SpawnConfig spawn{.distanceForward = 150.0f, .distanceUp = 50.0f};
        int spawnTier = 4;
        int weaponSpecificType = 0;
        EquipmentGenerator::ArmorGenerationOptions armorOptions;

        uint8_t currentCategoryIndex = 0;
        uint8_t currentSubcategoryIndex = 0;
        BlueprintRegistry::ItemIndex currentItemIndex = 0;
    };

private:
    enum class ProfileDraftSource : int { CurrentSelection, CustomPath, WeaponPreset, ArmorPreset, LoadedFallback };

    struct SpawnBinding {
        int id = 0;
        int key = -1;
        char name[64] = "";
        std::string summary;
        ItemSpawnPresetData data;
        std::string resolutionError;
        KeybindEntry keybind;
        std::atomic<std::shared_ptr<const ItemSpawnPresetData>> spawnSnapshot;
    };
    struct BindingOps;

    Config cfg;

    static inline char searchBuffer[128] = "";
    static inline std::vector<BlueprintRegistry::ItemIndex> filteredIndices;
    static inline float cachedFilteredWidth = 0;

    static inline char customPathBuffer[256] = "";

    std::vector<std::shared_ptr<SpawnBinding>> spawnBindings;
    int nextBindingId = 1;
    int pendingDeleteBindingId = -1;
    std::array<std::uint64_t, 2> spawnBindingCatalogRevisions{};
    PresetPickerState<WeaponPresetSerializer> weaponPicker;
    PresetPickerState<ArmorPresetSerializer> armorPicker;
    PresetSectionState<ItemSpawnPresetSerializer> itemSpawnPresets;
    PresetLinkPickerState<WeaponPresetSerializer> profileWeaponLink;
    PresetLinkPickerState<ArmorPresetSerializer> profileArmorLink;
    ProfileDraftSource profileDraftSource = ProfileDraftSource::CurrentSelection;
    ItemSpawnPresetData loadedProfileFallback;
    std::array<int, 3> pendingArmorModuleSelection{};
    std::string pendingArmorModuleClassPath;
    std::string profileDraftError;

    enum class SpawnTarget : uint8_t {
        SelectedItem,
        CustomItem,
        ItemPreset,
        WeaponPreset,
        ArmorPreset,
        Shortcuts,
        Count,
    };

    struct PendingSpawnResult {
        GuiUtils::StatusMessage::Token token = 0;
        std::string error;
    };

    static constexpr std::size_t SPAWN_ROUTE_COUNT = static_cast<std::size_t>(SpawnTarget::Count);
    std::array<GuiUtils::StatusMessage, SPAWN_ROUTE_COUNT> spawnStatuses;
    mutable std::array<std::optional<PendingSpawnResult>, SPAWN_ROUTE_COUNT> pendingSpawnResults;
    mutable std::mutex spawnFeedbackMutex;

    struct ArmorModuleBatch {
        std::string classPath;
        std::array<std::vector<std::string>, 3> slots;
        bool success = false;
    };
    struct ArmorModuleAsyncState {
        std::mutex mutex;
        std::vector<ArmorModuleBatch> completed;
    };
    struct {
        std::vector<std::string> slots[3];
        float cachedWidths[3] = {};
        int32_t selected[3] = {};
        std::string requestedFor;
        std::string populatedFor;
        bool loadQueued = false;
        bool loadSucceeded = false;
    } armorModules;
    std::shared_ptr<ArmorModuleAsyncState> armorModuleAsyncState = std::make_shared<ArmorModuleAsyncState>();

    void QueueModulesForCore(std::string classPath);
    void DrainPendingArmorModules();
    void RenderModuleCombo(const char* label, int slot);
    static inline std::vector<const char*> cachedItemNames;
    static inline float cachedItemNamesWidth = 0;
    static inline uint8_t lastCategoryIndex = 255;
    static inline uint8_t lastSubcategoryIndex = 255;

    bool IsRandomArmorCategory() const;
    const BlueprintRegistry::SubcategoryData* GetCurrentSubcategory() const;
    void RenderMaskedTierCombo(const char* comboLabel, uint16_t mask);
    void UpdateItemNamesCache();
    void UpdateFilteredItems();
    void SpawnSelectedItem();
    void SpawnCustomPath();
    void SpawnWeaponFromPreset();
    void SpawnArmorFromPreset();
    void CaptureSpawnOptions(ItemSpawnPresetData& data) const;
    bool TryBuildCurrentSelection(ItemSpawnPresetData& data, std::string& error) const;
    bool TryBuildItemSpawnPreset(ItemSpawnPresetData& data, std::string& error, bool validate = true) const;
    void ApplyItemSpawnPreset(const ItemSpawnPresetData& data);
    void SpawnItemSpawnPreset();
    bool SelectRegistryIndex(std::size_t index);
    bool SelectRegistryItemByClassPath(std::string_view classPath);
    bool SelectRegistryCustomizable(int customizable);
    void RenderItemSpawnProfiles(bool canSpawn);
    bool CaptureCurrentSelection(SpawnBinding& binding);
    void LoadSpawnBindings();
    void RenderSpawnBindings();
    [[nodiscard]] SpawnWorkflow::SpawnCompletion MakeSpawnCompletion(
        SpawnTarget target, GuiUtils::StatusMessage::Token token
    ) const;
    void StoreSpawnResult(SpawnTarget target, PendingSpawnResult result) const;
    GuiUtils::StatusMessage& SpawnStatus(SpawnTarget target) noexcept {
        return spawnStatuses[static_cast<std::size_t>(target)];
    }
    void ConsumeSpawnFeedback();

public:
    explicit ItemSpawnerSection(ModContext& ctx);
    void RenderSearchResults(BlueprintRegistry& reg);
    void RenderCategoryBrowser(BlueprintRegistry& reg);
    void RenderRandomArmorUI();
    void RenderBlueprintItemUI(BlueprintRegistry& reg);
    void RenderCustomPathSection(BlueprintRegistry& reg, bool canSpawn);
    void RenderPresetSection(bool canSpawn);
    void Render() override;
};
