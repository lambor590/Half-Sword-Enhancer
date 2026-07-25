#include "Menu/Sections/Spawner/ItemSpawnerSection.h"

#include <algorithm>
#include <cstring>

#include "Menu/Sections/Spawner/SpawnBindings.h"
#include "ConfigManager.h"
#include "Hooks/GameHook.h"

#include "Utils/ArmorGenerationUi.h"
#include "Utils/BlueprintRegistry.h"
#include "Utils/GameClass.h"
#include "Utils/Spawner.h"
#include "Utils/SpawnWorkflow.h"
#include "Utils/TierValidation.h"
#include "Utils/GuiUtils.h"
#include "Utils/PresetLinkResolution.h"
#include "Utils/WeaponGenerationUi.h"
#include "SDK/BP_Armor_Modular_Core_Master_classes.hpp"

namespace {
    constexpr const char* ITEM_SPAWNER_SECTION = "ItemSpawner";

    constexpr SpawnBindings::BindingConfig ITEM_BINDING_CONFIG{
        .indexSection = "ItemSpawnBindings",
        .bindingPrefix = "ItemSpawnBinding_",
        .defaultName = "Spawn Item",
        .addTooltip = "Create a shortcut for the current item and placement",
        .emptyText = "No item spawn shortcuts saved",
        .updateTooltip = "Use the current item and placement for this shortcut",
        .deletePopupTitle = "Delete Item Shortcut",
        .deletePrompt = "Delete this item spawn shortcut?",
        .spawnParams = {
            .forwardLabel = "Distance",
            .forwardMin = 50.0f,
            .forwardMax = 300.0f,
            .forwardTooltip = "How far in front the item appears",
            .upLabel = "Height",
            .upMin = 0.0f,
            .upMax = 200.0f,
            .upTooltip = "How high above the player the item appears",
            .scaleMin = 0.1f,
            .scaleMax = 5.0f,
            .scaleTooltip = "Item size",
        },
    };

    const char* ItemSpawnSourceName(ItemSpawnPresetSource source) {
        switch (source) {
            case ItemSpawnPresetSource::ClassPath: return "Selected item";
            case ItemSpawnPresetSource::CustomizableWeapon: return "Custom weapon";
            case ItemSpawnPresetSource::RandomArmor: return "Random armor";
            case ItemSpawnPresetSource::ModularArmor: return "Modular armor";
            case ItemSpawnPresetSource::WeaponPreset: return "Weapon preset";
            case ItemSpawnPresetSource::ArmorPreset: return "Armor preset";
        }
        return "Unknown";
    }

    std::array<std::uint64_t, 2> ItemBindingCatalogRevisions() {
        return {
            WeaponPresetSerializer::GetCatalogRevision(),
            ArmorPresetSerializer::GetCatalogRevision(),
        };
    }

}

SpawnWorkflow::SpawnCompletion ItemSpawnerSection::MakeSpawnCompletion(
    SpawnTarget target, GuiUtils::StatusMessage::Token token
) const {
    return [this, target, token](const SpawnWorkflow::SpawnResult& result) {
        if (result.success) {
            StoreSpawnResult(target, {.token = token});
            return;
        }

        static constexpr std::array<std::string_view, SPAWN_ROUTE_COUNT> FAILURE_PREFIXES{
            "Item failed: ",        "Custom item failed: ", "Item failed: ",
            "Weapon preset failed: ", "Armor preset failed: ", "Item shortcut failed: ",
        };
        const std::string_view prefix = FAILURE_PREFIXES[static_cast<std::size_t>(target)];
        std::string error;
        error.reserve(prefix.size() + result.error.size());
        error.append(prefix).append(result.error);
        StoreSpawnResult(target, {.token = token, .error = std::move(error)});
    };
}

void ItemSpawnerSection::StoreSpawnResult(SpawnTarget target, PendingSpawnResult result) const {
    std::lock_guard lock(spawnFeedbackMutex);
    auto& pending = pendingSpawnResults[static_cast<std::size_t>(target)];
    if (pending && result.token < pending->token) return;
    pending = std::move(result);
}

void ItemSpawnerSection::ConsumeSpawnFeedback() {
    std::array<std::optional<PendingSpawnResult>, SPAWN_ROUTE_COUNT> results;
    {
        std::lock_guard lock(spawnFeedbackMutex);
        results.swap(pendingSpawnResults);
    }

    for (std::size_t index = 0; index < results.size(); ++index) {
        auto& result = results[index];
        if (!result) continue;

        auto& status = spawnStatuses[index];
        if (result->token != 0 && result->token != status.revision) continue;
        if (!result->error.empty()) {
            status.SetError(std::move(result->error));
            continue;
        }

        if (result->token == 0)
            status.ClearText();
        else
            status.ClearText(result->token);
    }
}

void ItemSpawnerSection::QueueModulesForCore(std::string classPath) {
    if (classPath.empty() || classPath == armorModules.populatedFor ||
        (armorModules.loadQueued && classPath == armorModules.requestedFor))
        return;

    armorModules = {};
    armorModules.requestedFor = classPath;
    armorModules.loadQueued = true;

    const auto asyncState = armorModuleAsyncState;
    const bool queued = GameHook::QueueAction([asyncState,
                                               classPath = std::move(classPath)](const RuntimeContextSnapshot&) {
        ArmorModuleBatch batch;
        batch.classPath = classPath;

        auto* coreClass = Spawner::LoadClass(classPath);
        if (coreClass && coreClass->ClassDefaultObject && GameClass::IsModularArmor(coreClass->ClassDefaultObject)) {
            const auto* defaults = static_cast<SDK::ABP_Armor_Modular_Core_Master_C*>(coreClass->ClassDefaultObject);
            const auto collect = [](std::vector<std::string>& out, const SDK::TArray<SDK::UClass*>& source) {
                out.reserve(source.Num());
                for (int index = 0; index < source.Num(); ++index) {
                    if (source[index]) out.push_back(BlueprintRegistry::CleanDisplayName(source[index]->GetName()));
                }
            };
            collect(batch.slots[0], defaults->Available_Modules_1);
            collect(batch.slots[1], defaults->Available_Modules_2);
            collect(batch.slots[2], defaults->Available_Modules_3);
            batch.success = true;
        }

        std::scoped_lock lock(asyncState->mutex);
        asyncState->completed.push_back(std::move(batch));
    });
    if (!queued) {
        armorModules.loadQueued = false;
        armorModules.requestedFor.clear();
        SpawnStatus(SpawnTarget::SelectedItem).SetError("Could not load compatible armor parts");
    }
}

void ItemSpawnerSection::DrainPendingArmorModules() {
    std::vector<ArmorModuleBatch> completed;
    {
        std::scoped_lock lock(armorModuleAsyncState->mutex);
        completed = std::move(armorModuleAsyncState->completed);
        armorModuleAsyncState->completed.clear();
    }

    for (auto& batch : completed) {
        if (batch.classPath != armorModules.requestedFor) continue;
        for (int slot = 0; slot < 3; ++slot)
            armorModules.slots[slot] = std::move(batch.slots[slot]);
        armorModules.populatedFor = std::move(batch.classPath);
        armorModules.loadQueued = false;
        armorModules.loadSucceeded = batch.success;
    }
}

void ItemSpawnerSection::RenderModuleCombo(const char* label, int slot) {
    auto& modules = armorModules.slots[slot];
    if (modules.empty()) return;

    const char* preview =
        (armorModules.selected[slot] > 0 && armorModules.selected[slot] <= static_cast<int32_t>(modules.size()))
            ? modules[armorModules.selected[slot] - 1].c_str()
            : "None";

    if (armorModules.cachedWidths[slot] == 0.0f) {
        float maxW = 0;
        for (const auto& name : modules) {
            float w = ImGui::CalcTextSize(name.c_str()).x;
            if (w > maxW) maxW = w;
        }
        armorModules.cachedWidths[slot] = GuiUtils::ComboWidthFromText(maxW);
    }

    if (!GuiUtils::BeginSizedCombo(label, preview, armorModules.cachedWidths[slot])) return;

    if (ImGui::Selectable("None", armorModules.selected[slot] <= 0)) armorModules.selected[slot] = 0;
    for (int i = 0; i < static_cast<int>(modules.size()); ++i) {
        bool sel = (armorModules.selected[slot] == i + 1);
        if (ImGui::Selectable(modules[i].c_str(), sel)) armorModules.selected[slot] = i + 1;
        if (sel) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
}

bool ItemSpawnerSection::IsRandomArmorCategory() const {
    auto& reg = BlueprintRegistry::Get();
    return cfg.currentCategoryIndex == static_cast<uint8_t>(reg.GetCategoryCount());
}

const BlueprintRegistry::SubcategoryData* ItemSpawnerSection::GetCurrentSubcategory() const {
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

void ItemSpawnerSection::UpdateItemNamesCache() {
    auto& reg = BlueprintRegistry::Get();
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
    if (searchBuffer[0] == '\0') return;

    auto& reg = BlueprintRegistry::Get();
    reg.SearchItems(searchBuffer, filteredIndices);
    auto& allItems = reg.GetAllItems();

    float maxW = 0;
    for (BlueprintRegistry::ItemIndex i : filteredIndices) {
        float w = ImGui::CalcTextSize(allItems[i].displayName.c_str()).x;
        if (w > maxW) maxW = w;
    }
    cachedFilteredWidth = GuiUtils::ComboWidthFromText(maxW);
}

void ItemSpawnerSection::CaptureSpawnOptions(ItemSpawnPresetData& data) const {
    data.spawn = {
        .distanceForward = cfg.spawn.distanceForward,
        .distanceUp = cfg.spawn.distanceUp,
        .scale = cfg.spawn.scale,
        .snapToGround = cfg.spawn.snapToGround,
    };
    data.tier = cfg.spawnTier;
    data.weaponSpecificType = cfg.weaponSpecificType;
    data.armorGeneration = {
        .moduleChance = cfg.armorOptions.moduleChance,
        .forceMetalMaterial = cfg.armorOptions.forceMetalMaterial,
        .steelType = EquipmentGenerator::SteelTypeIndex(cfg.armorOptions.steelType),
        .metalPiecesType = EquipmentGenerator::SecondaryMetalTypeIndex(cfg.armorOptions.metalPiecesType),
    };
}

bool ItemSpawnerSection::TryBuildCurrentSelection(ItemSpawnPresetData& data, std::string& error) const {
    data = {};
    CaptureSpawnOptions(data);
    if (IsRandomArmorCategory()) {
        if (cfg.currentItemIndex >= GameConstants::ARMOR_SLOT_COUNT) {
            error = "Select a valid random armor slot";
            return false;
        }
        data.source = ItemSpawnPresetSource::RandomArmor;
        data.armorSlotIndex = static_cast<int>(cfg.currentItemIndex);
        data.name = std::string("Random ") + GameConstants::ARMOR_SLOTS[data.armorSlotIndex].name;
        return true;
    }

    const auto* subcategory = GetCurrentSubcategory();
    if (!subcategory || cfg.currentItemIndex >= subcategory->itemIndices.size()) {
        error = "Select an item above";
        return false;
    }

    const auto& item = BlueprintRegistry::Get().GetItem(subcategory->itemIndices[cfg.currentItemIndex]);
    data.name = item.displayName;
    data.classPath = item.classPath;

    if (item.customizable != CustomizableWeapon::None) {
        data.source = ItemSpawnPresetSource::CustomizableWeapon;
        data.customizableWeapon = static_cast<int>(item.customizable);
    } else if (
        Spawner::GetActorType(item.classPath) == Spawner::ActorType::Armor &&
        item.classPath.find("Modular_Core") != std::string::npos
    ) {
        data.source = ItemSpawnPresetSource::ModularArmor;
        data.modularArmorModules =
            pendingArmorModuleClassPath == data.classPath
                ? pendingArmorModuleSelection
                : std::array<int, 3>{armorModules.selected[0], armorModules.selected[1], armorModules.selected[2]};
    } else {
        data.source = ItemSpawnPresetSource::ClassPath;
        if (data.classPath.empty()) {
            error = "The selected item is unavailable";
            return false;
        }
    }
    return true;
}

void ItemSpawnerSection::SpawnSelectedItem() {
    constexpr SpawnTarget TARGET = SpawnTarget::SelectedItem;
    ItemSpawnPresetData data;
    std::string error;
    if (!TryBuildCurrentSelection(data, error)) {
        SpawnStatus(TARGET).SetError("Could not spawn item: " + error);
        return;
    }
    const auto token = SpawnStatus(TARGET).SetInfo({});
    (void)SpawnWorkflow::QueueItemPresetSpawn(RenderSnapshot(), data, MakeSpawnCompletion(TARGET, token));
}

void ItemSpawnerSection::SpawnCustomPath() {
    constexpr SpawnTarget TARGET = SpawnTarget::CustomItem;
    ItemSpawnPresetData data;
    CaptureSpawnOptions(data);
    data.source = ItemSpawnPresetSource::ClassPath;
    data.classPath = customPathBuffer;
    const auto token = SpawnStatus(TARGET).SetInfo({});
    (void)SpawnWorkflow::QueueItemPresetSpawn(RenderSnapshot(), data, MakeSpawnCompletion(TARGET, token));
}

void ItemSpawnerSection::SpawnWeaponFromPreset() {
    constexpr SpawnTarget TARGET = SpawnTarget::WeaponPreset;
    auto loaded = WeaponPresetSerializer::LoadFromFileResult(weaponPicker.SelectedPath());
    if (!loaded.success) {
        SpawnStatus(TARGET).SetError("Weapon preset: " + loaded.error);
        return;
    }

    ItemSpawnPresetData data;
    CaptureSpawnOptions(data);
    data.source = ItemSpawnPresetSource::WeaponPreset;
    data.weaponPreset = MakePresetCopyLink(std::move(loaded.value));
    const auto token = SpawnStatus(TARGET).SetInfo("Spawning weapon...");
    (void)SpawnWorkflow::QueueItemPresetSpawn(RenderSnapshot(), data, MakeSpawnCompletion(TARGET, token));
}

void ItemSpawnerSection::SpawnArmorFromPreset() {
    constexpr SpawnTarget TARGET = SpawnTarget::ArmorPreset;
    auto loaded = ArmorPresetSerializer::LoadFromFileResult(armorPicker.SelectedPath());
    if (!loaded.success) {
        SpawnStatus(TARGET).SetError("Armor preset: " + loaded.error);
        return;
    }

    ItemSpawnPresetData data;
    CaptureSpawnOptions(data);
    data.source = ItemSpawnPresetSource::ArmorPreset;
    data.armorPreset = MakePresetCopyLink(std::move(loaded.value));
    const auto token = SpawnStatus(TARGET).SetInfo("Spawning armor...");
    (void)SpawnWorkflow::QueueItemPresetSpawn(RenderSnapshot(), data, MakeSpawnCompletion(TARGET, token));
}

bool ItemSpawnerSection::TryBuildItemSpawnPreset(ItemSpawnPresetData& data, std::string& error, bool validate) const {
    data = {};
    error.clear();

    switch (profileDraftSource) {
        case ProfileDraftSource::LoadedFallback: data = loadedProfileFallback; break;
        case ProfileDraftSource::CurrentSelection:
            if (!TryBuildCurrentSelection(data, error)) return false;
            break;
        case ProfileDraftSource::CustomPath:
            data.source = ItemSpawnPresetSource::ClassPath;
            data.classPath = customPathBuffer;
            if (data.classPath.empty()) {
                error = "Enter a custom item address";
                return false;
            }
            data.name = BlueprintRegistry::CleanDisplayName(data.classPath);
            break;
        case ProfileDraftSource::WeaponPreset:
            data.source = ItemSpawnPresetSource::WeaponPreset;
            data.weaponPreset = profileWeaponLink.GetLink();
            if (!profileWeaponLink.HasLink() || profileWeaponLink.IsBroken()) {
                error = profileWeaponLink.HasLink() ? "Weapon preset: " + profileWeaponLink.GetDiagnostic()
                                                    : "Choose a weapon preset as Copy or Reference";
                return false;
            }
            if (const auto* copy = GetPresetCopy(data.weaponPreset)) data.name = copy->name;
            if (const auto* reference = GetPresetReference(data.weaponPreset)) data.name = reference->id;
            break;
        case ProfileDraftSource::ArmorPreset:
            data.source = ItemSpawnPresetSource::ArmorPreset;
            data.armorPreset = profileArmorLink.GetLink();
            if (!profileArmorLink.HasLink() || profileArmorLink.IsBroken()) {
                error = profileArmorLink.HasLink() ? "Armor preset: " + profileArmorLink.GetDiagnostic()
                                                   : "Choose an armor preset as Copy or Reference";
                return false;
            }
            if (const auto* copy = GetPresetCopy(data.armorPreset)) data.name = copy->name;
            if (const auto* reference = GetPresetReference(data.armorPreset)) data.name = reference->id;
            break;
    }

    data.id.clear();
    CaptureSpawnOptions(data);
    if (validate) {
        if (auto validation = data.ValidateForSave(); !validation) {
            error = std::move(validation.error);
            return false;
        }
    }
    return true;
}

bool ItemSpawnerSection::SelectRegistryIndex(std::size_t index) {
    auto& registry = BlueprintRegistry::Get();
    if (index >= registry.GetAllItems().size()) return false;
    const auto& location = registry.GetItemLocation(index);
    cfg.currentCategoryIndex = location.category;
    cfg.currentSubcategoryIndex = location.subcategory;
    cfg.currentItemIndex = location.item;
    searchBuffer[0] = '\0';
    lastCategoryIndex = 255;
    lastSubcategoryIndex = 255;
    return true;
}

bool ItemSpawnerSection::SelectRegistryItemByClassPath(std::string_view classPath) {
    if (classPath.empty()) return false;
    const auto& items = BlueprintRegistry::Get().GetAllItems();
    const auto item = std::find_if(items.begin(), items.end(), [classPath](const BlueprintEntry& entry) {
        return entry.classPath == classPath;
    });
    return item != items.end() && SelectRegistryIndex(static_cast<std::size_t>(std::distance(items.begin(), item)));
}

bool ItemSpawnerSection::SelectRegistryCustomizable(int customizable) {
    const auto& items = BlueprintRegistry::Get().GetAllItems();
    const auto item = std::find_if(items.begin(), items.end(), [customizable](const BlueprintEntry& entry) {
        return static_cast<int>(entry.customizable) == customizable;
    });
    return item != items.end() && SelectRegistryIndex(static_cast<std::size_t>(std::distance(items.begin(), item)));
}

void ItemSpawnerSection::ApplyItemSpawnPreset(const ItemSpawnPresetData& data) {
    loadedProfileFallback = data;
    profileDraftError.clear();
    pendingArmorModuleClassPath.clear();

    cfg.spawn = {
        .distanceForward = static_cast<float>(data.spawn.distanceForward),
        .distanceUp = static_cast<float>(data.spawn.distanceUp),
        .scale = static_cast<float>(data.spawn.scale),
        .snapToGround = data.spawn.snapToGround,
    };
    cfg.spawnTier = data.tier;
    cfg.weaponSpecificType = data.weaponSpecificType;
    cfg.armorOptions = {
        .moduleChance = static_cast<float>(data.armorGeneration.moduleChance),
        .forceMetalMaterial = data.armorGeneration.forceMetalMaterial,
        .steelType = EquipmentGenerator::SteelTypeFromIndex(data.armorGeneration.steelType),
        .metalPiecesType = EquipmentGenerator::SecondaryMetalTypeFromIndex(data.armorGeneration.metalPiecesType),
    };
    profileWeaponLink.SetLink(data.weaponPreset);
    profileArmorLink.SetLink(data.armorPreset);

    switch (data.source) {
        case ItemSpawnPresetSource::ClassPath:
            (void)SelectRegistryItemByClassPath(data.classPath);
            if (data.classPath.size() < sizeof(customPathBuffer)) {
                strncpy_s(customPathBuffer, data.classPath.c_str(), _TRUNCATE);
                profileDraftSource = ProfileDraftSource::CustomPath;
            } else {
                profileDraftSource = ProfileDraftSource::LoadedFallback;
            }
            break;
        case ItemSpawnPresetSource::CustomizableWeapon:
            profileDraftSource = SelectRegistryCustomizable(data.customizableWeapon)
                                     ? ProfileDraftSource::CurrentSelection
                                     : ProfileDraftSource::LoadedFallback;
            break;
        case ItemSpawnPresetSource::RandomArmor:
            cfg.currentCategoryIndex = static_cast<uint8_t>(BlueprintRegistry::Get().GetCategoryCount());
            cfg.currentSubcategoryIndex = 0;
            cfg.currentItemIndex = static_cast<BlueprintRegistry::ItemIndex>(data.armorSlotIndex);
            searchBuffer[0] = '\0';
            lastCategoryIndex = 255;
            lastSubcategoryIndex = 255;
            profileDraftSource = ProfileDraftSource::CurrentSelection;
            break;
        case ItemSpawnPresetSource::ModularArmor:
            if (SelectRegistryItemByClassPath(data.classPath)) {
                pendingArmorModuleSelection = data.modularArmorModules;
                pendingArmorModuleClassPath = data.classPath;
                profileDraftSource = ProfileDraftSource::CurrentSelection;
            } else {
                profileDraftSource = ProfileDraftSource::LoadedFallback;
            }
            break;
        case ItemSpawnPresetSource::WeaponPreset: profileDraftSource = ProfileDraftSource::WeaponPreset; break;
        case ItemSpawnPresetSource::ArmorPreset: profileDraftSource = ProfileDraftSource::ArmorPreset; break;
    }
}

void ItemSpawnerSection::SpawnItemSpawnPreset() {
    constexpr SpawnTarget TARGET = SpawnTarget::ItemPreset;
    ItemSpawnPresetData data;
    std::string error;
    if (!TryBuildItemSpawnPreset(data, error)) {
        SpawnStatus(TARGET).SetError(error.empty() ? "Could not spawn item" : std::move(error));
        return;
    }
    const auto token = SpawnStatus(TARGET).SetInfo("Spawning item...");
    (void)SpawnWorkflow::QueueItemPresetSpawn(RenderSnapshot(), data, MakeSpawnCompletion(TARGET, token));
}

ItemSpawnerSection::ItemSpawnerSection(ModContext& ctx) : Section(ctx, SECTION) {
    cfg.weaponSpecificType = ConfigManager::Get().GetInt(
        ITEM_SPAWNER_SECTION, WeaponGenerationUi::SPECIFIC_TYPE_CONFIG_KEY, cfg.weaponSpecificType
    );
    LoadSpawnBindings();
}

struct ItemSpawnerSection::BindingOps {
    ItemSpawnerSection& owner;

    bool Capture(SpawnBinding& binding) const { return owner.CaptureCurrentSelection(binding); }

    void LoadFields(SpawnBinding& binding, ConfigManager& config, const char* section) const {
        std::string serialized;
        const auto encoded = config.GetString(section, "data_hex", "");
        if (!SpawnBindings::DecodeData(encoded, serialized)) {
            binding.resolutionError = "saved shortcut is unavailable";
            Refresh(binding);
            return;
        }
        auto loaded = ItemSpawnPresetSerializer::DeserializeFromIniResult(serialized);
        if (!loaded.success) {
            binding.resolutionError = "saved shortcut is unavailable";
            Refresh(binding);
            return;
        }
        binding.data = std::move(loaded.value);
        Refresh(binding);
    }

    void SaveFields(const SpawnBinding& binding, ConfigManager& config, const char* section) const {
        if (binding.data.id.empty()) return;
        const auto encoded = SpawnBindings::EncodeData(ItemSpawnPresetSerializer::SerializeToIni(binding.data));
        config.SetString(section, "data_hex", encoded.c_str());
    }

    void Refresh(SpawnBinding& binding) const {
        if (binding.data.id.empty()) {
            if (binding.resolutionError.empty()) binding.resolutionError = "saved shortcut is unavailable";
            binding.summary = "Unavailable item shortcut";
            return;
        }

        binding.resolutionError.clear();
        binding.summary = binding.data.name.empty() ? ItemSpawnSourceName(binding.data.source) : binding.data.name;
        if (auto validation = binding.data.ValidateForSave(); !validation) {
            binding.resolutionError = std::move(validation.error);
            binding.summary += " + Unavailable";
            return;
        }

        if (binding.data.source == ItemSpawnPresetSource::WeaponPreset && !GetPresetCopy(binding.data.weaponPreset)) {
            auto resolved = PresetLinkResolution::Resolve<WeaponPresetSerializer>(binding.data.weaponPreset);
            if (!resolved.success || !resolved.value) {
                binding.resolutionError =
                    resolved.error.empty() ? "weapon preset is unavailable" : std::move(resolved.error);
            }
        } else if (
            binding.data.source == ItemSpawnPresetSource::ArmorPreset && !GetPresetCopy(binding.data.armorPreset)
        ) {
            auto resolved = PresetLinkResolution::Resolve<ArmorPresetSerializer>(binding.data.armorPreset);
            if (!resolved.success || !resolved.value) {
                binding.resolutionError =
                    resolved.error.empty() ? "armor preset is unavailable" : std::move(resolved.error);
            }
        }

        if (!binding.resolutionError.empty()) binding.summary += " + Unavailable";
    }

    std::shared_ptr<const ItemSpawnPresetData> MakeSnapshot(const SpawnBinding& binding) const {
        return std::make_shared<const ItemSpawnPresetData>(binding.data);
    }

    void Spawn(const ItemSpawnPresetData& data, const RuntimeContextSnapshot& runtime) const {
        (void)SpawnWorkflow::SpawnItemPreset(
            runtime, data, owner.MakeSpawnCompletion(SpawnTarget::Shortcuts, 0)
        );
    }

    void AppendParams(
        SpawnBinding& binding, std::vector<KeybindParam>& params, const SpawnBindings::SpawnParamConfig& config
    ) const {
        auto& spawn = binding.data.spawn;
        SpawnBindings::AppendSpawnParams(
            params, spawn.distanceForward, spawn.distanceUp, spawn.scale, spawn.snapToGround, config
        );
    }
};

bool ItemSpawnerSection::CaptureCurrentSelection(SpawnBinding& binding) {
    ItemSpawnPresetData data;
    std::string error;
    if (!TryBuildItemSpawnPreset(data, error)) {
        SpawnStatus(SpawnTarget::Shortcuts).SetError("Could not create item shortcut: " + error);
        return false;
    }
    data.name = data.name.empty() ? ItemSpawnSourceName(data.source) : data.name;
    data.id = "item-binding-" + std::to_string(binding.id);
    binding.summary = data.name;
    binding.data = std::move(data);
    return true;
}

void ItemSpawnerSection::LoadSpawnBindings() {
    SpawnBindings::BindingList<SpawnBinding, BindingOps>(
        spawnBindings, nextBindingId, pendingDeleteBindingId, ITEM_BINDING_CONFIG, BindingOps{*this}
    )
        .Load();
    spawnBindingCatalogRevisions = ItemBindingCatalogRevisions();
}

void ItemSpawnerSection::RenderSpawnBindings() {
    auto bindings = SpawnBindings::BindingList<SpawnBinding, BindingOps>(
        spawnBindings, nextBindingId, pendingDeleteBindingId, ITEM_BINDING_CONFIG, BindingOps{*this}
    );
    const auto revisions = ItemBindingCatalogRevisions();
    if (revisions != spawnBindingCatalogRevisions) {
        spawnBindingCatalogRevisions = revisions;
        bindings.PublishSnapshots();
    }
    bindings.Render();
}

void ItemSpawnerSection::RenderSearchResults(BlueprintRegistry& reg) {
    if (!filteredIndices.empty()) {
        ImGui::Text("%zu items found", filteredIndices.size());
        ImGui::Spacing();

        auto& allItems = reg.GetAllItems();

        if (GuiUtils::BeginSizedCombo("##FilteredItems", "Select item...", cachedFilteredWidth)) {
            GuiUtils::RenderClippedList(static_cast<int>(filteredIndices.size()), -1, [&](int row) {
                const auto itemIdx = filteredIndices[static_cast<size_t>(row)];
                auto& item = allItems[itemIdx];
                if (ImGui::Selectable(item.displayName.c_str(), false)) {
                    const auto& location = reg.GetItemLocation(itemIdx);
                    cfg.currentCategoryIndex = location.category;
                    cfg.currentSubcategoryIndex = location.subcategory;
                    cfg.currentItemIndex = location.item;
                    searchBuffer[0] = '\0';
                }
            });
            ImGui::EndCombo();
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No items found");
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
    if (catIndex >= static_cast<int>(totalCatCount)) {
        catIndex = 0;
        cfg.currentCategoryIndex = 0;
        cfg.currentSubcategoryIndex = 0;
        cfg.currentItemIndex = 0;
        lastCategoryIndex = 255;
        lastSubcategoryIndex = 255;
    }
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
    ArmorGenerationUi::RenderOptions(cfg.armorOptions);
}

void ItemSpawnerSection::RenderBlueprintItemUI(BlueprintRegistry& reg) {
    auto& cat = reg.GetCategory(cfg.currentCategoryIndex);

    if (cat.subcategories.size() > 1) {
        ImGui::Text("Subcategory");
        int subIndex = static_cast<int>(cfg.currentSubcategoryIndex);
        if (subIndex >= static_cast<int>(cat.subcategories.size())) {
            subIndex = 0;
            cfg.currentSubcategoryIndex = 0;
            cfg.currentItemIndex = 0;
            lastSubcategoryIndex = 255;
        }

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
        const auto actorType = Spawner::GetActorType(currentItem.classPath);
        if (currentItem.customizable != CustomizableWeapon::None) {
            reg.EnsureTiersScanned();
            RenderMaskedTierCombo(
                "##TierCombo", TierValidation::VALID_TIER_MASKS[static_cast<uint8_t>(currentItem.customizable)]
            );
        } else if (actorType == Spawner::ActorType::Weapon) {
            GuiUtils::RenderFreeTierCombo("Tier", cfg.spawnTier);
            if (WeaponGenerationUi::RenderSpecificTypeCombo("Weapon Type", cfg.weaponSpecificType)) {
                ConfigManager::Get()
                    .SetInt(ITEM_SPAWNER_SECTION, WeaponGenerationUi::SPECIFIC_TYPE_CONFIG_KEY, cfg.weaponSpecificType);
            }
            GuiUtils::HelpTooltip("Choose the kind of random weapon you want");
        }

        if (actorType == Spawner::ActorType::Armor &&
            currentItem.classPath.find("Modular_Core") != std::string::npos) {
            QueueModulesForCore(currentItem.classPath);
            if (armorModules.populatedFor == currentItem.classPath && armorModules.loadSucceeded) {
                if (pendingArmorModuleClassPath == currentItem.classPath) {
                    for (int slot = 0; slot < 3; ++slot)
                        armorModules.selected[slot] = pendingArmorModuleSelection[slot];
                    pendingArmorModuleClassPath.clear();
                }
                RenderModuleCombo("Armor Part 1##m1", 0);
                RenderModuleCombo("Armor Part 2##m2", 1);
                RenderModuleCombo("Armor Part 3##m3", 2);
            } else if (armorModules.loadQueued)
                ImGui::TextDisabled("Loading armor parts...");
            else
                ImGui::TextDisabled("No compatible armor parts were found");
        }
    }
}

void ItemSpawnerSection::RenderCustomPathSection(BlueprintRegistry& reg, bool canSpawn) {
    ImGui::Text("Custom Item Address");
    GuiUtils::SetNextInputWidth();
    ImGui::InputText("##CustomPath", customPathBuffer, sizeof(customPathBuffer));

    const bool hasPath = customPathBuffer[0] != '\0';
    if (!hasPath || !canSpawn) ImGui::BeginDisabled();
    if (GuiUtils::Button("Spawn Custom Item", GuiUtils::ButtonTone::Primary)) SpawnCustomPath();
    if (!hasPath || !canSpawn) ImGui::EndDisabled();
    if ((!hasPath || !canSpawn) &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetItemTooltip(!hasPath ? "Enter an item address first" : "Open a map before spawning");
    SpawnStatus(SpawnTarget::CustomItem).Render();

    if (!hasPath) ImGui::BeginDisabled();
    if (GuiUtils::Button("Save Address")) reg.AddCustomPath(customPathBuffer);
    if (!hasPath) ImGui::EndDisabled();
    if (!hasPath && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetItemTooltip("Enter an item address first");

    auto& savedPaths = reg.GetCustomPaths();
    if (!savedPaths.empty()) {
        ImGui::Spacing();
        ImGui::Text("Saved Addresses (%zu)", savedPaths.size());
        int removeIdx = -1;
        for (size_t i = 0; i < savedPaths.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::BulletText("%s", savedPaths[i].c_str());
            (void)GuiUtils::SameLineIfFitsButton("X");
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

void ItemSpawnerSection::RenderItemSpawnProfiles(bool canSpawn) {
    if (!ImGui::TreeNodeEx("Item Presets", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::SeparatorText("What to Spawn");

    const char* sourcePreview = "Current item";
    switch (profileDraftSource) {
        case ProfileDraftSource::CurrentSelection: sourcePreview = "Current item"; break;
        case ProfileDraftSource::CustomPath: sourcePreview = "Custom item address"; break;
        case ProfileDraftSource::WeaponPreset: sourcePreview = "Weapon preset"; break;
        case ProfileDraftSource::ArmorPreset: sourcePreview = "Armor preset"; break;
        case ProfileDraftSource::LoadedFallback: sourcePreview = "Loaded preset"; break;
    }

    if (GuiUtils::BeginSizedCombo("Item Type##ItemProfile", sourcePreview, 230.0f)) {
        struct SourceOption {
            ProfileDraftSource source;
            const char* label;
        };
        static constexpr SourceOption SOURCE_OPTIONS[] = {
            {ProfileDraftSource::CurrentSelection, "Current item"},
            {ProfileDraftSource::CustomPath, "Custom item address"},
            {ProfileDraftSource::WeaponPreset, "Weapon preset"},
            {ProfileDraftSource::ArmorPreset, "Armor preset"},
        };
        for (const auto& option : SOURCE_OPTIONS) {
            const bool selected = profileDraftSource == option.source;
            if (ImGui::Selectable(option.label, selected)) {
                profileDraftSource = option.source;
                profileDraftError.clear();
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    switch (profileDraftSource) {
        case ProfileDraftSource::CurrentSelection:
            ImGui::TextDisabled("Uses the item and options selected above.");
            break;
        case ProfileDraftSource::CustomPath:
            GuiUtils::SetNextInputWidth();
            ImGui::InputTextWithHint(
                "##ItemProfileCustomPath", "/Game/.../Blueprint.Blueprint_C", customPathBuffer, sizeof(customPathBuffer)
            );
            break;
        case ProfileDraftSource::WeaponPreset:
            (void)profileWeaponLink.Render("Weapon", "Select a weapon preset");
            break;
        case ProfileDraftSource::ArmorPreset: (void)profileArmorLink.Render("Armor", "Select an armor preset"); break;
        case ProfileDraftSource::LoadedFallback:
            ImGui::TextColored(
                ImVec4(0.45f, 0.8f, 1.0f, 1.0f), "[Preset] %s", ItemSpawnSourceName(loadedProfileFallback.source)
            );
            ImGui::TextWrapped("The original item is no longer listed. You can still spawn or resave this preset.");
            break;
    }

    ImGui::SeparatorText("Placement");
    const auto& style = ImGui::GetStyle();
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    GuiUtils::DebouncedDragFloat("Distance##ItemProfile", &cfg.spawn.distanceForward, 1.0f, 50.0f, 300.0f, "%.0f");
    (void)GuiUtils::SameLineIfFits(GuiUtils::K_DRAG_WIDTH + style.ItemInnerSpacing.x + ImGui::CalcTextSize("Height").x);
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    GuiUtils::DebouncedDragFloat("Height##ItemProfile", &cfg.spawn.distanceUp, 1.0f, 0.0f, 200.0f, "%.0f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    GuiUtils::DebouncedDragFloat("Size##ItemProfile", &cfg.spawn.scale, 0.05f, 0.1f, 5.0f, "%.2f");
    (void)GuiUtils::SameLineIfFitsCheckbox("Place on Ground##ItemProfile");
    ImGui::Checkbox("Place on Ground##ItemProfile", &cfg.spawn.snapToGround);

    ItemSpawnPresetData draft;
    profileDraftError.clear();
    const bool draftValid = TryBuildItemSpawnPreset(draft, profileDraftError, false);
    if (draftValid) {
        ImGui::TextColored(ImVec4(0.45f, 1.0f, 0.6f, 1.0f), "[Ready] %s", ItemSpawnSourceName(draft.source));
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[Incomplete]");
        ImGui::TextWrapped("%s", profileDraftError.c_str());
    }

    if (!draftValid || !canSpawn) ImGui::BeginDisabled();
    if (ImGui::Button("Spawn Item")) SpawnItemSpawnPreset();
    if (!draftValid || !canSpawn) ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (!draftValid)
            ImGui::SetItemTooltip("Complete the item preset before spawning");
        else if (!canSpawn)
            ImGui::SetItemTooltip("Open a map before spawning");
        else
            ImGui::SetItemTooltip("Spawn one item from this preset");
    }
    SpawnStatus(SpawnTarget::ItemPreset).Render();

    itemSpawnPresets.RenderPresetsTab(
        [draft](const char*, bool) { return PresetBuildResult<ItemSpawnPresetData>::Success(draft); },
        [this](const ItemSpawnPresetData& loaded) {
            ApplyItemSpawnPreset(loaded);
            return PresetApplyDisposition::Applied;
        },
        draftValid
    );
    itemSpawnPresets.status.Render();
    ImGui::TreePop();
}

void ItemSpawnerSection::RenderPresetSection(bool canSpawn) {
    ImGui::Separator();

    RenderItemSpawnProfiles(canSpawn);

    if (ImGui::TreeNode("Weapon & Armor Presets")) {
        weaponPicker.Render("Weapon Preset");
        if (weaponPicker.HasSelection()) {
            if (!canSpawn) ImGui::BeginDisabled();
            if (ImGui::Button("Spawn Weapon##WeaponPreset")) SpawnWeaponFromPreset();
            if (!canSpawn) ImGui::EndDisabled();
            if (!canSpawn && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetItemTooltip("Open a map before spawning");
            SpawnStatus(SpawnTarget::WeaponPreset).Render();
        }

        armorPicker.Render("Armor Preset");
        if (armorPicker.HasSelection()) {
            if (!canSpawn) ImGui::BeginDisabled();
            if (ImGui::Button("Spawn Armor##ArmorPreset")) SpawnArmorFromPreset();
            if (!canSpawn) ImGui::EndDisabled();
            if (!canSpawn && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetItemTooltip("Open a map before spawning");
            SpawnStatus(SpawnTarget::ArmorPreset).Render();
        }

        ImGui::TreePop();
    }
}

void ItemSpawnerSection::Render() {
    ConsumeSpawnFeedback();
    DrainPendingArmorModules();
    auto [world, player] = RenderPlayerWorld();
    const bool canSpawn = player && world;

    auto& reg = BlueprintRegistry::Get();
    auto scanState = reg.GetState();
    const auto requestRescan = [&]() {
        reg.RequestRescan();
        searchBuffer[0] = '\0';
        cfg.currentCategoryIndex = 0;
        cfg.currentSubcategoryIndex = 0;
        cfg.currentItemIndex = 0;
        lastCategoryIndex = 255;
        lastSubcategoryIndex = 255;
    };

    if (scanState == ScanState::NotStarted) {
        reg.RequestScan();
        GuiUtils::RenderCallout("item-catalog-start", "Preparing the item list...", GuiUtils::CalloutTone::Info);
        return;
    }

    if (scanState == ScanState::Scanning) {
        GuiUtils::RenderCallout("item-catalog-scan", "Building the item list...", GuiUtils::CalloutTone::Info);
        return;
    }

    if (scanState == ScanState::Failed) {
        const auto result = GuiUtils::RenderCallout(
            "item-catalog-failed",
            "Some items could not be added to the list. Custom weapons and saved addresses remain available.",
            GuiUtils::CalloutTone::Warning, false, "Try Again"
        );
        if (result.actionClicked) requestRescan();
    }

    ImGui::SeparatorText("Browse Items");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Search");
    ImGui::SameLine();
    bool searchChanged =
        ImGui::InputText("##ItemSearch", searchBuffer, sizeof(searchBuffer), ImGuiInputTextFlags_AutoSelectAll);
    if (searchChanged) UpdateFilteredItems();

    if (searchBuffer[0] != '\0') {
        RenderSearchResults(reg);
    } else {
        RenderCategoryBrowser(reg);
    }

    ImGui::Spacing();
    if (!canSpawn) ImGui::BeginDisabled();
    if (GuiUtils::Button("Spawn Selected Item", GuiUtils::ButtonTone::Primary)) SpawnSelectedItem();
    if (!canSpawn) ImGui::EndDisabled();
    if (!canSpawn && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetItemTooltip("Open a map before spawning an item");
    SpawnStatus(SpawnTarget::SelectedItem).Render();

    RenderPresetSection(canSpawn);

    SpawnStatus(SpawnTarget::Shortcuts).Render();
    if (ImGui::TreeNode("Spawn Shortcuts")) {
        RenderSpawnBindings();
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Advanced")) {
        RenderCustomPathSection(reg, canSpawn);
        ImGui::Spacing();
        if (ImGui::Button("Refresh Item List")) {
            requestRescan();
        }
        if (scanState == ScanState::Complete) {
            ImGui::SameLine();
            ImGui::TextDisabled("%zu items available", reg.GetAllItems().size());
        }
        ImGui::TreePop();
    }
}
