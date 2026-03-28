#pragma once

#include <vector>
#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Hooks/GameHook.h"
#include "Utils/Spawner.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/TierValidation.h"
#include "Utils/GuiUtils.h"
#include "Utils/BlueprintRegistry.h"
#include "Utils/PresetPickerState.h"
#include "Utils/WeaponPresetSerializer.h"
#include "Utils/ArmorPresetSerializer.h"
#include "SDK/BP_Armor_Modular_Core_Master_classes.hpp"

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

    void PopulateModulesForCore(SDK::UClass* coreClass) {
        if (coreClass == armorModules.populatedFor) return;
        armorModules = {};
        armorModules.populatedFor = coreClass;
        if (!coreClass || !coreClass->ClassDefaultObject) return;
        if (!coreClass->ClassDefaultObject->IsA(SDK::ABP_Armor_Modular_Core_Master_C::StaticClass())) return;

        auto* cdo = static_cast<SDK::ABP_Armor_Modular_Core_Master_C*>(coreClass->ClassDefaultObject);
        auto collect = [](std::vector<ModuleEntry>& out, const SDK::TArray<SDK::UClass*>& arr) {
            out.reserve(arr.Num());
            for (int i = 0; i < arr.Num(); ++i)
                if (arr[i]) out.push_back({arr[i], arr[i]->GetName()});
        };
        collect(armorModules.slots[0], cdo->Available_Modules_1);
        collect(armorModules.slots[1], cdo->Available_Modules_2);
        collect(armorModules.slots[2], cdo->Available_Modules_3);
    }

    void RenderModuleCombo(const char* label, int slot) {
        auto& modules = armorModules.slots[slot];
        if (modules.empty()) return;

        const char* preview =
            (armorModules.selected[slot] > 0 && armorModules.selected[slot] <= static_cast<int32_t>(modules.size()))
                ? modules[armorModules.selected[slot] - 1].name.c_str()
                : "None";

        if (armorModules.cachedWidths[slot] == 0.0f) {
            float maxW = 0;
            for (const auto& e : modules) {
                float w = ImGui::CalcTextSize(e.name.c_str()).x;
                if (w > maxW) maxW = w;
            }
            armorModules.cachedWidths[slot] = GuiUtils::ComboWidthFromText(maxW);
        }

        ImGui::SetNextItemWidth(armorModules.cachedWidths[slot]);
        if (!ImGui::BeginCombo(label, preview)) return;

        if (ImGui::Selectable("None", armorModules.selected[slot] <= 0)) armorModules.selected[slot] = 0;
        for (int i = 0; i < static_cast<int>(modules.size()); ++i) {
            bool sel = (armorModules.selected[slot] == i + 1);
            if (ImGui::Selectable(modules[i].name.c_str(), sel)) armorModules.selected[slot] = i + 1;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    bool IsCurrentItemModularArmor(const BlueprintEntry& item) const {
        if (item.classPath.empty()) return false;
        return Spawner::GetActorType(item.classPath) == Spawner::ActorType::Armor &&
               item.classPath.find("Modular_Core") != std::string::npos;
    }

    static inline std::vector<const char*> cachedItemNames;
    static inline float cachedItemNamesWidth = 0;
    static inline uint8_t lastCategoryIndex = 255;
    static inline uint8_t lastSubcategoryIndex = 255;

    bool IsRandomArmorCategory() const noexcept {
        auto& reg = BlueprintRegistry::Get();
        return cfg.currentCategoryIndex == static_cast<uint8_t>(reg.GetCategoryCount());
    }

    const BlueprintRegistry::SubcategoryData* GetCurrentSubcategory() const noexcept {
        auto& reg = BlueprintRegistry::Get();
        if (cfg.currentCategoryIndex >= reg.GetCategoryCount()) return nullptr;
        auto& cat = reg.GetCategory(cfg.currentCategoryIndex);
        if (cat.subcategories.empty()) return nullptr;
        uint8_t subIdx = (cat.subcategories.size() == 1) ? 0 : cfg.currentSubcategoryIndex;
        if (subIdx >= cat.subcategories.size()) return nullptr;
        return &cat.subcategories[subIdx];
    }

    void updateItemNamesCache() noexcept {
        auto& reg = BlueprintRegistry::Get();
        if (cfg.currentCategoryIndex >= reg.GetCategoryCount()) return;

        uint8_t subIdx = cfg.currentSubcategoryIndex;
        auto& cat = reg.GetCategory(cfg.currentCategoryIndex);
        if (cat.subcategories.size() == 1) subIdx = 0;

        if (lastCategoryIndex != cfg.currentCategoryIndex || lastSubcategoryIndex != subIdx) [[unlikely]] {
            if (subIdx < cat.subcategories.size()) {
                auto& items = cat.subcategories[subIdx].itemIndices;
                cachedItemNames.resize(items.size());
                for (size_t i = 0; i < items.size(); ++i) {
                    cachedItemNames[i] = reg.GetItem(items[i]).displayName.c_str();
                }
                cachedItemNamesWidth =
                    GuiUtils::CalcComboWidth(cachedItemNames.data(), static_cast<int>(cachedItemNames.size()));
            } else {
                cachedItemNames.clear();
                cachedItemNamesWidth = 0;
            }
            lastCategoryIndex = cfg.currentCategoryIndex;
            lastSubcategoryIndex = subIdx;
        }
    }

    void updateFilteredItems() {
        filteredIndices.clear();
        cachedFilteredWidth = 0;

        if (searchBuffer[0] == '\0') {
            searchActive = false;
            return;
        }

        searchActive = true;
        size_t filterLen = std::strlen(searchBuffer);

        auto& reg = BlueprintRegistry::Get();
        auto& allItems = reg.GetAllItems();

        float maxW = 0;
        for (uint16_t i = 0; i < static_cast<uint16_t>(allItems.size()); ++i) {
            if (GuiUtils::MatchesFilter(
                    allItems[i].displayName.c_str(), allItems[i].displayName.size(), searchBuffer, filterLen
                )) {
                filteredIndices.push_back(i);
                float w = ImGui::CalcTextSize(allItems[i].displayName.c_str()).x;
                if (w > maxW) maxW = w;
            }
        }
        cachedFilteredWidth = GuiUtils::ComboWidthFromText(maxW);
    }

    void SpawnSelectedItem() const noexcept {
        auto spawnTransform =
            Spawner::BuildSpawnTransform(player, cfg.spawn.distanceForward, cfg.spawn.distanceUp, cfg.spawn.scale);

        if (IsRandomArmorCategory()) {
            if (cfg.currentItemIndex >= GameConstants::ARMOR_SLOT_COUNT) return;
            auto slot = static_cast<SDK::EArmorSlots_Enum>(GameConstants::ARMOR_SLOTS[cfg.currentItemIndex].slotEnum);
            auto tier = static_cast<SDK::Enum_Ranks>(cfg.spawnTier);
            bool snap = cfg.spawn.snapToGround;
            auto transform = spawnTransform;
            GameHook::QueueAction([this, slot, tier, transform, snap]() {
                EquipmentGenerator::Init(world);
                auto passport = EquipmentGenerator::GenerateArmor(tier, slot, 0.5);
                if (passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43)
                    Spawner::SpawnArmorFromPassport(world, passport, transform, snap);
            });
            return;
        }

        auto* sub = GetCurrentSubcategory();
        if (!sub || cfg.currentItemIndex >= sub->itemIndices.size()) return;

        auto& reg = BlueprintRegistry::Get();
        auto& item = reg.GetItem(sub->itemIndices[cfg.currentItemIndex]);

        if (item.customizable != CustomizableWeapon::None) {
            Spawner::SpawnCustomizableWeapon(
                world, item.customizable, spawnTransform, cfg.spawn.snapToGround, cfg.spawnTier
            );
        } else if (IsCurrentItemModularArmor(item)) {
            auto classPath = item.classPath;
            int mod1 = armorModules.selected[0], mod2 = armorModules.selected[1], mod3 = armorModules.selected[2];
            auto transform = spawnTransform;
            bool snap = cfg.spawn.snapToGround;
            GameHook::QueueAction([w = world, classPath, mod1, mod2, mod3, transform, snap]() {
                auto* coreClass = Spawner::LoadClass(classPath);
                if (!coreClass) return;
                SDK::FStr_Passport_Armor1 passport{};
                passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = coreClass;
                passport.Module1_5_46B7198E4341C93CBF6AE989EF9898E4 = mod1;
                passport.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0 = mod2;
                passport.Module3_9_E282C465414F6D4EF2A8039FBA847AD2 = mod3;
                Spawner::SpawnArmorFromPassport(w, passport, transform, snap);
            });
        } else if (!item.classPath.empty()) {
            Spawner::SpawnActor(world, item.classPath, spawnTransform, nullptr, cfg.spawn.snapToGround, cfg.spawnTier);
        }
    }

    void SpawnCustomPath() const noexcept {
        if (customPathBuffer[0] == '\0') return;
        auto spawnTransform =
            Spawner::BuildSpawnTransform(player, cfg.spawn.distanceForward, cfg.spawn.distanceUp, cfg.spawn.scale);
        std::string path = customPathBuffer;
        Spawner::SpawnActor(world, path, spawnTransform, nullptr, cfg.spawn.snapToGround, cfg.spawnTier);
    }

    void SpawnWeaponFromPreset() {
        if (!weaponPicker.HasSelection()) return;
        auto data = WeaponPresetSerializer::LoadFromFile(weaponPicker.SelectedPath());
        if (!data.success) return;

        auto transform =
            Spawner::BuildSpawnTransform(player, cfg.spawn.distanceForward, cfg.spawn.distanceUp, cfg.spawn.scale);
        bool snap = cfg.spawn.snapToGround;
        auto rp = data.runtimeProps;

        GameHook::QueueAction([w = world, transform, snap, data = std::move(data), rp]() mutable {
            auto load = [](SDK::UClass*& target, const std::string& path) {
                if (!path.empty()) target = Spawner::LoadClass(path);
            };
            load(data.passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC, data.classPaths.weaponClass);
            load(data.passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139, data.classPaths.headModule);
            load(data.passport.GuardModule_13_6DD2B06245505E53B529D090333012F0, data.classPaths.guardModule);
            load(data.passport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4, data.classPaths.gripModule);
            load(data.passport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6, data.classPaths.pommelModule);
            load(data.passport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D, data.classPaths.subModule1);
            load(data.passport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9, data.classPaths.subModule2);

            if (!EquipmentGenerator::IsPassportValid(data.passport)) return;
            Spawner::SpawnCustomizableFromPassport(w, data.passport, transform, snap);
        });
    }

    void SpawnArmorFromPreset() {
        if (!armorPicker.HasSelection()) return;
        auto data = ArmorPresetSerializer::LoadFromFile(armorPicker.SelectedPath());
        if (!data.success) return;

        auto transform =
            Spawner::BuildSpawnTransform(player, cfg.spawn.distanceForward, cfg.spawn.distanceUp, cfg.spawn.scale);
        bool snap = cfg.spawn.snapToGround;

        GameHook::QueueAction([w = world, transform, snap, data = std::move(data)]() mutable {
            if (!data.armorCorePath.empty())
                data.passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = Spawner::LoadClass(data.armorCorePath);
            if (!data.passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) return;
            Spawner::SpawnArmorFromPassport(w, data.passport, transform, snap);
        });
    }

public:
    ItemSpawnerSection() : CollapsibleSection("Items") {
        Function("Spawn Item")
            .WithKey(&cfg.spawnItemKey)
            .WithParams(
                {Parameter(
                     "snap_to_ground", "Snap to Ground", &cfg.spawn.snapToGround,
                     "Automatically adjust height to touch the ground"
                 ),
                 Parameter(
                     "distance_forward", "Forward Distance", &cfg.spawn.distanceForward, 50.0f, 300.0f,
                     "How far in front the item appears"
                 ),
                 Parameter(
                     "distance_up", "Up Distance", &cfg.spawn.distanceUp, 0.0f, 200.0f,
                     "Height offset for spawn position"
                 ),
                 Parameter("scale", "Scale", &cfg.spawn.scale, 0.1f, 5.0f, "Size multiplier for the spawned item")}
            )
            .WithTooltip("Spawns the selected item with configurable position and size")
            .Action([this]() { SpawnSelectedItem(); }, player, world);
    }

    void RenderContent() override {
        SectionStyle::StyleRAII style;

        for (auto& function : functions) {
            function->Render();
            ImGui::Spacing();
        }

        auto& reg = BlueprintRegistry::Get();
        auto scanState = reg.GetState();

        if (scanState == ScanState::NotStarted) {
            reg.RequestScan();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Initializing Blueprint Browser...");
            return;
        }

        if (scanState == ScanState::Scanning) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Scanning Asset Registry...");
            return;
        }

        if (scanState == ScanState::Failed) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "Using fallback data");
        }

        size_t registryCatCount = reg.GetCategoryCount();
        size_t totalCatCount = registryCatCount + 1;

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Search");
        ImGui::SameLine();
        bool searchChanged =
            ImGui::InputText("##ItemSearch", searchBuffer, sizeof(searchBuffer), ImGuiInputTextFlags_AutoSelectAll);
        if (searchChanged) updateFilteredItems();

        if (searchActive && !filteredIndices.empty()) {
            ImGui::Text("Found: %zu items", filteredIndices.size());
            ImGui::Spacing();

            ImGui::SetNextItemWidth(cachedFilteredWidth);
            if (ImGui::BeginCombo("##FilteredItems", "Select item...")) {
                auto& allItems = reg.GetAllItems();
                for (uint16_t itemIdx : filteredIndices) {
                    if (itemIdx >= allItems.size()) continue;
                    auto& item = allItems[itemIdx];
                    if (ImGui::Selectable(item.displayName.c_str(), false)) {
                        for (size_t ci = 0; ci < registryCatCount; ++ci) {
                            auto& cat = reg.GetCategory(ci);
                            for (size_t si = 0; si < cat.subcategories.size(); ++si) {
                                auto& sub = cat.subcategories[si];
                                for (size_t ii = 0; ii < sub.itemIndices.size(); ++ii) {
                                    if (sub.itemIndices[ii] == itemIdx) {
                                        cfg.currentCategoryIndex = static_cast<uint8_t>(ci);
                                        cfg.currentSubcategoryIndex = static_cast<uint8_t>(si);
                                        cfg.currentItemIndex = static_cast<uint16_t>(ii);
                                        searchBuffer[0] = '\0';
                                        searchActive = false;
                                        goto found;
                                    }
                                }
                            }
                        }
                    found:;
                    }
                }
                ImGui::EndCombo();
            }

            if (ImGui::Button("Clear Search")) {
                searchBuffer[0] = '\0';
                searchActive = false;
            }

        } else if (searchActive && filteredIndices.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No items found");
            if (ImGui::Button("Clear Search")) {
                searchBuffer[0] = '\0';
                searchActive = false;
            }

        } else {
            ImGui::Text("Category");

            auto categoryGetter = [](void* data, int idx) -> const char* {
                auto* reg = static_cast<BlueprintRegistry*>(data);
                size_t regCount = reg->GetCategoryCount();
                if (static_cast<size_t>(idx) < regCount) return reg->GetCategory(idx).name.c_str();
                if (static_cast<size_t>(idx) == regCount) return "Random Armor";
                return "???";
            };

            int catIndex = static_cast<int>(cfg.currentCategoryIndex);
            static float categoryComboW = 0;
            static size_t lastCatCount = 0;
            if (lastCatCount != totalCatCount) {
                categoryComboW = GuiUtils::CalcComboWidth(categoryGetter, &reg, static_cast<int>(totalCatCount));
                lastCatCount = totalCatCount;
            }
            ImGui::SetNextItemWidth(categoryComboW);
            if (ImGui::Combo("##CategorySelector", &catIndex, categoryGetter, &reg, static_cast<int>(totalCatCount)))
                [[unlikely]] {
                cfg.currentCategoryIndex = static_cast<uint8_t>(catIndex);
                cfg.currentSubcategoryIndex = 0;
                cfg.currentItemIndex = 0;
            }

            if (IsRandomArmorCategory()) {
                ImGui::Text("Armor Slot");
                int slotIndex = static_cast<int>(cfg.currentItemIndex);
                auto armorSlotGetter = [](void* data, int idx) -> const char* {
                    return static_cast<const GameConstants::ArmorSlotInfo*>(data)[idx].name;
                };
                static float armorSlotComboW = GuiUtils::CalcComboWidth(
                    armorSlotGetter, (void*)GameConstants::ARMOR_SLOTS, GameConstants::ARMOR_SLOT_COUNT
                );
                ImGui::SetNextItemWidth(armorSlotComboW);
                if (ImGui::Combo(
                        "##ArmorSlotSelector", &slotIndex, armorSlotGetter, (void*)GameConstants::ARMOR_SLOTS,
                        GameConstants::ARMOR_SLOT_COUNT
                    )) {
                    cfg.currentItemIndex = static_cast<uint16_t>(slotIndex);
                }

                if (cfg.currentItemIndex < TierValidation::VALID_ARMOR_TIER_MASKS.size()) {
                    uint16_t mask = TierValidation::VALID_ARMOR_TIER_MASKS[cfg.currentItemIndex];
                    cfg.spawnTier = TierValidation::NearestValidTier(mask, cfg.spawnTier);

                    ImGui::Text("Tier");
                    ImGui::SetNextItemWidth(GuiUtils::CachedTierComboWidth());
                    if (ImGui::BeginCombo("##ArmorTierCombo", GuiUtils::TIER_LABELS[cfg.spawnTier])) {
                        for (int t = 0; t <= 8; ++t) {
                            if (!(mask & (1 << t))) continue;
                            if (ImGui::Selectable(GuiUtils::TIER_LABELS[t], t == cfg.spawnTier)) cfg.spawnTier = t;
                            if (t == cfg.spawnTier) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }

            } else if (cfg.currentCategoryIndex < registryCatCount) {
                auto& cat = reg.GetCategory(cfg.currentCategoryIndex);

                if (cat.subcategories.size() > 1) {
                    ImGui::Text("Subcategory");
                    int subIndex = static_cast<int>(cfg.currentSubcategoryIndex);
                    if (subIndex >= static_cast<int>(cat.subcategories.size())) subIndex = 0;

                    auto subGetter = [](void* data, int idx) -> const char* {
                        auto* cat = static_cast<const BlueprintRegistry::CategoryData*>(data);
                        return cat->subcategories[idx].name.c_str();
                    };
                    static float subcatComboW = 0;
                    static uint8_t subcatCacheForCat = 255;
                    if (subcatCacheForCat != cfg.currentCategoryIndex) {
                        subcatComboW = GuiUtils::CalcComboWidth(
                            subGetter, (void*)&cat, static_cast<int>(cat.subcategories.size())
                        );
                        subcatCacheForCat = cfg.currentCategoryIndex;
                    }
                    ImGui::SetNextItemWidth(subcatComboW);
                    if (ImGui::Combo(
                            "##SubcategorySelector", &subIndex, subGetter, (void*)&cat,
                            static_cast<int>(cat.subcategories.size())
                        )) [[unlikely]] {
                        cfg.currentSubcategoryIndex = static_cast<uint8_t>(subIndex);
                        cfg.currentItemIndex = 0;
                    }
                }

                updateItemNamesCache();

                if (!cachedItemNames.empty()) {
                    ImGui::Text("Item");
                    int itemIndex = static_cast<int>(cfg.currentItemIndex);
                    if (itemIndex >= static_cast<int>(cachedItemNames.size())) {
                        itemIndex = 0;
                        cfg.currentItemIndex = 0;
                    }
                    ImGui::SetNextItemWidth(cachedItemNamesWidth);
                    if (ImGui::Combo(
                            "##ItemSelector", &itemIndex, cachedItemNames.data(),
                            static_cast<int>(cachedItemNames.size())
                        )) [[unlikely]] {
                        cfg.currentItemIndex = static_cast<uint16_t>(itemIndex);
                    }
                }

                auto* sub = GetCurrentSubcategory();
                if (sub && cfg.currentItemIndex < sub->itemIndices.size()) {
                    auto& currentItem = reg.GetItem(sub->itemIndices[cfg.currentItemIndex]);
                    if (currentItem.customizable != CustomizableWeapon::None) {
                        reg.EnsureTiersScanned();
                        uint16_t mask =
                            TierValidation::VALID_TIER_MASKS[static_cast<uint8_t>(currentItem.customizable)];
                        cfg.spawnTier = TierValidation::NearestValidTier(mask, cfg.spawnTier);

                        ImGui::Text("Tier");
                        ImGui::SetNextItemWidth(GuiUtils::CachedTierComboWidth());
                        if (ImGui::BeginCombo("##TierCombo", GuiUtils::TIER_LABELS[cfg.spawnTier])) {
                            for (int t = 0; t <= 8; ++t) {
                                if (!(mask & (1 << t))) continue;
                                if (ImGui::Selectable(GuiUtils::TIER_LABELS[t], t == cfg.spawnTier)) cfg.spawnTier = t;
                                if (t == cfg.spawnTier) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    }

                    if (IsCurrentItemModularArmor(currentItem)) {
                        auto* coreClass = Spawner::LoadClass(currentItem.classPath);
                        if (coreClass) {
                            PopulateModulesForCore(coreClass);
                            RenderModuleCombo("Module 1##m1", 0);
                            RenderModuleCombo("Module 2##m2", 1);
                            RenderModuleCombo("Module 3##m3", 2);
                        }
                    }
                }
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Spawn Item")) [[unlikely]] {
            if (ComponentValidator::Validate(player) && ComponentValidator::Validate(world)) {
                SpawnSelectedItem();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Custom Blueprint Path");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120.0f);
        ImGui::InputText("##CustomPath", customPathBuffer, sizeof(customPathBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Spawn##Custom")) {
            if (ComponentValidator::Validate(player) && ComponentValidator::Validate(world) &&
                customPathBuffer[0] != '\0') {
                SpawnCustomPath();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            if (customPathBuffer[0] != '\0') {
                BlueprintRegistry::Get().AddCustomPath(customPathBuffer);
            }
        }

        auto& savedPaths = reg.GetCustomPaths();
        if (!savedPaths.empty()) {
            ImGui::Spacing();
            ImGui::Text("Saved Paths (%zu)", savedPaths.size());
            int removeIdx = -1;
            for (size_t i = 0; i < savedPaths.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                ImGui::BulletText("%s", savedPaths[i].c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    removeIdx = static_cast<int>(i);
                }
                ImGui::PopID();
            }
            if (removeIdx >= 0) {
                BlueprintRegistry::Get().RemoveCustomPath(static_cast<size_t>(removeIdx));
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::TreeNode("Spawn from Preset")) {
            bool canSpawn = ComponentValidator::Validate(player) && ComponentValidator::Validate(world);

            weaponPicker.Render("Weapon Preset");
            if (weaponPicker.HasSelection()) {
                ImGui::SameLine();
                if (ImGui::Button("Spawn##WeaponPreset") && canSpawn) SpawnWeaponFromPreset();
            }

            armorPicker.Render("Armor Preset");
            if (armorPicker.HasSelection()) {
                ImGui::SameLine();
                if (ImGui::Button("Spawn##ArmorPreset") && canSpawn) SpawnArmorFromPreset();
            }

            ImGui::TreePop();
        }

        ImGui::Spacing();
        if (ImGui::Button("Rescan Blueprints")) {
            reg.RequestRescan();
            lastCategoryIndex = 255;
            lastSubcategoryIndex = 255;
        }
        if (scanState == ScanState::Complete) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(%zu items)", reg.GetAllItems().size());
        }
    }
};
