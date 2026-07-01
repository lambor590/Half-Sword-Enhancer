#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

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
    static constexpr SectionDefinition SECTION{MenuTab::Equipment, "Weapon Editor"};

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
    bool weaponGenerationPending = false;
    bool modulePoolQueued = false;

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

    std::vector<OverrideDescriptor> combatFields;
    std::vector<OverrideDescriptor> physicsFields;
    std::vector<OverrideDescriptor> dismemberFields;
    std::vector<OverrideDescriptor> toggleFields;
    std::vector<OverrideDescriptor> staminaFields;

    void BuildDescriptors();
    int CountAllActive() const;

    enum class WeaponModuleSlot : std::uint8_t { Head = 0, Guard, Grip, Pommel, Count };
    static constexpr int MODULE_SLOT_COUNT = static_cast<int>(WeaponModuleSlot::Count);
    static constexpr const char* MODULE_SLOT_NAMES[] = {"Head", "Guard", "Grip", "Pommel"};

    struct MeshPoolEntry {
        SDK::UObject* mesh;
        std::string name;
        std::string path;
        std::string display;
        const char* category;
        MeshType type;
    };

    struct MeshOverride : MeshOverrideSettings {
        SDK::UObject* mesh = nullptr;
        int poolIndex = -1;
    };

    struct MeshSnapshot {
        MeshOverride slots[MODULE_SLOT_COUNT];
    };

    std::vector<MeshPoolEntry> meshPool;
    std::unordered_map<std::string, int> meshPathIndex;
    std::unordered_map<SDK::UObject*, int> meshObjectIndex;
    std::unordered_set<SDK::UObject*> meshSeen;
    bool meshScanQueued = false;
    float meshComboWidth = 0.0f;
    int staticMeshCount = 0;
    int skeletalMeshCount = 0;

    std::vector<MeshPoolEntry> pendingMeshEntries;
    std::atomic<bool> meshPendingReady{false};
    std::atomic<bool> meshResolvePending{false};
    bool meshPendingIsFullReplace = false;
    char meshFilters[MODULE_SLOT_COUNT][64] = {};
    std::vector<int> filteredMeshIndices;
    uint32_t meshPoolVersion = 0;
    uint32_t filteredMeshVersion = 0;
    std::string filteredMeshFilter;
    char assetPathBuf[256] = {};

    MeshOverride meshOverrides[MODULE_SLOT_COUNT];
    SDK::USkeletalMeshComponent* skeletalPreviewComps[MODULE_SLOT_COUNT] = {};

    GlobalModulePool& globalModules = GlobalModulePool::Get();

    static void ClearWeaponPassportPadding(SDK::FStr_Passport_Weapon1& p);
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
    void ApplyMeshToPreview();
    void CreateBlankWeaponPassport();
    void QueueGeneration(CustomizableWeapon type, SDK::Enum_Ranks tier);
    void GenerateWeaponPassport();
    void RandomizeWeaponPassport();
    void ApplyOverridesToActor(SDK::AActor* actor) const;
    void SpawnPreview();
    void SpawnFromPassport();
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
    void DrainPendingMeshEntries();
    void RebuildMeshDisplayCache();
    int FindMeshPoolIndexByPath(const std::string& path) const;
    int FindMeshPoolIndexByObject(SDK::UObject* mesh) const;
    void ResolveMeshOverrideIndices();
    void RenderMeshTab();
    void RenderStatsTab();
    WeaponPresetData BuildPresetData() const;
    void ApplyPresetData(WeaponPresetData d);
    void SetStatus(const std::string& msg, bool isError = false);
    void RenderSpawnFooter();
    void InitKeybinds();

public:
    explicit WeaponEditorSection(ModContext& ctx);
    void OnOpen() override;
    void Render() override;
    KeybindList* GetSearchKeybinds() noexcept override { return &keybinds; }
};
