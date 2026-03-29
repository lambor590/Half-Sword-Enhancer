#include "Utils/NPCSpawnHelpers.h"

#include "Utils/LoadoutPresetSerializer.h"
#include "Utils/Spawner.h"
#include "SDK/Str_Passport_Armor1_structs.hpp"

void NPCSpawnHelpers::ApplyNPCLoadout(SDK::UWorld* world, SDK::AWillie_BP_C* npc, const LoadoutPresetData& loadout) {
    for (const auto& sd : loadout.armorSlots) {
        SDK::UClass* cls = sd.armorClass.empty() ? nullptr : Spawner::LoadClass(sd.armorClass);
        if (!cls) continue;

        SDK::FStr_Passport_Armor1 armorPassport{};
        armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = cls;
        armorPassport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393 = sd.color1;
        armorPassport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C = sd.color2;
        armorPassport.Slot_30_7561CB484566A4512003EA96ED44F88D = sd.slot;
        Spawner::SpawnAndEquipArmor(world, npc, armorPassport);
    }

    auto& weapons = npc->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
    for (int i = 0; i < 7; ++i) {
        const auto& wd = loadout.weaponSlots[i];
        if (wd.weaponClass.empty()) continue;
        auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, i);
        auto load = [](const std::string& path) -> SDK::UClass* {
            return path.empty() ? nullptr : Spawner::LoadClass(path);
        };
        slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 = load(wd.weaponClass);
        slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 = load(wd.gripModule);
        slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F = load(wd.headModule);
        slot.GuardModule_21_774015784EB0300D2671C894D57ED144 = load(wd.guardModule);
        slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 = load(wd.pommelModule);
        slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 = load(wd.subModule1);
        slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 = load(wd.subModule2);
        slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA = wd.headSize;
        slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D = wd.guardSize;
        slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524 = wd.pommelSize;
        slot.COAInt_63_593665BE4EF020F95F7D1A92564C1239 = wd.coaInt;
    }
    npc->Set_Up_Armor(true, false);
}
