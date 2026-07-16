#pragma once

#include <functional>
#include <string>
#include <vector>

#include "SDK/ArmorSlots_Enum_structs.hpp"
#include "SDK/Str_Passport_Armor1_structs.hpp"
#include "SDK/Str_Passport_Weapon1_structs.hpp"
#include "SDK/Str_WeaponParts_structs.hpp"

namespace SDK {
    class AWillie_BP_C;
    class UWorld;
}

struct ArmorPresetData;
struct ResolvedLoadoutPresetData;
struct WeaponPresetData;

namespace EquipmentApplication {
    SDK::FStr_Passport_Weapon1 DefaultWeaponPassport();

    void WriteWeaponPassportToSlot(const SDK::FStr_Passport_Weapon1& passport, SDK::FStr_WeaponParts& slot);
    bool WriteWeaponPresetToSlot(WeaponPresetData& preset, SDK::FStr_WeaponParts& slot, std::string* error = nullptr);
    void ClearWeaponSlot(SDK::FStr_WeaponParts& slot);
    bool EquipWeaponSlot(SDK::AWillie_BP_C* willie, int slotIndex, const SDK::FStr_WeaponParts& slot);

    // Game-thread snapshots used when a loadout slot has no explicit Copy/Reference draft link.
    bool CaptureEquippedWeaponPreset(SDK::AWillie_BP_C* willie, int handIndex, WeaponPresetData& result);
    bool CaptureConfiguredWeaponPreset(SDK::AWillie_BP_C* willie, int slotIndex, WeaponPresetData& result);
    bool CaptureEquippedArmorPreset(SDK::AWillie_BP_C* willie, SDK::EArmorSlots_Enum slot, ArmorPresetData& result);

    // Rebuilds the edited actor slot from the native configuration while preserving the other runtime actors.
    bool SynchronizeConfiguredWeaponActors(
        SDK::UWorld* world, SDK::AWillie_BP_C* willie, int overrideSlot,
        const WeaponPresetData* overridePreset = nullptr, std::string* error = nullptr
    );
    void ClearWeaponActors(SDK::AWillie_BP_C* willie);

    using LoadoutApplyCallback = std::function<void(bool)>;

    // Resolves/materializes before mutation, then applies armor over a few game ticks.
    bool ApplyPlayerLoadout(
        SDK::UWorld* world, SDK::AWillie_BP_C* player, const ResolvedLoadoutPresetData& loadout,
        std::string* error = nullptr, LoadoutApplyCallback onComplete = nullptr
    );
    bool ApplyPlayerArmorSet(
        SDK::UWorld* world, SDK::AWillie_BP_C* player, const std::vector<ArmorPresetData>& armor,
        std::string* error = nullptr, LoadoutApplyCallback onComplete = nullptr
    );

    // NPC spawning already finishes the deferred actor before these helpers run. Two queued ticks are enough for the
    // normal Blueprint initialization path before applying final overrides/equipment.
    bool WaitForNPCInitialization(
        SDK::UWorld* world, SDK::AWillie_BP_C* npc, LoadoutApplyCallback onComplete, std::string* error = nullptr
    );
    bool ApplyNPCLoadout(
        SDK::UWorld* world, SDK::AWillie_BP_C* npc, ResolvedLoadoutPresetData loadout, std::string* error = nullptr,
        LoadoutApplyCallback onComplete = nullptr
    );

    // Game-thread lifecycle cleanup. Interrupted requests complete once with failure.
    void AbortRuntimeTransactionsForShutdown() noexcept;
    void OnRuntimeShutdown() noexcept;
}
