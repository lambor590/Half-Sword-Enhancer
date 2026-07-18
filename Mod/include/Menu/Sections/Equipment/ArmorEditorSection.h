#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "Menu/Override.h"
#include "Menu/SectionConfig.h"
#include "Utils/ArmorGenerationOptions.h"
#include "Utils/ArmorPresetSerializer.h"
#include "Utils/GameConstants.h"
#include "Utils/LivePreviewManager.h"
#include "Utils/PresetSectionState.h"

class ArmorEditorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::Equipment, "Armor Editor", "Create armor with the parts, colors, weight, and protection you want."
    };

    struct Config {
        int armorSlotIndex = 0;
        int armorTier = 4;
        EquipmentGenerator::ArmorGenerationOptions armorOptions;
        SpawnConfig spawn{.distanceForward = 150.0f, .distanceUp = 50.0f};
        int spawnKey = -1;
        PreviewConfig preview;
    };

private:
    Config cfg;

    static constexpr auto& ARMOR_SLOTS = GameConstants::ARMOR_SLOTS;
    static constexpr int ARMOR_SLOT_COUNT = GameConstants::ARMOR_SLOT_COUNT;

    KeybindList keybinds;
    SDK::FStr_Passport_Armor1 armorPassport{};
    std::string armorCorePath;
    std::atomic<bool> armorGenerationPending{false};
    std::atomic<std::uint64_t> draftRevision{0};
    std::uint64_t renderDraftRevision = 0;
    std::uint64_t pendingPresetApplyRevision = 0;

    ArmorRuntimeProps runtimeProps{};

    struct SpawnDraftSnapshot {
        SpawnConfig spawn;
        ArmorPresetData preset;
    };

    std::mutex spawnDraftMutex;
    SpawnDraftSnapshot publishedSpawnDraft{};
    std::uint64_t publishedSpawnDraftRevision = 0;

    LivePreviewManager preview{cfg.preview};
    SDK::FStr_Passport_Armor1 lastPreviewedPassport{};
    ArmorRuntimeProps lastPreviewedProps{};

    struct ModuleEntry {
        SDK::UClass* cls;
        std::string name;
    };

    struct ArmorModulePool {
        std::vector<ModuleEntry> modules1, modules2, modules3;
        float cachedWidths[3] = {};
        bool populated = false;
        SDK::UClass* populatedForCore = nullptr;
    } armorModules;

    char moduleFilters[3][64] = {};

    PresetSectionState<ArmorPresetSerializer> presets;
    int activeTab = 0;

    struct PendingDraftUpdate {
        std::uint64_t revision = 0;
        ArmorPresetData data;
        bool replaceAll = false;
        bool presetApply = false;
    };

    struct PendingError {
        std::string message;
        std::uint64_t revision = 0;
    };

    enum class StatusOrigin : std::uint8_t { Generation, Spawn, Count };

    struct PendingStatus {
        std::uint64_t sequence = 0;
        std::uint64_t request = 0;
        StatusOrigin origin = StatusOrigin::Generation;
        std::string error;
    };

    struct PendingRenderUpdates {
        std::optional<PendingDraftUpdate> draft;
        std::optional<PendingError> presetError;
        std::array<std::optional<PendingStatus>, static_cast<std::size_t>(StatusOrigin::Count)> statuses;
    };

    std::mutex pendingRenderMutex;
    PendingRenderUpdates pendingRenderUpdates;
    std::atomic<bool> pendingRenderReady{false};
    std::atomic<std::uint64_t> statusSequence{0};
    std::atomic<std::uint64_t> spawnRequest{0};
    GuiUtils::StatusMessage::Token generationStatusToken = 0;
    GuiUtils::StatusMessage::Token spawnStatusToken = 0;

    std::array<OverrideDescriptor, 3> protectionFields;
    std::array<OverrideDescriptor, 2> physicsFields;
    std::array<OverrideDescriptor, 5> behaviorFields;

    void BuildDescriptors();
    int CountAllActive() const;

    bool IsModularCore() const;
    void PopulateModulePoolForCurrentCore();
    void ResetArmorPassport();
    void QueueGeneration(
        SDK::EArmorSlots_Enum slot, SDK::Enum_Ranks tier, EquipmentGenerator::ArmorGenerationOptions options
    );
    void GenerateArmorPassport();
    void RandomizeArmorPassport();
    void SpawnPreview();
    void RenderArmorTierCombo();
    void SpawnArmor();
    void PublishSpawnDraftSnapshot();
    bool PublishAppliedPresetSpawnSnapshot(const PendingDraftUpdate& update);
    void SpawnArmor(const RuntimeContextSnapshot& runtime, SpawnDraftSnapshot draft);
    void RenderGenerationControls();
    void RenderModulesTab();
    void RenderColorsTab();
    void RenderStatsTab();
    ArmorPresetData BuildPresetData() const;
    PresetApplyDisposition ApplyPresetData(const ArmorPresetData& data);
    void PublishDraftUpdate(PendingDraftUpdate update);
    void PublishError(std::string message, std::uint64_t revision = 0, bool presetApply = false);
    void PublishStatus(StatusOrigin origin, std::uint64_t request, std::string error = {});
    void DrainPendingRenderUpdates();
    void ApplyDraftUpdate(PendingDraftUpdate update);
    void InitKeybinds();

public:
    explicit ArmorEditorSection(ModContext& ctx);
    void Render() override;
    KeybindList* GetSearchKeybinds() noexcept override { return &keybinds; }
};
