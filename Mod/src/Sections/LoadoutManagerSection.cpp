#include "Menu/Sections/Equipment/LoadoutManagerSection.h"
#include "Menu/SectionRegistry.h"
#include "Menu/SectionStyle.h"
#include "SDK/Willie_BP_classes.hpp"

REGISTER_SECTION(LoadoutManagerSection, MenuTab::Equipment);

#include "Hooks/GameHook.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/GuiUtils.h"
#include "Utils/Spawner.h"
#include "Utils/WeaponPassportBuilder.h"
#include "SDK/BP_Armor_Modular_Core_Master_classes.hpp"
#include "SDK/Enum_Weapon_Material_Type_structs.hpp"

namespace {

    void CopyPassportToSlot(const SDK::FStr_Passport_Weapon1& passport, SDK::FStr_WeaponParts& slot) {
        slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 =
            passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC;
        slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F = passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139;
        slot.GuardModule_21_774015784EB0300D2671C894D57ED144 = passport.GuardModule_13_6DD2B06245505E53B529D090333012F0;
        slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 = passport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4;
        slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 =
            passport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6;
        slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 =
            passport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D;
        slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 =
            passport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9;
        slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA = passport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57;
        slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D = passport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704;
        slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524 =
            passport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E;
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

    const char* GetWeaponMaterialLabel(SDK::Enum_Weapon_Material_Type type) {
        switch (type) {
            case SDK::Enum_Weapon_Material_Type::NewEnumerator0: return "Steel##mat";
            case SDK::Enum_Weapon_Material_Type::NewEnumerator1: return "Colored Metal##mat";
            case SDK::Enum_Weapon_Material_Type::NewEnumerator2: return "Wood##mat";
            case SDK::Enum_Weapon_Material_Type::NewEnumerator3: return "Leather##mat";
            default: return nullptr;
        }
    }

    const char* GetWeaponColorLabel(SDK::Enum_Weapon_Material_Type type) {
        switch (type) {
            case SDK::Enum_Weapon_Material_Type::NewEnumerator2: return "Wood Color##col";
            case SDK::Enum_Weapon_Material_Type::NewEnumerator3: return "Leather Color##col";
            default: return nullptr;
        }
    }

} // namespace

const char* LoadoutManagerSection::ClassNameCache::Get(SDK::UClass* cls) {
    if (cls != ptr) {
        ptr = cls;
        name = cls ? cls->GetName() : "(empty)";
    }
    return name.c_str();
}

const char* LoadoutManagerSection::GetArmorSlotDisplayName(SDK::EArmorSlots_Enum slot) {
    int val = static_cast<int>(slot);
    for (int i = 0; i < ARMOR_SLOT_COUNT; ++i) {
        if (ARMOR_SLOTS[i].slotEnum == val) return ARMOR_SLOTS[i].name;
    }
    return "Unknown";
}

void LoadoutManagerSection::ScheduleSlotApply(SDK::EArmorSlots_Enum slot) {
    pendingSlot = slot;
    pendingSlotApply = true;
}

void LoadoutManagerSection::RemoveArmorSlot(SDK::AWillie_BP_C* p, SDK::EArmorSlots_Enum slot) {
    SDK::FTransform transform{};
    transform.Scale3D = {1.0, 1.0, 1.0};
    SDK::ABP_Armor_Modular_Core_Master_C* dropped = nullptr;
    p->Remove_Armor(transform, slot, &dropped);
    if (dropped) dropped->K2_DestroyActor();
}

void LoadoutManagerSection::EnsureModulePool() {
    if (modulePool.populated.load(std::memory_order_acquire) || modulePoolQueued) return;
    modulePoolQueued = true;
    GameHook::QueueAction([this](const RuntimeContextSnapshot& runtime) {
        modulePool.Populate();
        modulePoolQueued = false;
    });
}

void LoadoutManagerSection::RenderVectorDrag(const char* label, SDK::FVector& vec) {
    float v[3] = {static_cast<float>(vec.X), static_cast<float>(vec.Y), static_cast<float>(vec.Z)};
    GuiUtils::DebouncedDragFloat3(label, v, 0.01f, 0.0f, 0.0f, "%.3f");
    GuiUtils::StoreEdited(vec, v);
}

void LoadoutManagerSection::BuildArmorOps(std::vector<LoadoutAction>& ops, SDK::AWillie_BP_C* player) {
    if (!player) return;

    auto& map = player->Currently_Equipped_Armor;

    std::vector<SDK::EArmorSlots_Enum> slotsToRemove;
    std::vector<SDK::FStr_Passport_Armor1> passportsToSpawn;

    for (auto it = begin(map); it != end(map); ++it) {
        if (it->Value().ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) {
            slotsToRemove.push_back(it->Key());
            passportsToSpawn.push_back(it->Value());
        }
    }

    ops.clear();

    for (auto slot : slotsToRemove)
        ops.push_back([slot](const RuntimeContextSnapshot& runtime) {
            if (runtime.player) RemoveArmorSlot(runtime.player, slot);
        });

    for (auto& passport : passportsToSpawn)
        ops.push_back([passport](const RuntimeContextSnapshot& runtime) {
            if (runtime.world && runtime.player) Spawner::SpawnAndEquipArmor(runtime.world, runtime.player, passport);
        });
}

void LoadoutManagerSection::ApplyArmorToPlayer() {
    GameHook::QueueAction([this](const RuntimeContextSnapshot& runtime) {
        if (!runtime.player) return;
        BuildArmorOps(pendingStaggeredOps, runtime.player);
        hasPendingStaggeredOps.store(true, std::memory_order_release);
    });
}

void LoadoutManagerSection::ReapplyArmorSlot(SDK::EArmorSlots_Enum slot) {
    GameHook::QueueAction([slot](const RuntimeContextSnapshot& runtime) {
        if (!runtime.player || !runtime.world) return;
        auto& map = runtime.player->Currently_Equipped_Armor;
        for (auto it = begin(map); it != end(map); ++it) {
            if (it->Key() != slot) continue;
            auto& passport = it->Value();
            if (!passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) break;
            SDK::FStr_Passport_Armor1 copy = passport;
            RemoveArmorSlot(runtime.player, slot);
            Spawner::SpawnAndEquipArmor(runtime.world, runtime.player, copy);
            break;
        }
    });
}

void LoadoutManagerSection::ApplyWeaponToPlayer(int slotIndex) {
    if (slotIndex != 0 && slotIndex != 1) return;

    GameHook::QueueAction([slotIndex](const RuntimeContextSnapshot& runtime) {
        auto* player = runtime.player;
        if (!player) return;
        auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, slotIndex);
        auto* weaponClass = slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066;
        if (!weaponClass) return;

        auto passport = WeaponPassportBuilder::FromWeaponParts(slot);
        if (slotIndex == 0) {
            player->Set_Up_Right_Hand_Weapon(weaponClass, player->Weapon_R, false, true, passport);
        } else {
            player->Set_Up_Left_Hand_Weapon(weaponClass, player->Weapon_L, false, true, passport);
        }
    });
}

void LoadoutManagerSection::StripAllArmor() {
    GameHook::QueueAction([](const RuntimeContextSnapshot& runtime) {
        if (!runtime.player) return;
        auto& map = runtime.player->Currently_Equipped_Armor;
        std::vector<SDK::EArmorSlots_Enum> slots;
        for (auto it = begin(map); it != end(map); ++it)
            if (it->Value().ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) slots.push_back(it->Key());
        for (auto slot : slots)
            RemoveArmorSlot(runtime.player, slot);
    });
}

void LoadoutManagerSection::ClearAllWeapons() {
    GameHook::QueueAction([](const RuntimeContextSnapshot& runtime) {
        auto* player = runtime.player;
        if (!player) return;
        auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        for (int i = 0; i < 7; ++i) {
            auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, i);
            ClearWeaponSlot(slot);
        }
        player->Set_Up_Right_Hand_Weapon(nullptr, player->Weapon_R, false, true, {});
        player->Set_Up_Left_Hand_Weapon(nullptr, player->Weapon_L, false, true, {});
    });
}

void LoadoutManagerSection::GenerateArmorForSlot(SDK::EArmorSlots_Enum slotEnum) {
    auto tier = static_cast<SDK::Enum_Ranks>(cfg.generateTier);

    GameHook::QueueAction([slotEnum, tier](const RuntimeContextSnapshot& runtime) {
        if (!runtime.player || !runtime.world) return;
        auto passport = EquipmentGenerator::GenerateArmor(runtime.world, tier, slotEnum, 0.5);
        if (!passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) return;

        RemoveArmorSlot(runtime.player, slotEnum);
        Spawner::SpawnAndEquipArmor(runtime.world, runtime.player, passport);
    });
}

void LoadoutManagerSection::RandomizeAllArmor() {
    if (staggeredIdx < staggeredOps.size()) return;

    auto tier = static_cast<SDK::Enum_Ranks>(cfg.generateTier);

    GameHook::QueueAction([this, tier](const RuntimeContextSnapshot& runtime) {
        if (!runtime.player || !runtime.world) return;
        auto& dstMap = runtime.player->Currently_Equipped_Armor;
        std::vector<SDK::EArmorSlots_Enum> removeSlots;
        std::vector<SDK::FStr_Passport_Armor1> newPassports;

        for (auto it = begin(dstMap); it != end(dstMap); ++it) {
            removeSlots.push_back(it->Key());
            auto passport = EquipmentGenerator::GenerateArmor(runtime.world, tier, it->Key(), 0.5);
            if (passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) newPassports.push_back(passport);
        }

        pendingStaggeredOps.clear();

        for (auto slot : removeSlots)
            pendingStaggeredOps.push_back([slot](const RuntimeContextSnapshot& runtime) {
                if (runtime.player) RemoveArmorSlot(runtime.player, slot);
            });

        for (auto& passport : newPassports)
            pendingStaggeredOps.push_back([passport](const RuntimeContextSnapshot& runtime) {
                if (runtime.world && runtime.player)
                    Spawner::SpawnAndEquipArmor(runtime.world, runtime.player, passport);
            });

        hasPendingStaggeredOps.store(true, std::memory_order_release);
    });
}

void LoadoutManagerSection::GenerateWeaponForSlot(int slotIndex) {
    auto tier = static_cast<SDK::Enum_Ranks>(cfg.generateTier);
    auto type = static_cast<SDK::Enum_WeaponType>(0);

    GameHook::QueueAction([slotIndex, tier, type](const RuntimeContextSnapshot& runtime) {
        if (!runtime.player || !runtime.world) return;
        auto passport = EquipmentGenerator::GenerateWeapon(runtime.world, type, tier);

        auto& weapons = runtime.player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, slotIndex);
        CopyPassportToSlot(passport, slot);
    });

    if (slotIndex <= 1) ApplyWeaponToPlayer(slotIndex);
}

void LoadoutManagerSection::ImportWeaponPreset(int slotIndex) {
    if (!weaponPicker.HasSelection()) return;
    auto data = WeaponPresetSerializer::LoadFromFile(weaponPicker.SelectedPath());
    if (!data.success) return;

    GameHook::QueueAction([slotIndex, data = std::move(data)](const RuntimeContextSnapshot& runtime) {
        if (!runtime.player) return;
        auto& weapons = runtime.player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, slotIndex);

        auto load = [](const std::string& path) -> SDK::UClass* {
            return path.empty() ? nullptr : Spawner::LoadClass(path);
        };
        slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 = load(data.classPaths.weaponClass);
        slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F = load(data.classPaths.headModule);
        slot.GuardModule_21_774015784EB0300D2671C894D57ED144 = load(data.classPaths.guardModule);
        slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 = load(data.classPaths.gripModule);
        slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 = load(data.classPaths.pommelModule);
        slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 = load(data.classPaths.subModule1);
        slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 = load(data.classPaths.subModule2);
        slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA = data.passport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57;
        slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D =
            data.passport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704;
        slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524 =
            data.passport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E;
    });

    if (slotIndex <= 1) ApplyWeaponToPlayer(slotIndex);
}

void LoadoutManagerSection::ImportArmorPreset(SDK::EArmorSlots_Enum slotEnum) {
    if (!armorPicker.HasSelection()) return;
    auto data = ArmorPresetSerializer::LoadFromFile(armorPicker.SelectedPath());
    if (!data.success) return;

    GameHook::QueueAction([slotEnum, data = std::move(data)](const RuntimeContextSnapshot& runtime) {
        if (!runtime.player || !runtime.world) return;
        SDK::UClass* cls = data.armorCorePath.empty() ? nullptr : Spawner::LoadClass(data.armorCorePath);
        if (!cls) return;

        SDK::FStr_Passport_Armor1 passport = data.passport;
        passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = cls;
        passport.Slot_30_7561CB484566A4512003EA96ED44F88D = slotEnum;

        RemoveArmorSlot(runtime.player, slotEnum);
        Spawner::SpawnAndEquipArmor(runtime.world, runtime.player, passport);
    });
}

void LoadoutManagerSection::ApplyLoadoutPreset(const LoadoutPresetData& data) {
    if (!RenderPlayer()) return;

    GameHook::QueueAction([this, data](const RuntimeContextSnapshot& runtime) {
        auto* world = runtime.world;
        auto* player = runtime.player;
        if (!player || !world) return;

        auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        auto& equipArmorMap = player->Load_Equipment.Armor_84_A1BA4DD44FD262BCA53B9DACF03CDF04
                                  .ArmorinSlots_31_702A9C5C40C7F4335C6B4687EC09936A;
        auto& dstMap = player->Currently_Equipped_Armor;

        for (auto it = begin(equipArmorMap); it != end(equipArmorMap); ++it)
            it->Value().ArmorBPClass_2_0A22459840BF9E6989DFA4BA6CFED1D3 = nullptr;
        for (auto it = begin(dstMap); it != end(dstMap); ++it)
            it->Value().ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = nullptr;

        for (const auto& sd : data.armorSlots) {
            SDK::UClass* cls = sd.armorClass.empty() ? nullptr : Spawner::LoadClass(sd.armorClass);
            if (!cls) continue;

            for (auto it = begin(equipArmorMap); it != end(equipArmorMap); ++it) {
                if (it->Key() == sd.slot) {
                    auto& elem = it->Value();
                    elem.ArmorBPClass_2_0A22459840BF9E6989DFA4BA6CFED1D3 = cls;
                    elem.Color1_5_5527FC7C442DCF594A4DA5BA8D94351F = sd.color1;
                    elem.Color2_7_1FF790D94C8CD95FF2D76183E7102E1B = sd.color2;
                    elem.Color3_9_D8B5A08742A87F5492F8138A4F686141 = sd.color3;
                    break;
                }
            }

            for (auto it = begin(dstMap); it != end(dstMap); ++it) {
                if (it->Key() == sd.slot) {
                    auto& passport = it->Value();
                    passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = cls;
                    passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393 = sd.color1;
                    passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C = sd.color2;
                    passport.Slot_30_7561CB484566A4512003EA96ED44F88D = sd.slot;
                    break;
                }
            }
        }

        for (int i = 0; i < 7; ++i) {
            const auto& wd = data.weaponSlots[i];
            auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, i);
            auto load = [](SDK::UClass*& target, const std::string& path) {
                target = path.empty() ? nullptr : Spawner::LoadClass(path);
            };
            load(slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066, wd.weaponClass);
            load(slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867, wd.gripModule);
            load(slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F, wd.headModule);
            load(slot.GuardModule_21_774015784EB0300D2671C894D57ED144, wd.guardModule);
            load(slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984, wd.pommelModule);
            load(slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0, wd.subModule1);
            load(slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980, wd.subModule2);
            slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA = wd.headSize;
            slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D = wd.guardSize;
            slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524 = wd.pommelSize;
            slot.COAInt_63_593665BE4EF020F95F7D1A92564C1239 = wd.coaInt;
        }

        BuildArmorOps(pendingStaggeredOps, player);
        hasPendingStaggeredOps.store(true, std::memory_order_release);

        for (int slotIndex = 0; slotIndex < 2; ++slotIndex) {
            auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, slotIndex);
            auto* weaponClass = slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066;
            if (!weaponClass) continue;

            auto passport = WeaponPassportBuilder::FromWeaponParts(slot);
            if (slotIndex == 0) {
                player->Set_Up_Right_Hand_Weapon(weaponClass, player->Weapon_R, false, true, passport);
            } else {
                player->Set_Up_Left_Hand_Weapon(weaponClass, player->Weapon_L, false, true, passport);
            }
        }
    });
}

LoadoutPresetData LoadoutManagerSection::BuildPresetFromPlayer() {
    auto* player = RenderPlayer();
    if (!player) return {};
    LoadoutPresetData data;
    data.success = true;

    auto& armorMap = player->Currently_Equipped_Armor;
    auto& equipArmorMap = player->Load_Equipment.Armor_84_A1BA4DD44FD262BCA53B9DACF03CDF04
                              .ArmorinSlots_31_702A9C5C40C7F4335C6B4687EC09936A;

    for (auto it = begin(armorMap); it != end(armorMap); ++it) {
        auto& passport = it->Value();
        if (!passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) continue;

        LoadoutPresetData::ArmorSlotData slotData;
        slotData.slot = it->Key();
        slotData.armorClass = PresetUtils::ObjectToAbsolutePath(passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43);
        slotData.color1 = passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393;
        slotData.color2 = passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C;

        for (auto eit = begin(equipArmorMap); eit != end(equipArmorMap); ++eit) {
            if (eit->Key() == it->Key()) {
                slotData.color3 = eit->Value().Color3_9_D8B5A08742A87F5492F8138A4F686141;
                break;
            }
        }

        data.armorSlots.push_back(std::move(slotData));
    }

    auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
    for (int i = 0; i < 7; ++i)
        LoadoutPresetData::ReadWeaponSlot(LoadoutPresetData::GetWeaponSlot(weapons, i), data.weaponSlots[i]);

    return data;
}

void LoadoutManagerSection::RenderArmorTab() {
    ImGui::PushID("armor");
    auto* player = RenderPlayer();

    if (!player) {
        ImGui::TextDisabled("Player not available");
        ImGui::PopID();
        return;
    }

    ImGui::PushID("actions");
    float halfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("Strip All Armor", ImVec2(halfWidth, 0))) StripAllArmor();
    ImGui::SameLine();
    if (ImGui::Button("Randomize All", ImVec2(halfWidth, 0))) RandomizeAllArmor();
    ImGui::PopID();

    armorPicker.Render("Armor Preset##ap");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto& armorMap = player->Currently_Equipped_Armor;

    if (armorMap.Num() == 0) {
        ImGui::TextDisabled("No armor slots available");
        ImGui::PopID();
        return;
    }

    for (auto it = begin(armorMap); it != end(armorMap); ++it) {
        auto slotEnum = it->Key();
        auto& passport = it->Value();
        const char* slotName = GetArmorSlotDisplayName(slotEnum);

        ImGui::PushID(static_cast<int>(slotEnum));

        bool hasArmor = passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 != nullptr;
        const char* className =
            armorNameCache[static_cast<int>(slotEnum)].Get(passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43);
        bool open = ImGui::TreeNodeEx(slotName, ImGuiTreeNodeFlags_DefaultOpen, "%s - %s", slotName, className);

        if (open) {
            if (hasArmor) {
                bool colorChanged = false;
                float col1[4] = {
                    passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393.R,
                    passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393.G,
                    passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393.B,
                    passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393.A};
                float col2[4] = {
                    passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C.R,
                    passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C.G,
                    passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C.B,
                    passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C.A};

                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
                if (ImGui::ColorEdit4(
                        "Fabric Color 1##c1", col1, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar
                    )) {
                    passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393 = {col1[0], col1[1], col1[2], col1[3]};
                    colorChanged = true;
                }
                if (ImGui::ColorEdit4(
                        "Fabric Color 2##c2", col2, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar
                    )) {
                    passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C = {col2[0], col2[1], col2[2], col2[3]};
                    colorChanged = true;
                }
                ImGui::PopItemWidth();

                if (colorChanged && cfg.livePreview) ScheduleSlotApply(slotEnum);

                if (ImGui::Button("Remove")) {
                    auto s = slotEnum;
                    GameHook::QueueAction([s](const RuntimeContextSnapshot& runtime) {
                        if (runtime.player) RemoveArmorSlot(runtime.player, s);
                    });
                }
                ImGui::SameLine();
                if (ImGui::Button("Regenerate")) {
                    GenerateArmorForSlot(slotEnum);
                }
            } else {
                if (ImGui::Button("Generate")) {
                    GenerateArmorForSlot(slotEnum);
                }
            }

            if (armorPicker.HasSelection()) {
                if (ImGui::Button("Import from Preset")) ImportArmorPreset(slotEnum);
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImGui::PopID();
}

void LoadoutManagerSection::RenderWeaponSlotModules(SDK::FStr_WeaponParts& slot) {
    if (!modulePool.populated.load(std::memory_order_acquire)) {
        ImGui::TextDisabled("Loading modules...");
        return;
    }

    GuiUtils::RenderGlobalModuleCombo(
        "Head", slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F, modulePool.heads, moduleFilters[0],
        modulePool.cachedWidths[0]
    );
    GuiUtils::RenderGlobalModuleCombo(
        "Guard", slot.GuardModule_21_774015784EB0300D2671C894D57ED144, modulePool.guards, moduleFilters[1],
        modulePool.cachedWidths[1]
    );
    GuiUtils::RenderGlobalModuleCombo(
        "Grip", slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867, modulePool.grips, moduleFilters[2],
        modulePool.cachedWidths[2]
    );
    GuiUtils::RenderGlobalModuleCombo(
        "Pommel", slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984, modulePool.pommels, moduleFilters[3],
        modulePool.cachedWidths[3]
    );
    if (!modulePool.subMods1.empty())
        GuiUtils::RenderGlobalModuleCombo(
            "Sub-Mod 1", slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0, modulePool.subMods1, moduleFilters[4],
            modulePool.cachedWidths[4]
        );
    if (!modulePool.subMods2.empty())
        GuiUtils::RenderGlobalModuleCombo(
            "Sub-Mod 2", slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980, modulePool.subMods2, moduleFilters[5],
            modulePool.cachedWidths[5]
        );
}

void LoadoutManagerSection::RenderWeaponSlotSizes(SDK::FStr_WeaponParts& slot) {
    ImGui::TextDisabled("Sizes");
    RenderVectorDrag("Head##sz", slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA);
    RenderVectorDrag("Guard##sz", slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D);
    RenderVectorDrag("Pommel##sz", slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524);
}

void LoadoutManagerSection::RenderWeaponSlotMaterials(SDK::FStr_WeaponParts& slot) {
    auto& matMap = slot.MemberVar_40_43_0B501E224AC2292FC03A999C237C2C81;
    if (matMap.Num() == 0) return;

    ImGui::TextDisabled("Materials");
    for (auto it = begin(matMap); it != end(matMap); ++it) {
        const char* label = GetWeaponMaterialLabel(it->Key());
        if (!label) continue;
        GuiUtils::RenderMaterialCombo(label, it->Value());
    }

    auto& colorMap = slot.MemberVar_44_45_FF627FBE4FE882E7D295BFA0BB6716C0;
    for (auto it = begin(colorMap); it != end(colorMap); ++it) {
        const char* label = GetWeaponColorLabel(it->Key());
        if (!label) continue;
        float col[4] = {it->Value().R, it->Value().G, it->Value().B, it->Value().A};
        if (ImGui::ColorEdit4(label, col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
            it->Value() = {col[0], col[1], col[2], col[3]};
    }
}

void LoadoutManagerSection::RenderWeaponsTab() {
    ImGui::PushID("weapons");
    auto* player = RenderPlayer();

    if (!player) {
        ImGui::TextDisabled("Player not available");
        ImGui::PopID();
        return;
    }

    EnsureModulePool();

    if (ImGui::Button("Clear All Weapons", ImVec2(ImGui::GetContentRegionAvail().x, 0))) ClearAllWeapons();

    weaponPicker.Render("Weapon Preset##wp");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;

    for (int i = 0; i < 7; ++i) {
        auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, i);
        bool hasWeapon = slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 != nullptr;
        const char* className = weaponNameCache[i].Get(slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066);

        ImGui::PushID(i);

        bool open = ImGui::TreeNodeEx(WEAPON_SLOT_NAMES[i], 0, "%s - %s", WEAPON_SLOT_NAMES[i], className);

        if (open) {
            if (hasWeapon) {
                RenderWeaponSlotModules(slot);

                ImGui::Spacing();
                RenderWeaponSlotSizes(slot);

                ImGui::Spacing();
                RenderWeaponSlotMaterials(slot);

                ImGui::Spacing();
                ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
                ImGui::InputInt("CoA##coa", &slot.COAInt_63_593665BE4EF020F95F7D1A92564C1239);

                ImGui::Spacing();
                bool canApplyDirectly = (i <= 1);
                float buttonW = canApplyDirectly
                                    ? (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f
                                    : ImGui::GetContentRegionAvail().x;
                if (canApplyDirectly && ImGui::Button("Apply", ImVec2(buttonW, 0))) ApplyWeaponToPlayer(i);
                if (canApplyDirectly) ImGui::SameLine();
                if (ImGui::Button("Remove", ImVec2(buttonW, 0))) {
                    ClearWeaponSlot(slot);
                    if (cfg.livePreview && i <= 1) ApplyWeaponToPlayer(i);
                }
            } else {
                ImGui::TextDisabled("(empty slot)");
            }

            if (ImGui::Button(hasWeapon ? "Regenerate" : "Generate", ImVec2(-1, 0))) GenerateWeaponForSlot(i);

            if (weaponPicker.HasSelection()) {
                if (ImGui::Button("Import from Preset", ImVec2(-1, 0))) ImportWeaponPreset(i);
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImGui::PopID();
}

LoadoutManagerSection::LoadoutManagerSection(ModContext& ctx) : Section(ctx, "Loadout Manager") {
    InitKeybinds();
}

void LoadoutManagerSection::InitKeybinds() {
    AddKeybind(
        keybinds,
        {
            .name = "Apply Loadout",
            .tooltip = "Reapply the current equipment to the player",
            .configSection = "ApplyLoadout",
            .keyPtr = &cfg.applyKey,
            .callback = [this]([[maybe_unused]] bool, const RuntimeContextSnapshot&) { ApplyArmorToPlayer(); },
        }
    );

    AddKeybind(
        keybinds,
        {
            .name = "Randomize Equipment",
            .tooltip = "Generate random armor for all equipped slots",
            .configSection = "RandomizeEquipment",
            .keyPtr = &cfg.randomizeKey,
            .callback = [this]([[maybe_unused]] bool, const RuntimeContextSnapshot&) { RandomizeAllArmor(); },
            .params =
                {KeybindParam("live_preview", "Live Preview", &cfg.livePreview, "Auto-apply changes to the player"),
                 KeybindParam("tier", "Generate Tier", &cfg.generateTier, 0, 8, "Tier for generated equipment")},
        }
    );
}

void LoadoutManagerSection::Render() {
    SectionStyle::StyleRAII style;
    auto* player = RenderPlayer();

    KeybindUI::RenderKeybindList(keybinds);
    ImGui::Spacing();

    if (cfg.livePreview && pendingSlotApply) {
        static constexpr double APPLY_COOLDOWN = 0.3;
        if (ImGui::GetTime() - lastSlotApplyTime >= APPLY_COOLDOWN) {
            ReapplyArmorSlot(pendingSlot);
            lastSlotApplyTime = ImGui::GetTime();
            pendingSlotApply = false;
        }
    }

    if (hasPendingStaggeredOps.exchange(false, std::memory_order_acquire)) {
        staggeredOps = std::move(pendingStaggeredOps);
        pendingStaggeredOps.clear();
        staggeredIdx = 0;
        staggeredBusy.store(false, std::memory_order_relaxed);
    }

    if (staggeredIdx < staggeredOps.size() && !staggeredBusy.load(std::memory_order_acquire)) {
        staggeredBusy.store(true, std::memory_order_release);
        auto op = staggeredOps[staggeredIdx++];
        GameHook::QueueAction([op, flag = &staggeredBusy](const RuntimeContextSnapshot& runtime) {
            op(runtime);
            flag->store(false, std::memory_order_release);
        });
        if (staggeredIdx >= staggeredOps.size()) {
            staggeredOps.clear();
            staggeredIdx = 0;
        }
    }

    ImGui::Spacing();
    static constexpr const char* EQ_TAB_LABELS[] = {"Armor", "Weapons", "Presets"};
    GuiUtils::RenderUnderlineTabs("##EquipmentTabs", activeTab, EQ_TAB_LABELS, 3);
    switch (activeTab) {
        case 0: RenderArmorTab(); break;
        case 1: RenderWeaponsTab(); break;
        case 2:
            presets.status.Render();
            presets.RenderPresetsTab(
                [this]() { return BuildPresetFromPlayer(); },
                [this](LoadoutPresetData d) { ApplyLoadoutPreset(std::move(d)); }, player != nullptr
            );
            break;
        default: break;
    }
}
