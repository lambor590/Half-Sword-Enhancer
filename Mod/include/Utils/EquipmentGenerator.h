#pragma once

#include "Utils/CustomizableWeapon.h"
#include "SDK/BP_Generator_Weapons_Random_classes.hpp"
#include "SDK/BP_Generator_Armor_Random_classes.hpp"
#include "SDK/BP_Generator_Characters_Random_classes.hpp"
#include "SDK/Str_Passport_Weapon1_structs.hpp"
#include "SDK/Str_Passport_Armor1_structs.hpp"
#include "SDK/Str_Passport_Character1_structs.hpp"

namespace EquipmentGenerator {

    void Init(const SDK::UWorld* world);
    void ClearCache();

    SDK::FStr_Passport_Weapon1 GenerateWeapon(SDK::Enum_WeaponType type, SDK::Enum_Ranks tier);
    SDK::FStr_Passport_Weapon1 GenerateSpecificWeapon(SDK::UClass* weaponClass, SDK::Enum_Ranks tier);
    SDK::FStr_Passport_Weapon1 GenerateCustomizableWeapon(CustomizableWeapon type, SDK::Enum_Ranks tier);
    SDK::FStr_Passport_Armor1 GenerateArmor(SDK::Enum_Ranks tier, SDK::EArmorSlots_Enum slot, double moduleChance = 0.5);
    SDK::FStr_Passport_Character1 GenerateCharacter(SDK::UClass* actorClass, SDK::Enum_Nationalities nationality, SDK::Enum_Ranks tier, bool mercenary = false);

}
