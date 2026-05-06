#include "Menu/Sections/World/MapLoaderSection.h"
#include "Menu/SectionRegistry.h"
#include "Menu/SectionStyle.h"
#include "Hooks/GameHook.h"

REGISTER_SECTION(MapLoaderSection, MenuTab::World);
#include "Utils/MapRegistry.h"
#include "Utils/GuiUtils.h"
#include "Utils/Spawner.h"
#include "Utils/NPCSpawnHelpers.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/GameConstants.h"
#include "SDK/GI_Settings_classes.hpp"

void MapLoaderSection::RefreshLevelName() {
    if (!levelNameDirty) return;
    auto* world = RenderWorld();
    if (world) {
        SDK::FString currentLevel = SDK::UGameplayStatics::GetCurrentLevelName(world, true);
        cachedLevelName = currentLevel.ToString();
    } else {
        cachedLevelName.clear();
    }
    levelNameDirty = false;
}

void MapLoaderSection::RebuildFilter(MapRegistry& reg) {
    if (!filterDirty) return;
    filterDirty = false;

    const auto& maps = reg.GetMaps();
    const auto& cats = reg.GetCategories();
    const size_t prevCapacity = filteredIndices.capacity();
    filteredIndices.clear();
    if (prevCapacity > 0) filteredIndices.reserve(prevCapacity);

    const size_t filterLen = std::strlen(searchBuffer);
    const bool hasCategory = selectedCategoryIndex > 0 && selectedCategoryIndex <= static_cast<int>(cats.size());
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
            if (hasCategory && map.category != selectedCat) continue;
            if (hasFilter &&
                !GuiUtils::MatchesFilter(map.displayName.c_str(), map.displayName.size(), searchBuffer, filterLen))
                continue;
            filteredIndices.push_back(i);
            float w = ImGui::CalcTextSize(map.displayName.c_str()).x;
            if (w > maxW) maxW = w;
        }
        cachedComboW = GuiUtils::ComboWidthFromText(maxW);
    }

    selectedFilteredIndex = 0;
}

void MapLoaderSection::LoadMap(const std::string& packageName) {
    if (!RenderWorld()) return;
    if (optAutoSpawn) pendingAutoSpawn = true;

    bool freshStart = optFreshStart, tutorial = optTutorial;
    bool freeMode = optFreeMode, carnage = optCarnage;
    int foes = optFoesAmount, foeTier = optFoeTier;
    int combatants = optCombatantsAmount, opponentTier = optOpponentTier;

    GameHook::QueueAction([this, pn = packageName, freshStart, tutorial, freeMode, carnage, foes, foeTier, combatants,
                           opponentTier](const RuntimeContextSnapshot& runtime) {
        auto* world = runtime.world;
        if (!world) return;
        auto* gi = static_cast<SDK::UGI_Settings_C*>(SDK::UGameplayStatics::GetGameInstance(world));
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

void MapLoaderSection::SpawnAutoNPCs(
    SDK::UWorld* w, SDK::AWillie_BP_C* willie, const NPCPresetData& npcPreset, int npcCount
) {
    for (int n = 0; n < npcCount; ++n) {
        float angle = (6.2832f / npcCount) * n;
        float dist = 300.0f;
        SDK::FTransform npcTransform = willie->GetTransform();
        auto fwd = willie->GetActorForwardVector();
        npcTransform.Translation.X += fwd.X * dist + std::cos(angle) * 100.0f * n;
        npcTransform.Translation.Y += fwd.Y * dist + std::sin(angle) * 100.0f * n;
        npcTransform.Scale3D = {1.0, 1.0, 1.0};

        auto nationality = static_cast<SDK::Enum_Nationalities>(std::clamp(npcPreset.nationality, 0, 6));
        auto tier = static_cast<SDK::Enum_Ranks>(std::clamp(npcPreset.tier, 0, 8));

        Spawner::SpawnActor(
            w, std::string(GameConstants::WILLIE_BP_PATH), npcTransform,
            [w, nationality, tier, mercenary = npcPreset.mercenary, ovr = npcPreset.overrides](SDK::AActor* actor) {
                auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);
                if (!npc) return;
                auto passport = EquipmentGenerator::GenerateCharacter(w, npc->Class, nationality, tier, mercenary);
                NPCSpawnHelpers::ApplyPassportOverrides(passport, ovr);
                npc->Character_Passport = passport;
                NPCSpawnHelpers::ApplyPropertyOverrides(npc, ovr);
            },
            true, Spawner::DEFAULT_SPAWN_TIER,
            [ovr = npcPreset.overrides](SDK::AActor* actor) {
                auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);
                if (!npc) return;

                NPCSpawnHelpers::ApplyAIFearlessOverride(npc, ovr);
                NPCSpawnHelpers::ApplyHairColor(npc, ovr);
            }
        );
    }
}

void MapLoaderSection::SpawnPlayer() {
    std::filesystem::path presetPath;
    if (playerPicker.HasSelection()) presetPath = playerPicker.SelectedPath();

    bool hasLoadout = loadoutPicker.HasSelection();
    LoadoutPresetData loadoutData;
    if (hasLoadout) {
        loadoutData = LoadoutPresetSerializer::LoadFromFile(loadoutPicker.SelectedPath());
        if (!loadoutData.success) hasLoadout = false;
    }

    bool hasNPCPreset = npcPicker.HasSelection() && optAutoNPCCount > 0;
    NPCPresetData npcData;
    if (hasNPCPreset) {
        npcData = NPCPresetSerializer::LoadFromFile(npcPicker.SelectedPath());
        if (!npcData.success) hasNPCPreset = false;
    }
    int npcCount = hasNPCPreset ? optAutoNPCCount : 0;

    GameHook::QueueAction([this, presetPath, hasLoadout, loadout = std::move(loadoutData), hasNPCPreset,
                           npcPreset = std::move(npcData), npcCount](const RuntimeContextSnapshot& runtime) {
        auto* controller = runtime.controller;
        auto* world = runtime.world;
        if (!controller || !world) return;

        auto* gi = static_cast<SDK::UGI_Settings_C*>(SDK::UGameplayStatics::GetGameInstance(world));

        auto* willieClass = Spawner::LoadClass(GameConstants::WILLIE_BP_PATH);
        if (!willieClass) return;

        auto transform = controller->GetTransform();

        auto* newActor = Spawner::DeferredSpawn(world, willieClass, transform, [&](SDK::AActor* actor) {
            auto* willie = static_cast<SDK::AWillie_BP_C*>(actor);
            willie->Player = true;
            willie->Team_Int = 0;
            if (gi) willie->Character_Passport = gi->Player_Character;

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
        });
        if (!newActor) return;

        auto* willie = static_cast<SDK::AWillie_BP_C*>(newActor);

        controller->Possess(willie);
        willie->Set_Up_Armor(true, false);

        if (hasLoadout) NPCSpawnHelpers::ApplyNPCLoadout(world, willie, loadout);
        if (hasNPCPreset) SpawnAutoNPCs(world, willie, npcPreset, npcCount);
    });
}

void MapLoaderSection::RenderPreLoadOptions() {
    if (!ImGui::TreeNode("Pre-Load Options")) return;

    ImGui::Checkbox("Fresh Start", &optFreshStart);
    ImGui::SameLine();
    ImGui::Checkbox("Tutorial", &optTutorial);

    ImGui::SeparatorText("Free Mode");
    ImGui::Checkbox("Free Mode", &optFreeMode);
    ImGui::SameLine();
    ImGui::Checkbox("Carnage", &optCarnage);
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    GuiUtils::DebouncedDragInt("##Foes", &optFoesAmount, 0.2f, 0, 7, "Foes: %d");
    ImGui::SameLine();
    GuiUtils::RenderFreeTierCombo("##FoeTier", optFoeTier);

    ImGui::SeparatorText("Combat");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    GuiUtils::DebouncedDragInt("##Enemies", &optCombatantsAmount, 0.2f, 0, 7, "Enemies: %d");
    ImGui::SameLine();
    GuiUtils::RenderFreeTierCombo("##OpponentTier", optOpponentTier);

    ImGui::SeparatorText("Player");
    ImGui::Checkbox("Auto-Spawn Player", &optAutoSpawn);

    if (optAutoSpawn) {
        playerPicker.Render("Player Preset", "None (use save)");
        loadoutPicker.Render("Loadout Preset");

        ImGui::SeparatorText("Auto-Spawn NPCs");
        npcPicker.Render("NPC Preset");
        if (npcPicker.HasSelection()) {
            ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
            GuiUtils::DebouncedDragInt("##NPCCount", &optAutoNPCCount, 0.2f, 0, 10, "Count: %d");
        }
    }

    ImGui::TreePop();
}

void MapLoaderSection::RenderMapSelector(MapRegistry& reg) {
    const auto& maps = reg.GetMaps();
    const auto& cats = reg.GetCategories();

    if (ImGui::InputTextWithHint("##MapSearch", "Search...", searchBuffer, sizeof(searchBuffer))) filterDirty = true;

    if (cats.size() > 1) {
        if (cachedCatComboW == 0.0f) {
            float maxW = ImGui::CalcTextSize("All").x;
            for (const auto& c : cats) {
                float w = ImGui::CalcTextSize(c.c_str()).x;
                if (w > maxW) maxW = w;
            }
            cachedCatComboW = GuiUtils::ComboWidthFromText(maxW);
        }

        const char* catPreview = selectedCategoryIndex == 0 ? "All" : cats[selectedCategoryIndex - 1].c_str();
        if (GuiUtils::BeginSizedCombo("##MapCategory", catPreview, cachedCatComboW)) {
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
        ImGui::TextColored(K_GRAY_TEXT, "No matches");
    } else {
        if (selectedFilteredIndex >= static_cast<int>(filteredIndices.size())) selectedFilteredIndex = 0;

        const char* preview = maps[filteredIndices[selectedFilteredIndex]].displayName.c_str();
        if (GuiUtils::BeginSizedCombo("##MapSelector", preview, cachedComboW)) {
            for (int i = 0; i < static_cast<int>(filteredIndices.size()); ++i) {
                const auto& entry = maps[filteredIndices[i]];
                bool selected = (i == selectedFilteredIndex);
                if (ImGui::Selectable(entry.displayName.c_str(), selected)) selectedFilteredIndex = i;
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                    ImGui::SetItemTooltip("%s", entry.packageName.c_str());
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        if (ImGui::Button("Load Map")) LoadMap(maps[filteredIndices[selectedFilteredIndex]].packageName);
        ImGui::SameLine();
        if (ImGui::Button("Restart Current") && !cachedLevelName.empty()) LoadMap(cachedLevelName);
    }
}

MapLoaderSection::MapLoaderSection(ModContext& ctx) : Section(ctx, "Map Loader") {}

void MapLoaderSection::Render() {
    const SectionStyle::StyleRAII style;
    auto [world, player] = RenderPlayerWorld();

    auto& reg = MapRegistry::Get();
    auto scanState = reg.GetState();

    if (scanState == ScanState::NotStarted || scanState == ScanState::Scanning) {
        if (scanState == ScanState::NotStarted) reg.RequestScan();
        ImGui::TextColored(K_YELLOW_TEXT, "Scanning maps...");
        return;
    }

    if (scanState == ScanState::Failed) {
        ImGui::TextColored(K_ORANGE_TEXT, "No maps found");
        if (ImGui::Button("Retry")) reg.RequestRescan();
        return;
    }

    const auto& maps = reg.GetMaps();

    RefreshLevelName();
    if (!cachedLevelName.empty()) {
        ImGui::Text("Current: %s", cachedLevelName.c_str());
        ImGui::Spacing();
    }

    bool hasPlayer = player != nullptr;

    if (pendingAutoSpawn && !hasPlayer && world) {
        SpawnPlayer();
        pendingAutoSpawn = false;
    }

    if (!hasPlayer) {
        ImGui::TextColored(K_ORANGE_TEXT, "No player detected");
        ImGui::SameLine();
        if (ImGui::Button("Spawn Player")) SpawnPlayer();
        ImGui::Spacing();
    }

    RenderMapSelector(reg);

    ImGui::Spacing();

    RenderPreLoadOptions();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Custom path");
    ImGui::InputTextWithHint("##CustomMapPath", "/Game/Maps/...", customPathBuffer, sizeof(customPathBuffer));
    if (ImGui::Button("Load Custom") && customPathBuffer[0] != '\0') LoadMap(std::string(customPathBuffer));

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
        playerPicker.Invalidate();
        loadoutPicker.Invalidate();
        npcPicker.Invalidate();
    }
    ImGui::SameLine();
    ImGui::TextColored(K_GRAY_TEXT, "(%zu maps)", maps.size());
}
