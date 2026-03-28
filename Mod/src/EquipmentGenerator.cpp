#include "Utils/EquipmentGenerator.h"
#include "SDK/Engine_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Sword_Arming_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Sword_Short_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Sword_Long_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Mace_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Mace_Short_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Mace_Long_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Hafted_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Hafted_Short_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Hafted_Long_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Polearm_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Polearm_Short_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Polearm_Long_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Pollaxe_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Pollaxe_Short_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Pollaxe_Long_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Casted_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Casted_Short_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Casted_Long_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Messer_classes.hpp"

namespace EquipmentGenerator {

    namespace {
        constexpr int MAX_ATTEMPTS = 3;

        const SDK::UWorld* cachedWorld = nullptr;
        SDK::ABP_Generator_Weapons_Random_C* weaponGenerator = nullptr;
        SDK::ABP_Generator_Armor_Random_C* armorGenerator = nullptr;
        SDK::ABP_Generator_Characters_Random_C* characterGenerator = nullptr;

        template <typename T> T* SpawnGenerator(const SDK::UWorld* world) {
            SDK::UClass* genClass = T::StaticClass();
            if (!genClass) return nullptr;

            SDK::FTransform transform{};
            transform.Rotation = SDK::FQuat(0, 0, 0, 1);
            transform.Scale3D = SDK::FVector(1, 1, 1);

            auto* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
                world, genClass, transform, SDK::ESpawnActorCollisionHandlingMethod::AlwaysSpawn, nullptr,
                SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime
            );
            if (!actor) return nullptr;

            SDK::UGameplayStatics::FinishSpawningActor(
                actor, transform, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime
            );
            return static_cast<T*>(actor);
        }

        SDK::ABP_Generator_Weapons_Random_C* GetWeaponGenerator() {
            if (!weaponGenerator && cachedWorld)
                weaponGenerator = SpawnGenerator<SDK::ABP_Generator_Weapons_Random_C>(cachedWorld);
            return weaponGenerator;
        }

        SDK::ABP_Generator_Armor_Random_C* GetArmorGenerator() {
            if (!armorGenerator && cachedWorld)
                armorGenerator = SpawnGenerator<SDK::ABP_Generator_Armor_Random_C>(cachedWorld);
            return armorGenerator;
        }

        SDK::ABP_Generator_Characters_Random_C* GetCharacterGenerator() {
            if (!characterGenerator && cachedWorld)
                characterGenerator = SpawnGenerator<SDK::ABP_Generator_Characters_Random_C>(cachedWorld);
            return characterGenerator;
        }
    }

    void Init(const SDK::UWorld* world) {
        cachedWorld = world;
    }

    void ClearCache() {
        weaponGenerator = nullptr;
        armorGenerator = nullptr;
        characterGenerator = nullptr;
        cachedWorld = nullptr;
    }

    SDK::FStr_Passport_Weapon1 GenerateWeapon(SDK::Enum_WeaponType type, SDK::Enum_Ranks tier) {
        SDK::FStr_Passport_Weapon1 output{};
        auto* gen = GetWeaponGenerator();
        if (!gen) return output;

        SDK::FStr_Passport_Weapon1 emptyPassport{};
        for (int i = 0; i < MAX_ATTEMPTS; ++i) {
            gen->Generate_Weapon(type, tier, false, nullptr, emptyPassport, &output);
            if (IsPassportValid(output)) return output;
        }
        return output;
    }

    SDK::FStr_Passport_Weapon1 GenerateSpecificWeapon(SDK::UClass* weaponClass, SDK::Enum_Ranks tier) {
        SDK::FStr_Passport_Weapon1 output{};
        auto* gen = GetWeaponGenerator();
        if (!gen) return output;

        SDK::FStr_Passport_Weapon1 emptyPassport{};
        for (int i = 0; i < MAX_ATTEMPTS; ++i) {
            gen->Generate_Weapon(SDK::Enum_WeaponType::NewEnumerator0, tier, true, weaponClass, emptyPassport, &output);
            if (IsPassportValid(output)) return output;
        }
        return output;
    }

    SDK::UClass* GetCustomizableModulesClass(CustomizableWeapon type) {
        switch (type) {
            case CustomizableWeapon::SwordArming: return SDK::UBP_GameWeapon_Customizable_Sword_Arming_C::StaticClass();
            case CustomizableWeapon::SwordShort: return SDK::UBP_GameWeapon_Customizable_Sword_Short_C::StaticClass();
            case CustomizableWeapon::SwordLong: return SDK::UBP_GameWeapon_Customizable_Sword_Long_C::StaticClass();
            case CustomizableWeapon::MaceShort: return SDK::UBP_GameWeapon_Customizable_Mace_Short_C::StaticClass();
            case CustomizableWeapon::Mace: return SDK::UBP_GameWeapon_Customizable_Mace_C::StaticClass();
            case CustomizableWeapon::MaceLong: return SDK::UBP_GameWeapon_Customizable_Mace_Long_C::StaticClass();
            case CustomizableWeapon::HaftedShort: return SDK::UBP_GameWeapon_Customizable_Hafted_Short_C::StaticClass();
            case CustomizableWeapon::Hafted: return SDK::UBP_GameWeapon_Customizable_Hafted_C::StaticClass();
            case CustomizableWeapon::HaftedLong: return SDK::UBP_GameWeapon_Customizable_Hafted_Long_C::StaticClass();
            case CustomizableWeapon::PolearmShort:
                return SDK::UBP_GameWeapon_Customizable_Polearm_Short_C::StaticClass();
            case CustomizableWeapon::Polearm: return SDK::UBP_GameWeapon_Customizable_Polearm_C::StaticClass();
            case CustomizableWeapon::PolearmLong: return SDK::UBP_GameWeapon_Customizable_Polearm_Long_C::StaticClass();
            case CustomizableWeapon::PollaxeShort:
                return SDK::UBP_GameWeapon_Customizable_Pollaxe_Short_C::StaticClass();
            case CustomizableWeapon::Pollaxe: return SDK::UBP_GameWeapon_Customizable_Pollaxe_C::StaticClass();
            case CustomizableWeapon::PollaxeLong: return SDK::UBP_GameWeapon_Customizable_Pollaxe_Long_C::StaticClass();
            case CustomizableWeapon::CastedShort: return SDK::UBP_GameWeapon_Customizable_Casted_Short_C::StaticClass();
            case CustomizableWeapon::Casted: return SDK::UBP_GameWeapon_Customizable_Casted_C::StaticClass();
            case CustomizableWeapon::CastedLong: return SDK::UBP_GameWeapon_Customizable_Casted_Long_C::StaticClass();
            case CustomizableWeapon::Messer: return SDK::UBP_GameWeapon_Customizable_Messer_C::StaticClass();
            default: return nullptr;
        }
    }

    SDK::FStr_Passport_Weapon1 GenerateCustomizableWeapon(CustomizableWeapon type, SDK::Enum_Ranks tier) {
        SDK::FStr_Passport_Weapon1 output{};
        auto* gen = GetWeaponGenerator();
        if (!gen) return output;

        gen->Customizable_Modules = GetCustomizableModulesClass(type);
        SDK::FStr_Passport_Weapon1 emptyPassport{};
        for (int i = 0; i < MAX_ATTEMPTS; ++i) {
            gen->Generate_Weapon(SDK::Enum_WeaponType::NewEnumerator0, tier, false, nullptr, emptyPassport, &output);
            if (IsPassportValid(output)) return output;
        }
        return output;
    }

    SDK::FStr_Passport_Armor1 GenerateArmor(SDK::Enum_Ranks tier, SDK::EArmorSlots_Enum slot, double moduleChance) {
        SDK::FStr_Passport_Armor1 output{};
        auto* gen = GetArmorGenerator();
        if (gen) {
            gen->Generate_Armor(tier, slot, moduleChance, false, &output);
        }
        return output;
    }

    SDK::FStr_Passport_Character1 GenerateCharacter(
        SDK::UClass* actorClass, SDK::Enum_Nationalities nationality, SDK::Enum_Ranks tier, bool mercenary
    ) {
        SDK::FStr_Passport_Character1 output{};
        auto* gen = GetCharacterGenerator();
        if (gen) {
            gen->Generate_Character(actorClass, nationality, tier, mercenary, &output);
        }
        return output;
    }

}
