#include "Menu/Sections/Spawner/ItemSpawnerSection.h"

#include <algorithm>

#include "Menu/SectionStyle.h"
#include "ConfigManager.h"

#include "Utils/Spawner.h"
#include "Utils/SpawnWorkflow.h"
#include "Utils/TierValidation.h"
#include "Utils/GuiUtils.h"
#include "Utils/SpawnBindingUtils.h"
#include "SDK/BP_Armor_Modular_Core_Master_classes.hpp"

namespace {
    using ItemSpawnRequest = SpawnWorkflow::ItemSpawnRequest;

    constexpr const char* ITEM_BINDINGS_SECTION = "ItemSpawnBindings";
    constexpr const char* ITEM_BINDING_PREFIX = "ItemSpawnBinding_";

    std::string ItemBindingSection(int id) {
        return SpawnBindingUtils::SectionName(ITEM_BINDING_PREFIX, id);
    }
}

void ItemSpawnerSection::PopulateModulesForCore(SDK::UClass* coreClass) {
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

void ItemSpawnerSection::RenderModuleCombo(const char* label, int slot) {
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

    if (!GuiUtils::BeginSizedCombo(label, preview, armorModules.cachedWidths[slot])) return;

    if (ImGui::Selectable("None", armorModules.selected[slot] <= 0)) armorModules.selected[slot] = 0;
    for (int i = 0; i < static_cast<int>(modules.size()); ++i) {
        bool sel = (armorModules.selected[slot] == i + 1);
        if (ImGui::Selectable(modules[i].name.c_str(), sel)) armorModules.selected[slot] = i + 1;
        if (sel) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
}

bool ItemSpawnerSection::IsCurrentItemModularArmor(const BlueprintEntry& item) const {
    if (item.classPath.empty()) return false;
    return Spawner::GetActorType(item.classPath) == Spawner::ActorType::Armor &&
           item.classPath.find("Modular_Core") != std::string::npos;
}

bool ItemSpawnerSection::IsRandomArmorCategory() const noexcept {
    auto& reg = BlueprintRegistry::Get();
    return cfg.currentCategoryIndex == static_cast<uint8_t>(reg.GetCategoryCount());
}

const BlueprintRegistry::SubcategoryData* ItemSpawnerSection::GetCurrentSubcategory() const noexcept {
    auto& reg = BlueprintRegistry::Get();
    if (cfg.currentCategoryIndex >= reg.GetCategoryCount()) return nullptr;
    auto& cat = reg.GetCategory(cfg.currentCategoryIndex);
    if (cat.subcategories.empty()) return nullptr;
    uint8_t subIdx = (cat.subcategories.size() == 1) ? 0 : cfg.currentSubcategoryIndex;
    if (subIdx >= cat.subcategories.size()) return nullptr;
    return &cat.subcategories[subIdx];
}

void ItemSpawnerSection::RenderMaskedTierCombo(const char* comboLabel, uint16_t mask) {
    cfg.spawnTier = TierValidation::NearestValidTier(mask, cfg.spawnTier);

    ImGui::Text("Tier");
    if (GuiUtils::BeginSizedCombo(comboLabel, GuiUtils::TIER_LABELS[cfg.spawnTier], GuiUtils::CachedTierComboWidth())) {
        for (int t = 0; t <= 8; ++t) {
            if (!(mask & (1 << t))) continue;
            if (ImGui::Selectable(GuiUtils::TIER_LABELS[t], t == cfg.spawnTier)) cfg.spawnTier = t;
            if (t == cfg.spawnTier) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void ItemSpawnerSection::UpdateItemNamesCache() noexcept {
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

void ItemSpawnerSection::UpdateFilteredItems() {
    filteredIndices.clear();
    cachedFilteredWidth = 0;

    if (searchBuffer[0] == '\0') {
        searchActive = false;
        return;
    }

    searchActive = true;

    auto& reg = BlueprintRegistry::Get();
    reg.SearchItems(searchBuffer, filteredIndices);
    auto& allItems = reg.GetAllItems();

    float maxW = 0;
    for (BlueprintRegistry::ItemIndex i : filteredIndices) {
        if (i >= allItems.size()) continue;
        float w = ImGui::CalcTextSize(allItems[i].displayName.c_str()).x;
        if (w > maxW) maxW = w;
    }
    cachedFilteredWidth = GuiUtils::ComboWidthFromText(maxW);
}

void ItemSpawnerSection::SpawnSelectedItem() const noexcept {
    auto snapshot = RenderSnapshot();
    if (!snapshot.player || !snapshot.world) return;
    auto tier = static_cast<SDK::Enum_Ranks>(cfg.spawnTier);

    if (IsRandomArmorCategory()) {
        if (cfg.currentItemIndex >= GameConstants::ARMOR_SLOT_COUNT) return;
        auto slot = static_cast<SDK::EArmorSlots_Enum>(GameConstants::ARMOR_SLOTS[cfg.currentItemIndex].slotEnum);
        SpawnWorkflow::QueueItemSpawn(snapshot, cfg.spawn, ItemSpawnRequest::RandomArmor(slot, tier));
        return;
    }

    auto* sub = GetCurrentSubcategory();
    if (!sub || cfg.currentItemIndex >= sub->itemIndices.size()) return;

    auto& reg = BlueprintRegistry::Get();
    auto& item = reg.GetItem(sub->itemIndices[cfg.currentItemIndex]);

    if (item.customizable != CustomizableWeapon::None) {
        SpawnWorkflow::QueueItemSpawn(
            snapshot, cfg.spawn, ItemSpawnRequest::GeneratedCustomizableWeapon(item.customizable, tier)
        );
    } else if (IsCurrentItemModularArmor(item)) {
        SpawnWorkflow::QueueItemSpawn(
            snapshot, cfg.spawn,
            ItemSpawnRequest::ModularArmor(
                item.classPath, {armorModules.selected[0], armorModules.selected[1], armorModules.selected[2]}
            )
        );
    } else if (!item.classPath.empty()) {
        SpawnWorkflow::QueueItemSpawn(snapshot, cfg.spawn, ItemSpawnRequest::ClassPath(item.classPath, tier));
    }
}

void ItemSpawnerSection::SpawnBindingItem(const SpawnBinding& binding, const RuntimeContextSnapshot& runtime) const {
    if (!runtime.world || !runtime.player) return;

    auto tier = static_cast<SDK::Enum_Ranks>(binding.tier);

    switch (binding.source) {
        case BindingSource::RandomArmor: {
            if (binding.armorSlot < 0 || binding.armorSlot >= GameConstants::ARMOR_SLOT_COUNT) return;
            auto slot = static_cast<SDK::EArmorSlots_Enum>(GameConstants::ARMOR_SLOTS[binding.armorSlot].slotEnum);
            SpawnWorkflow::SpawnItem(runtime, binding.spawn, ItemSpawnRequest::RandomArmor(slot, tier));
            break;
        }
        case BindingSource::CustomizableWeapon: {
            SpawnWorkflow::SpawnItem(
                runtime, binding.spawn,
                ItemSpawnRequest::GeneratedCustomizableWeapon(
                    static_cast<CustomizableWeapon>(binding.customizable), tier
                )
            );
            break;
        }
        case BindingSource::ModularArmor: {
            SpawnWorkflow::SpawnItem(
                runtime, binding.spawn, ItemSpawnRequest::ModularArmor(binding.classPath, binding.modules)
            );
            break;
        }
        case BindingSource::ClassPath:
            if (!binding.classPath.empty())
                SpawnWorkflow::SpawnItem(runtime, binding.spawn, ItemSpawnRequest::ClassPath(binding.classPath, tier));
            break;
    }
}

void ItemSpawnerSection::SpawnCustomPath() const noexcept {
    if (customPathBuffer[0] == '\0') return;
    auto snapshot = RenderSnapshot();
    if (!snapshot.player || !snapshot.world) return;
    auto tier = static_cast<SDK::Enum_Ranks>(cfg.spawnTier);
    SpawnWorkflow::QueueItemSpawn(snapshot, cfg.spawn, ItemSpawnRequest::ClassPath(customPathBuffer, tier));
}

void ItemSpawnerSection::SpawnWeaponFromPreset() {
    if (!weaponPicker.HasSelection()) return;
    auto snapshot = RenderSnapshot();
    if (!snapshot.player || !snapshot.world) return;
    auto data = WeaponPresetSerializer::LoadFromFile(weaponPicker.SelectedPath());
    if (!data.success) return;

    SpawnWorkflow::QueueWeaponSpawn(snapshot, cfg.spawn, data.passport, std::move(data.classPaths));
}

void ItemSpawnerSection::SpawnArmorFromPreset() {
    if (!armorPicker.HasSelection()) return;
    auto snapshot = RenderSnapshot();
    if (!snapshot.player || !snapshot.world) return;
    auto data = ArmorPresetSerializer::LoadFromFile(armorPicker.SelectedPath());
    if (!data.success) return;

    SpawnWorkflow::QueueItemSpawn(
        snapshot, cfg.spawn, ItemSpawnRequest::ArmorPreset(data.passport, std::move(data.armorCorePath))
    );
}

ItemSpawnerSection::ItemSpawnerSection(ModContext& ctx) : Section(ctx, "Items") {
    LoadSpawnBindings();
}

void ItemSpawnerSection::InitBindingKeybind(const std::shared_ptr<SpawnBinding>& binding) {
    SpawnBindingUtils::InitKeybind(
        binding, ItemBindingSection(binding->id),
        [this](const SpawnBinding& binding, const RuntimeContextSnapshot& runtime) {
            SpawnBindingItem(binding, runtime);
        },
        std::vector<KeybindParam>{
            KeybindParam(
                "snap_to_ground", "Snap to Ground", &binding->spawn.snapToGround,
                "Automatically adjust height to touch the ground"
            ),
            KeybindParam(
                "distance_forward", "Forward Distance", &binding->spawn.distanceForward, 50.0f, 300.0f,
                "How far in front the item appears"
            ),
            KeybindParam(
                "distance_up", "Up Distance", &binding->spawn.distanceUp, 0.0f, 200.0f,
                "Height offset for spawn position"
            ),
            KeybindParam("scale", "Scale", &binding->spawn.scale, 0.1f, 5.0f, "Size multiplier for the spawned item"),
        }
    );
}

bool ItemSpawnerSection::CaptureCurrentSelection(SpawnBinding& binding) const {
    binding.spawn = cfg.spawn;
    binding.tier = cfg.spawnTier;

    if (IsRandomArmorCategory()) {
        if (cfg.currentItemIndex >= GameConstants::ARMOR_SLOT_COUNT) return false;
        binding.source = BindingSource::RandomArmor;
        binding.armorSlot = static_cast<int>(cfg.currentItemIndex);
        binding.classPath.clear();
        binding.customizable = 0;
        binding.summary = std::string("Random ") + GameConstants::ARMOR_SLOTS[binding.armorSlot].name;
        return true;
    }

    auto* sub = GetCurrentSubcategory();
    if (!sub || cfg.currentItemIndex >= sub->itemIndices.size()) return false;

    const auto& item = BlueprintRegistry::Get().GetItem(sub->itemIndices[cfg.currentItemIndex]);
    binding.summary = item.displayName;
    binding.classPath = item.classPath;

    if (item.customizable != CustomizableWeapon::None) {
        binding.source = BindingSource::CustomizableWeapon;
        binding.customizable = static_cast<int>(item.customizable);
    } else if (IsCurrentItemModularArmor(item)) {
        binding.source = BindingSource::ModularArmor;
        binding.modules = {armorModules.selected[0], armorModules.selected[1], armorModules.selected[2]};
    } else {
        if (item.classPath.empty()) return false;
        binding.source = BindingSource::ClassPath;
    }
    return true;
}

void ItemSpawnerSection::AddBindingFromCurrentSelection() {
    auto binding = std::make_shared<SpawnBinding>();
    binding->id = nextBindingId++;
    if (!CaptureCurrentSelection(*binding)) return;
    SpawnBindingUtils::CopyName(binding->name, binding->summary);
    InitBindingKeybind(binding);
    spawnBindings.push_back(std::move(binding));
    SaveSpawnBindings();
}

void ItemSpawnerSection::LoadSpawnBindings() {
    SpawnBindingUtils::LoadBindings<SpawnBinding>(
        ITEM_BINDINGS_SECTION, nextBindingId, spawnBindings, ItemBindingSection, "Spawn Item",
        [](SpawnBinding& binding, const std::string& section, ConfigManager& config) {
            binding.source = static_cast<BindingSource>(config.GetInt(section, "source", 0));
            binding.classPath = config.GetString(section, "class_path", "");
            binding.customizable = config.GetInt(section, "customizable", 0);
            binding.armorSlot = config.GetInt(section, "armor_slot", 0);
            binding.modules[0] = config.GetInt(section, "module_1", 0);
            binding.modules[1] = config.GetInt(section, "module_2", 0);
            binding.modules[2] = config.GetInt(section, "module_3", 0);
            binding.tier = config.GetInt(section, "tier", 4);
            binding.spawn = SpawnBindingUtils::LoadSpawnConfig(section, binding.spawn);
        },
        [this](const std::shared_ptr<SpawnBinding>& binding) { InitBindingKeybind(binding); }
    );
}

void ItemSpawnerSection::SaveSpawnBindings() {
    SpawnBindingUtils::SaveBindings<SpawnBinding>(
        ITEM_BINDINGS_SECTION, nextBindingId, spawnBindings, ItemBindingSection,
        [](const SpawnBinding& binding, const std::string& section, ConfigManager& config) {
            config.SetInt(section, "source", static_cast<int>(binding.source));
            config.SetString(section, "class_path", binding.classPath);
            config.SetInt(section, "customizable", binding.customizable);
            config.SetInt(section, "armor_slot", binding.armorSlot);
            config.SetInt(section, "module_1", binding.modules[0]);
            config.SetInt(section, "module_2", binding.modules[1]);
            config.SetInt(section, "module_3", binding.modules[2]);
            config.SetInt(section, "tier", binding.tier);
            SpawnBindingUtils::SaveSpawnConfig(section, binding.spawn);
        }
    );
}

void ItemSpawnerSection::RenderSpawnBindings() {
    SpawnBindingUtils::RenderList(
        spawnBindings, pendingDeleteBindingId, "Save the current item selection as its own keybind",
        "No item spawn bindings saved", "Replace this binding with the current item selection", "Delete Item Binding",
        "Delete item spawn binding?", [this] { AddBindingFromCurrentSelection(); },
        [this](SpawnBinding& binding) { CaptureCurrentSelection(binding); }, ItemBindingSection,
        [this] { SaveSpawnBindings(); }
    );
}

void ItemSpawnerSection::RenderSearchResults(BlueprintRegistry& reg) {
    if (!filteredIndices.empty()) {
        ImGui::Text("Found: %zu items", filteredIndices.size());
        ImGui::Spacing();

        auto& allItems = reg.GetAllItems();

        if (GuiUtils::BeginSizedCombo("##FilteredItems", "Select item...", cachedFilteredWidth)) {
            for (BlueprintRegistry::ItemIndex itemIdx : filteredIndices) {
                if (itemIdx >= allItems.size()) continue;
                auto& item = allItems[itemIdx];
                if (ImGui::Selectable(item.displayName.c_str(), false)) {
                    const auto& location = reg.GetItemLocation(itemIdx);
                    cfg.currentCategoryIndex = location.category;
                    cfg.currentSubcategoryIndex = location.subcategory;
                    cfg.currentItemIndex = location.item;
                    searchBuffer[0] = '\0';
                    searchActive = false;
                }
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No items found");
    }

    if (ImGui::Button("Clear Search")) {
        searchBuffer[0] = '\0';
        searchActive = false;
    }
}

void ItemSpawnerSection::RenderCategoryBrowser(BlueprintRegistry& reg) {
    size_t registryCatCount = reg.GetCategoryCount();
    size_t totalCatCount = registryCatCount + 1;

    ImGui::Text("Category");

    auto categoryGetter = [](void* data, int idx) -> const char* {
        auto* r = static_cast<BlueprintRegistry*>(data);
        size_t regCount = r->GetCategoryCount();
        if (static_cast<size_t>(idx) < regCount) return r->GetCategory(idx).name.c_str();
        if (static_cast<size_t>(idx) == regCount) return "Random Armor";
        return "";
    };

    int catIndex = static_cast<int>(cfg.currentCategoryIndex);
    if (catIndex < 0 || catIndex >= static_cast<int>(totalCatCount)) catIndex = 0;
    static float categoryComboW = 0;
    static size_t lastCatCount = 0;
    if (lastCatCount != totalCatCount) {
        categoryComboW = GuiUtils::CalcComboWidth(categoryGetter, &reg, static_cast<int>(totalCatCount));
        lastCatCount = totalCatCount;
    }
    GuiUtils::PrepareNextCombo(categoryComboW);
    if (ImGui::Combo("##CategorySelector", &catIndex, categoryGetter, &reg, static_cast<int>(totalCatCount)))
        [[unlikely]] {
        cfg.currentCategoryIndex = static_cast<uint8_t>(catIndex);
        cfg.currentSubcategoryIndex = 0;
        cfg.currentItemIndex = 0;
    }

    if (IsRandomArmorCategory()) {
        RenderRandomArmorUI();
    } else if (cfg.currentCategoryIndex < registryCatCount) {
        RenderBlueprintItemUI(reg);
    }
}

void ItemSpawnerSection::RenderRandomArmorUI() {
    ImGui::Text("Armor Slot");
    int slotIndex = static_cast<int>(cfg.currentItemIndex);
    auto armorSlotGetter = [](void* data, int idx) -> const char* {
        return static_cast<const GameConstants::ArmorSlotInfo*>(data)[idx].name;
    };
    static float armorSlotComboW =
        GuiUtils::CalcComboWidth(armorSlotGetter, (void*)GameConstants::ARMOR_SLOTS, GameConstants::ARMOR_SLOT_COUNT);
    GuiUtils::PrepareNextCombo(armorSlotComboW);
    if (ImGui::Combo(
            "##ArmorSlotSelector", &slotIndex, armorSlotGetter, (void*)GameConstants::ARMOR_SLOTS,
            GameConstants::ARMOR_SLOT_COUNT
        )) {
        cfg.currentItemIndex = static_cast<BlueprintRegistry::ItemIndex>(slotIndex);
    }

    if (cfg.currentItemIndex < TierValidation::VALID_ARMOR_TIER_MASKS.size()) {
        RenderMaskedTierCombo("##ArmorTierCombo", TierValidation::VALID_ARMOR_TIER_MASKS[cfg.currentItemIndex]);
    }
}

void ItemSpawnerSection::RenderBlueprintItemUI(BlueprintRegistry& reg) {
    auto& cat = reg.GetCategory(cfg.currentCategoryIndex);

    if (cat.subcategories.size() > 1) {
        ImGui::Text("Subcategory");
        int subIndex = static_cast<int>(cfg.currentSubcategoryIndex);
        if (subIndex >= static_cast<int>(cat.subcategories.size())) subIndex = 0;

        auto subGetter = [](void* data, int idx) -> const char* {
            auto* c = static_cast<const BlueprintRegistry::CategoryData*>(data);
            return c->subcategories[idx].name.c_str();
        };
        static float subcatComboW = 0;
        static uint8_t subcatCacheForCat = 255;
        if (subcatCacheForCat != cfg.currentCategoryIndex) {
            subcatComboW = GuiUtils::CalcComboWidth(subGetter, (void*)&cat, static_cast<int>(cat.subcategories.size()));
            subcatCacheForCat = cfg.currentCategoryIndex;
        }
        GuiUtils::PrepareNextCombo(subcatComboW);
        if (ImGui::Combo(
                "##SubcategorySelector", &subIndex, subGetter, (void*)&cat, static_cast<int>(cat.subcategories.size())
            )) [[unlikely]] {
            cfg.currentSubcategoryIndex = static_cast<uint8_t>(subIndex);
            cfg.currentItemIndex = 0;
        }
    }

    UpdateItemNamesCache();

    if (!cachedItemNames.empty()) {
        ImGui::Text("Item");
        int itemIndex = static_cast<int>(cfg.currentItemIndex);
        if (itemIndex >= static_cast<int>(cachedItemNames.size())) {
            itemIndex = 0;
            cfg.currentItemIndex = 0;
        }
        GuiUtils::PrepareNextCombo(cachedItemNamesWidth);
        if (ImGui::Combo(
                "##ItemSelector", &itemIndex, cachedItemNames.data(), static_cast<int>(cachedItemNames.size())
            )) [[unlikely]] {
            cfg.currentItemIndex = static_cast<BlueprintRegistry::ItemIndex>(itemIndex);
        }
    }

    auto* sub = GetCurrentSubcategory();
    if (sub && cfg.currentItemIndex < sub->itemIndices.size()) {
        auto& currentItem = reg.GetItem(sub->itemIndices[cfg.currentItemIndex]);
        if (currentItem.customizable != CustomizableWeapon::None) {
            reg.EnsureTiersScanned();
            RenderMaskedTierCombo(
                "##TierCombo", TierValidation::VALID_TIER_MASKS[static_cast<uint8_t>(currentItem.customizable)]
            );
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

void ItemSpawnerSection::RenderCustomPathSection(BlueprintRegistry& reg) {
    auto [world, player] = RenderPlayerWorld();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Custom Blueprint Path");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120.0f);
    ImGui::InputText("##CustomPath", customPathBuffer, sizeof(customPathBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Spawn##Custom")) {
        if (player && world && customPathBuffer[0] != '\0') {
            SpawnCustomPath();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (customPathBuffer[0] != '\0') {
            reg.AddCustomPath(customPathBuffer);
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
            reg.RemoveCustomPath(static_cast<size_t>(removeIdx));
        }
    }
}

void ItemSpawnerSection::RenderPresetSection() {
    auto [world, player] = RenderPlayerWorld();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::TreeNode("Spawn from Preset")) {
        bool canSpawn = player && world;

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
}

void ItemSpawnerSection::Render() {
    SectionStyle::StyleRAII style;
    auto [world, player] = RenderPlayerWorld();

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
        ImGui::TextColored(
            ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "Asset registry scan failed; showing customizable weapons and saved paths"
        );
    }

    RenderSpawnBindings();
    ImGui::Spacing();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Search");
    ImGui::SameLine();
    bool searchChanged =
        ImGui::InputText("##ItemSearch", searchBuffer, sizeof(searchBuffer), ImGuiInputTextFlags_AutoSelectAll);
    if (searchChanged) UpdateFilteredItems();

    if (searchActive) {
        RenderSearchResults(reg);
    } else {
        RenderCategoryBrowser(reg);
    }

    ImGui::Spacing();
    if (ImGui::Button("Spawn Item")) [[unlikely]] {
        if (player && world) {
            SpawnSelectedItem();
        }
    }

    RenderCustomPathSection(reg);
    RenderPresetSection();

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
