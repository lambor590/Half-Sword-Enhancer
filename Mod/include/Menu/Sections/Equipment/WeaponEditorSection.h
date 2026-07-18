#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "Menu/Override.h"
#include "Menu/SectionConfig.h"
#include "Utils/GameConstants.h"
#include "Utils/GlobalModulePool.h"
#include "Utils/WeaponPresetSerializer.h"
#include "Utils/LivePreviewManager.h"
#include "Utils/PresetSectionState.h"
#include "SDK/Str_Passport_Weapon1_structs.hpp"

namespace SDK {
    class AModularWeaponBP_C;
}

class WeaponEditorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::Equipment, "Weapon Editor", "Create weapons with the shape, weight, appearance, and power you want."
    };

    struct Config {
        int weaponType = 1;
        int weaponTier = 4;
        SpawnConfig spawn{.distanceForward = 150.0f, .distanceUp = 50.0f};
        int spawnKey = -1;
        PreviewConfig preview;
    };

private:
    Config cfg;

    static constexpr auto& WEAPON_TYPE_NAMES = GameConstants::WEAPON_TYPE_NAMES;
    static constexpr int WEAPON_TYPE_COUNT = GameConstants::WEAPON_TYPE_COUNT;

    static constexpr auto& MATERIAL_LAYER_NAMES = GameConstants::MATERIAL_LAYER_NAMES;

    KeybindList keybinds;
    SDK::FStr_Passport_Weapon1 weaponPassport{};
    WeaponClassPaths weaponPaths{};
    std::string gripMeshPath;
    int coaInt = 0;
    std::string deferredWeaponName;
    std::atomic<bool> weaponGenerationPending{false};
    bool modulePoolQueued = false;
    std::atomic<std::uint64_t> draftRevision{0};
    std::uint64_t renderDraftRevision = 0;
    std::uint64_t pendingPresetApplyRevision = 0;

    using WeaponRuntimeProps = WeaponPresetData::WeaponRuntimeProps;

    WeaponRuntimeProps runtimeProps{};

    LivePreviewManager preview{cfg.preview};
    SDK::FStr_Passport_Weapon1 lastPreviewedPassport{};
    WeaponClassPaths lastPreviewedPaths{};
    WeaponRuntimeProps lastPreviewedProps{};

    char moduleFilters[6][64] = {};
    char weaponTypeFilter[64] = {};

    PresetSectionState<WeaponPresetSerializer> presets;
    int activeTab = 0;

    std::array<OverrideDescriptor, 10> combatFields;
    std::array<OverrideDescriptor, 1> physicsFields;
    std::array<OverrideDescriptor, 2> dismemberFields;
    std::array<OverrideDescriptor, 3> toggleFields;
    std::array<OverrideDescriptor, 4> staminaFields;

    void BuildDescriptors();
    int CountAllActive() const;

    enum class WeaponModuleSlot : std::uint8_t { Head = 0, Guard, Grip, Pommel, Count };
    static constexpr int MODULE_SLOT_COUNT = static_cast<int>(WeaponModuleSlot::Count);
    static constexpr const char* MODULE_SLOT_NAMES[] = {"Head", "Guard", "Grip", "Pommel"};

    struct MeshPoolEntry {
        SDK::UObject* mesh;
        std::string name;
        std::string display;
        MeshType type;
    };

    struct MeshOverride : MeshOverrideSettings {
        SDK::UObject* mesh = nullptr;
        int poolIndex = -1;
        std::string path;
    };

    struct MeshSnapshotSlot : MeshOverrideSettings {
        SDK::UObject* mesh = nullptr;
    };

    using MeshSnapshot = std::array<MeshSnapshotSlot, MODULE_SLOT_COUNT>;

    struct SpawnDraftSnapshot {
        SpawnConfig spawn;
        SDK::FStr_Passport_Weapon1 passport{};
        WeaponClassPaths classPaths;
        std::string deferredName;
        WeaponRuntimeProps runtime;
        MeshSnapshot meshes;
    };

    std::mutex spawnDraftMutex;
    SpawnDraftSnapshot publishedSpawnDraft{};
    std::uint64_t publishedSpawnDraftRevision = 0;

    std::vector<MeshPoolEntry> meshPool;
    bool meshScanQueued = false;
    float meshComboWidth = 0.0f;

    struct PendingMeshBatch {
        std::vector<MeshPoolEntry> entries;
        bool fullReplace = false;
    };

    struct PendingDraftUpdate {
        std::uint64_t revision = 0;
        WeaponPresetData data;
        SDK::UObject* loadedMeshes[MODULE_SLOT_COUNT] = {};
        bool replaceAll = false;
        bool presetApply = false;
    };

    struct PendingPresetError {
        std::string message;
        std::uint64_t revision = 0;
    };

    enum class FeedbackOrigin : std::uint8_t { Generation, Spawn, AddModel, Count };
    static constexpr std::size_t FEEDBACK_ORIGIN_COUNT = static_cast<std::size_t>(FeedbackOrigin::Count);

    struct PendingFeedback {
        FeedbackOrigin origin;
        std::uint64_t sequence = 0;
        std::uint64_t request = 0;
        std::uint64_t revision = 0;
        std::string error;
    };

    struct PendingRenderUpdates {
        std::optional<PendingDraftUpdate> draft;
        std::vector<PendingMeshBatch> meshBatches;
        std::array<std::optional<PendingFeedback>, FEEDBACK_ORIGIN_COUNT> feedback;
        std::optional<PendingPresetError> presetError;
    };

    std::mutex pendingRenderMutex;
    PendingRenderUpdates pendingRenderUpdates;
    std::atomic<bool> pendingRenderReady{false};
    std::uint64_t feedbackSequence = 0;
    std::array<std::atomic<std::uint64_t>, FEEDBACK_ORIGIN_COUNT> feedbackRequests{};
    std::array<GuiUtils::StatusMessage::Token, FEEDBACK_ORIGIN_COUNT> feedbackStatusTokens{};
    char meshFilters[MODULE_SLOT_COUNT][64] = {};
    std::vector<int> filteredMeshIndices;
    uint32_t meshPoolVersion = 0;
    uint32_t filteredMeshVersion = 0;
    std::string filteredMeshFilter;
    char assetPathBuf[256] = {};

    MeshOverride meshOverrides[MODULE_SLOT_COUNT];
    std::mutex skeletalPreviewMutex;
    SDK::USkeletalMeshComponent* skeletalPreviewComps[MODULE_SLOT_COUNT] = {};

    GlobalModulePool& globalModules = GlobalModulePool::Get();

    static const char* ExtractCategory(const std::string& fullName);
    static bool HasExcludedPath(const std::string& fullName);
    static bool HasExcludedName(std::string_view name);
    static bool IsStaticMeshInvalid(SDK::UStaticMesh* sm);
    static bool IsSkeletalMeshInvalid(SDK::USkeletalMesh* sk);
    void CollectMeshesFromWeapon(SDK::AModularWeaponBP_C* weapon);
    SDK::UObject* LoadAssetByPath(const char* pathStr);
    void ScanAllMeshes();
    void QueueMeshScan();
    bool HasAnyMeshOverride() const;
    void ApplyMeshOverrides(
        SDK::AModularWeaponBP_C* weapon, const MeshSnapshot& snap,
        SDK::USkeletalMeshComponent** outSkeletalComps = nullptr, bool enableSkeletalCollision = false
    );
    MeshSnapshot BuildMeshSnapshot() const;
    SpawnDraftSnapshot BuildSpawnDraftSnapshot() const;
    bool SpawnDraftMatchesCurrent(const SpawnDraftSnapshot& snapshot) const;
    void PublishSpawnDraftSnapshot();
    bool PublishAppliedPresetSpawnSnapshot(const PendingDraftUpdate& update);
    void ApplyMeshToPreview(const MeshSnapshot& snapshot);
    void ResetWeaponPassport();
    void QueueGeneration(CustomizableWeapon type, SDK::Enum_Ranks tier);
    void GenerateWeaponPassport();
    void RandomizeWeaponPassport();
    void SpawnPreview();
    void SpawnWeapon();
    void SpawnWeapon(const RuntimeContextSnapshot& runtime, SpawnDraftSnapshot draft);
    static void RenderVectorDrag(const char* label, SDK::FVector& vec, float speed = 0.01f);
    static void RenderMassDrag(const char* label, double& mass, float speed = 0.01f);
    void RenderWeaponTypeCombo();
    static void RenderValidatedTierCombo(const char* label, int& tier, uint16_t validMask);
    static void RenderSizeMassRow(const char* label, SDK::FVector& size, double& mass);
    void RenderGenerationControls();
    void RenderModulesTab();
    void RenderGeometryTab();
    void RenderAppearanceTab();
    void RenderMeshTransformControls(MeshOverride& ovr);
    void RenderMeshCombo(int slotIdx);
    bool PrepareDraftUpdate(PendingDraftUpdate& update, std::string& error);
    void PublishMeshEntries(std::vector<MeshPoolEntry> entries, bool fullReplace);
    void PublishDraftUpdate(PendingDraftUpdate update);
    std::uint64_t BeginFeedbackRequest(FeedbackOrigin origin) noexcept;
    void PublishFeedback(
        FeedbackOrigin origin, std::string error, std::uint64_t request = 0, std::uint64_t revision = 0
    );
    void PublishPresetError(std::string message, std::uint64_t revision);
    void DrainPendingRenderUpdates();
    void ApplyDraftUpdate(PendingDraftUpdate update);
    int FindMeshPoolIndexByObject(SDK::UObject* mesh) const;
    void ResolveMeshOverrideIndices();
    void RenderMeshTab();
    void RenderStatsTab();
    WeaponPresetData BuildPresetData() const;
    PresetApplyDisposition ApplyPresetData(const WeaponPresetData& data);
    void RenderSpawnFooter();
    void InitKeybinds();

public:
    explicit WeaponEditorSection(ModContext& ctx);
    void OnOpen() override;
    void Render() override;
    KeybindList* GetSearchKeybinds() noexcept override { return &keybinds; }
};
