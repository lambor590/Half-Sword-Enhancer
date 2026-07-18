#pragma once

#include <array>
#include <vector>
#include <string>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "Utils/ArmorGenerationOptions.h"
#include "Utils/GlobalModulePool.h"
#include "Utils/LoadoutPresetSerializer.h"
#include "Utils/PresetSectionState.h"
#include "Utils/PresetLinkPickerState.h"
#include "Utils/PresetLinkState.h"
#include "Utils/WeaponPresetSerializer.h"
#include "Utils/ArmorPresetSerializer.h"
#include "SDK/ArmorSlots_Enum_structs.hpp"

class LoadoutManagerSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::Equipment, "Loadout Manager", "Create and reuse complete sets of weapons and armor."
    };

    struct Config {
        bool livePreview = true;
        int applyKey = -1;
        int randomizeKey = -1;
        int generateTier = 4;
        int weaponSpecificType = 0;
        EquipmentGenerator::ArmorGenerationOptions armorOptions;
    };

private:
    Config cfg;

    struct ClassNameCache {
        SDK::UClass* ptr = nullptr;
        std::string name;
        const char* Get(SDK::UClass* cls);
    };
    std::array<ClassNameCache, LoadoutPresetData::K_ARMOR_SLOT_COUNT> armorNameCache{};
    std::array<ClassNameCache, LoadoutPresetData::K_WEAPON_SLOT_COUNT> weaponNameCache{};

    KeybindList keybinds;
    GlobalModulePool& modulePool = GlobalModulePool::Get();
    std::atomic<bool> modulePoolQueued{false};
    char moduleFilters[6][64] = {};

    double lastSlotApplyTime = 0.0;
    SDK::EArmorSlots_Enum pendingSlot{};
    bool pendingSlotApply = false;

    using ArmorTransactionBuilder =
        std::function<bool(const RuntimeContextSnapshot&, std::vector<ArmorPresetData>&, std::string&)>;
    using ArmorTransactionSuccess = std::function<void(SDK::AWillie_BP_C*)>;
    std::atomic<bool> armorOperationInProgress{false};
    std::mutex equipmentOperationMutex;
    struct KeybindArmorConfig {
        int tier = 4;
        EquipmentGenerator::ArmorGenerationOptions options;
    } keybindConfigSnapshot;
    std::mutex keybindConfigMutex;

    PresetSectionState<LoadoutPresetSerializer> presets;
    PresetLinkPickerState<WeaponPresetSerializer> weaponPresetComposer;
    PresetLinkPickerState<ArmorPresetSerializer> armorPresetComposer;
    std::array<PresetLinkState<WeaponPresetSerializer>, LoadoutPresetData::K_WEAPON_SLOT_COUNT> weaponSlotLinkStates{};
    std::array<PresetLinkState<ArmorPresetSerializer>, LoadoutPresetData::K_ARMOR_SLOT_COUNT> armorSlotLinkStates{};
    SDK::AWillie_BP_C* draftOwner = nullptr;
    struct PendingDraftUpdates {
        std::optional<LoadoutPresetData> loadout;
        std::array<std::optional<PresetLink<WeaponPresetData>>, LoadoutPresetData::K_WEAPON_SLOT_COUNT> weapons;
        std::array<std::optional<PresetLink<ArmorPresetData>>, LoadoutPresetData::K_ARMOR_SLOT_COUNT> armor;
        std::optional<std::string> result;
        bool clearArmorLinks = false;
    } pendingDraftUpdates;
    std::mutex pendingDraftMutex;
    std::atomic<bool> pendingDraftReady{false};
    GuiUtils::StatusMessage::Token draftStatusToken = 0;
    bool draftDetachedFromRuntime = false;
    enum class LoadoutApplyResult : std::uint8_t { None, Success, Failure, PlayerChanged };
    std::atomic<std::uint64_t> loadoutApplyGeneration{0};
    std::atomic<LoadoutApplyResult> loadoutApplyResult{LoadoutApplyResult::None};
    std::atomic<bool> loadoutApplyInProgress{false};
    struct PresetSaveState {
        struct Completion {
            std::optional<LoadoutPresetData> data;
            PresetOperationResult operation;
            bool overwrite = false;
        };

        std::atomic<bool> inProgress{false};
        std::atomic<bool> completionReady{false};
        std::mutex completionMutex;
        std::optional<Completion> completion;

        void Publish(Completion result);
        std::optional<Completion> TakeCompletion();
    };
    std::shared_ptr<PresetSaveState> presetSaveState = std::make_shared<PresetSaveState>();
    int activeTab = 0;

    static const char* GetArmorSlotDisplayName(SDK::EArmorSlots_Enum slot) noexcept;
    static const char* GetWeaponSlotDisplayName(int slot) noexcept;
    void ScheduleSlotApply(SDK::EArmorSlots_Enum slot);
    void EnsureModulePool();
    static bool RenderVectorDrag(const char* label, SDK::FVector& vec);
    void QueueArmorTransaction(
        std::string label, ArmorTransactionBuilder buildTarget, ArmorTransactionSuccess onSuccess = nullptr,
        const RuntimeContextSnapshot* immediateRuntime = nullptr
    );
    void ApplyArmorToPlayer(const RuntimeContextSnapshot* immediateRuntime = nullptr);
    void ReapplyArmorSlot(SDK::EArmorSlots_Enum slot);
    void RemoveArmorForSlot(SDK::EArmorSlots_Enum slot);
    void ApplyWeaponToPlayer(int slotIndex);
    void StripAllArmor();
    void ClearAllWeapons();
    void GenerateArmorForSlot(SDK::EArmorSlots_Enum slotEnum);
    void RandomizeAllArmor(const RuntimeContextSnapshot* immediateRuntime = nullptr);
    void GenerateWeaponForSlot(int slotIndex);
    void ImportWeaponPreset(int slotIndex);
    void ImportArmorPreset(std::optional<SDK::EArmorSlots_Enum> expectedSlot = std::nullopt);
    [[nodiscard]] PresetApplyDisposition ApplyLoadoutPreset(const LoadoutPresetData& data);
    void SetDraftError(std::string error);
    void QueueDraftResult(std::string error = {});
    void QueueWeaponDraftUpdate(SDK::AWillie_BP_C* owner, int slotIndex, PresetLink<WeaponPresetData> link);
    void QueueArmorDraftUpdate(SDK::AWillie_BP_C* owner, int slotIndex, PresetLink<ArmorPresetData> link);
    void QueueClearArmorDraftLinks(SDK::AWillie_BP_C* owner);
    void FinishLoadoutApply(
        SDK::AWillie_BP_C* owner, std::uint64_t generation, std::optional<LoadoutPresetData> loadout,
        LoadoutApplyResult failureResult = LoadoutApplyResult::Failure
    );
    void AdoptLoadoutDraft(LoadoutPresetData loadout, bool detachedFromRuntime);
    void ResetDraftOwner(SDK::AWillie_BP_C* owner);
    void ConsumePendingDraftUpdates(SDK::AWillie_BP_C* owner);
    void CheckDraftLinks();
    [[nodiscard]] bool HasBrokenDraft() const noexcept;
    [[nodiscard]] std::optional<std::string> GetBrokenDraftDiagnostic() const;
    [[nodiscard]] bool IsEquipmentBusy() const noexcept;
    PresetBuildResult<LoadoutPresetData> QueuePresetSave(std::string name, bool overwrite);
    void ConsumePresetSaveCompletion();
    void RenderArmorTab();
    void RenderWeaponSlotModules(int slotIndex, const SDK::FStr_WeaponParts& slot);
    void RenderWeaponSlotSizes(int slotIndex, const SDK::FStr_WeaponParts& slot);
    static void RenderWeaponSlotMaterials(int slotIndex, const SDK::FStr_WeaponParts& slot);
    void RenderWeaponsTab();
    void InitKeybinds();

public:
    explicit LoadoutManagerSection(ModContext& ctx);
    void Render() override;
    KeybindList* GetSearchKeybinds() noexcept override { return &keybinds; }
};
