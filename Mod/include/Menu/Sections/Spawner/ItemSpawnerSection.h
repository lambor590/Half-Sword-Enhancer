#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include <vector>
#include <string>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "Menu/SectionConfig.h"
#include "Utils/ArmorGenerationOptions.h"
#include "Utils/BlueprintRegistry.h"
#include "Utils/PresetPickerState.h"
#include "Utils/WeaponPresetSerializer.h"
#include "Utils/ArmorPresetSerializer.h"

class ItemSpawnerSection : public Section {
public:
    static constexpr SectionDefinition SECTION{MenuTab::Spawner, "Items"};

    struct Config {
        SpawnConfig spawn{.distanceForward = 150.0f, .distanceUp = 50.0f};
        int spawnTier = 4;
        EquipmentGenerator::ArmorGenerationOptions armorOptions;

        uint8_t currentCategoryIndex = 0;
        uint8_t currentSubcategoryIndex = 0;
        BlueprintRegistry::ItemIndex currentItemIndex = 0;
    };

private:
    enum class BindingSource : int { ClassPath, CustomizableWeapon, RandomArmor, ModularArmor };

    struct SpawnBinding {
        int id = 0;
        int key = -1;
        char name[64] = "";
        BindingSource source = BindingSource::ClassPath;
        std::string summary;
        std::string classPath;
        int customizable = 0;
        int armorSlot = 0;
        std::array<int, 3> modules{};
        SpawnConfig spawn{.distanceForward = 150.0f, .distanceUp = 50.0f};
        int tier = 4;
        EquipmentGenerator::ArmorGenerationOptions armorOptions;
        KeybindEntry keybind;
    };

    Config cfg;

    static inline char searchBuffer[128] = "";
    static inline std::vector<BlueprintRegistry::ItemIndex> filteredIndices;
    static inline float cachedFilteredWidth = 0;
    static inline bool searchActive = false;

    static inline char customPathBuffer[256] = "";

    std::vector<std::shared_ptr<SpawnBinding>> spawnBindings;
    int nextBindingId = 1;
    int pendingDeleteBindingId = -1;
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
    void RenderMaskedTierCombo(const char* comboLabel, uint16_t mask);
    void UpdateItemNamesCache() noexcept;
    void UpdateFilteredItems();
    void SpawnSelectedItem() const noexcept;
    void SpawnBindingItem(const SpawnBinding& binding, const RuntimeContextSnapshot& runtime) const;
    void SpawnCustomPath() const noexcept;
    void SpawnWeaponFromPreset();
    void SpawnArmorFromPreset();
    void InitBindingKeybind(const std::shared_ptr<SpawnBinding>& binding);
    void AddBindingFromCurrentSelection();
    bool CaptureCurrentSelection(SpawnBinding& binding) const;
    void LoadSpawnBindings();
    void SaveSpawnBindings();
    void RenderSpawnBindings();

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
