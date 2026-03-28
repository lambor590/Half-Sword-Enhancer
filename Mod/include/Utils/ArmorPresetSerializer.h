#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstring>

#include "Utils/PresetUtils.h"
#include "Utils/PresetDataBase.h"
#include "Utils/PresetSerializerBase.h"
#include "Utils/OverrideTypes.h"
#include "SDK/Str_Passport_Armor1_structs.hpp"
#include "SDK/BP_Armor_Master_classes.hpp"

struct ArmorPresetData : PresetDataBase {
    SDK::FStr_Passport_Armor1 passport{};

    struct {
        RuntimeOverride protectionBlunt, protectionCut, protectionStab;
        RuntimeOverride materialDensity, massScale;
        RuntimeOverride handsRigidity, strapPower, aiInvincibilityRate, price;
        BoolOverride pickUp;
    } runtimeProps{};

    std::string armorCorePath;
};

class ArmorPresetSerializer : public PresetSerializerBase<ArmorPresetSerializer, ArmorPresetData> {
private:
    static constexpr SDK::FLinearColor DEFAULT_FABRIC_COLOR = {0.5f, 0.5f, 0.5f, 1.0f};

public:
    static constexpr const char* kPresetsSubdir = "armor_presets";

    static std::string SerializeToIni(const ArmorPresetData& data, bool minimalMode = false) {
        const auto& passport = data.passport;
        CSimpleIniA ini;
        ini.SetUnicode(false);

        ini.SetValue("Preset", "name", data.name.c_str());
        ini.SetValue("Preset", "version", "1");

        auto armorCorePath = PresetUtils::ObjectToAbsolutePath(passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43);
        if (!minimalMode || !armorCorePath.empty()) ini.SetValue("Passport", "armorCore", armorCorePath.c_str());

        if (!minimalMode || passport.ID_54_C6BBB1A64A3828B5AB1D8E804EC7C8F7 != 0)
            ini.SetValue("Passport", "id", std::to_string(passport.ID_54_C6BBB1A64A3828B5AB1D8E804EC7C8F7).c_str());
        if (!minimalMode || passport.CoreRemoved_12_5CFF8F6D4A05C15812594CAF6771C66B)
            ini.SetValue(
                "Passport", "coreRemoved", passport.CoreRemoved_12_5CFF8F6D4A05C15812594CAF6771C66B ? "1" : "0"
            );

        if (!minimalMode || passport.Module1_5_46B7198E4341C93CBF6AE989EF9898E4 != 0)
            ini.SetValue(
                "Passport", "module1", std::to_string(passport.Module1_5_46B7198E4341C93CBF6AE989EF9898E4).c_str()
            );
        if (!minimalMode || passport.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0 != 0)
            ini.SetValue(
                "Passport", "module2", std::to_string(passport.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0).c_str()
            );
        if (!minimalMode || passport.Module3_9_E282C465414F6D4EF2A8039FBA847AD2 != 0)
            ini.SetValue(
                "Passport", "module3", std::to_string(passport.Module3_9_E282C465414F6D4EF2A8039FBA847AD2).c_str()
            );

        if (!minimalMode || !PresetUtils::IsDefaultColor(
                                passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393, DEFAULT_FABRIC_COLOR
                            ))
            ini.SetValue(
                "Passport", "fabricColor1",
                PresetUtils::ColorToString(passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393).c_str()
            );
        if (!minimalMode || !PresetUtils::IsDefaultColor(
                                passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C, DEFAULT_FABRIC_COLOR
                            ))
            ini.SetValue(
                "Passport", "fabricColor2",
                PresetUtils::ColorToString(passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C).c_str()
            );

        if (!minimalMode || std::abs(passport.Price_27_8E3ADD54484EFC4A59FE9381485AC192 - 50.0) > 0.01)
            ini.SetValue(
                "Passport", "price", std::to_string(passport.Price_27_8E3ADD54484EFC4A59FE9381485AC192).c_str()
            );

        int slot = static_cast<int>(passport.Slot_30_7561CB484566A4512003EA96ED44F88D);
        if (!minimalMode || slot != 0) ini.SetValue("Passport", "slot", std::to_string(slot).c_str());

        if (!minimalMode || passport.ProvidesUpperAP_34_A85C3E3B4E4EF35DA44FFA960797B6C6)
            ini.SetValue(
                "Passport", "providesUpperAP", passport.ProvidesUpperAP_34_A85C3E3B4E4EF35DA44FFA960797B6C6 ? "1" : "0"
            );
        if (!minimalMode || passport.ProvidesLowerAP_36_FFA5916240E32AC30239D58BCDD69D62)
            ini.SetValue(
                "Passport", "providesLowerAP", passport.ProvidesLowerAP_36_FFA5916240E32AC30239D58BCDD69D62 ? "1" : "0"
            );
        if (!minimalMode || passport.RequiresUpperAP_38_079BBCD74D92FB832584E8B776EC8A6E)
            ini.SetValue(
                "Passport", "requiresUpperAP", passport.RequiresUpperAP_38_079BBCD74D92FB832584E8B776EC8A6E ? "1" : "0"
            );
        if (!minimalMode || passport.RequiresLowerAP_40_BF13845C4B210380A7A569A912A6F614)
            ini.SetValue(
                "Passport", "requiresLowerAP", passport.RequiresLowerAP_40_BF13845C4B210380A7A569A912A6F614 ? "1" : "0"
            );
        if (!minimalMode || passport.RequiresModuleHirarchy_47_9ED58E2C48514BE5153606977BE68B6A)
            ini.SetValue(
                "Passport", "requiresModuleHierarchy",
                passport.RequiresModuleHirarchy_47_9ED58E2C48514BE5153606977BE68B6A ? "1" : "0"
            );

        int tier = static_cast<int>(passport.Tier_50_E497AE434B01B84C559DEE8A863BB42E);
        if (!minimalMode || tier != 4) ini.SetValue("Passport", "tier", std::to_string(tier).c_str());

        const auto& rp = data.runtimeProps;
        auto setOvr = [&](const char* key, bool enabled, double val) {
            if (!minimalMode || enabled)
                ini.SetValue("Overrides", key, PresetUtils::DoubleOverrideToString(enabled, val).c_str());
        };
        auto setOvrBool = [&](const char* key, bool enabled, bool val) {
            if (!minimalMode || enabled)
                ini.SetValue("Overrides", key, PresetUtils::IntOverrideToString(enabled, val ? 1 : 0).c_str());
        };

        setOvr("protectionBlunt", rp.protectionBlunt.enabled, rp.protectionBlunt.value);
        setOvr("protectionCut", rp.protectionCut.enabled, rp.protectionCut.value);
        setOvr("protectionStab", rp.protectionStab.enabled, rp.protectionStab.value);
        setOvr("materialDensity", rp.materialDensity.enabled, rp.materialDensity.value);
        setOvr("massScale", rp.massScale.enabled, rp.massScale.value);
        setOvr("handsRigidity", rp.handsRigidity.enabled, rp.handsRigidity.value);
        setOvr("strapPower", rp.strapPower.enabled, rp.strapPower.value);
        setOvr("aiInvincibilityRate", rp.aiInvincibilityRate.enabled, rp.aiInvincibilityRate.value);
        setOvr("price", rp.price.enabled, rp.price.value);
        setOvrBool("pickUp", rp.pickUp.enabled, rp.pickUp.value);

        std::string output;
        ini.Save(output);
        return output;
    }

    static ArmorPresetData DeserializeFromIni(const std::string& iniContent) {
        ArmorPresetData result;
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

        result.armorCorePath = ini.GetValue("Passport", "armorCore", "");
        p.ID_54_C6BBB1A64A3828B5AB1D8E804EC7C8F7 = std::atoi(ini.GetValue("Passport", "id", "0"));
        p.CoreRemoved_12_5CFF8F6D4A05C15812594CAF6771C66B =
            std::atoi(ini.GetValue("Passport", "coreRemoved", "0")) != 0;

        p.Module1_5_46B7198E4341C93CBF6AE989EF9898E4 = std::atoi(ini.GetValue("Passport", "module1", "0"));
        p.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0 = std::atoi(ini.GetValue("Passport", "module2", "0"));
        p.Module3_9_E282C465414F6D4EF2A8039FBA847AD2 = std::atoi(ini.GetValue("Passport", "module3", "0"));

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

        auto& rp = result.runtimeProps;
        PresetUtils::ParseDoubleOverride(
            ini.GetValue("Overrides", "protectionBlunt", ""), rp.protectionBlunt.enabled, rp.protectionBlunt.value
        );
        PresetUtils::ParseDoubleOverride(
            ini.GetValue("Overrides", "protectionCut", ""), rp.protectionCut.enabled, rp.protectionCut.value
        );
        PresetUtils::ParseDoubleOverride(
            ini.GetValue("Overrides", "protectionStab", ""), rp.protectionStab.enabled, rp.protectionStab.value
        );
        PresetUtils::ParseDoubleOverride(
            ini.GetValue("Overrides", "materialDensity", ""), rp.materialDensity.enabled, rp.materialDensity.value
        );
        PresetUtils::ParseDoubleOverride(
            ini.GetValue("Overrides", "massScale", ""), rp.massScale.enabled, rp.massScale.value
        );
        PresetUtils::ParseDoubleOverride(
            ini.GetValue("Overrides", "handsRigidity", ""), rp.handsRigidity.enabled, rp.handsRigidity.value
        );
        PresetUtils::ParseDoubleOverride(
            ini.GetValue("Overrides", "strapPower", ""), rp.strapPower.enabled, rp.strapPower.value
        );
        PresetUtils::ParseDoubleOverride(
            ini.GetValue("Overrides", "aiInvincibilityRate", ""), rp.aiInvincibilityRate.enabled,
            rp.aiInvincibilityRate.value
        );
        PresetUtils::ParseDoubleOverride(ini.GetValue("Overrides", "price", ""), rp.price.enabled, rp.price.value);
        PresetUtils::ParseBoolOverride(ini.GetValue("Overrides", "pickUp", ""), rp.pickUp.enabled, rp.pickUp.value);

        result.success = true;
        return result;
    }
};
