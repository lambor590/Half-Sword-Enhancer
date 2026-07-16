#pragma once

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
    SpawnDraftSnapshot publishedSpawnDraft;
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
        bool completesPresetApply = false;
    };

    struct PendingStatus {
        std::string message;
        bool isError = false;
        std::uint64_t revision = 0;
        bool completesPresetApply = false;
    };

    struct PendingRenderUpdates {
        std::optional<PendingDraftUpdate> draft;
        std::vector<PendingStatus> statuses;
    };

    std::mutex pendingRenderMutex;
    PendingRenderUpdates pendingRenderUpdates;
    std::atomic<bool> pendingRenderReady{false};

    std::vector<OverrideDescriptor> protectionFields;
    std::vector<OverrideDescriptor> physicsFields;
    std::vector<OverrideDescriptor> behaviorFields;

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
    static bool PassportChanged(const SDK::FStr_Passport_Armor1& a, const SDK::FStr_Passport_Armor1& b);
    void RenderArmorTierCombo();
    void SpawnArmor();
    SpawnDraftSnapshot BuildSpawnDraftSnapshot() const;
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
    void PublishStatus(
        std::string message, bool isError = false, std::uint64_t revision = 0, bool completesPresetApply = false
    );
    void DrainPendingRenderUpdates();
    void ApplyDraftUpdate(PendingDraftUpdate update);
    void InitKeybinds();

public:
    explicit ArmorEditorSection(ModContext& ctx);
    void OnOpen() override;
    void Render() override;
    KeybindList* GetSearchKeybinds() noexcept override { return &keybinds; }
};
