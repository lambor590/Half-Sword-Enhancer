#pragma once

#include "Menu/Section.h"
#include "Utils/PresetPickerState.h"
#include "Utils/PlayerPresetSerializer.h"
#include "Utils/LoadoutPresetSerializer.h"
#include "Utils/NPCPresetSerializer.h"

class MapRegistry;

class MapLoaderSection : public Section {
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

    void RefreshLevelName();
    void RebuildFilter(MapRegistry& reg);
    void LoadMap(const std::string& packageName);
    static void SpawnAutoNPCs(SDK::UWorld* w, SDK::AWillie_BP_C* willie, const NPCPresetData& npcPreset, int npcCount);
    void SpawnPlayer();
    void RenderPreLoadOptions();
    void RenderMapSelector(MapRegistry& reg);

public:
    explicit MapLoaderSection(ModContext& ctx);
    void Render() override;
};
