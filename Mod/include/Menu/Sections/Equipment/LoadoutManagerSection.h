#pragma once

#include <vector>
#include <string>
#include <atomic>
#include <functional>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "Utils/GameConstants.h"
#include "Utils/GlobalModulePool.h"
#include "Utils/LoadoutPresetSerializer.h"
#include "Utils/PresetSectionState.h"
#include "Utils/PresetPickerState.h"
#include "Utils/WeaponPresetSerializer.h"
#include "Utils/ArmorPresetSerializer.h"
#include "SDK/ArmorSlots_Enum_structs.hpp"

class LoadoutManagerSection : public Section {
public:
    struct Config {
        bool livePreview = true;
        int applyKey = -1;
        int randomizeKey = -1;
        int generateTier = 4;
    };

private:
    Config cfg;

    static constexpr auto& ARMOR_SLOTS = GameConstants::ARMOR_SLOTS;
    static constexpr int ARMOR_SLOT_COUNT = GameConstants::ARMOR_SLOT_COUNT;

    static constexpr const char* WEAPON_SLOT_NAMES[] = {"Right Hand", "Left Hand", "Slot R1", "Slot R2",
                                                        "Slot L1",    "Slot L2",   "Back"};

    static constexpr auto& MATERIAL_LAYER_NAMES = GameConstants::MATERIAL_LAYER_NAMES;

    struct ClassNameCache {
        SDK::UClass* ptr = nullptr;
        std::string name;
        const char* Get(SDK::UClass* cls);
    };
    ClassNameCache armorNameCache[17]{};
    ClassNameCache weaponNameCache[7]{};

    std::vector<KeybindEntry> keybinds;
    GlobalModulePool& modulePool = GlobalModulePool::Get();
    bool modulePoolQueued = false;
    char moduleFilters[6][64] = {};

    double lastSlotApplyTime = 0.0;
    SDK::EArmorSlots_Enum pendingSlot{};
    bool pendingSlotApply = false;

    std::vector<std::function<void()>> staggeredOps;
    size_t staggeredIdx = 0;
    std::atomic<bool> staggeredBusy{false};

    std::vector<std::function<void()>> pendingStaggeredOps;
    std::atomic<bool> hasPendingStaggeredOps{false};

    PresetSectionState<LoadoutPresetSerializer> presets;
    PresetPickerState<WeaponPresetSerializer> weaponPicker;
    PresetPickerState<ArmorPresetSerializer> armorPicker;
    int activeTab = 0;

    static const char* GetArmorSlotDisplayName(SDK::EArmorSlots_Enum slot);
    void ScheduleSlotApply(SDK::EArmorSlots_Enum slot);
    static void RemoveArmorSlot(SDK::AWillie_BP_C* p, SDK::EArmorSlots_Enum slot);
    void EnsureModulePool();
    static void RenderVectorDrag(const char* label, SDK::FVector& vec);
    void BuildArmorOps(std::vector<std::function<void()>>& ops);
    void ApplyArmorToPlayer();
    void ReapplyArmorSlot(SDK::EArmorSlots_Enum slot);
    void ApplyWeaponToPlayer(int slotIndex);
    void StripAllArmor();
    void ClearAllWeapons();
    void GenerateArmorForSlot(SDK::EArmorSlots_Enum slotEnum);
    void RandomizeAllArmor();
    void GenerateWeaponForSlot(int slotIndex);
    void ImportWeaponPreset(int slotIndex);
    void ImportArmorPreset(SDK::EArmorSlots_Enum slotEnum);
    void ApplyLoadoutPreset(const LoadoutPresetData& data);
    LoadoutPresetData BuildPresetFromPlayer();
    void RenderArmorTab();
    void RenderWeaponSlotModules(SDK::FStr_WeaponParts& slot);
    void RenderWeaponSlotSizes(SDK::FStr_WeaponParts& slot);
    static void RenderWeaponSlotMaterials(SDK::FStr_WeaponParts& slot);
    void RenderWeaponsTab();
    void InitKeybinds();

public:
    explicit LoadoutManagerSection(ModContext& ctx);
    void Render() override;
};
