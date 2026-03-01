#pragma once

#include "Menu/ICollapsibleSection.h"
#include "Utils/MapRegistry.h"
#include "Utils/GuiUtils.h"
#include "Utils/Spawner.h"
#include "Utils/PlayerPresetSerializer.h"
#include "SDK/GI_Settings_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"

class MapLoaderSection : public CollapsibleSection {
    int selectedFilteredIndex = 0;
    int selectedCategoryIndex = 0;
    char searchBuffer[128] = "";
    std::vector<int> filteredIndices;
    float cachedComboW = 0.0f;
    float cachedCatComboW = 0.0f;
    std::string cachedLevelName;
    bool levelNameDirty = true;
    bool filterDirty = true;
    char customPathBuffer[512] = "";

    bool optFreshStart = false;
    bool optTutorial = false;
    bool optFreeMode = false;
    bool optCarnage = false;
    int  optFoesAmount = 3;
    int  optFoeTier = 0;
    int  optCombatantsAmount = 3;
    int  optOpponentTier = 0;

    bool optAutoSpawn = false;
    bool pendingAutoSpawn = false;
    std::vector<PresetListEntry> playerPresets;
    int selectedPresetIndex = -1;
    bool presetListDirty = true;

    static constexpr ImVec4 kYellowText{1.0f, 0.85f, 0.3f, 1.0f};
    static constexpr ImVec4 kOrangeText{1.0f, 0.5f, 0.3f, 1.0f};
    static constexpr ImVec4 kGrayText{0.5f, 0.5f, 0.5f, 1.0f};

    void RefreshLevelName() {
        if (!levelNameDirty) return;
        if (ComponentValidator::Validate(world)) {
            SDK::FString currentLevel = SDK::UGameplayStatics::GetCurrentLevelName(world, true);
            cachedLevelName = currentLevel.ToString();
        } else {
            cachedLevelName.clear();
        }
        levelNameDirty = false;
    }

    void CollectPresets(const PresetUtils::PresetTreeNode& node) {
        for (const auto& p : node.presets) playerPresets.push_back(p);
        for (const auto& child : node.children) CollectPresets(child);
    }

    void RefreshPresetList() {
        if (!presetListDirty) return;
        presetListDirty = false;
        playerPresets.clear();
        CollectPresets(PlayerPresetSerializer::ListPresetsTree());
    }

    void RebuildFilter(MapRegistry& reg) {
        if (!filterDirty) return;
        filterDirty = false;

        const auto& maps = reg.GetMaps();
        const auto& cats = reg.GetCategories();
        const size_t prevCapacity = filteredIndices.capacity();
        filteredIndices.clear();
        if (prevCapacity > 0)
            filteredIndices.reserve(prevCapacity);

        const size_t filterLen = std::strlen(searchBuffer);
        const bool hasCategory = selectedCategoryIndex > 0 &&
                                 selectedCategoryIndex <= static_cast<int>(cats.size());
        const char* selectedCat = hasCategory ? cats[selectedCategoryIndex - 1].c_str() : nullptr;

        const bool hasFilter = filterLen > 0;
        const bool unfiltered = !hasCategory && !hasFilter;

        if (unfiltered) {
            filteredIndices.resize(static_cast<int>(maps.size()));
            for (int i = 0; i < static_cast<int>(maps.size()); ++i)
                filteredIndices[i] = i;
            cachedComboW = GuiUtils::ComboWidthFromText(reg.GetMaxDisplayNameWidth());
        } else {
            float maxW = 0;
            for (int i = 0; i < static_cast<int>(maps.size()); ++i) {
                const auto& map = maps[i];
                if (hasCategory && map.category != selectedCat)
                    continue;
                if (hasFilter && !GuiUtils::MatchesFilter(map.displayName.c_str(),
                                                          map.displayName.size(),
                                                          searchBuffer, filterLen))
                    continue;
                filteredIndices.push_back(i);
                float w = ImGui::CalcTextSize(map.displayName.c_str()).x;
                if (w > maxW) maxW = w;
            }
            cachedComboW = GuiUtils::ComboWidthFromText(maxW);
        }

        selectedFilteredIndex = 0;
    }

    void LoadMap(const std::string& packageName) {
        if (!ComponentValidator::Validate(world)) return;
        if (optAutoSpawn) pendingAutoSpawn = true;

        bool freshStart = optFreshStart, tutorial = optTutorial;
        bool freeMode = optFreeMode, carnage = optCarnage;
        int foes = optFoesAmount, foeTier = optFoeTier;
        int combatants = optCombatantsAmount, opponentTier = optOpponentTier;

        GameHook::QueueAction([this, pn = packageName,
                               freshStart, tutorial, freeMode, carnage,
                               foes, foeTier, combatants, opponentTier]() {
            auto* gi = static_cast<SDK::UGI_Settings_C*>(
                SDK::UGameplayStatics::GetGameInstance(world));
            if (gi) {
                gi->Fresh_Start_Map__Temp_ = freshStart;
                gi->Tutorial_Enabled = tutorial;
                gi->Free_Mode_Activated = freeMode;
                gi->Free_Mode_Carnage = carnage;
                gi->Free_Mode_Foes_Amount = foes;
                gi->Free_Mode_Tier = static_cast<SDK::Enum_Ranks>(foeTier);
                gi->Combatants_Amount = combatants;
                gi->Current_Opponent_TIer_to_Spawn = static_cast<SDK::Enum_Ranks>(opponentTier);
            }

            std::wstring wideName(pn.begin(), pn.end());
            auto levelName = SDK::BasicFilesImpleUtils::StringToName(wideName.c_str());
            SDK::UGameplayStatics::OpenLevel(world, levelName, true, SDK::FString(L""));
            levelNameDirty = true;
        });
    }

    void SpawnPlayer() {
        std::filesystem::path presetPath;
        if (selectedPresetIndex >= 0 && selectedPresetIndex < static_cast<int>(playerPresets.size()))
            presetPath = playerPresets[selectedPresetIndex].path;

        GameHook::QueueAction([this, presetPath]() {
            SDK::APlayerController* c;
            SDK::UWorld* w;
            if (!ComponentValidator::Validate(c) || !ComponentValidator::Validate(w)) return;

            auto* gi = static_cast<SDK::UGI_Settings_C*>(
                SDK::UGameplayStatics::GetGameInstance(w));

            auto* willieClass = Spawner::LoadClass("/Game/Character/Blueprints/Willie_BP.Willie_BP_C");
            if (!willieClass) return;

            auto transform = c->GetTransform();

            auto* newActor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
                w, willieClass, transform,
                SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
                nullptr, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);
            if (!newActor) return;

            auto* willie = static_cast<SDK::AWillie_BP_C*>(newActor);
            willie->Player = true;
            willie->Team_Int = 0;
            if (gi)
                willie->Character_Passport = gi->Player_Character;

            if (!presetPath.empty()) {
                auto preset = PlayerPresetSerializer::LoadFromFile(presetPath);
                if (preset.success) {
                    auto& o = preset.overrides;
                    if (o.heightRate.enabled) {
                        willie->Height_Rate = o.heightRate.value;
                        willie->Character_Passport.Height_21_0EB204DF4978B92AD0ED188FD32EEC7B = o.heightRate.value;
                    }
                    if (o.muscleRate.enabled) {
                        willie->Muscle_Rate = o.muscleRate.value;
                        willie->Character_Passport.Weight_23_65E4C6534D14653F96EB739F159E58CD = o.muscleRate.value;
                    }
                }
            }

            SDK::UGameplayStatics::FinishSpawningActor(newActor, transform,
                SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);

            c->Possess(willie);
            willie->Set_Up_Armor(true, false);
        });
    }

public:
    MapLoaderSection() : CollapsibleSection("Map Loader") {}

    void RenderContent() override {
        const SectionStyle::StyleRAII style;

        auto& reg = MapRegistry::Get();
        auto scanState = reg.GetState();

        if (scanState == ScanState::NotStarted || scanState == ScanState::Scanning) {
            if (scanState == ScanState::NotStarted) reg.RequestScan();
            ImGui::TextColored(kYellowText, "Scanning maps...");
            return;
        }

        if (scanState == ScanState::Failed) {
            ImGui::TextColored(kOrangeText, "No maps found");
            if (ImGui::Button("Retry")) reg.RequestRescan();
            return;
        }

        const auto& maps = reg.GetMaps();
        const auto& cats = reg.GetCategories();

        RefreshLevelName();
        if (!cachedLevelName.empty()) {
            ImGui::Text("Current: %s", cachedLevelName.c_str());
            ImGui::Spacing();
        }

        SDK::AWillie_BP_C* playerCheck = nullptr;
        bool hasPlayer = ComponentValidator::Validate(playerCheck);

        if (pendingAutoSpawn && !hasPlayer && ComponentValidator::Validate(world)) {
            SpawnPlayer();
            pendingAutoSpawn = false;
        }

        if (!hasPlayer) {
            ImGui::TextColored(kOrangeText, "No player detected");
            ImGui::SameLine();
            if (ImGui::Button("Spawn Player"))
                SpawnPlayer();
            ImGui::Spacing();
        }

        if (ImGui::InputTextWithHint("##MapSearch", "Search...", searchBuffer, sizeof(searchBuffer)))
            filterDirty = true;

        if (cats.size() > 1) {
            if (cachedCatComboW == 0.0f) {
                float maxW = ImGui::CalcTextSize("All").x;
                for (const auto& c : cats) {
                    float w = ImGui::CalcTextSize(c.c_str()).x;
                    if (w > maxW) maxW = w;
                }
                cachedCatComboW = GuiUtils::ComboWidthFromText(maxW);
            }

            const char* catPreview = selectedCategoryIndex == 0 ? "All"
                : cats[selectedCategoryIndex - 1].c_str();
            ImGui::SetNextItemWidth(cachedCatComboW);
            if (ImGui::BeginCombo("##MapCategory", catPreview)) {
                if (ImGui::Selectable("All", selectedCategoryIndex == 0)) {
                    selectedCategoryIndex = 0;
                    filterDirty = true;
                }
                for (int i = 0; i < static_cast<int>(cats.size()); ++i) {
                    bool selected = (selectedCategoryIndex == i + 1);
                    if (ImGui::Selectable(cats[i].c_str(), selected)) {
                        selectedCategoryIndex = i + 1;
                        filterDirty = true;
                    }
                }
                ImGui::EndCombo();
            }
        }

        RebuildFilter(reg);

        if (filteredIndices.empty()) {
            ImGui::TextColored(kGrayText, "No matches");
        } else {
            if (selectedFilteredIndex >= static_cast<int>(filteredIndices.size()))
                selectedFilteredIndex = 0;

            const char* preview = maps[filteredIndices[selectedFilteredIndex]].displayName.c_str();
            ImGui::SetNextItemWidth(cachedComboW);
            if (ImGui::BeginCombo("##MapSelector", preview)) {
                for (int i = 0; i < static_cast<int>(filteredIndices.size()); ++i) {
                    const auto& entry = maps[filteredIndices[i]];
                    bool selected = (i == selectedFilteredIndex);
                    if (ImGui::Selectable(entry.displayName.c_str(), selected))
                        selectedFilteredIndex = i;
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                        ImGui::SetItemTooltip("%s", entry.packageName.c_str());
                }
                ImGui::EndCombo();
            }

            ImGui::Spacing();
            if (ImGui::Button("Load Map"))
                LoadMap(maps[filteredIndices[selectedFilteredIndex]].packageName);
            ImGui::SameLine();
            if (ImGui::Button("Restart Current") && !cachedLevelName.empty())
                LoadMap(cachedLevelName);
        }

        ImGui::Spacing();

        if (ImGui::TreeNode("Pre-Load Options")) {
            ImGui::Checkbox("Fresh Start", &optFreshStart);
            ImGui::SameLine();
            ImGui::Checkbox("Tutorial", &optTutorial);

            ImGui::SeparatorText("Free Mode");
            ImGui::Checkbox("Free Mode", &optFreeMode);
            ImGui::SameLine();
            ImGui::Checkbox("Carnage", &optCarnage);
            ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
            ImGui::DragInt("##Foes", &optFoesAmount, 0.2f, 0, 7, "Foes: %d");
            ImGui::SameLine();
            GuiUtils::RenderFreeTierCombo("##FoeTier", optFoeTier);

            ImGui::SeparatorText("Combat");
            ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
            ImGui::DragInt("##Enemies", &optCombatantsAmount, 0.2f, 0, 7, "Enemies: %d");
            ImGui::SameLine();
            GuiUtils::RenderFreeTierCombo("##OpponentTier", optOpponentTier);

            ImGui::SeparatorText("Player");
            ImGui::Checkbox("Auto-Spawn Player", &optAutoSpawn);

            if (optAutoSpawn) {
                RefreshPresetList();
                const char* presetPreview = selectedPresetIndex < 0 ? "None (use save)"
                    : playerPresets[selectedPresetIndex].name.c_str();
                if (ImGui::BeginCombo("Preset", presetPreview)) {
                    if (ImGui::Selectable("None (use save)", selectedPresetIndex < 0))
                        selectedPresetIndex = -1;
                    for (int i = 0; i < static_cast<int>(playerPresets.size()); ++i) {
                        if (ImGui::Selectable(playerPresets[i].name.c_str(), i == selectedPresetIndex))
                            selectedPresetIndex = i;
                    }
                    ImGui::EndCombo();
                }
            }

            ImGui::TreePop();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Custom path");
        ImGui::InputTextWithHint("##CustomMapPath", "/Game/Maps/...", customPathBuffer, sizeof(customPathBuffer));
        if (ImGui::Button("Load Custom") && customPathBuffer[0] != '\0')
            LoadMap(std::string(customPathBuffer));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Rescan Maps")) {
            reg.RequestRescan();
            selectedFilteredIndex = 0;
            selectedCategoryIndex = 0;
            searchBuffer[0] = '\0';
            filterDirty = true;
            cachedCatComboW = 0.0f;
            presetListDirty = true;
        }
        ImGui::SameLine();
        ImGui::TextColored(kGrayText, "(%zu maps)", maps.size());
    }
};
