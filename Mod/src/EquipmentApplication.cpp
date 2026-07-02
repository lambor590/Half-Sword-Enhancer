#include "Utils/EquipmentApplication.h"

#include <cstdint>
#include <cstring>

#include "Utils/LoadoutPresetSerializer.h"
#include "Utils/Spawner.h"
#include "SDK/Enum_Weapon_Material_Type_structs.hpp"
#include "SDK/Willie_BP_classes.hpp"

namespace EquipmentApplication {

    namespace {
        SDK::UClass* ResolveClassPath(const std::string& path) {
            return path.empty() ? nullptr : Spawner::LoadClass(path);
        }

        void WriteLoadoutWeaponSlot(
            SDK::FStr_WeaponParts& slot, const LoadoutPresetData::WeaponSlotData& weaponSlot
        ) {
            slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 = ResolveClassPath(weaponSlot.weaponClass);
            slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 = ResolveClassPath(weaponSlot.gripModule);
            slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F = ResolveClassPath(weaponSlot.headModule);
            slot.GuardModule_21_774015784EB0300D2671C894D57ED144 = ResolveClassPath(weaponSlot.guardModule);
            slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 = ResolveClassPath(weaponSlot.pommelModule);
            slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 = ResolveClassPath(weaponSlot.subModule1);
            slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 = ResolveClassPath(weaponSlot.subModule2);
            slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA = weaponSlot.headSize;
            slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D = weaponSlot.guardSize;
            slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524 = weaponSlot.pommelSize;
            slot.COAInt_63_593665BE4EF020F95F7D1A92564C1239 = weaponSlot.coaInt;
        }

        bool BuildLoadoutArmorPassport(
            SDK::FStr_Passport_Armor1& passport, const LoadoutPresetData::ArmorSlotData& armorSlot
        ) {
            auto* armorClass = ResolveClassPath(armorSlot.armorClass);
            if (!armorClass) return false;

            passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = armorClass;
            passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393 = armorSlot.color1;
            passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C = armorSlot.color2;
            passport.Slot_30_7561CB484566A4512003EA96ED44F88D = armorSlot.slot;
            return true;
        }

        SDK::FStr_Passport_Weapon1 BuildWeaponPassportFromSlot(const SDK::FStr_WeaponParts& slot) {
            SDK::FStr_Passport_Weapon1 passport{};

            passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC =
                slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066;
            passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 =
                slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F;
            passport.GuardModule_13_6DD2B06245505E53B529D090333012F0 =
                slot.GuardModule_21_774015784EB0300D2671C894D57ED144;
            passport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4 =
                slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867;
            passport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6 =
                slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984;
            passport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D =
                slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0;
            passport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9 =
                slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980;

            passport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57 =
                slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA;
            passport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 =
                slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D;
            passport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF = {1.0, 1.0, 1.0};
            passport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E =
                slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524;

            passport.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7 = 1.0;
            passport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927 = 1.0;
            passport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10 = 1.0;
            passport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173 = 1.0;

            passport.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36 =
                static_cast<SDK::Enum_MaterialLayer>(3);
            passport.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2 =
                static_cast<SDK::Enum_MaterialLayer>(0);
            passport.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A =
                static_cast<SDK::Enum_MaterialLayer>(14);
            passport.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB =
                static_cast<SDK::Enum_MaterialLayer>(10);

            const auto& matMap = slot.MemberVar_40_43_0B501E224AC2292FC03A999C237C2C81;
            for (auto it = begin(matMap); it != end(matMap); ++it) {
                switch (it->Key()) {
                    case SDK::Enum_Weapon_Material_Type::NewEnumerator0:
                        passport.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36 = it->Value();
                        break;
                    case SDK::Enum_Weapon_Material_Type::NewEnumerator1:
                        passport.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2 = it->Value();
                        break;
                    case SDK::Enum_Weapon_Material_Type::NewEnumerator2:
                        passport.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A = it->Value();
                        break;
                    case SDK::Enum_Weapon_Material_Type::NewEnumerator3:
                        passport.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB = it->Value();
                        break;
                    default: break;
                }
            }

            passport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 = {0.4f, 0.26f, 0.13f, 1.0f};
            passport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 = {0.3f, 0.18f, 0.08f, 1.0f};

            const auto& colorMap = slot.MemberVar_44_45_FF627FBE4FE882E7D295BFA0BB6716C0;
            for (auto it = begin(colorMap); it != end(colorMap); ++it) {
                switch (it->Key()) {
                    case SDK::Enum_Weapon_Material_Type::NewEnumerator2:
                        passport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 = it->Value();
                        break;
                    case SDK::Enum_Weapon_Material_Type::NewEnumerator3:
                        passport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 = it->Value();
                        break;
                    default: break;
                }
            }

            passport.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461 = static_cast<SDK::Enum_Ranks>(4);
            passport.Price_60_83FE5A624EA188485BBE4E9C8606AEE5 = 100.0;
            return passport;
        }
    }

    SDK::FStr_Passport_Weapon1 DefaultWeaponPassport() {
        SDK::FStr_Passport_Weapon1 passport{};
        passport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57 = {1.0, 1.0, 1.0};
        passport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 = {1.0, 1.0, 1.0};
        passport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF = {1.0, 1.0, 1.0};
        passport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E = {1.0, 1.0, 1.0};
        passport.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7 = 1.0;
        passport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927 = 1.0;
        passport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10 = 1.0;
        passport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173 = 1.0;
        passport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 = {0.4f, 0.26f, 0.13f, 1.0f};
        passport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 = {0.3f, 0.18f, 0.08f, 1.0f};
        passport.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461 = static_cast<SDK::Enum_Ranks>(4);
        passport.Price_60_83FE5A624EA188485BBE4E9C8606AEE5 = 100.0;
        return passport;
    }

    void ClearWeaponPassportPadding(SDK::FStr_Passport_Weapon1& passport) {
        std::memset(passport.Pad_14, 0, sizeof(passport.Pad_14));
        std::memset(passport.Pad_EC, 0, sizeof(passport.Pad_EC));
        std::memset(reinterpret_cast<uint8_t*>(&passport) + 0xF9, 0, 7);
    }

    void WriteWeaponPassportToSlot(const SDK::FStr_Passport_Weapon1& passport, SDK::FStr_WeaponParts& slot) {
        slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 =
            passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC;
        slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F =
            passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139;
        slot.GuardModule_21_774015784EB0300D2671C894D57ED144 =
            passport.GuardModule_13_6DD2B06245505E53B529D090333012F0;
        slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 =
            passport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4;
        slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 =
            passport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6;
        slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 =
            passport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D;
        slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 =
            passport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9;
        slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA =
            passport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57;
        slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D =
            passport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704;
        slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524 =
            passport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E;
    }

    void ResolveWeaponPassportClasses(SDK::FStr_Passport_Weapon1& passport, const WeaponClassPaths& paths) {
        passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC = ResolveClassPath(paths.weaponClass);
        passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 = ResolveClassPath(paths.headModule);
        passport.GuardModule_13_6DD2B06245505E53B529D090333012F0 = ResolveClassPath(paths.guardModule);
        passport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4 = ResolveClassPath(paths.gripModule);
        passport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6 = ResolveClassPath(paths.pommelModule);
        passport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D = ResolveClassPath(paths.subModule1);
        passport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9 = ResolveClassPath(paths.subModule2);
    }

    void ClearWeaponSlot(SDK::FStr_WeaponParts& slot) {
        slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 = nullptr;
        slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F = nullptr;
        slot.GuardModule_21_774015784EB0300D2671C894D57ED144 = nullptr;
        slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 = nullptr;
        slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 = nullptr;
        slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 = nullptr;
        slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 = nullptr;
    }

    bool EquipWeaponSlot(SDK::AWillie_BP_C* willie, int slotIndex, const SDK::FStr_WeaponParts& slot) {
        if (!willie || (slotIndex != 0 && slotIndex != 1)) return false;

        auto* weaponClass = slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066;
        if (!weaponClass) return false;

        auto passport = BuildWeaponPassportFromSlot(slot);
        if (slotIndex == 0) {
            willie->Set_Up_Right_Hand_Weapon(weaponClass, willie->Weapon_R, false, true, passport);
        } else {
            willie->Set_Up_Left_Hand_Weapon(weaponClass, willie->Weapon_L, false, true, passport);
        }
        return true;
    }

    void WritePlayerLoadout(SDK::AWillie_BP_C& player, const LoadoutPresetData& loadout) {
        auto& weapons = player.Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        auto& equipArmorMap = player.Load_Equipment.Armor_84_A1BA4DD44FD262BCA53B9DACF03CDF04
                                  .ArmorinSlots_31_702A9C5C40C7F4335C6B4687EC09936A;
        auto& currentArmorMap = player.Currently_Equipped_Armor;

        for (auto it = begin(equipArmorMap); it != end(equipArmorMap); ++it)
            it->Value().ArmorBPClass_2_0A22459840BF9E6989DFA4BA6CFED1D3 = nullptr;
        for (auto it = begin(currentArmorMap); it != end(currentArmorMap); ++it)
            it->Value().ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = nullptr;

        for (const auto& armorSlot : loadout.armorSlots) {
            SDK::FStr_Passport_Armor1 passport{};
            if (!BuildLoadoutArmorPassport(passport, armorSlot)) continue;

            for (auto it = begin(equipArmorMap); it != end(equipArmorMap); ++it) {
                if (it->Key() == armorSlot.slot) {
                    auto& elem = it->Value();
                    elem.ArmorBPClass_2_0A22459840BF9E6989DFA4BA6CFED1D3 =
                        passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
                    elem.Color1_5_5527FC7C442DCF594A4DA5BA8D94351F = armorSlot.color1;
                    elem.Color2_7_1FF790D94C8CD95FF2D76183E7102E1B = armorSlot.color2;
                    elem.Color3_9_D8B5A08742A87F5492F8138A4F686141 = armorSlot.color3;
                    break;
                }
            }

            for (auto it = begin(currentArmorMap); it != end(currentArmorMap); ++it) {
                if (it->Key() == armorSlot.slot) {
                    auto& target = it->Value();
                    target.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 =
                        passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
                    target.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393 = armorSlot.color1;
                    target.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C = armorSlot.color2;
                    target.Slot_30_7561CB484566A4512003EA96ED44F88D = armorSlot.slot;
                    break;
                }
            }
        }

        for (int i = 0; i < 7; ++i) {
            auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, i);
            WriteLoadoutWeaponSlot(slot, loadout.weaponSlots[i]);
        }
    }

    void ApplyNPCLoadout(SDK::UWorld* world, SDK::AWillie_BP_C* npc, const LoadoutPresetData& loadout) {
        for (const auto& armorSlot : loadout.armorSlots) {
            SDK::FStr_Passport_Armor1 armorPassport{};
            if (!BuildLoadoutArmorPassport(armorPassport, armorSlot)) continue;

            Spawner::SpawnAndEquipArmor(world, npc, armorPassport);
        }

        auto& weapons = npc->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        for (int i = 0; i < 7; ++i) {
            const auto& weaponSlot = loadout.weaponSlots[i];
            if (weaponSlot.weaponClass.empty()) continue;

            auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, i);
            WriteLoadoutWeaponSlot(slot, weaponSlot);
        }

        npc->Set_Up_Armor(true, false);
    }
}
