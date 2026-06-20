#pragma once

#include "Utils/ArmorGenerationOptions.h"
#include "Utils/CustomizableWeapon.h"
#include "SDK/BP_Generator_Weapons_Random_classes.hpp"
#include "SDK/BP_Generator_Armor_Random_classes.hpp"
#include "SDK/BP_Generator_Characters_Random_classes.hpp"
#include "SDK/Str_Passport_Weapon1_structs.hpp"
#include "SDK/Str_Passport_Armor1_structs.hpp"
#include "SDK/Str_Passport_Character1_structs.hpp"

namespace EquipmentGenerator {

    void ClearCache();

    inline bool IsPassportValid(const SDK::FStr_Passport_Weapon1& passport) {
        return passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 != nullptr;
    }

    inline bool IsArmorPassportValid(const SDK::FStr_Passport_Armor1& passport) {
        return passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 != nullptr;
    }

    SDK::UClass* GetCustomizableModulesClass(CustomizableWeapon type);

    SDK::FStr_Passport_Weapon1 GenerateWeapon(
        const SDK::UWorld* world, SDK::Enum_WeaponType type, SDK::Enum_Ranks tier
    );
    SDK::FStr_Passport_Weapon1 GenerateSpecificWeapon(
        const SDK::UWorld* world, SDK::UClass* weaponClass, SDK::Enum_Ranks tier
    );
    SDK::FStr_Passport_Weapon1 GenerateCustomizableWeapon(
        const SDK::UWorld* world, CustomizableWeapon type, SDK::Enum_Ranks tier
    );
    SDK::FStr_Passport_Armor1 GenerateArmor(
        const SDK::UWorld* world, SDK::Enum_Ranks tier, SDK::EArmorSlots_Enum slot,
        ArmorGenerationOptions options = {}
    );
    SDK::FStr_Passport_Character1 GenerateCharacter(
        const SDK::UWorld* world, SDK::UClass* actorClass, SDK::Enum_Nationalities nationality, SDK::Enum_Ranks tier,
        bool mercenary = false
    );

}
