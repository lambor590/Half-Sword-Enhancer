#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstring>

#include "Utils/PresetUtils.h"
#include "SDK/Str_Loadout_Equipment_structs.hpp"
#include "SDK/Str_ArmorElements_structs.hpp"
#include "SDK/ArmorSlots_Enum_structs.hpp"

struct LoadoutPresetData {
    struct ArmorSlotData {
        SDK::EArmorSlots_Enum slot{};
        std::string armorClass;
        SDK::FLinearColor color1{0.5f, 0.5f, 0.5f, 1.0f};
        SDK::FLinearColor color2{0.5f, 0.5f, 0.5f, 1.0f};
        SDK::FLinearColor color3{0.5f, 0.5f, 0.5f, 1.0f};
    };

    struct WeaponSlotData {
        std::string weaponClass;
        std::string gripModule;
        std::string headModule;
        std::string guardModule;
        std::string pommelModule;
        std::string subModule1;
        std::string subModule2;
        SDK::FVector headSize{1.0, 1.0, 1.0};
        SDK::FVector guardSize{1.0, 1.0, 1.0};
        SDK::FVector pommelSize{1.0, 1.0, 1.0};
        int32_t coaInt = 0;
    };

    std::vector<ArmorSlotData> armorSlots;
    WeaponSlotData weaponSlots[7];
    std::string name;
    bool success = false;
    std::string error;
};

class LoadoutPresetSerializer {
private:

    static constexpr const char* WEAPON_SLOT_KEYS[] = {
        "HandR", "HandL", "SlotR1", "SlotR2", "SlotL1", "SlotL2", "Back"
    };

    static constexpr const char* ARMOR_SLOT_NAMES[] = {
        "Head", "Hands", "Neck_Bevor", "Neck_Gorget", "Neck_Standard",
        "Arms", "Shoulders", "Tabard", "Chest_Plate",
        "Hauberk", "Cuisses", "Body_Clothing",
        "Waist", "Legs_Greaves", "Feet", "Hosen", "Slot16"
    };
    static constexpr int ARMOR_SLOT_NAME_COUNT = 17;

    static const char* ArmorSlotToKey(SDK::EArmorSlots_Enum slot) {
        int idx = static_cast<int>(slot);
        if (idx >= 0 && idx < ARMOR_SLOT_NAME_COUNT)
            return ARMOR_SLOT_NAMES[idx];
        return "Unknown";
    }

    static SDK::EArmorSlots_Enum KeyToArmorSlot(const char* key) {
        for (int i = 0; i < ARMOR_SLOT_NAME_COUNT; ++i) {
            if (strcmp(key, ARMOR_SLOT_NAMES[i]) == 0)
                return static_cast<SDK::EArmorSlots_Enum>(i);
        }
        return SDK::EArmorSlots_Enum::ArmorSlots_MAX;
    }

public:
    static SDK::FStr_WeaponParts& GetWeaponSlot(SDK::FStr_Loadout_Weapons& weapons, int index) {
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

private:
    static const SDK::FStr_WeaponParts& GetWeaponSlotConst(const SDK::FStr_Loadout_Weapons& weapons, int index) {
        return GetWeaponSlot(const_cast<SDK::FStr_Loadout_Weapons&>(weapons), index);
    }

public:
    static void ReadWeaponSlot(const SDK::FStr_WeaponParts& wp, LoadoutPresetData::WeaponSlotData& out) {
        out.weaponClass = PresetUtils::ClassToString(wp.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066);
        out.gripModule = PresetUtils::ClassToString(wp.GripModule_38_15B14C3F4E9701389A9B35A3B0909867);
        out.headModule = PresetUtils::ClassToString(wp.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F);
        out.guardModule = PresetUtils::ClassToString(wp.GuardModule_21_774015784EB0300D2671C894D57ED144);
        out.pommelModule = PresetUtils::ClassToString(wp.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984);
        out.subModule1 = PresetUtils::ClassToString(wp.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0);
        out.subModule2 = PresetUtils::ClassToString(wp.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980);
        out.headSize = wp.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA;
        out.guardSize = wp.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D;
        out.pommelSize = wp.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524;
        out.coaInt = wp.COAInt_63_593665BE4EF020F95F7D1A92564C1239;
    }

    static void WriteWeaponSlot(SDK::FStr_WeaponParts& wp, const LoadoutPresetData::WeaponSlotData& data) {
        wp.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 = PresetUtils::StringToClass(data.weaponClass);
        wp.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 = PresetUtils::StringToClass(data.gripModule);
        wp.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F = PresetUtils::StringToClass(data.headModule);
        wp.GuardModule_21_774015784EB0300D2671C894D57ED144 = PresetUtils::StringToClass(data.guardModule);
        wp.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 = PresetUtils::StringToClass(data.pommelModule);
        wp.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 = PresetUtils::StringToClass(data.subModule1);
        wp.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 = PresetUtils::StringToClass(data.subModule2);
        wp.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA = data.headSize;
        wp.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D = data.guardSize;
        wp.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524 = data.pommelSize;
        wp.COAInt_63_593665BE4EF020F95F7D1A92564C1239 = data.coaInt;
    }

    static LoadoutPresetData ReadFromEquipment(const SDK::FStr_Loadout_Equipment& equip) {
        LoadoutPresetData data;
        data.success = true;

        auto& armorMap = equip.Armor_84_A1BA4DD44FD262BCA53B9DACF03CDF04
                             .ArmorinSlots_31_702A9C5C40C7F4335C6B4687EC09936A;
        for (auto it = begin(armorMap); it != end(armorMap); ++it) {
            auto& elem = it->Value();
            if (!elem.ArmorBPClass_2_0A22459840BF9E6989DFA4BA6CFED1D3) continue;

            LoadoutPresetData::ArmorSlotData slotData;
            slotData.slot = it->Key();
            slotData.armorClass = PresetUtils::ClassToString(elem.ArmorBPClass_2_0A22459840BF9E6989DFA4BA6CFED1D3);
            slotData.color1 = elem.Color1_5_5527FC7C442DCF594A4DA5BA8D94351F;
            slotData.color2 = elem.Color2_7_1FF790D94C8CD95FF2D76183E7102E1B;
            slotData.color3 = elem.Color3_9_D8B5A08742A87F5492F8138A4F686141;
            data.armorSlots.push_back(slotData);
        }

        for (int i = 0; i < 7; ++i)
            ReadWeaponSlot(GetWeaponSlotConst(equip.Weapons_83_06F076E247B54D0D9942B383323C1968, i), data.weaponSlots[i]);

        return data;
    }

    static void ApplyToEquipment(SDK::FStr_Loadout_Equipment& equip, const LoadoutPresetData& data) {
        auto& armorMap = equip.Armor_84_A1BA4DD44FD262BCA53B9DACF03CDF04
                             .ArmorinSlots_31_702A9C5C40C7F4335C6B4687EC09936A;

        for (auto it = begin(armorMap); it != end(armorMap); ++it) {
            auto& elem = it->Value();
            elem.ArmorBPClass_2_0A22459840BF9E6989DFA4BA6CFED1D3 = nullptr;
        }

        for (const auto& slotData : data.armorSlots) {
            SDK::UClass* cls = PresetUtils::StringToClass(slotData.armorClass);
            if (!cls) continue;

            for (auto it = begin(armorMap); it != end(armorMap); ++it) {
                if (it->Key() == slotData.slot) {
                    auto& elem = it->Value();
                    elem.ArmorBPClass_2_0A22459840BF9E6989DFA4BA6CFED1D3 = cls;
                    elem.Color1_5_5527FC7C442DCF594A4DA5BA8D94351F = slotData.color1;
                    elem.Color2_7_1FF790D94C8CD95FF2D76183E7102E1B = slotData.color2;
                    elem.Color3_9_D8B5A08742A87F5492F8138A4F686141 = slotData.color3;
                    break;
                }
            }
        }

        for (int i = 0; i < 7; ++i)
            WriteWeaponSlot(GetWeaponSlot(equip.Weapons_83_06F076E247B54D0D9942B383323C1968, i), data.weaponSlots[i]);
    }

    static std::string SerializeToIni(const LoadoutPresetData& data) {
        CSimpleIniA ini;
        ini.SetUnicode(false);

        ini.SetValue("Preset", "name", data.name.c_str());
        ini.SetValue("Preset", "version", "1");

        for (size_t i = 0; i < data.armorSlots.size(); ++i) {
            const auto& slot = data.armorSlots[i];
            std::string section = std::string("Armor.") + ArmorSlotToKey(slot.slot);
            ini.SetValue(section.c_str(), "class", slot.armorClass.c_str());
            ini.SetValue(section.c_str(), "color1", PresetUtils::ColorToString(slot.color1).c_str());
            ini.SetValue(section.c_str(), "color2", PresetUtils::ColorToString(slot.color2).c_str());
            ini.SetValue(section.c_str(), "color3", PresetUtils::ColorToString(slot.color3).c_str());
        }

        for (int i = 0; i < 7; ++i) {
            const auto& wp = data.weaponSlots[i];
            if (wp.weaponClass.empty()) continue;

            std::string section = std::string("Weapon.") + WEAPON_SLOT_KEYS[i];
            ini.SetValue(section.c_str(), "class", wp.weaponClass.c_str());
            if (!wp.gripModule.empty())   ini.SetValue(section.c_str(), "gripModule", wp.gripModule.c_str());
            if (!wp.headModule.empty())   ini.SetValue(section.c_str(), "headModule", wp.headModule.c_str());
            if (!wp.guardModule.empty())  ini.SetValue(section.c_str(), "guardModule", wp.guardModule.c_str());
            if (!wp.pommelModule.empty()) ini.SetValue(section.c_str(), "pommelModule", wp.pommelModule.c_str());
            if (!wp.subModule1.empty())   ini.SetValue(section.c_str(), "subModule1", wp.subModule1.c_str());
            if (!wp.subModule2.empty())   ini.SetValue(section.c_str(), "subModule2", wp.subModule2.c_str());
            ini.SetValue(section.c_str(), "headSize", PresetUtils::VecToString(wp.headSize).c_str());
            ini.SetValue(section.c_str(), "guardSize", PresetUtils::VecToString(wp.guardSize).c_str());
            ini.SetValue(section.c_str(), "pommelSize", PresetUtils::VecToString(wp.pommelSize).c_str());
            if (wp.coaInt != 0) ini.SetValue(section.c_str(), "coaInt", std::to_string(wp.coaInt).c_str());
        }

        std::string out;
        ini.Save(out);
        return out;
    }

    static LoadoutPresetData DeserializeFromIni(const std::string& iniStr) {
        LoadoutPresetData data;
        CSimpleIniA ini;
        ini.SetUnicode(false);
        if (ini.LoadData(iniStr.c_str(), iniStr.size()) < 0) {
            data.error = "Failed to parse INI data";
            return data;
        }

        data.name = ini.GetValue("Preset", "name", "Unnamed");

        for (int slotIdx = 0; slotIdx < ARMOR_SLOT_NAME_COUNT; ++slotIdx) {
            std::string section = std::string("Armor.") + ARMOR_SLOT_NAMES[slotIdx];
            const char* cls = ini.GetValue(section.c_str(), "class", nullptr);
            if (!cls) continue;

            LoadoutPresetData::ArmorSlotData slotData;
            slotData.slot = static_cast<SDK::EArmorSlots_Enum>(slotIdx);
            slotData.armorClass = cls;
            slotData.color1 = PresetUtils::StringToColor(ini.GetValue(section.c_str(), "color1", ""), {0.5f, 0.5f, 0.5f, 1.0f});
            slotData.color2 = PresetUtils::StringToColor(ini.GetValue(section.c_str(), "color2", ""), {0.5f, 0.5f, 0.5f, 1.0f});
            slotData.color3 = PresetUtils::StringToColor(ini.GetValue(section.c_str(), "color3", ""), {0.5f, 0.5f, 0.5f, 1.0f});
            data.armorSlots.push_back(slotData);
        }

        for (int i = 0; i < 7; ++i) {
            std::string section = std::string("Weapon.") + WEAPON_SLOT_KEYS[i];
            const char* cls = ini.GetValue(section.c_str(), "class", nullptr);
            if (!cls) continue;

            auto& wp = data.weaponSlots[i];
            wp.weaponClass = cls;
            wp.gripModule = ini.GetValue(section.c_str(), "gripModule", "");
            wp.headModule = ini.GetValue(section.c_str(), "headModule", "");
            wp.guardModule = ini.GetValue(section.c_str(), "guardModule", "");
            wp.pommelModule = ini.GetValue(section.c_str(), "pommelModule", "");
            wp.subModule1 = ini.GetValue(section.c_str(), "subModule1", "");
            wp.subModule2 = ini.GetValue(section.c_str(), "subModule2", "");
            wp.headSize = PresetUtils::StringToVec(ini.GetValue(section.c_str(), "headSize", ""));
            wp.guardSize = PresetUtils::StringToVec(ini.GetValue(section.c_str(), "guardSize", ""));
            wp.pommelSize = PresetUtils::StringToVec(ini.GetValue(section.c_str(), "pommelSize", ""));
            wp.coaInt = atoi(ini.GetValue(section.c_str(), "coaInt", "0"));
        }

        data.success = true;
        return data;
    }

    static std::filesystem::path GetPresetsDir() {
        return PresetUtils::EnsureDirectory(
            std::filesystem::path(ConfigManager::GetAppDataPath()) / "loadout_presets");
    }

    static bool SaveToFile(const std::filesystem::path& path, const LoadoutPresetData& data) {
        return PresetUtils::SaveStringToFile(path, SerializeToIni(data));
    }

    static LoadoutPresetData LoadFromFile(const std::filesystem::path& path) {
        std::string content = PresetUtils::LoadStringFromFile(path);
        if (content.empty()) {
            LoadoutPresetData data;
            data.error = "Failed to read file";
            return data;
        }
        return DeserializeFromIni(content);
    }

    static bool SavePresetByName(const LoadoutPresetData& data) {
        auto dir = GetPresetsDir();
        auto filename = PresetUtils::SanitizeFilename(data.name) + ".ini";
        return SaveToFile(dir / filename, data);
    }

    static std::vector<PresetListEntry> ListPresets() {
        return PresetUtils::ListPresetsInDir(GetPresetsDir());
    }
};
