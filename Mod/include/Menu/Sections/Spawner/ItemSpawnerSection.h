#pragma once

#include <vector>
#include <string>

#include "Menu/CollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Utils/BlueprintRegistry.h"
#include "Utils/PresetPickerState.h"
#include "Utils/WeaponPresetSerializer.h"
#include "Utils/ArmorPresetSerializer.h"

class ItemSpawnerSection : public CollapsibleSection {
private:
    SectionConfig::ItemConfig& cfg = SectionConfig::item;

    static inline char searchBuffer[128] = "";
    static inline std::vector<uint16_t> filteredIndices;
    static inline float cachedFilteredWidth = 0;
    static inline bool searchActive = false;

    static inline char customPathBuffer[256] = "";

    PresetPickerState<WeaponPresetSerializer> weaponPicker;
    PresetPickerState<ArmorPresetSerializer> armorPicker;

    struct ModuleEntry {
        SDK::UClass* cls;
        std::string name;
    };
    struct {
        std::vector<ModuleEntry> slots[3];
        float cachedWidths[3] = {};
        int32_t selected[3] = {};
        SDK::UClass* populatedFor = nullptr;
    } armorModules;

    void PopulateModulesForCore(SDK::UClass* coreClass);
    void RenderModuleCombo(const char* label, int slot);
    bool IsCurrentItemModularArmor(const BlueprintEntry& item) const;

    static inline std::vector<const char*> cachedItemNames;
    static inline float cachedItemNamesWidth = 0;
    static inline uint8_t lastCategoryIndex = 255;
    static inline uint8_t lastSubcategoryIndex = 255;

    bool IsRandomArmorCategory() const noexcept;
    const BlueprintRegistry::SubcategoryData* GetCurrentSubcategory() const noexcept;
    void updateItemNamesCache() noexcept;
    void updateFilteredItems();
    void SpawnSelectedItem() const noexcept;
    void SpawnCustomPath() const noexcept;
    void SpawnWeaponFromPreset();
    void SpawnArmorFromPreset();

public:
    explicit ItemSpawnerSection(ModContext& ctx);
    void RenderSearchResults(BlueprintRegistry& reg);
    void RenderCategoryBrowser(BlueprintRegistry& reg);
    void RenderRandomArmorUI();
    void RenderBlueprintItemUI(BlueprintRegistry& reg);
    void RenderCustomPathSection(BlueprintRegistry& reg);
    void RenderPresetSection();
    void Render() override;
};
