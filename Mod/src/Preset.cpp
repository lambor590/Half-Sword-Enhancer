#include "Menu/Preset.h"

#include <cstdlib>
#include <string>

#include "SimpleIni.h"
#include "Utils/PresetUtils.h"

// Include all preset data types for their GetPresetFields/GetOverrideGroups implementations
#include "Utils/PlayerPresetSerializer.h"
#include "Utils/NPCPresetSerializer.h"
#include "Utils/WeaponPresetSerializer.h"
#include "Utils/ArmorPresetSerializer.h"
#include "Utils/LoadoutPresetSerializer.h"

// Unified field serialize/deserialize

void SerializePresetFields(std::span<const PresetFieldDescriptor> fields, CSimpleIniA& ini) {
    for (const auto& f : fields) {
        switch (f.type) {
            case PresetFieldType::String: {
                const auto& str = *static_cast<std::string*>(f.value);
                ini.SetValue(f.section, f.key, str.c_str());
                break;
            }
            case PresetFieldType::Int:
                ini.SetValue(f.section, f.key, std::to_string(*static_cast<int*>(f.value)).c_str());
                break;
            case PresetFieldType::Double:
                ini.SetValue(f.section, f.key, std::to_string(*static_cast<double*>(f.value)).c_str());
                break;
            case PresetFieldType::Bool: ini.SetValue(f.section, f.key, *static_cast<bool*>(f.value) ? "1" : "0"); break;
            case PresetFieldType::Vec3:
                ini.SetValue(f.section, f.key, PresetUtils::VecToString(*static_cast<SDK::FVector*>(f.value)).c_str());
                break;
            case PresetFieldType::Rotator:
                ini.SetValue(f.section, f.key, PresetUtils::RotToString(*static_cast<SDK::FRotator*>(f.value)).c_str());
                break;
            case PresetFieldType::Color:
                ini.SetValue(
                    f.section, f.key, PresetUtils::ColorToString(*static_cast<SDK::FLinearColor*>(f.value)).c_str()
                );
                break;
        }
    }
}

void DeserializePresetFields(std::span<const PresetFieldDescriptor> fields, const CSimpleIniA& ini) {
    for (const auto& f : fields) {
        const char* raw = ini.GetValue(f.section, f.key, f.defaultStr);

        switch (f.type) {
            case PresetFieldType::String: *static_cast<std::string*>(f.value) = raw ? raw : ""; break;
            case PresetFieldType::Int: *static_cast<int*>(f.value) = raw ? std::atoi(raw) : 0; break;
            case PresetFieldType::Double: *static_cast<double*>(f.value) = raw ? std::atof(raw) : 0.0; break;
            case PresetFieldType::Bool: *static_cast<bool*>(f.value) = raw ? (std::atoi(raw) != 0) : false; break;
            case PresetFieldType::Vec3: *static_cast<SDK::FVector*>(f.value) = PresetUtils::StringToVec(raw); break;
            case PresetFieldType::Rotator: *static_cast<SDK::FRotator*>(f.value) = PresetUtils::StringToRot(raw); break;
            case PresetFieldType::Color: {
                SDK::FLinearColor def{0.5f, 0.5f, 0.5f, 1.0f};
                if (f.defaultStr && f.defaultStr[0]) def = PresetUtils::StringToColor(f.defaultStr, def);
                *static_cast<SDK::FLinearColor*>(f.value) = PresetUtils::StringToColor(raw, def);
                break;
            }
        }
    }
}

// PlayerPresetData descriptors

std::vector<OverrideGroupDescriptor> PlayerPresetData::GetOverrideGroups(PlayerPresetData& data) {
    auto& o = data.overrides;
    static thread_local std::vector<OverrideDescriptor> physical, health, physics, movement, combat, skills, state;

    physical = {
        OverrideField("heightRate", o.heightRate),
        OverrideField("muscleRate", o.muscleRate),
        OverrideField("scaleMutationInhibitor", o.scaleMutationInhibitor),
    };
    health = {
        OverrideField("health", o.health),
        OverrideField("headHealth", o.headHealth),
        OverrideField("neckHealth", o.neckHealth),
        OverrideField("armRHealth", o.armRHealth),
        OverrideField("armLHealth", o.armLHealth),
        OverrideField("bodyUpperHealth", o.bodyUpperHealth),
        OverrideField("bodyLowerHealth", o.bodyLowerHealth),
        OverrideField("legRHealth", o.legRHealth),
        OverrideField("legLHealth", o.legLHealth),
        OverrideField("backHealth", o.backHealth),
        OverrideField("consciousness", o.consciousness),
        OverrideField("regenRate", o.regenRate),
    };
    physics = {
        OverrideField("allBodyTonus", o.allBodyTonus),
        OverrideField("headTonus", o.headTonus),
        OverrideField("armRTonus", o.armRTonus),
        OverrideField("armLTonus", o.armLTonus),
        OverrideField("legRTonus", o.legRTonus),
        OverrideField("legLTonus", o.legLTonus),
        OverrideField("musclePower", o.musclePower),
        OverrideField("orientationStrength", o.orientationStrength),
        OverrideField("angularStrength", o.angularStrength),
        OverrideField("hitRigidity", o.hitRigidity),
    };
    movement = {
        OverrideField("runningSpeedRate", o.runningSpeedRate),
        OverrideField("walkSpeedRateRun", o.walkSpeedRateRun),
        OverrideField("jumpRate", o.jumpRate),
        OverrideField("dodgeRate", o.dodgeRate),
        OverrideField("crawlRate", o.crawlRate),
        OverrideField("getUpRate", o.getUpRate),
        OverrideField("fallenRate", o.fallenRate),
    };
    combat = {
        OverrideField("damageRate", o.damageRate),
        OverrideField("limbDamageRate", o.limbDamageRate),
        OverrideField("dismemberThreshold", o.dismemberThreshold),
        OverrideField("stamina", o.stamina),
        OverrideField("staminaBurnSwingR", o.staminaBurnSwingR),
        OverrideField("staminaBurnSwingL", o.staminaBurnSwingL),
        OverrideField("staminaBurnDodge", o.staminaBurnDodge),
        OverrideField("grabForceR", o.grabForceR),
        OverrideField("grabForceL", o.grabForceL),
        OverrideField("handsRigidity", o.handsRigidity),
        OverrideField("bodySkill", o.bodySkill),
        OverrideField("weaponSkill", o.weaponSkill),
    };
    skills = {
        OverrideField("thrust", o.skillThrust),   OverrideField("parry", o.skillParry),
        OverrideField("altGrip", o.skillAltGrip), OverrideField("altStance", o.skillAltStance),
        OverrideField("rotate", o.skillRotate),   OverrideField("crouch", o.skillCrouch),
        OverrideField("dodge", o.skillDodge),     OverrideField("kick", o.skillKick),
        OverrideField("slomo", o.skillSlomo),
    };
    state = {
        OverrideField("exhaustion", o.exhaustion),
        OverrideField("drunk", o.drunk),
        OverrideField("fear", o.fear),
        OverrideField("invulnerable", o.invulnerable),
        OverrideField("fearless", o.fearless),
    };

    return {
        {"Physical", physical}, {"Health", health}, {"Physics", physics}, {"Movement", movement},
        {"Combat", combat},     {"Skills", skills}, {"State", state},
    };
}

// NPCPresetData descriptors

std::vector<PresetFieldDescriptor> NPCPresetData::GetPresetFields(NPCPresetData& data) {
    return {
        PresetField::Int("Generator", "npcType", &data.npcTypeIndex, "0"),
        PresetField::Int("Generator", "nationality", &data.nationality, "0"),
        PresetField::Int("Generator", "tier", &data.tier, "4"),
        PresetField::Bool("Generator", "mercenary", &data.mercenary, "0"),
    };
}

std::vector<OverrideGroupDescriptor> NPCPresetData::GetOverrideGroups(NPCPresetData& data) {
    auto& o = data.overrides;
    static thread_local std::vector<OverrideDescriptor> physical, combat, behavior, body;

    physical = {
        OverrideField("heightRate", o.heightRate),
        OverrideField("muscleRate", o.muscleRate),
        OverrideField("scaleMutationInhibitor", o.scaleMutationInhibitor),
        OverrideField("faceType", o.faceType),
        OverrideField("eyeColor", o.eyeColor),
        OverrideField("hairLength", o.hairLength),
        OverrideField("hairColor", o.hairColor),
    };
    combat = {
        OverrideField("damageRate", o.damageRate),
        OverrideField("limbDamageRate", o.limbDamageRate),
        OverrideField("dismemberThreshold", o.dismemberThreshold),
        OverrideField("regenRate", o.regenRate),
        OverrideField("aiInvincibility", o.aiInvincibility),
        OverrideField("aiArmorInvincibility", o.aiArmorInvincibility),
        OverrideField("bodySkill", o.bodySkill),
    };
    behavior = {
        OverrideField("fearless", o.fearless),
        OverrideField("startKneeled", o.startKneeled),
        OverrideField("spawnInPants", o.spawnInPants),
        OverrideField("clearSpawnArea", o.clearSpawnArea),
        OverrideField("drunk", o.drunk),
        OverrideField("boltsInQuiver", o.boltsInQuiver),
    };
    body = {
        OverrideField("headHealth", o.headHealth),           OverrideField("neckHealth", o.neckHealth),
        OverrideField("armRHealth", o.armRHealth),           OverrideField("armLHealth", o.armLHealth),
        OverrideField("bodyUpperHealth", o.bodyUpperHealth), OverrideField("bodyLowerHealth", o.bodyLowerHealth),
        OverrideField("legRHealth", o.legRHealth),           OverrideField("legLHealth", o.legLHealth),
    };

    return {
        {"Physical", physical},
        {"Combat", combat},
        {"Behavior", behavior},
        {"BodyCondition", body},
    };
}

// WeaponPresetData descriptors

std::vector<PresetFieldDescriptor> WeaponPresetData::GetPresetFields(WeaponPresetData& data) {
    auto& p = data.passport;
    return {
        PresetField::String("Passport", "weaponClass", &data.classPaths.weaponClass),
        PresetField::String("Passport", "headModule", &data.classPaths.headModule),
        PresetField::String("Passport", "guardModule", &data.classPaths.guardModule),
        PresetField::String("Passport", "gripModule", &data.classPaths.gripModule),
        PresetField::String("Passport", "pommelModule", &data.classPaths.pommelModule),
        PresetField::String("Passport", "subModule1", &data.classPaths.subModule1),
        PresetField::String("Passport", "subModule2", &data.classPaths.subModule2),
        PresetField::Vec3("Passport", "headSize", &p.HeadSize_21_2D425E61473B8F64FBAB51B223459D57),
        PresetField::Vec3("Passport", "guardSize", &p.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704),
        PresetField::Vec3("Passport", "gripSize", &p.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF),
        PresetField::Vec3("Passport", "pommelSize", &p.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E),
        PresetField::Double("Passport", "massHead", &p.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7, "1.0"),
        PresetField::Double(
            "Passport", "massGuard", &p.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927, "1.0"
        ),
        PresetField::Double("Passport", "massGrip", &p.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10, "1.0"),
        PresetField::Double(
            "Passport", "massPommel", &p.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173, "1.0"
        ),
        PresetField::Double("Passport", "price", &p.Price_60_83FE5A624EA188485BBE4E9C8606AEE5, "100.0"),
    };
}

std::vector<OverrideGroupDescriptor> WeaponPresetData::GetOverrideGroups(WeaponPresetData& data) {
    auto& rp = data.runtimeProps;
    static thread_local std::vector<OverrideDescriptor> overrides;

    overrides = {
        OverrideField("rigidity", rp.rigidity),
        OverrideField("edgeSharpness", rp.edgeSharpness),
        OverrideField("rawDamage", rp.rawDamage),
        OverrideField("cuttingRate", rp.cuttingRate),
        OverrideField("stabRate", rp.stabRate),
        OverrideField("defRating", rp.defRating),
        OverrideField("gripRate", rp.gripRate),
        OverrideField("drawCutRate", rp.drawCutRate),
        OverrideField("tipSharpness", rp.tipSharpness),
        OverrideField("kickPower", rp.kickPower),
        OverrideField("matDensity", rp.matDensity),
        OverrideField("dismemberSharp", rp.dismemberSharp),
        OverrideField("dismemberBlunt", rp.dismemberBlunt),
        OverrideField("doubleEdged", rp.doubleEdged),
        OverrideField("piercing", rp.piercing),
        OverrideField("noStab", rp.noStab),
        OverrideField("staminaBurnR", rp.staminaBurnR),
        OverrideField("staminaBurnL", rp.staminaBurnL),
        OverrideField("staminaBurn2H", rp.staminaBurn2H),
        OverrideField("staminaBurn2HAlt", rp.staminaBurn2HAlt),
    };

    return {{"Overrides", overrides}};
}

void WeaponPresetData::SerializeCustom(const WeaponPresetData& data, CSimpleIniA& ini) {
    // Material enums and colors (passport fields that need int/enum casting)
    auto& p = data.passport;
    ini.SetValue(
        "Passport", "materialSteel",
        std::to_string(static_cast<int>(p.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36)).c_str()
    );
    ini.SetValue(
        "Passport", "materialColored",
        std::to_string(static_cast<int>(p.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2)).c_str()
    );
    ini.SetValue(
        "Passport", "materialWood",
        std::to_string(static_cast<int>(p.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A)).c_str()
    );
    ini.SetValue(
        "Passport", "materialLeather",
        std::to_string(static_cast<int>(p.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB)).c_str()
    );

    ini.SetValue(
        "Passport", "colorWood", PresetUtils::ColorToString(p.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743).c_str()
    );
    ini.SetValue(
        "Passport", "colorLeather",
        PresetUtils::ColorToString(p.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638).c_str()
    );

    ini.SetValue(
        "Passport", "tier", std::to_string(static_cast<int>(p.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461)).c_str()
    );

    // Mesh overrides
    static constexpr const char* MESH_KEYS[] = {"head", "guard", "grip", "pommel"};
    for (int slot = 0; slot < MODULE_SLOT_COUNT; ++slot) {
        const auto& mp = data.meshPresets[slot];
        char buf[512];
        std::snprintf(
            buf, sizeof(buf), "%d|%s|%d|%s|%s|%s", mp.enabled ? 1 : 0, mp.meshPath.c_str(),
            static_cast<int>(mp.meshType), PresetUtils::VecToString(mp.scale).c_str(),
            PresetUtils::RotToString(mp.rotation).c_str(), PresetUtils::VecToString(mp.offset).c_str()
        );
        ini.SetValue("MeshOverrides", MESH_KEYS[slot], buf);
    }
}

void WeaponPresetData::DeserializeCustom(WeaponPresetData& data, const CSimpleIniA& ini) {
    auto& p = data.passport;

    // Material enums
    p.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36 =
        static_cast<SDK::Enum_MaterialLayer>(std::atoi(ini.GetValue("Passport", "materialSteel", "3")));
    p.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2 =
        static_cast<SDK::Enum_MaterialLayer>(std::atoi(ini.GetValue("Passport", "materialColored", "0")));
    p.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A =
        static_cast<SDK::Enum_MaterialLayer>(std::atoi(ini.GetValue("Passport", "materialWood", "14")));
    p.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB =
        static_cast<SDK::Enum_MaterialLayer>(std::atoi(ini.GetValue("Passport", "materialLeather", "10")));

    // Colors
    static constexpr SDK::FLinearColor DEFAULT_WOOD_COLOR = {0.4f, 0.26f, 0.13f, 1.0f};
    static constexpr SDK::FLinearColor DEFAULT_LEATHER_COLOR = {0.3f, 0.18f, 0.08f, 1.0f};
    p.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 =
        PresetUtils::StringToColor(ini.GetValue("Passport", "colorWood", nullptr), DEFAULT_WOOD_COLOR);
    p.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 =
        PresetUtils::StringToColor(ini.GetValue("Passport", "colorLeather", nullptr), DEFAULT_LEATHER_COLOR);

    // Tier
    p.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461 =
        static_cast<SDK::Enum_Ranks>(std::atoi(ini.GetValue("Passport", "tier", "4")));

    // Mesh overrides
    static constexpr const char* MESH_KEYS[] = {"head", "guard", "grip", "pommel"};
    for (int slot = 0; slot < MODULE_SLOT_COUNT; ++slot) {
        const char* raw = ini.GetValue("MeshOverrides", MESH_KEYS[slot], "");
        if (!raw || !raw[0]) continue;

        std::string val(raw);
        auto& mp = data.meshPresets[slot];

        size_t p1 = val.find('|');
        if (p1 == std::string::npos) continue;
        mp.enabled = (val[0] == '1');

        size_t p2 = val.find('|', p1 + 1);
        if (p2 == std::string::npos) {
            mp.meshPath = val.substr(p1 + 1);
            continue;
        }
        mp.meshPath = val.substr(p1 + 1, p2 - p1 - 1);

        size_t p3 = val.find('|', p2 + 1);
        std::string field3 = (p3 != std::string::npos) ? val.substr(p2 + 1, p3 - p2 - 1) : val.substr(p2 + 1);

        size_t scaleStart;
        if (field3.find(',') == std::string::npos && field3.length() <= 1) {
            mp.meshType = (field3 == "1") ? MeshType::Skeletal : MeshType::Static;
            scaleStart = p3;
        } else {
            mp.meshType = MeshType::Static;
            scaleStart = p2;
        }

        if (scaleStart == std::string::npos) continue;

        size_t pScaleEnd = val.find('|', scaleStart + 1);
        if (pScaleEnd == std::string::npos) {
            mp.scale = PresetUtils::StringToVec(val.c_str() + scaleStart + 1);
            continue;
        }
        mp.scale = PresetUtils::StringToVec(val.c_str() + scaleStart + 1);

        size_t pRotEnd = val.find('|', pScaleEnd + 1);
        if (pRotEnd == std::string::npos) {
            mp.rotation = PresetUtils::StringToRot(val.c_str() + pScaleEnd + 1);
            continue;
        }
        mp.rotation = PresetUtils::StringToRot(val.c_str() + pScaleEnd + 1);
        mp.offset = PresetUtils::StringToVec(val.c_str() + pRotEnd + 1, {0.0, 0.0, 0.0});
    }
}

// ArmorPresetData descriptors

std::vector<PresetFieldDescriptor> ArmorPresetData::GetPresetFields(ArmorPresetData& data) {
    return {
        PresetField::String("Passport", "armorCore", &data.armorCorePath),
    };
}

std::vector<OverrideGroupDescriptor> ArmorPresetData::GetOverrideGroups(ArmorPresetData& data) {
    auto& rp = data.runtimeProps;
    static thread_local std::vector<OverrideDescriptor> overrides;

    overrides = {
        OverrideField("protectionBlunt", rp.protectionBlunt),
        OverrideField("protectionCut", rp.protectionCut),
        OverrideField("protectionStab", rp.protectionStab),
        OverrideField("materialDensity", rp.materialDensity),
        OverrideField("massScale", rp.massScale),
        OverrideField("handsRigidity", rp.handsRigidity),
        OverrideField("strapPower", rp.strapPower),
        OverrideField("aiInvincibilityRate", rp.aiInvincibilityRate),
        OverrideField("price", rp.price),
        OverrideField("pickUp", rp.pickUp),
    };

    return {{"Overrides", overrides}};
}

void ArmorPresetData::SerializeCustom(const ArmorPresetData& data, CSimpleIniA& ini) {
    const auto& p = data.passport;

    ini.SetValue("Passport", "id", std::to_string(p.ID_54_C6BBB1A64A3828B5AB1D8E804EC7C8F7).c_str());
    ini.SetValue("Passport", "coreRemoved", p.CoreRemoved_12_5CFF8F6D4A05C15812594CAF6771C66B ? "1" : "0");
    ini.SetValue("Passport", "module1", std::to_string(p.Module1_5_46B7198E4341C93CBF6AE989EF9898E4).c_str());
    ini.SetValue("Passport", "module2", std::to_string(p.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0).c_str());
    ini.SetValue("Passport", "module3", std::to_string(p.Module3_9_E282C465414F6D4EF2A8039FBA847AD2).c_str());
    ini.SetValue(
        "Passport", "fabricColor1",
        PresetUtils::ColorToString(p.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393).c_str()
    );
    ini.SetValue(
        "Passport", "fabricColor2",
        PresetUtils::ColorToString(p.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C).c_str()
    );
    ini.SetValue("Passport", "price", std::to_string(p.Price_27_8E3ADD54484EFC4A59FE9381485AC192).c_str());
    ini.SetValue(
        "Passport", "slot", std::to_string(static_cast<int>(p.Slot_30_7561CB484566A4512003EA96ED44F88D)).c_str()
    );
    ini.SetValue("Passport", "providesUpperAP", p.ProvidesUpperAP_34_A85C3E3B4E4EF35DA44FFA960797B6C6 ? "1" : "0");
    ini.SetValue("Passport", "providesLowerAP", p.ProvidesLowerAP_36_FFA5916240E32AC30239D58BCDD69D62 ? "1" : "0");
    ini.SetValue("Passport", "requiresUpperAP", p.RequiresUpperAP_38_079BBCD74D92FB832584E8B776EC8A6E ? "1" : "0");
    ini.SetValue("Passport", "requiresLowerAP", p.RequiresLowerAP_40_BF13845C4B210380A7A569A912A6F614 ? "1" : "0");
    ini.SetValue(
        "Passport", "requiresModuleHierarchy", p.RequiresModuleHirarchy_47_9ED58E2C48514BE5153606977BE68B6A ? "1" : "0"
    );
    ini.SetValue(
        "Passport", "tier", std::to_string(static_cast<int>(p.Tier_50_E497AE434B01B84C559DEE8A863BB42E)).c_str()
    );
}

void ArmorPresetData::DeserializeCustom(ArmorPresetData& data, const CSimpleIniA& ini) {
    auto& p = data.passport;

    p.ID_54_C6BBB1A64A3828B5AB1D8E804EC7C8F7 = std::atoi(ini.GetValue("Passport", "id", "0"));
    p.CoreRemoved_12_5CFF8F6D4A05C15812594CAF6771C66B = std::atoi(ini.GetValue("Passport", "coreRemoved", "0")) != 0;
    p.Module1_5_46B7198E4341C93CBF6AE989EF9898E4 = std::atoi(ini.GetValue("Passport", "module1", "0"));
    p.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0 = std::atoi(ini.GetValue("Passport", "module2", "0"));
    p.Module3_9_E282C465414F6D4EF2A8039FBA847AD2 = std::atoi(ini.GetValue("Passport", "module3", "0"));

    static constexpr SDK::FLinearColor DEFAULT_FABRIC_COLOR = {0.5f, 0.5f, 0.5f, 1.0f};
    p.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393 =
        PresetUtils::StringToColor(ini.GetValue("Passport", "fabricColor1", nullptr), DEFAULT_FABRIC_COLOR);
    p.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C =
        PresetUtils::StringToColor(ini.GetValue("Passport", "fabricColor2", nullptr), DEFAULT_FABRIC_COLOR);

    p.Price_27_8E3ADD54484EFC4A59FE9381485AC192 = std::atof(ini.GetValue("Passport", "price", "50.0"));
    p.Slot_30_7561CB484566A4512003EA96ED44F88D =
        static_cast<SDK::EArmorSlots_Enum>(std::atoi(ini.GetValue("Passport", "slot", "0")));
    p.ProvidesUpperAP_34_A85C3E3B4E4EF35DA44FFA960797B6C6 =
        std::atoi(ini.GetValue("Passport", "providesUpperAP", "0")) != 0;
    p.ProvidesLowerAP_36_FFA5916240E32AC30239D58BCDD69D62 =
        std::atoi(ini.GetValue("Passport", "providesLowerAP", "0")) != 0;
    p.RequiresUpperAP_38_079BBCD74D92FB832584E8B776EC8A6E =
        std::atoi(ini.GetValue("Passport", "requiresUpperAP", "0")) != 0;
    p.RequiresLowerAP_40_BF13845C4B210380A7A569A912A6F614 =
        std::atoi(ini.GetValue("Passport", "requiresLowerAP", "0")) != 0;
    p.RequiresModuleHirarchy_47_9ED58E2C48514BE5153606977BE68B6A =
        std::atoi(ini.GetValue("Passport", "requiresModuleHierarchy", "0")) != 0;
    p.Tier_50_E497AE434B01B84C559DEE8A863BB42E =
        static_cast<SDK::Enum_Ranks>(std::atoi(ini.GetValue("Passport", "tier", "4")));
}

// LoadoutPresetData descriptors + utilities

SDK::FStr_WeaponParts& LoadoutPresetData::GetWeaponSlot(SDK::FStr_Loadout_Weapons& weapons, int index) {
    switch (index) {
        case 0: return weapons.WeaponHandR_2_64D3388F445655CA2E9E60B639016D17;
        case 1: return weapons.WeaponHandL_4_4BF5616F480598D39F54058D5181EB86;
        case 2: return weapons.WeaponSlotR1_6_140F311C4B659EE501761B8D99781B20;
        case 3: return weapons.WeaponSlotR2_8_8B0CA70A4477398EB3B1E58EBB1AD2DC;
        case 4: return weapons.WeaponSlotL1_10_908E8A984A1C041B0CC6238D804CEB60;
        case 5: return weapons.WeaponSlotL2_12_EF7AA9044E150C11545E349E5AD7C2E0;
        case 6: return weapons.WeaponBack_14_2CBE21CA47095EF150DD5791D72AC8C9;
        default: return weapons.WeaponHandR_2_64D3388F445655CA2E9E60B639016D17;
    }
}

void LoadoutPresetData::ReadWeaponSlot(const SDK::FStr_WeaponParts& wp, WeaponSlotData& out) {
    out.weaponClass = PresetUtils::ObjectToAbsolutePath(wp.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066);
    out.gripModule = PresetUtils::ObjectToAbsolutePath(wp.GripModule_38_15B14C3F4E9701389A9B35A3B0909867);
    out.headModule = PresetUtils::ObjectToAbsolutePath(wp.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F);
    out.guardModule = PresetUtils::ObjectToAbsolutePath(wp.GuardModule_21_774015784EB0300D2671C894D57ED144);
    out.pommelModule = PresetUtils::ObjectToAbsolutePath(wp.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984);
    out.subModule1 = PresetUtils::ObjectToAbsolutePath(wp.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0);
    out.subModule2 = PresetUtils::ObjectToAbsolutePath(wp.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980);
    out.headSize = wp.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA;
    out.guardSize = wp.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D;
    out.pommelSize = wp.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524;
    out.coaInt = wp.COAInt_63_593665BE4EF020F95F7D1A92564C1239;
}

LoadoutPresetData LoadoutPresetData::ReadFromEquipment(const SDK::FStr_Loadout_Equipment& equip) {
    LoadoutPresetData data;
    data.success = true;

    auto& armorMap = equip.Armor_84_A1BA4DD44FD262BCA53B9DACF03CDF04.ArmorinSlots_31_702A9C5C40C7F4335C6B4687EC09936A;
    for (auto it = begin(armorMap); it != end(armorMap); ++it) {
        auto& elem = it->Value();
        if (!elem.ArmorBPClass_2_0A22459840BF9E6989DFA4BA6CFED1D3) continue;

        ArmorSlotData slotData;
        slotData.slot = it->Key();
        slotData.armorClass = PresetUtils::ObjectToAbsolutePath(elem.ArmorBPClass_2_0A22459840BF9E6989DFA4BA6CFED1D3);
        slotData.color1 = elem.Color1_5_5527FC7C442DCF594A4DA5BA8D94351F;
        slotData.color2 = elem.Color2_7_1FF790D94C8CD95FF2D76183E7102E1B;
        slotData.color3 = elem.Color3_9_D8B5A08742A87F5492F8138A4F686141;
        data.armorSlots.push_back(std::move(slotData));
    }

    for (int i = 0; i < 7; ++i) {
        const auto& slot =
            GetWeaponSlot(const_cast<SDK::FStr_Loadout_Weapons&>(equip.Weapons_83_06F076E247B54D0D9942B383323C1968), i);
        ReadWeaponSlot(slot, data.weaponSlots[i]);
    }

    return data;
}

static constexpr const char* WEAPON_SLOT_KEYS[] = {"HandR", "HandL", "SlotR1", "SlotR2", "SlotL1", "SlotL2", "Back"};

static constexpr const char* ARMOR_SLOT_NAMES[] = {"Head",          "Hands",        "Neck_Bevor", "Neck_Gorget",
                                                   "Neck_Standard", "Arms",         "Shoulders",  "Tabard",
                                                   "Chest_Plate",   "Hauberk",      "Cuisses",    "Body_Clothing",
                                                   "Waist",         "Legs_Greaves", "Feet",       "Hosen",
                                                   "Slot16"};
static constexpr int ARMOR_SLOT_NAME_COUNT = 17;

static const char* ArmorSlotToKey(SDK::EArmorSlots_Enum slot) {
    int idx = static_cast<int>(slot);
    if (idx >= 0 && idx < ARMOR_SLOT_NAME_COUNT) return ARMOR_SLOT_NAMES[idx];
    return "Unknown";
}

void LoadoutPresetData::SerializeCustom(const LoadoutPresetData& data, CSimpleIniA& ini) {
    for (size_t i = 0; i < data.armorSlots.size(); ++i) {
        const auto& slot = data.armorSlots[i];
        char section[64];
        std::snprintf(section, sizeof(section), "Armor.%s", ArmorSlotToKey(slot.slot));
        ini.SetValue(section, "class", slot.armorClass.c_str());
        ini.SetValue(section, "color1", PresetUtils::ColorToString(slot.color1).c_str());
        ini.SetValue(section, "color2", PresetUtils::ColorToString(slot.color2).c_str());
        ini.SetValue(section, "color3", PresetUtils::ColorToString(slot.color3).c_str());
    }

    for (int i = 0; i < 7; ++i) {
        const auto& wp = data.weaponSlots[i];
        if (wp.weaponClass.empty()) continue;

        char section[64];
        std::snprintf(section, sizeof(section), "Weapon.%s", WEAPON_SLOT_KEYS[i]);
        ini.SetValue(section, "class", wp.weaponClass.c_str());
        if (!wp.gripModule.empty()) ini.SetValue(section, "gripModule", wp.gripModule.c_str());
        if (!wp.headModule.empty()) ini.SetValue(section, "headModule", wp.headModule.c_str());
        if (!wp.guardModule.empty()) ini.SetValue(section, "guardModule", wp.guardModule.c_str());
        if (!wp.pommelModule.empty()) ini.SetValue(section, "pommelModule", wp.pommelModule.c_str());
        if (!wp.subModule1.empty()) ini.SetValue(section, "subModule1", wp.subModule1.c_str());
        if (!wp.subModule2.empty()) ini.SetValue(section, "subModule2", wp.subModule2.c_str());
        ini.SetValue(section, "headSize", PresetUtils::VecToString(wp.headSize).c_str());
        ini.SetValue(section, "guardSize", PresetUtils::VecToString(wp.guardSize).c_str());
        ini.SetValue(section, "pommelSize", PresetUtils::VecToString(wp.pommelSize).c_str());
        if (wp.coaInt != 0) ini.SetValue(section, "coaInt", std::to_string(wp.coaInt).c_str());
    }
}

void LoadoutPresetData::DeserializeCustom(LoadoutPresetData& data, const CSimpleIniA& ini) {
    for (int slotIdx = 0; slotIdx < ARMOR_SLOT_NAME_COUNT; ++slotIdx) {
        char section[64];
        std::snprintf(section, sizeof(section), "Armor.%s", ARMOR_SLOT_NAMES[slotIdx]);
        const char* cls = ini.GetValue(section, "class", nullptr);
        if (!cls || !cls[0]) continue;

        ArmorSlotData slotData;
        slotData.slot = static_cast<SDK::EArmorSlots_Enum>(slotIdx);
        slotData.armorClass = cls;
        slotData.color1 = PresetUtils::StringToColor(ini.GetValue(section, "color1", ""), {0.5f, 0.5f, 0.5f, 1.0f});
        slotData.color2 = PresetUtils::StringToColor(ini.GetValue(section, "color2", ""), {0.5f, 0.5f, 0.5f, 1.0f});
        slotData.color3 = PresetUtils::StringToColor(ini.GetValue(section, "color3", ""), {0.5f, 0.5f, 0.5f, 1.0f});
        data.armorSlots.push_back(std::move(slotData));
    }

    for (int i = 0; i < 7; ++i) {
        char section[64];
        std::snprintf(section, sizeof(section), "Weapon.%s", WEAPON_SLOT_KEYS[i]);
        const char* cls = ini.GetValue(section, "class", nullptr);
        if (!cls || !cls[0]) continue;

        auto& wp = data.weaponSlots[i];
        wp.weaponClass = cls;
        wp.gripModule = ini.GetValue(section, "gripModule", "");
        wp.headModule = ini.GetValue(section, "headModule", "");
        wp.guardModule = ini.GetValue(section, "guardModule", "");
        wp.pommelModule = ini.GetValue(section, "pommelModule", "");
        wp.subModule1 = ini.GetValue(section, "subModule1", "");
        wp.subModule2 = ini.GetValue(section, "subModule2", "");
        wp.headSize = PresetUtils::StringToVec(ini.GetValue(section, "headSize", ""));
        wp.guardSize = PresetUtils::StringToVec(ini.GetValue(section, "guardSize", ""));
        wp.pommelSize = PresetUtils::StringToVec(ini.GetValue(section, "pommelSize", ""));
        wp.coaInt = std::atoi(ini.GetValue(section, "coaInt", "0"));
    }
}
