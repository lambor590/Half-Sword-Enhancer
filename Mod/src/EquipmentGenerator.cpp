#include <string>

#include "Utils/EquipmentGenerator.h"
#include "Utils/GameConstants.h"
#include "SDK/Engine_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/Modular_Weapon_Part_Master_classes.hpp"
#include "SDK/BP_GameWeapon_Customizable_Master_classes.hpp"
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

        SDK::AModular_Weapon_Part_Master_C* GetModuleDefault(SDK::UClass* moduleClass) {
            if (!moduleClass || !moduleClass->ClassDefaultObject ||
                !moduleClass->IsSubclassOf(SDK::AModular_Weapon_Part_Master_C::StaticClass()))
                return nullptr;
            return static_cast<SDK::AModular_Weapon_Part_Master_C*>(moduleClass->ClassDefaultObject);
        }

        struct ModulePoolStats {
            int minModuleTier = 9;
            int maxModuleTier = -1;
            int minNameTier = 9;
            int maxNameTier = -1;
            double minPrice = 0.0;
            double maxPrice = 0.0;
            bool hasPrice = false;

            bool HasModuleTierRange() const { return maxModuleTier > minModuleTier; }
            bool HasNameTierRange() const { return maxNameTier > minNameTier; }
            bool HasPriceRange() const { return hasPrice && maxPrice > minPrice; }
            bool HasQualitySignal() const { return HasModuleTierRange() || HasNameTierRange() || HasPriceRange(); }
        };

        int NameTierScore(SDK::UClass* moduleClass) {
            if (!moduleClass) return -1;

            std::string name = moduleClass->GetName();
            if (name.find("Low_Tier") != std::string::npos) return 1;
            if (name.find("Mid_Tier") != std::string::npos) return 4;
            if (name.find("High_Tier") != std::string::npos) return 7;

            for (int i = 1; i <= 5; ++i) {
                std::string marker = "_T" + std::to_string(i);
                if (name.find(marker + "_") != std::string::npos ||
                    (name.size() >= marker.size() && name.rfind(marker) == name.size() - marker.size()))
                    return (i - 1) * 2;
            }
            return -1;
        }

        void AddModuleStats(SDK::UClass* moduleClass, ModulePoolStats& stats) {
            auto* module = GetModuleDefault(moduleClass);
            if (module) {
                int tier = static_cast<int>(module->Module_Tier);
                if (tier >= 0 && tier <= 8) {
                    if (tier < stats.minModuleTier) stats.minModuleTier = tier;
                    if (tier > stats.maxModuleTier) stats.maxModuleTier = tier;
                }
                if (!stats.hasPrice || module->Price < stats.minPrice) stats.minPrice = module->Price;
                if (!stats.hasPrice || module->Price > stats.maxPrice) stats.maxPrice = module->Price;
                stats.hasPrice = true;
            }

            int nameTier = NameTierScore(moduleClass);
            if (nameTier >= 0) {
                if (nameTier < stats.minNameTier) stats.minNameTier = nameTier;
                if (nameTier > stats.maxNameTier) stats.maxNameTier = nameTier;
            }
        }

        int ModuleScore(
            SDK::UClass* moduleClass, SDK::AModular_Weapon_Part_Master_C* module, const ModulePoolStats& stats
        ) {
            if (stats.HasModuleTierRange() && module) return static_cast<int>(module->Module_Tier);
            if (stats.HasNameTierRange()) return NameTierScore(moduleClass);
            if (stats.HasPriceRange() && module)
                return static_cast<int>(
                    ((module->Price - stats.minPrice) * 8.0 / (stats.maxPrice - stats.minPrice)) + 0.5
                );
            return -1;
        }

        SDK::UClass* PickModule(const SDK::TArray<SDK::UClass*>& classes, int requestedTier, double& price) {
            price = 0.0;
            if (classes.Num() == 0) return nullptr;

            ModulePoolStats stats{};
            for (int i = 0; i < classes.Num(); ++i)
                AddModuleStats(classes[i], stats);

            bool hasQualitySignal = stats.HasQualitySignal();
            int bestDistance = 1000;
            int bestCount = 0;
            for (int i = 0; i < classes.Num(); ++i) {
                auto* module = GetModuleDefault(classes[i]);
                int score = ModuleScore(classes[i], module, stats);
                int distance = score >= 0 ? score - requestedTier : (hasQualitySignal ? 1000 : 0);
                if (distance < 0) distance = -distance;
                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestCount = 1;
                } else if (distance == bestDistance) {
                    ++bestCount;
                }
            }

            int pick = GameConstants::RandomInt(0, bestCount - 1);
            for (int i = 0; i < classes.Num(); ++i) {
                auto* module = GetModuleDefault(classes[i]);
                int score = ModuleScore(classes[i], module, stats);
                int distance = score >= 0 ? score - requestedTier : (hasQualitySignal ? 1000 : 0);
                if (distance < 0) distance = -distance;
                if (distance != bestDistance || pick-- != 0) continue;

                price = module ? module->Price : 0.0;
                return classes[i];
            }

            auto* selected = classes[GameConstants::RandomInt(0, classes.Num() - 1)];
            auto* module = GetModuleDefault(selected);
            price = module ? module->Price : 0.0;
            return selected;
        }

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

        void ResetForWorld(const SDK::UWorld* world) {
            if (cachedWorld == world) [[likely]]
                return;

            weaponGenerator = nullptr;
            armorGenerator = nullptr;
            characterGenerator = nullptr;
            cachedWorld = world;
        }

        SDK::ABP_Generator_Weapons_Random_C* GetWeaponGenerator(const SDK::UWorld* world) {
            ResetForWorld(world);
            if (!weaponGenerator && cachedWorld)
                weaponGenerator = SpawnGenerator<SDK::ABP_Generator_Weapons_Random_C>(cachedWorld);
            return weaponGenerator;
        }

        SDK::ABP_Generator_Armor_Random_C* GetArmorGenerator(const SDK::UWorld* world) {
            ResetForWorld(world);
            if (!armorGenerator && cachedWorld)
                armorGenerator = SpawnGenerator<SDK::ABP_Generator_Armor_Random_C>(cachedWorld);
            return armorGenerator;
        }

        SDK::ABP_Generator_Characters_Random_C* GetCharacterGenerator(const SDK::UWorld* world) {
            ResetForWorld(world);
            if (!characterGenerator && cachedWorld)
                characterGenerator = SpawnGenerator<SDK::ABP_Generator_Characters_Random_C>(cachedWorld);
            return characterGenerator;
        }

    }

    void ClearCache() {
        weaponGenerator = nullptr;
        armorGenerator = nullptr;
        characterGenerator = nullptr;
        cachedWorld = nullptr;
    }

    SDK::FStr_Passport_Weapon1 GenerateWeapon(
        const SDK::UWorld* world, SDK::Enum_WeaponType type, SDK::Enum_Ranks tier
    ) {
        SDK::FStr_Passport_Weapon1 output{};
        auto* gen = GetWeaponGenerator(world);
        if (!gen) return output;

        SDK::FStr_Passport_Weapon1 emptyPassport{};
        for (int i = 0; i < MAX_ATTEMPTS; ++i) {
            gen->Generate_Weapon(type, tier, false, nullptr, emptyPassport, &output);
            if (IsPassportValid(output)) return output;
        }
        return output;
    }

    SDK::FStr_Passport_Weapon1 GenerateSpecificWeapon(
        const SDK::UWorld* world, SDK::UClass* weaponClass, SDK::Enum_Ranks tier
    ) {
        SDK::FStr_Passport_Weapon1 output{};
        auto* gen = GetWeaponGenerator(world);
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

    SDK::FStr_Passport_Weapon1 GenerateCustomizableWeapon(
        const SDK::UWorld* world, CustomizableWeapon type, SDK::Enum_Ranks tier
    ) {
        ResetForWorld(world);
        auto* modulesClass = GetCustomizableModulesClass(type);
        if (!modulesClass || !modulesClass->ClassDefaultObject) return {};

        auto* cdo = static_cast<SDK::UBP_GameWeapon_Customizable_Master_C*>(modulesClass->ClassDefaultObject);
        int requestedTier = static_cast<int>(tier);
        if (requestedTier < 0) requestedTier = 0;
        if (requestedTier > 8) requestedTier = 8;

        double totalPrice = 0.0;
        double modulePrice = 0.0;
        auto* head = PickModule(cdo->Module_Heads_Array, requestedTier, modulePrice);
        totalPrice += modulePrice;
        auto* grip = PickModule(cdo->Module_Grips_Array, requestedTier, modulePrice);
        totalPrice += modulePrice;
        if (!head || !grip) return {};

        SDK::FStr_Passport_Weapon1 output{};
        output.WeaponClass_54_B478ECF7499977809745A3973AD678EC = SDK::AModularWeaponBP_C::StaticClass();
        output.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 = head;
        output.GuardModule_13_6DD2B06245505E53B529D090333012F0 =
            PickModule(cdo->Module_Guards_Array, requestedTier, modulePrice);
        totalPrice += modulePrice;
        output.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4 = grip;
        output.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6 =
            PickModule(cdo->Module_Pommels_Array, requestedTier, modulePrice);
        totalPrice += modulePrice;
        output.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D =
            PickModule(cdo->Head_Sub_Module_1_Array, requestedTier, modulePrice);
        totalPrice += modulePrice;
        output.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9 =
            PickModule(cdo->Head_Sub_Module_2_Array, requestedTier, modulePrice);
        totalPrice += modulePrice;
        output.HeadSize_21_2D425E61473B8F64FBAB51B223459D57 = {1.0, 1.0, 1.0};
        output.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 = {1.0, 1.0, 1.0};
        output.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF = {1.0, 1.0, 1.0};
        output.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E = {1.0, 1.0, 1.0};
        output.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7 = 1.0;
        output.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927 = 1.0;
        output.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10 = 1.0;
        output.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173 = 1.0;
        output.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36 = static_cast<SDK::Enum_MaterialLayer>(3);
        output.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2 = static_cast<SDK::Enum_MaterialLayer>(0);
        output.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A = static_cast<SDK::Enum_MaterialLayer>(14);
        output.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB = static_cast<SDK::Enum_MaterialLayer>(10);
        output.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 = {0.4f, 0.26f, 0.13f, 1.0f};
        output.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 = {0.3f, 0.18f, 0.08f, 1.0f};
        output.Price_60_83FE5A624EA188485BBE4E9C8606AEE5 = totalPrice > 0.0 ? totalPrice : 100.0;
        output.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461 = tier;
        return output;
    }

    SDK::FStr_Passport_Armor1 GenerateArmor(
        const SDK::UWorld* world, SDK::Enum_Ranks tier, SDK::EArmorSlots_Enum slot, double moduleChance
    ) {
        SDK::FStr_Passport_Armor1 output{};
        auto* gen = GetArmorGenerator(world);
        if (gen) {
            gen->Generate_Armor(tier, slot, moduleChance, false, &output);
        }
        return output;
    }

    SDK::FStr_Passport_Character1 GenerateCharacter(
        const SDK::UWorld* world, SDK::UClass* actorClass, SDK::Enum_Nationalities nationality, SDK::Enum_Ranks tier,
        bool mercenary
    ) {
        SDK::FStr_Passport_Character1 output{};
        auto* gen = GetCharacterGenerator(world);
        if (gen) {
            gen->Generate_Character(actorClass, nationality, tier, mercenary, &output);
        }
        return output;
    }

}
