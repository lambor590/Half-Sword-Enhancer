#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstring>

#include "Utils/PresetUtils.h"
#include "Utils/GuiUtils.h"
#include "SDK/Str_Passport_Weapon1_structs.hpp"

struct WeaponPresetData {
    SDK::FStr_Passport_Weapon1 passport{};

    struct {
        RuntimeOverride rigidity, edgeSharpness, rawDamage, cuttingRate, stabRate;
        RuntimeOverride defRating, gripRate, drawCutRate, tipSharpness, kickPower, matDensity;
        IntOverride dismemberSharp, dismemberBlunt;
        BoolOverride doubleEdged, piercing, noStab;
        RuntimeOverride staminaBurnR, staminaBurnL, staminaBurn2H, staminaBurn2HAlt;
    } runtimeProps{};

    std::string name;
    bool success = false;
    std::string error;
};

class WeaponPresetSerializer {
private:
    static constexpr const char* CLIPBOARD_PREFIX = "HSE:";

    static bool IsDefaultVec(const SDK::FVector& v) {
        return std::abs(v.X - 1.0) < 1e-4 && std::abs(v.Y - 1.0) < 1e-4 && std::abs(v.Z - 1.0) < 1e-4;
    }

    static bool IsDefaultMass(double m) { return std::abs(m - 1.0) < 1e-4; }

    static bool IsDefaultColor(const SDK::FLinearColor& c, const SDK::FLinearColor& def) {
        return std::abs(c.R - def.R) < 1e-3f && std::abs(c.G - def.G) < 1e-3f
            && std::abs(c.B - def.B) < 1e-3f && std::abs(c.A - def.A) < 1e-3f;
    }

    static constexpr SDK::FLinearColor DEFAULT_WOOD_COLOR = {0.4f, 0.26f, 0.13f, 1.0f};
    static constexpr SDK::FLinearColor DEFAULT_LEATHER_COLOR = {0.3f, 0.18f, 0.08f, 1.0f};

public:
    static std::string SerializeToIni(const SDK::FStr_Passport_Weapon1& passport,
        const WeaponPresetData& data, bool minimalMode = false)
    {
        CSimpleIniA ini;
        ini.SetUnicode(false);

        ini.SetValue("Preset", "name", data.name.c_str());
        ini.SetValue("Preset", "version", "1");

        auto setIfNotDefault = [&](const char* section, const char* key, const std::string& val, const char* def) {
            if (!minimalMode || val != def)
                ini.SetValue(section, key, val.c_str());
        };

        setIfNotDefault("Passport", "weaponClass", PresetUtils::ClassToString(passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC), "");
        setIfNotDefault("Passport", "headModule", PresetUtils::ClassToString(passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139), "");
        setIfNotDefault("Passport", "guardModule", PresetUtils::ClassToString(passport.GuardModule_13_6DD2B06245505E53B529D090333012F0), "");
        setIfNotDefault("Passport", "gripModule", PresetUtils::ClassToString(passport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4), "");
        setIfNotDefault("Passport", "pommelModule", PresetUtils::ClassToString(passport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6), "");
        setIfNotDefault("Passport", "subModule1", PresetUtils::ClassToString(passport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D), "");
        setIfNotDefault("Passport", "subModule2", PresetUtils::ClassToString(passport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9), "");

        if (!minimalMode || !IsDefaultVec(passport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57))
            ini.SetValue("Passport", "headSize", PresetUtils::VecToString(passport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57).c_str());
        if (!minimalMode || !IsDefaultVec(passport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704))
            ini.SetValue("Passport", "guardSize", PresetUtils::VecToString(passport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704).c_str());
        if (!minimalMode || !IsDefaultVec(passport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF))
            ini.SetValue("Passport", "gripSize", PresetUtils::VecToString(passport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF).c_str());
        if (!minimalMode || !IsDefaultVec(passport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E))
            ini.SetValue("Passport", "pommelSize", PresetUtils::VecToString(passport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E).c_str());

        if (!minimalMode || !IsDefaultMass(passport.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7))
            ini.SetValue("Passport", "massHead", std::to_string(passport.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7).c_str());
        if (!minimalMode || !IsDefaultMass(passport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927))
            ini.SetValue("Passport", "massGuard", std::to_string(passport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927).c_str());
        if (!minimalMode || !IsDefaultMass(passport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10))
            ini.SetValue("Passport", "massGrip", std::to_string(passport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10).c_str());
        if (!minimalMode || !IsDefaultMass(passport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173))
            ini.SetValue("Passport", "massPommel", std::to_string(passport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173).c_str());

        int steel = static_cast<int>(passport.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36);
        int colored = static_cast<int>(passport.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2);
        int wood = static_cast<int>(passport.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A);
        int leather = static_cast<int>(passport.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB);

        if (!minimalMode || steel != 3)   ini.SetValue("Passport", "materialSteel", std::to_string(steel).c_str());
        if (!minimalMode || colored != 0)  ini.SetValue("Passport", "materialColored", std::to_string(colored).c_str());
        if (!minimalMode || wood != 14)    ini.SetValue("Passport", "materialWood", std::to_string(wood).c_str());
        if (!minimalMode || leather != 10) ini.SetValue("Passport", "materialLeather", std::to_string(leather).c_str());

        if (!minimalMode || !IsDefaultColor(passport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743, DEFAULT_WOOD_COLOR))
            ini.SetValue("Passport", "colorWood", PresetUtils::ColorToString(passport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743).c_str());
        if (!minimalMode || !IsDefaultColor(passport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638, DEFAULT_LEATHER_COLOR))
            ini.SetValue("Passport", "colorLeather", PresetUtils::ColorToString(passport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638).c_str());

        int tier = static_cast<int>(passport.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461);
        if (!minimalMode || tier != 4)
            ini.SetValue("Passport", "tier", std::to_string(tier).c_str());
        if (!minimalMode || std::abs(passport.Price_60_83FE5A624EA188485BBE4E9C8606AEE5 - 100.0) > 0.01)
            ini.SetValue("Passport", "price", std::to_string(passport.Price_60_83FE5A624EA188485BBE4E9C8606AEE5).c_str());

        const auto& rp = data.runtimeProps;
        auto setOvr = [&](const char* key, bool enabled, double val) {
            if (!minimalMode || enabled)
                ini.SetValue("Overrides", key, PresetUtils::DoubleOverrideToString(enabled, val).c_str());
        };
        auto setOvrInt = [&](const char* key, bool enabled, int val) {
            if (!minimalMode || enabled)
                ini.SetValue("Overrides", key, PresetUtils::IntOverrideToString(enabled, val).c_str());
        };
        auto setOvrBool = [&](const char* key, bool enabled, bool val) {
            if (!minimalMode || enabled)
                ini.SetValue("Overrides", key, PresetUtils::IntOverrideToString(enabled, val ? 1 : 0).c_str());
        };

        setOvr("rigidity", rp.rigidity.enabled, rp.rigidity.value);
        setOvr("edgeSharpness", rp.edgeSharpness.enabled, rp.edgeSharpness.value);
        setOvr("rawDamage", rp.rawDamage.enabled, rp.rawDamage.value);
        setOvr("cuttingRate", rp.cuttingRate.enabled, rp.cuttingRate.value);
        setOvr("stabRate", rp.stabRate.enabled, rp.stabRate.value);
        setOvr("defRating", rp.defRating.enabled, rp.defRating.value);
        setOvr("gripRate", rp.gripRate.enabled, rp.gripRate.value);
        setOvr("drawCutRate", rp.drawCutRate.enabled, rp.drawCutRate.value);
        setOvr("tipSharpness", rp.tipSharpness.enabled, rp.tipSharpness.value);
        setOvr("kickPower", rp.kickPower.enabled, rp.kickPower.value);
        setOvr("matDensity", rp.matDensity.enabled, rp.matDensity.value);
        setOvrInt("dismemberSharp", rp.dismemberSharp.enabled, rp.dismemberSharp.value);
        setOvrInt("dismemberBlunt", rp.dismemberBlunt.enabled, rp.dismemberBlunt.value);
        setOvrBool("doubleEdged", rp.doubleEdged.enabled, rp.doubleEdged.value);
        setOvrBool("piercing", rp.piercing.enabled, rp.piercing.value);
        setOvrBool("noStab", rp.noStab.enabled, rp.noStab.value);
        setOvr("staminaBurnR", rp.staminaBurnR.enabled, rp.staminaBurnR.value);
        setOvr("staminaBurnL", rp.staminaBurnL.enabled, rp.staminaBurnL.value);
        setOvr("staminaBurn2H", rp.staminaBurn2H.enabled, rp.staminaBurn2H.value);
        setOvr("staminaBurn2HAlt", rp.staminaBurn2HAlt.enabled, rp.staminaBurn2HAlt.value);

        std::string output;
        ini.Save(output);
        return output;
    }

    static WeaponPresetData DeserializeFromIni(const std::string& iniContent) {
        WeaponPresetData result;
        CSimpleIniA ini;
        ini.SetUnicode(false);

        if (ini.LoadData(iniContent) < 0) {
            result.error = "Failed to parse INI data";
            return result;
        }

        const char* ver = ini.GetValue("Preset", "version", "0");
        if (std::strcmp(ver, "1") != 0) {
            result.error = "Unsupported preset version: " + std::string(ver);
            return result;
        }

        result.name = ini.GetValue("Preset", "name", "Unnamed");

        auto& p = result.passport;
        p = {};

        p.WeaponClass_54_B478ECF7499977809745A3973AD678EC = PresetUtils::StringToClass(ini.GetValue("Passport", "weaponClass", ""));
        p.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 = PresetUtils::StringToClass(ini.GetValue("Passport", "headModule", ""));
        p.GuardModule_13_6DD2B06245505E53B529D090333012F0 = PresetUtils::StringToClass(ini.GetValue("Passport", "guardModule", ""));
        p.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4 = PresetUtils::StringToClass(ini.GetValue("Passport", "gripModule", ""));
        p.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6 = PresetUtils::StringToClass(ini.GetValue("Passport", "pommelModule", ""));
        p.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D = PresetUtils::StringToClass(ini.GetValue("Passport", "subModule1", ""));
        p.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9 = PresetUtils::StringToClass(ini.GetValue("Passport", "subModule2", ""));

        p.HeadSize_21_2D425E61473B8F64FBAB51B223459D57 = PresetUtils::StringToVec(ini.GetValue("Passport", "headSize", nullptr));
        p.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 = PresetUtils::StringToVec(ini.GetValue("Passport", "guardSize", nullptr));
        p.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF = PresetUtils::StringToVec(ini.GetValue("Passport", "gripSize", nullptr));
        p.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E = PresetUtils::StringToVec(ini.GetValue("Passport", "pommelSize", nullptr));

        p.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7 = std::atof(ini.GetValue("Passport", "massHead", "1.0"));
        p.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927 = std::atof(ini.GetValue("Passport", "massGuard", "1.0"));
        p.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10 = std::atof(ini.GetValue("Passport", "massGrip", "1.0"));
        p.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173 = std::atof(ini.GetValue("Passport", "massPommel", "1.0"));

        p.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36 = static_cast<SDK::Enum_MaterialLayer>(std::atoi(ini.GetValue("Passport", "materialSteel", "3")));
        p.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2 = static_cast<SDK::Enum_MaterialLayer>(std::atoi(ini.GetValue("Passport", "materialColored", "0")));
        p.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A = static_cast<SDK::Enum_MaterialLayer>(std::atoi(ini.GetValue("Passport", "materialWood", "14")));
        p.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB = static_cast<SDK::Enum_MaterialLayer>(std::atoi(ini.GetValue("Passport", "materialLeather", "10")));

        p.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 = PresetUtils::StringToColor(ini.GetValue("Passport", "colorWood", nullptr), DEFAULT_WOOD_COLOR);
        p.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 = PresetUtils::StringToColor(ini.GetValue("Passport", "colorLeather", nullptr), DEFAULT_LEATHER_COLOR);

        p.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461 = static_cast<SDK::Enum_Ranks>(std::atoi(ini.GetValue("Passport", "tier", "4")));
        p.Price_60_83FE5A624EA188485BBE4E9C8606AEE5 = std::atof(ini.GetValue("Passport", "price", "100.0"));

        auto& rp = result.runtimeProps;
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "rigidity", ""), rp.rigidity.enabled, rp.rigidity.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "edgeSharpness", ""), rp.edgeSharpness.enabled, rp.edgeSharpness.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "rawDamage", ""), rp.rawDamage.enabled, rp.rawDamage.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "cuttingRate", ""), rp.cuttingRate.enabled, rp.cuttingRate.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "stabRate", ""), rp.stabRate.enabled, rp.stabRate.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "defRating", ""), rp.defRating.enabled, rp.defRating.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "gripRate", ""), rp.gripRate.enabled, rp.gripRate.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "drawCutRate", ""), rp.drawCutRate.enabled, rp.drawCutRate.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "tipSharpness", ""), rp.tipSharpness.enabled, rp.tipSharpness.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "kickPower", ""), rp.kickPower.enabled, rp.kickPower.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "matDensity", ""), rp.matDensity.enabled, rp.matDensity.value);
        PresetUtils::ParseIntOverride(ini.GetValue("Overrides", "dismemberSharp", ""), rp.dismemberSharp.enabled, rp.dismemberSharp.value);
        PresetUtils::ParseIntOverride(ini.GetValue("Overrides", "dismemberBlunt", ""), rp.dismemberBlunt.enabled, rp.dismemberBlunt.value);
        PresetUtils::ParseBoolOverride(ini.GetValue("Overrides", "doubleEdged", ""), rp.doubleEdged.enabled, rp.doubleEdged.value);
        PresetUtils::ParseBoolOverride(ini.GetValue("Overrides", "piercing", ""), rp.piercing.enabled, rp.piercing.value);
        PresetUtils::ParseBoolOverride(ini.GetValue("Overrides", "noStab", ""), rp.noStab.enabled, rp.noStab.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "staminaBurnR", ""), rp.staminaBurnR.enabled, rp.staminaBurnR.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "staminaBurnL", ""), rp.staminaBurnL.enabled, rp.staminaBurnL.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "staminaBurn2H", ""), rp.staminaBurn2H.enabled, rp.staminaBurn2H.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "staminaBurn2HAlt", ""), rp.staminaBurn2HAlt.enabled, rp.staminaBurn2HAlt.value);

        result.success = true;
        return result;
    }

    static std::string EncodeForClipboard(const SDK::FStr_Passport_Weapon1& passport,
        const WeaponPresetData& data)
    {
        std::string ini = SerializeToIni(passport, data, true);
        return std::string(CLIPBOARD_PREFIX) + PresetUtils::Base64Encode(ini);
    }

    static WeaponPresetData DecodeFromClipboard(const std::string& clipboardText) {
        WeaponPresetData result;
        static constexpr size_t PREFIX_LEN = 4;
        if (clipboardText.size() < PREFIX_LEN || std::memcmp(clipboardText.data(), CLIPBOARD_PREFIX, PREFIX_LEN) != 0) {
            result.error = "Invalid clipboard data (missing HSE: prefix)";
            return result;
        }
        std::string decoded = PresetUtils::Base64Decode(
            clipboardText.data() + PREFIX_LEN, clipboardText.size() - PREFIX_LEN);
        if (decoded.empty()) {
            result.error = "Failed to decode base64 data";
            return result;
        }
        return DeserializeFromIni(decoded);
    }

    static std::filesystem::path GetPresetsDirectory() {
        return PresetUtils::EnsureDirectory(ConfigManager::GetAppDataPath() / "weapon_presets");
    }

    static bool SaveToFile(const std::filesystem::path& path,
        const SDK::FStr_Passport_Weapon1& passport, const WeaponPresetData& data)
    {
        return PresetUtils::SaveStringToFile(path, SerializeToIni(passport, data, false));
    }

    static WeaponPresetData LoadFromFile(const std::filesystem::path& path) {
        WeaponPresetData result;
        std::string content = PresetUtils::LoadStringFromFile(path);
        if (content.empty()) {
            result.error = "Cannot open file: " + path.string();
            return result;
        }
        return DeserializeFromIni(content);
    }

    static std::vector<PresetListEntry> ListPresets() {
        return PresetUtils::ListPresetsInDir(GetPresetsDirectory());
    }

    static bool DeletePreset(const std::filesystem::path& path) {
        return PresetUtils::DeletePreset(path);
    }

    static bool SavePresetByName(const std::string& name,
        const SDK::FStr_Passport_Weapon1& passport, const WeaponPresetData& data)
    {
        auto dir = GetPresetsDirectory();
        auto filename = PresetUtils::SanitizeFilename(name) + ".ini";
        return SaveToFile(dir / filename, passport, data);
    }
};
