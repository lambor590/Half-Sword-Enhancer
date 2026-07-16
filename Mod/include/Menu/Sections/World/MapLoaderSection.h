#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Menu/EventBus.h"
#include "Menu/Section.h"
#include "Utils/GuiUtils.h"
#include "Utils/LoadoutPresetResolver.h"
#include "Utils/MapScenarioPresetSerializer.h"
#include "Utils/PresetLinkPickerState.h"
#include "Utils/PresetSectionState.h"

class MapRegistry;

class MapLoaderSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::World, "Map Setup", "Choose where to play and how the next session should begin."
    };
    static void OnRuntimeStart();
    static void OnRuntimeShutdown() noexcept;
    [[nodiscard]] static bool RunScenario(
        const RuntimeContextSnapshot& runtime, const MapScenarioPresetData& scenario, std::string* error = nullptr
    );

private:
    static std::atomic<MapLoaderSection*> runtimeInstance;
    int selectedFilteredIndex = 0;
    int selectedCategoryIndex = 0;
    char searchBuffer[128] = "";
    std::vector<int> filteredIndices;
    float cachedComboW = 0.0f;
    float cachedCatComboW = 0.0f;
    std::string cachedLevelName;
    struct LevelNameResult {
        SDK::UWorld* world = nullptr;
        std::string name;
    };
    SDK::UWorld* observedLevelWorld = nullptr;
    std::atomic<bool> levelNameRequestPending{false};
    std::mutex levelNameMutex;
    std::optional<LevelNameResult> pendingLevelName;
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
    struct PreparedAutoSpawn {
        std::optional<PlayerPresetData> player;
        std::optional<ResolvedLoadoutPresetData> loadout;
        std::optional<NPCPresetData> npc;
        std::optional<ResolvedLoadoutPresetData> npcLoadout;
        int npcCount = 0;
    };
    struct PendingAutoSpawn {
        std::shared_ptr<const PreparedAutoSpawn> prepared;
        SDK::UWorld* sourceWorld = nullptr;
        SDK::UClass* playerClass = nullptr;
        std::chrono::steady_clock::time_point deadline{};
        std::uint64_t generation = 0;
    };
    std::optional<PendingAutoSpawn> pendingAutoSpawn;
    std::uint64_t autoSpawnGeneration = 0;
    std::mutex autoSpawnMutex;
    struct AutoSpawnFeedback {
        std::string message;
        bool error = false;
    };
    std::optional<AutoSpawnFeedback> autoSpawnFeedback;
    EventBus::SubscriptionGroup autoSpawnSubscriptions;
    PresetLinkPickerState<PlayerPresetSerializer> playerPresetLink;
    PresetLinkPickerState<LoadoutPresetSerializer> loadoutPresetLink;
    PresetLinkPickerState<NPCPresetSerializer> npcPresetLink;
    PresetSectionState<MapScenarioPresetSerializer> scenarioPresets;
    GuiUtils::StatusMessage presetStatus;
    int optAutoNPCCount = 0;
    std::string packageOverride;

    static constexpr ImVec4 K_GRAY_TEXT{0.5f, 0.5f, 0.5f, 1.0f};

    void RefreshLevelName();
    void StartAutoSpawnSubscription();
    void RebuildFilter(MapRegistry& reg);
    void LoadMap(const std::string& packageName);
    [[nodiscard]] bool RunScenarioImpl(
        const RuntimeContextSnapshot& runtime, const MapScenarioPresetData& scenario, std::string* error
    );
    [[nodiscard]] std::shared_ptr<const PreparedAutoSpawn> PrepareAutoSpawn(
        const MapScenarioPresetData::AutoSpawnOptions& options, std::string& error
    );
    [[nodiscard]] bool IsAutoSpawnAttemptCurrent(std::uint64_t generation);
    void FinishAutoSpawnAttempt(std::uint64_t generation, std::string message, bool error);
    void FlushAutoSpawnFeedback();
    void AdvancePendingAutoSpawn(const RuntimeContextSnapshot& runtime);
    void ApplyPreparedAutoSpawn(
        SDK::UWorld* world, SDK::AWillie_BP_C* willie, const std::shared_ptr<const PreparedAutoSpawn>& prepared,
        std::uint64_t generation
    );
    void SpawnAutoNPCs(
        SDK::UWorld* w, SDK::AWillie_BP_C* willie, const NPCPresetData& npcPreset,
        const std::optional<ResolvedLoadoutPresetData>& npcLoadout, int npcCount,
        std::function<void(int, int)> onComplete
    );
    void SpawnPlayer();
    void RenderPreLoadOptions();
    void RenderMapSelector(MapRegistry& reg);
    [[nodiscard]] std::string CurrentPackageName(const MapRegistry& reg) const;
    [[nodiscard]] MapScenarioPresetData BuildScenarioPreset(std::string packageName) const;
    [[nodiscard]] MapScenarioPresetData BuildScenarioPreset(const MapRegistry& reg) const;
    void ApplyScenarioPreset(MapScenarioPresetData data);
    void RenderScenarioPresets(MapRegistry& reg);

public:
    explicit MapLoaderSection(ModContext& ctx);
    void Render() override;
};
