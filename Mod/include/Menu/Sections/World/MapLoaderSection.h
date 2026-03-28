#pragma once

#include "Menu/ICollapsibleSection.h"
#include "Hooks/GameHook.h"
#include "Utils/MapRegistry.h"
#include "Utils/GuiUtils.h"
#include "Utils/Spawner.h"
#include "Utils/PlayerPresetSerializer.h"
#include "Utils/LoadoutPresetSerializer.h"
#include "Utils/NPCPresetSerializer.h"
#include "Utils/NPCSpawnHelpers.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/PresetPickerState.h"
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
    int optFoesAmount = 3;
    int optFoeTier = 0;
    int optCombatantsAmount = 3;
    int optOpponentTier = 0;

    bool optAutoSpawn = false;
    bool pendingAutoSpawn = false;
    PresetPickerState<PlayerPresetSerializer> playerPicker;
    PresetPickerState<LoadoutPresetSerializer> loadoutPicker;
    PresetPickerState<NPCPresetSerializer> npcPicker;
    int optAutoNPCCount = 0;

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

    void RebuildFilter(MapRegistry& reg) {
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

    void LoadMap(const std::string& packageName) {
        if (!ComponentValidator::Validate(world)) return;
        if (optAutoSpawn) pendingAutoSpawn = true;

        bool freshStart = optFreshStart, tutorial = optTutorial;
        bool freeMode = optFreeMode, carnage = optCarnage;
        int foes = optFoesAmount, foeTier = optFoeTier;
        int combatants = optCombatantsAmount, opponentTier = optOpponentTier;

        GameHook::QueueAction([this, pn = packageName, freshStart, tutorial, freeMode, carnage, foes, foeTier,
                               combatants, opponentTier]() {
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

    void SpawnPlayer() {
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
                               npcPreset = std::move(npcData), npcCount]() {
            SDK::APlayerController* c;
            SDK::UWorld* w;
            if (!ComponentValidator::Validate(c) || !ComponentValidator::Validate(w)) return;

            auto* gi = static_cast<SDK::UGI_Settings_C*>(SDK::UGameplayStatics::GetGameInstance(w));

            auto* willieClass = Spawner::LoadClass(GameConstants::WILLIE_BP_PATH);
            if (!willieClass) return;

            auto transform = c->GetTransform();

            auto* newActor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
                w, willieClass, transform, SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
                nullptr, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime
            );
            if (!newActor) return;

            auto* willie = static_cast<SDK::AWillie_BP_C*>(newActor);
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

            SDK::UGameplayStatics::FinishSpawningActor(
                newActor, transform, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime
            );

            c->Possess(willie);
            willie->Set_Up_Armor(true, false);

            if (hasLoadout) {
                for (const auto& sd : loadout.armorSlots) {
                    SDK::UClass* cls = sd.armorClass.empty() ? nullptr : Spawner::LoadClass(sd.armorClass);
                    if (!cls) continue;

                    SDK::FStr_Passport_Armor1 armorPassport{};
                    armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = cls;
                    armorPassport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393 = sd.color1;
                    armorPassport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C = sd.color2;
                    armorPassport.Slot_30_7561CB484566A4512003EA96ED44F88D = sd.slot;
                    Spawner::SpawnAndEquipArmor(w, willie, armorPassport);
                }

                auto& weapons = willie->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
                for (int i = 0; i < 7; ++i) {
                    const auto& wd = loadout.weaponSlots[i];
                    if (wd.weaponClass.empty()) continue;
                    auto& slot = LoadoutPresetSerializer::GetWeaponSlot(weapons, i);
                    auto load = [](const std::string& path) -> SDK::UClass* {
                        return path.empty() ? nullptr : Spawner::LoadClass(path);
                    };
                    slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 = load(wd.weaponClass);
                    slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 = load(wd.gripModule);
                    slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F = load(wd.headModule);
                    slot.GuardModule_21_774015784EB0300D2671C894D57ED144 = load(wd.guardModule);
                    slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 = load(wd.pommelModule);
                    slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 = load(wd.subModule1);
                    slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 = load(wd.subModule2);
                    slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA = wd.headSize;
                    slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D = wd.guardSize;
                    slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524 = wd.pommelSize;
                    slot.COAInt_63_593665BE4EF020F95F7D1A92564C1239 = wd.coaInt;
                }
                willie->Set_Up_Armor(true, false);
            }

            if (hasNPCPreset) {
                EquipmentGenerator::Init(w);
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

                    std::string npcClassName = GameConstants::WILLIE_BP_PATH;

                    Spawner::SpawnActor(
                        w, npcClassName, npcTransform,
                        [w, nationality, tier, mercenary = npcPreset.mercenary,
                         ovr = npcPreset.overrides](SDK::AActor* actor) {
                            auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);
                            if (!npc) return;
                            auto passport =
                                EquipmentGenerator::GenerateCharacter(npc->Class, nationality, tier, mercenary);
                            NPCSpawnHelpers::ApplyPassportOverrides(passport, ovr);
                            npc->Character_Passport = passport;
                            NPCSpawnHelpers::ApplyPropertyOverrides(npc, ovr);
                        },
                        true, 4,
                        [ovr = npcPreset.overrides](SDK::AActor* actor) {
                            auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);
                            if (npc) NPCSpawnHelpers::ApplyHairColor(npc, ovr);
                        }
                    );
                }
            }
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
            if (ImGui::Button("Spawn Player")) SpawnPlayer();
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

            const char* catPreview = selectedCategoryIndex == 0 ? "All" : cats[selectedCategoryIndex - 1].c_str();
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
            if (selectedFilteredIndex >= static_cast<int>(filteredIndices.size())) selectedFilteredIndex = 0;

            const char* preview = maps[filteredIndices[selectedFilteredIndex]].displayName.c_str();
            ImGui::SetNextItemWidth(cachedComboW);
            if (ImGui::BeginCombo("##MapSelector", preview)) {
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
                playerPicker.Render("Player Preset", "None (use save)");
                loadoutPicker.Render("Loadout Preset");

                ImGui::SeparatorText("Auto-Spawn NPCs");
                npcPicker.Render("NPC Preset");
                if (npcPicker.HasSelection()) {
                    ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
                    ImGui::DragInt("##NPCCount", &optAutoNPCCount, 0.2f, 0, 10, "Count: %d");
                }
            }

            ImGui::TreePop();
        }

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
        ImGui::TextColored(kGrayText, "(%zu maps)", maps.size());
    }
};
