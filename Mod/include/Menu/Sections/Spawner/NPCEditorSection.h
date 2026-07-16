#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "Menu/Override.h"
#include "Menu/SectionConfig.h"
#include "Utils/NPCPresetSerializer.h"
#include "Utils/LoadoutPresetResolver.h"
#include "Utils/PresetLinkPickerState.h"
#include "Utils/PresetSectionState.h"
#include "Utils/SpawnWorkflow.h"

class NPCEditorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::Spawner, "NPC Editor", "Create reusable NPCs with their appearance, behavior, and equipment."
    };

    struct Config {
        SpawnConfig spawn{.distanceForward = 200.0f, .distanceUp = 0.0f};
        bool bodyguard = false;
        int npcTeam = 0;
        int npcTypeIndex = 0;
        int npcNationality = 0;
        int npcTier = 4;
        bool npcMercenary = false;
    };

private:
    struct SpawnSnapshot {
        NPCPresetData npc;
    };

    struct SpawnBinding {
        int id = 0;
        int key = -1;
        char name[64] = "";
        NPCPresetData npc;
        std::string resolutionError;
        std::string summary;
        KeybindEntry keybind;
        std::atomic<std::shared_ptr<const SpawnSnapshot>> spawnSnapshot;
    };
    struct BindingOps;

    Config cfg;

    NPCOverrides overrides{};
    std::vector<std::shared_ptr<SpawnBinding>> spawnBindings;
    int nextBindingId = 1;
    int pendingDeleteBindingId = -1;
    struct SpawnFeedbackState {
        std::mutex feedbackMutex;
        std::optional<std::pair<std::string, bool>> feedback;
    };
    std::shared_ptr<SpawnFeedbackState> spawnFeedbackState = std::make_shared<SpawnFeedbackState>();
    std::array<std::uint64_t, 3> spawnBindingCatalogRevisions{};

    PresetSectionState<NPCPresetSerializer> presets;
    PresetLinkPickerState<LoadoutPresetSerializer> loadoutPresetLink;
    int activeTab = 0;

    std::vector<OverrideDescriptor> physicalFields;
    std::vector<OverrideDescriptor> combatFields;
    std::vector<OverrideDescriptor> behaviorFields;
    std::vector<OverrideDescriptor> bodyConditionFields;

    void BuildDescriptors();
    int CountAllActive() const;
    bool ResolveNPCLoadout(std::optional<ResolvedLoadoutPresetData>& resolved, std::string& error);
    void SpawnNPC();
    void SpawnBindingNPC(const SpawnSnapshot& binding, const RuntimeContextSnapshot& runtime) const;
    void ConsumeSpawnFeedback();
    [[nodiscard]] SpawnWorkflow::SpawnCompletion MakeSpawnCompletion(std::string action) const;
    static void StoreSpawnFeedback(const std::shared_ptr<SpawnFeedbackState>& state, std::string message, bool error);
    void PublishSpawnFeedback(std::string message, bool error) const;
    NPCPresetData BuildPresetData() const;
    void ApplyPresetData(const NPCPresetData& d);
    void RenderPhysicalTab();
    void RenderCombatTab();
    void RenderBehaviorTab();
    void RenderBodyConditionTab();
    void LoadSpawnBindings();
    void RenderSpawnBindings();

public:
    explicit NPCEditorSection(ModContext& ctx);
    void Render() override;
};
