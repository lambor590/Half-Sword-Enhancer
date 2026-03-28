#pragma once

#include "SDK/Str_WeaponParts_structs.hpp"
#include "SDK/Str_Passport_Weapon1_structs.hpp"
#include "SDK/Enum_Weapon_Material_Type_structs.hpp"

namespace WeaponPassportBuilder {

    inline SDK::FStr_Passport_Weapon1 FromWeaponParts(SDK::FStr_WeaponParts& parts) {
        SDK::FStr_Passport_Weapon1 p{};

        p.WeaponClass_54_B478ECF7499977809745A3973AD678EC = parts.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066;
        p.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 = parts.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F;
        p.GuardModule_13_6DD2B06245505E53B529D090333012F0 = parts.GuardModule_21_774015784EB0300D2671C894D57ED144;
        p.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4 = parts.GripModule_38_15B14C3F4E9701389A9B35A3B0909867;
        p.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6 = parts.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984;
        p.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D = parts.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0;
        p.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9 = parts.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980;

        p.HeadSize_21_2D425E61473B8F64FBAB51B223459D57 = parts.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA;
        p.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 = parts.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D;
        p.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF = {1.0, 1.0, 1.0};
        p.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E = parts.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524;

        p.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7 = 1.0;
        p.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927 = 1.0;
        p.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10 = 1.0;
        p.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173 = 1.0;

        p.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36 = static_cast<SDK::Enum_MaterialLayer>(3);
        p.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2 = static_cast<SDK::Enum_MaterialLayer>(0);
        p.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A = static_cast<SDK::Enum_MaterialLayer>(14);
        p.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB = static_cast<SDK::Enum_MaterialLayer>(10);

        auto& matMap = parts.MemberVar_40_43_0B501E224AC2292FC03A999C237C2C81;
        for (auto it = begin(matMap); it != end(matMap); ++it) {
            switch (it->Key()) {
                case SDK::Enum_Weapon_Material_Type::NewEnumerator0:
                    p.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36 = it->Value();
                    break;
                case SDK::Enum_Weapon_Material_Type::NewEnumerator1:
                    p.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2 = it->Value();
                    break;
                case SDK::Enum_Weapon_Material_Type::NewEnumerator2:
                    p.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A = it->Value();
                    break;
                case SDK::Enum_Weapon_Material_Type::NewEnumerator3:
                    p.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB = it->Value();
                    break;
                default: break;
            }
        }

        p.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 = {0.4f, 0.26f, 0.13f, 1.0f};
        p.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 = {0.3f, 0.18f, 0.08f, 1.0f};

        auto& colorMap = parts.MemberVar_44_45_FF627FBE4FE882E7D295BFA0BB6716C0;
        for (auto it = begin(colorMap); it != end(colorMap); ++it) {
            switch (it->Key()) {
                case SDK::Enum_Weapon_Material_Type::NewEnumerator2:
                    p.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 = it->Value();
                    break;
                case SDK::Enum_Weapon_Material_Type::NewEnumerator3:
                    p.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 = it->Value();
                    break;
                default: break;
            }
        }

        p.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461 = static_cast<SDK::Enum_Ranks>(4);
        p.Price_60_83FE5A624EA188485BBE4E9C8606AEE5 = 100.0;

        return p;
    }

}
