#pragma once

#include <string>

#include "Utils/WeaponClassPaths.h"
#include "SDK/ArmorSlots_Enum_structs.hpp"
#include "SDK/Str_Passport_Armor1_structs.hpp"
#include "SDK/Str_Passport_Weapon1_structs.hpp"
#include "SDK/Str_WeaponParts_structs.hpp"

namespace SDK {
    class AWillie_BP_C;
    class UWorld;
}

struct LoadoutPresetData;

namespace EquipmentApplication {
    SDK::FStr_Passport_Weapon1 DefaultWeaponPassport();
    void ClearWeaponPassportPadding(SDK::FStr_Passport_Weapon1& passport);

    void WriteWeaponPassportToSlot(const SDK::FStr_Passport_Weapon1& passport, SDK::FStr_WeaponParts& slot);
    void ResolveWeaponPassportClasses(SDK::FStr_Passport_Weapon1& passport, const WeaponClassPaths& paths);
    void ClearWeaponSlot(SDK::FStr_WeaponParts& slot);

    bool EquipHandWeaponFromSlot(SDK::AWillie_BP_C* willie, int slotIndex, const SDK::FStr_WeaponParts& slot);

    void WritePlayerLoadoutState(SDK::AWillie_BP_C& player, const LoadoutPresetData& loadout);
    void ApplyNPCLoadoutNow(SDK::UWorld* world, SDK::AWillie_BP_C* npc, const LoadoutPresetData& loadout);
}
