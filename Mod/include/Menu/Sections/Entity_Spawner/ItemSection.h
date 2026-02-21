#pragma once

#include <vector>
#include <cstdio>
#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Hooks/GameHook.h"
#include "Utils/Spawner.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/TierValidation.h"
#include "Utils/GuiUtils.h"
#include "Utils/BlueprintRegistry.h"

class ItemSection : public CollapsibleSection {
private:
    SectionConfig::ItemConfig& cfg = SectionConfig::item;

    struct ArmorSlotInfo {
        const char* displayName;
        int slotEnum;
    };

    static constexpr std::array<ArmorSlotInfo, 15> randomArmorSlots{{
        {"Head", 0}, {"Hands", 1}, {"Neck (Bevor)", 4}, {"Neck (Standard)", 5},
        {"Arms", 6}, {"Shoulders", 7}, {"Tabard", 8}, {"Chest (Plate)", 9},
        {"Hauberk", 10}, {"Cuisses", 11}, {"Body (Clothing)", 12},
        {"Waist", 13}, {"Legs (Greaves)", 14}, {"Feet", 15}, {"Hosen", 16}
    }};

    static inline char searchBuffer[128] = "";
    static inline std::vector<uint16_t> filteredIndices;
    static inline float cachedFilteredWidth = 0;
    static inline bool searchActive = false;

    static inline char customPathBuffer[256] = "";

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
                cachedItemNamesWidth = GuiUtils::CalcComboWidth(cachedItemNames.data(), static_cast<int>(cachedItemNames.size()));
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
            if (GuiUtils::MatchesFilter(allItems[i].displayName.c_str(), allItems[i].displayName.size(),
                                        searchBuffer, filterLen)) {
                filteredIndices.push_back(i);
                float w = ImGui::CalcTextSize(allItems[i].displayName.c_str()).x;
                if (w > maxW) maxW = w;
            }
        }
        cachedFilteredWidth = GuiUtils::ComboWidthFromText(maxW);
    }

    void SpawnSelectedItem() const noexcept {
        auto spawnTransform = player->GetTransform();
        const auto forward = player->GetActorForwardVector();
        spawnTransform.Translation.X += forward.X * cfg.spawnDistanceForward;
        spawnTransform.Translation.Y += forward.Y * cfg.spawnDistanceForward;
        spawnTransform.Translation.Z += cfg.spawnDistanceUp;
        spawnTransform.Scale3D = {cfg.spawnScale, cfg.spawnScale, cfg.spawnScale};

        if (IsRandomArmorCategory()) {
            if (cfg.currentItemIndex >= randomArmorSlots.size()) return;
            auto slot = static_cast<SDK::EArmorSlots_Enum>(randomArmorSlots[cfg.currentItemIndex].slotEnum);
            auto tier = static_cast<SDK::Enum_Ranks>(cfg.spawnTier);
            bool snap = cfg.snapToGround;
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
            Spawner::SpawnCustomizableWeapon(world, item.customizable, spawnTransform, cfg.snapToGround, cfg.spawnTier);
        } else if (!item.classPath.empty()) {
            Spawner::SpawnActor(world, item.classPath, spawnTransform, nullptr, cfg.snapToGround, cfg.spawnTier);
        }
    }

    void SpawnCustomPath() const noexcept {
        if (customPathBuffer[0] == '\0') return;
        auto spawnTransform = player->GetTransform();
        const auto forward = player->GetActorForwardVector();
        spawnTransform.Translation.X += forward.X * cfg.spawnDistanceForward;
        spawnTransform.Translation.Y += forward.Y * cfg.spawnDistanceForward;
        spawnTransform.Translation.Z += cfg.spawnDistanceUp;
        spawnTransform.Scale3D = {cfg.spawnScale, cfg.spawnScale, cfg.spawnScale};
        std::string path = customPathBuffer;
        Spawner::SpawnActor(world, path, spawnTransform, nullptr, cfg.snapToGround, cfg.spawnTier);
    }

public:
    ItemSection() : CollapsibleSection("Item") {
        Function("Spawn Item")
            .WithKey(&cfg.spawnItemKey)
            .WithParams({
                Parameter("snap_to_ground", "Snap to Ground", &cfg.snapToGround, "Automatically adjust height to touch the ground"),
                Parameter("distance_forward", "Forward Distance", &cfg.spawnDistanceForward, 50.0f, 300.0f, "How far in front the item appears"),
                Parameter("distance_up", "Up Distance", &cfg.spawnDistanceUp, 0.0f, 200.0f, "Height offset for spawn position"),
                Parameter("scale", "Scale", &cfg.spawnScale, 0.1f, 5.0f, "Size multiplier for the spawned item")
            })
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

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##ItemSearch", "Search items...", searchBuffer, sizeof(searchBuffer));

        float treeHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 3 - ImGui::GetStyle().ItemSpacing.y * 4;
        if (treeHeight < 100.0f) treeHeight = 100.0f;
        ImGui::BeginChild("##ItemTree", ImVec2(0, treeHeight), ImGuiChildFlags_Borders);

        const size_t filterLen = std::strlen(searchBuffer);
        const bool hasFilter = filterLen > 0;

        auto isItemVisible = [&](uint16_t globalIdx) -> bool {
            if (!hasFilter) return true;
            auto& item = reg.GetItem(globalIdx);
            return GuiUtils::MatchesFilter(item.displayName.c_str(), item.displayName.size(), searchBuffer, filterLen);
        };

        auto hasVisibleItems = [&](const BlueprintRegistry::SubcategoryData& sub) -> bool {
            if (!hasFilter) return !sub.itemIndices.empty();
            for (auto idx : sub.itemIndices)
                if (isItemVisible(idx)) return true;
            return false;
        };

        auto hasCategoryVisible = [&](size_t ci) -> bool {
            if (!hasFilter) return true;
            auto& cat = reg.GetCategory(ci);
            for (auto& sub : cat.subcategories)
                if (hasVisibleItems(sub)) return true;
            return false;
        };

        auto isSelectedCurrent = [&](uint8_t ci, uint8_t si, uint16_t ii) -> bool {
            return cfg.currentCategoryIndex == ci && cfg.currentSubcategoryIndex == si && cfg.currentItemIndex == ii;
        };

        for (size_t ci = 0; ci < registryCatCount; ++ci) {
            if (!hasCategoryVisible(ci)) continue;
            auto& cat = reg.GetCategory(ci);

            if (!ImGui::TreeNodeEx(cat.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) continue;

            for (size_t si = 0; si < cat.subcategories.size(); ++si) {
                auto& sub = cat.subcategories[si];
                if (!hasVisibleItems(sub)) continue;

                bool skipSubNode = (cat.subcategories.size() == 1);
                bool subOpen = skipSubNode || ImGui::TreeNodeEx(sub.name.c_str(), hasFilter ? ImGuiTreeNodeFlags_DefaultOpen : 0);

                if (subOpen) {
                    for (size_t ii = 0; ii < sub.itemIndices.size(); ++ii) {
                        if (!isItemVisible(sub.itemIndices[ii])) continue;
                        auto& item = reg.GetItem(sub.itemIndices[ii]);

                        ImGuiTreeNodeFlags leafFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                        if (isSelectedCurrent(static_cast<uint8_t>(ci), static_cast<uint8_t>(si), static_cast<uint16_t>(ii)))
                            leafFlags |= ImGuiTreeNodeFlags_Selected;

                        ImGui::TreeNodeEx(item.displayName.c_str(), leafFlags);
                        if (ImGui::IsItemClicked()) {
                            cfg.currentCategoryIndex = static_cast<uint8_t>(ci);
                            cfg.currentSubcategoryIndex = static_cast<uint8_t>(si);
                            cfg.currentItemIndex = static_cast<uint16_t>(ii);
                        }
                    }
                    if (!skipSubNode) ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        {
            bool armorVisible = !hasFilter;
            if (hasFilter) {
                for (const auto& slot : randomArmorSlots) {
                    if (GuiUtils::MatchesFilter(slot.displayName, std::strlen(slot.displayName), searchBuffer, filterLen)) {
                        armorVisible = true;
                        break;
                    }
                }
            }

            if (armorVisible && ImGui::TreeNodeEx("Random Armor", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (size_t i = 0; i < randomArmorSlots.size(); ++i) {
                    if (hasFilter && !GuiUtils::MatchesFilter(randomArmorSlots[i].displayName,
                        std::strlen(randomArmorSlots[i].displayName), searchBuffer, filterLen))
                        continue;

                    ImGuiTreeNodeFlags leafFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    bool isRandom = IsRandomArmorCategory() && cfg.currentItemIndex == static_cast<uint16_t>(i);
                    if (isRandom) leafFlags |= ImGuiTreeNodeFlags_Selected;

                    ImGui::TreeNodeEx(randomArmorSlots[i].displayName, leafFlags);
                    if (ImGui::IsItemClicked()) {
                        cfg.currentCategoryIndex = static_cast<uint8_t>(registryCatCount);
                        cfg.currentItemIndex = static_cast<uint16_t>(i);
                    }
                }
                ImGui::TreePop();
            }
        }

        ImGui::EndChild();

        {
            bool showTier = false;
            uint16_t tierMask = 0;

            if (IsRandomArmorCategory()) {
                if (cfg.currentItemIndex < TierValidation::VALID_ARMOR_TIER_MASKS.size()) {
                    tierMask = TierValidation::VALID_ARMOR_TIER_MASKS[cfg.currentItemIndex];
                    showTier = true;
                }
            } else {
                auto* sub = GetCurrentSubcategory();
                if (sub && cfg.currentItemIndex < sub->itemIndices.size()) {
                    auto& currentItem = reg.GetItem(sub->itemIndices[cfg.currentItemIndex]);
                    if (currentItem.customizable != CustomizableWeapon::None) {
                        tierMask = TierValidation::VALID_TIER_MASKS[static_cast<uint8_t>(currentItem.customizable)];
                        showTier = true;
                    }
                }
            }

            if (showTier) {
                cfg.spawnTier = TierValidation::NearestValidTier(tierMask, cfg.spawnTier);
                ImGui::SetNextItemWidth(GuiUtils::CachedTierComboWidth());
                if (ImGui::BeginCombo("##TierCombo", GuiUtils::TIER_LABELS[cfg.spawnTier])) {
                    for (int t = 0; t <= 8; ++t) {
                        if (!(tierMask & (1 << t))) continue;
                        if (ImGui::Selectable(GuiUtils::TIER_LABELS[t], t == cfg.spawnTier))
                            cfg.spawnTier = t;
                        if (t == cfg.spawnTier)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
        }

        if (ImGui::Button("Spawn Item", ImVec2(-1, 0))) {
            if (ComponentValidator::Validate(player) && ComponentValidator::Validate(world)) {
                SpawnSelectedItem();
            }
        }

        // Custom path input
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Custom Blueprint Path");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120.0f);
        ImGui::InputText("##CustomPath", customPathBuffer, sizeof(customPathBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Spawn##Custom")) {
            if (ComponentValidator::Validate(player) && ComponentValidator::Validate(world) && customPathBuffer[0] != '\0') {
                SpawnCustomPath();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            if (customPathBuffer[0] != '\0') {
                BlueprintRegistry::Get().AddCustomPath(customPathBuffer);
            }
        }

        // Custom saved paths
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

        // Rescan button
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
