#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <atomic>
#include <functional>

#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Hooks/GameHook.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/LoadoutPresetSerializer.h"
#include "Utils/Spawner.h"
#include "Utils/GuiUtils.h"
#include "Utils/WeaponPassportBuilder.h"
#include "Utils/GlobalModulePool.h"
#include "SDK/Willie_BP_classes.hpp"
#include "SDK/ArmorSlots_Enum_structs.hpp"
#include "SDK/Str_Passport_Armor1_structs.hpp"
#include "SDK/BP_Armor_Modular_Core_Master_classes.hpp"
#include "SDK/Enum_Weapon_Material_Type_structs.hpp"
#include "SDK/Enum_MaterialLayer_structs.hpp"

class EquipmentManagerSection : public CollapsibleSection {
private:
    SectionConfig::EquipmentManagerConfig& cfg = SectionConfig::equipmentManager;

    static constexpr auto& ARMOR_SLOTS = GameConstants::ARMOR_SLOTS;
    static constexpr int ARMOR_SLOT_COUNT = GameConstants::ARMOR_SLOT_COUNT;

    static constexpr const char* WEAPON_SLOT_NAMES[] = {
        "Right Hand", "Left Hand", "Slot R1", "Slot R2", "Slot L1", "Slot L2", "Back"
    };

    static constexpr auto& MATERIAL_LAYER_NAMES = GameConstants::MATERIAL_LAYER_NAMES;

    GlobalModulePool& modulePool = GlobalModulePool::Get();
    bool modulePoolQueued = false;
    char moduleFilters[6][64] = {};

    double lastSlotApplyTime = 0.0;
    SDK::EArmorSlots_Enum pendingSlot{};
    bool pendingSlotApply = false;

    std::vector<std::function<void()>> staggeredOps;
    size_t staggeredIdx = 0;
    std::atomic<bool> staggeredBusy{false};

    char presetNameBuf[128] = {};
    PresetUtils::PresetTreeNode presetTree;
    bool presetListDirty = true;
    GuiUtils::StatusMessage status;
    int activeTab = 0;

    void RefreshPresetTree() {
        presetTree = LoadoutPresetSerializer::ListPresetsTree();
        presetListDirty = false;
    }

    static const char* GetArmorSlotDisplayName(SDK::EArmorSlots_Enum slot) {
        int val = static_cast<int>(slot);
        for (int i = 0; i < ARMOR_SLOT_COUNT; ++i) {
            if (ARMOR_SLOTS[i].slotEnum == val)
                return ARMOR_SLOTS[i].name;
        }
        return "Unknown";
    }

    void ScheduleSlotApply(SDK::EArmorSlots_Enum slot) {
        pendingSlot = slot;
        pendingSlotApply = true;
    }

    static void RemoveArmorSlot(SDK::AWillie_BP_C* p, SDK::EArmorSlots_Enum slot) {
        SDK::FTransform transform{};
        transform.Scale3D = {1.0, 1.0, 1.0};
        SDK::ABP_Armor_Modular_Core_Master_C* dropped = nullptr;
        p->Remove_Armor(transform, slot, &dropped);
        if (dropped) dropped->K2_DestroyActor();
    }

    static bool SpawnAndEquipArmor(const SDK::UWorld* w, SDK::AWillie_BP_C* p,
                                    const SDK::FStr_Passport_Armor1& passport)
    {
        auto* armorClass = passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
        if (!armorClass) return false;

        SDK::FTransform transform{};
        transform.Scale3D = {1.0, 1.0, 1.0};

        auto* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
            w, armorClass, transform,
            SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
            nullptr, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);
        if (!actor) return false;

        auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(actor);
        armor->Armor_Passport = passport;
        armor->World_Transform = transform;

        SDK::UGameplayStatics::FinishSpawningActor(actor, transform,
            SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);

        bool pickedUp = false;
        p->Pick_Up_Armor(armor->DefaultSceneRoot, armor, &pickedUp);
        return pickedUp;
    }

    void EnsureModulePool() {
        if (modulePool.populated.load(std::memory_order_acquire) || modulePoolQueued) return;
        modulePoolQueued = true;
        GameHook::QueueAction([this]() {
            modulePool.Populate();
            modulePoolQueued = false;
        });
    }

    static void RenderModuleCombo(const char* label, SDK::UClass*& current,
        const std::vector<GlobalModuleEntry>& options, char* filterBuf, float& cachedWidth)
    {
        const char* preview = "None";
        for (const auto& e : options)
            if (e.cls == current) { preview = e.name.c_str(); break; }

        if (cachedWidth == 0.0f) {
            float maxW = 0;
            for (const auto& e : options) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%-36s [%s]", e.name.c_str(), e.sourceType);
                float w = ImGui::CalcTextSize(buf).x;
                if (w > maxW) maxW = w;
            }
            cachedWidth = GuiUtils::ComboWidthFromText(maxW);
        }

        ImGui::SetNextItemWidth(cachedWidth);
        if (!ImGui::BeginCombo(label, preview)) return;

        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##filter", "Search...", filterBuf, 64);
        const size_t filterLen = std::strlen(filterBuf);

        ImGui::Separator();
        if (ImGui::Selectable("None", current == nullptr))
            current = nullptr;

        char display[128];
        for (const auto& e : options) {
            if (filterLen > 0 && !GuiUtils::MatchesFilter(e.name.c_str(), e.name.size(), filterBuf, filterLen))
                continue;
            std::snprintf(display, sizeof(display), "%-36s [%s]", e.name.c_str(), e.sourceType);
            if (ImGui::Selectable(display, e.cls == current))
                current = e.cls;
            if (e.cls == current) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    static void RenderVectorDrag(const char* label, SDK::FVector& vec) {
        float v[3] = {static_cast<float>(vec.X), static_cast<float>(vec.Y), static_cast<float>(vec.Z)};
        if (ImGui::DragFloat3(label, v, 0.01f, 0.0f, 0.0f, "%.3f")) {
            vec.X = v[0]; vec.Y = v[1]; vec.Z = v[2];
        }
    }

    void ApplyArmorToPlayer() {
        if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;

        auto& map = player->Currently_Equipped_Armor;

        std::vector<SDK::EArmorSlots_Enum> slotsToRemove;
        std::vector<SDK::FStr_Passport_Armor1> passportsToSpawn;

        for (auto it = begin(map); it != end(map); ++it) {
            if (it->Value().ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) {
                slotsToRemove.push_back(it->Key());
                passportsToSpawn.push_back(it->Value());
            }
        }

        staggeredOps.clear();
        staggeredIdx = 0;
        staggeredBusy.store(false, std::memory_order_relaxed);

        auto* p = player;
        auto* w = world;

        for (auto slot : slotsToRemove)
            staggeredOps.push_back([p, slot]() { RemoveArmorSlot(p, slot); });

        for (auto& passport : passportsToSpawn)
            staggeredOps.push_back([w, p, passport]() { SpawnAndEquipArmor(w, p, passport); });
    }

    void ReapplyArmorSlot(SDK::EArmorSlots_Enum slot) {
        if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;
        GameHook::QueueAction([p = player, w = world, slot]() {
            auto& map = p->Currently_Equipped_Armor;
            for (auto it = begin(map); it != end(map); ++it) {
                if (it->Key() != slot) continue;
                auto& passport = it->Value();
                if (!passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) break;
                SDK::FStr_Passport_Armor1 copy = passport;
                RemoveArmorSlot(p, slot);
                SpawnAndEquipArmor(w, p, copy);
                break;
            }
        });
    }

    void ApplyWeaponToPlayer(int slotIndex) {
        if (!ComponentValidator::Validate(player)) return;

        auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        auto& slot = LoadoutPresetSerializer::GetWeaponSlot(weapons, slotIndex);
        auto* weaponClass = slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066;
        if (!weaponClass) return;

        if (slotIndex == 0) {
            auto passport = WeaponPassportBuilder::FromWeaponParts(slot);
            GameHook::QueueAction([p = player, weaponClass, passport]() {
                if (p) p->Set_Up_Right_Hand_Weapon(weaponClass, p->Weapon_R, false, true, passport);
            });
        } else if (slotIndex == 1) {
            auto passport = WeaponPassportBuilder::FromWeaponParts(slot);
            GameHook::QueueAction([p = player, weaponClass, passport]() {
                if (p) p->Set_Up_Left_Hand_Weapon(weaponClass, p->Weapon_L, false, true, passport);
            });
        } else {
            ApplyArmorToPlayer();
        }
    }

    void StripAllArmor() {
        if (!ComponentValidator::Validate(player)) return;
        GameHook::QueueAction([p = player]() {
            auto& map = p->Currently_Equipped_Armor;
            std::vector<SDK::EArmorSlots_Enum> slots;
            for (auto it = begin(map); it != end(map); ++it)
                if (it->Value().ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43)
                    slots.push_back(it->Key());
            for (auto slot : slots)
                RemoveArmorSlot(p, slot);
        });
    }

    void ClearAllWeapons() {
        if (!ComponentValidator::Validate(player)) return;
        auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        for (int i = 0; i < 7; ++i) {
            auto& slot = LoadoutPresetSerializer::GetWeaponSlot(weapons, i);
            slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 = nullptr;
            slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 = nullptr;
            slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F = nullptr;
            slot.GuardModule_21_774015784EB0300D2671C894D57ED144 = nullptr;
            slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 = nullptr;
            slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 = nullptr;
            slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 = nullptr;
        }
        ApplyWeaponToPlayer(0);
        ApplyWeaponToPlayer(1);
    }

    void GenerateArmorForSlot(SDK::EArmorSlots_Enum slotEnum) {
        if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;
        auto tier = static_cast<SDK::Enum_Ranks>(cfg.generateTier);

        GameHook::QueueAction([this, slotEnum, tier]() {
            EquipmentGenerator::Init(world);
            auto passport = EquipmentGenerator::GenerateArmor(tier, slotEnum, 0.5);
            if (!passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) return;

            RemoveArmorSlot(player, slotEnum);
            SpawnAndEquipArmor(world, player, passport);
        });
    }

    void RandomizeAllArmor() {
        if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;
        if (staggeredIdx < staggeredOps.size()) return;

        auto tier = static_cast<SDK::Enum_Ranks>(cfg.generateTier);

        GameHook::QueueAction([this, tier]() {
            EquipmentGenerator::Init(world);

            auto& dstMap = player->Currently_Equipped_Armor;
            std::vector<SDK::EArmorSlots_Enum> removeSlots;
            std::vector<SDK::FStr_Passport_Armor1> newPassports;

            for (auto it = begin(dstMap); it != end(dstMap); ++it) {
                removeSlots.push_back(it->Key());
                auto passport = EquipmentGenerator::GenerateArmor(tier, it->Key(), 0.5);
                if (passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43)
                    newPassports.push_back(passport);
            }

            auto* p = player;
            auto* w = world;

            staggeredOps.clear();
            staggeredIdx = 0;
            staggeredBusy.store(false, std::memory_order_relaxed);

            for (auto slot : removeSlots)
                staggeredOps.push_back([p, slot]() { RemoveArmorSlot(p, slot); });

            for (auto& passport : newPassports)
                staggeredOps.push_back([w, p, passport]() { SpawnAndEquipArmor(w, p, passport); });
        });
    }

    void GenerateWeaponForSlot(int slotIndex) {
        if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;
        auto tier = static_cast<SDK::Enum_Ranks>(cfg.generateTier);
        auto type = static_cast<SDK::Enum_WeaponType>(0);

        GameHook::QueueAction([this, slotIndex, tier, type]() {
            EquipmentGenerator::Init(world);
            auto passport = EquipmentGenerator::GenerateWeapon(type, tier);

            auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
            auto& slot = LoadoutPresetSerializer::GetWeaponSlot(weapons, slotIndex);
            slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 = passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC;
            slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F = passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139;
            slot.GuardModule_21_774015784EB0300D2671C894D57ED144 = passport.GuardModule_13_6DD2B06245505E53B529D090333012F0;
            slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 = passport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4;
            slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 = passport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6;
            slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 = passport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D;
            slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 = passport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9;
            slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA = passport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57;
            slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D = passport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704;
            slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524 = passport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E;
        });

        if (slotIndex <= 1)
            ApplyWeaponToPlayer(slotIndex);
    }

    void ApplyLoadoutPreset(LoadoutPresetData data) {
        if (!ComponentValidator::Validate(player)) return;

        auto& dstMap = player->Currently_Equipped_Armor;
        for (auto it = begin(dstMap); it != end(dstMap); ++it)
            it->Value().ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = nullptr;

        GameHook::QueueAction([this, data = std::move(data)]() {
            auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
            auto& equipArmorMap = player->Load_Equipment.Armor_84_A1BA4DD44FD262BCA53B9DACF03CDF04
                                       .ArmorinSlots_31_702A9C5C40C7F4335C6B4687EC09936A;
            auto& dstMap = player->Currently_Equipped_Armor;

            for (auto it = begin(equipArmorMap); it != end(equipArmorMap); ++it)
                it->Value().ArmorBPClass_2_0A22459840BF9E6989DFA4BA6CFED1D3 = nullptr;

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
                auto& slot = LoadoutPresetSerializer::GetWeaponSlot(weapons, i);
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

            ApplyArmorToPlayer();
            ApplyWeaponToPlayer(0);
            ApplyWeaponToPlayer(1);
        });
    }

    LoadoutPresetData BuildPresetFromPlayer() {
        if (!ComponentValidator::Validate(player)) return {};
        LoadoutPresetData data;
        data.success = true;

        auto& armorMap = player->Currently_Equipped_Armor;
        for (auto it = begin(armorMap); it != end(armorMap); ++it) {
            auto& passport = it->Value();
            if (!passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) continue;

            LoadoutPresetData::ArmorSlotData slotData;
            slotData.slot = it->Key();
            slotData.armorClass = PresetUtils::ObjectToAbsolutePath(passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43);
            slotData.color1 = passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393;
            slotData.color2 = passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C;
            data.armorSlots.push_back(std::move(slotData));
        }

        auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        for (int i = 0; i < 7; ++i)
            LoadoutPresetSerializer::ReadWeaponSlot(LoadoutPresetSerializer::GetWeaponSlot(weapons, i), data.weaponSlots[i]);

        data.name = presetNameBuf;
        return data;
    }

    void RenderArmorTab() {
        ImGui::PushID("armor");

        if (!ComponentValidator::Validate(player)) {
            ImGui::TextDisabled("Player not available");
            ImGui::PopID();
            return;
        }

        ImGui::PushID("actions");
        float halfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Strip All Armor", ImVec2(halfWidth, 0)))
            StripAllArmor();
        ImGui::SameLine();
        if (ImGui::Button("Randomize All", ImVec2(halfWidth, 0)))
            RandomizeAllArmor();
        ImGui::PopID();

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
            std::string className = hasArmor
                ? passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43->GetName()
                : "(empty)";

            bool open = ImGui::TreeNodeEx(slotName, ImGuiTreeNodeFlags_DefaultOpen, "%s - %s", slotName, className.c_str());

            if (open) {
                if (hasArmor) {
                    bool colorChanged = false;
                    float col1[4] = {passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393.R,
                                     passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393.G,
                                     passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393.B,
                                     passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393.A};
                    float col2[4] = {passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C.R,
                                     passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C.G,
                                     passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C.B,
                                     passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C.A};

                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
                    if (ImGui::ColorEdit4("Fabric Color 1##c1", col1, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar)) {
                        passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393 = {col1[0], col1[1], col1[2], col1[3]};
                        colorChanged = true;
                    }
                    if (ImGui::ColorEdit4("Fabric Color 2##c2", col2, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar)) {
                        passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C = {col2[0], col2[1], col2[2], col2[3]};
                        colorChanged = true;
                    }
                    ImGui::PopItemWidth();

                    if (colorChanged && cfg.livePreview)
                        ScheduleSlotApply(slotEnum);

                    if (ImGui::Button("Remove")) {
                        auto s = slotEnum;
                        GameHook::QueueAction([p = player, s]() {
                            RemoveArmorSlot(p, s);
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

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::PopID();
    }

    void RenderWeaponSlotModules(SDK::FStr_WeaponParts& slot) {
        if (!modulePool.populated.load(std::memory_order_acquire)) {
            ImGui::TextDisabled("Loading modules...");
            return;
        }

        RenderModuleCombo("Head", slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F,
            modulePool.heads, moduleFilters[0], modulePool.cachedWidths[0]);
        RenderModuleCombo("Guard", slot.GuardModule_21_774015784EB0300D2671C894D57ED144,
            modulePool.guards, moduleFilters[1], modulePool.cachedWidths[1]);
        RenderModuleCombo("Grip", slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867,
            modulePool.grips, moduleFilters[2], modulePool.cachedWidths[2]);
        RenderModuleCombo("Pommel", slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984,
            modulePool.pommels, moduleFilters[3], modulePool.cachedWidths[3]);
        if (!modulePool.subMods1.empty())
            RenderModuleCombo("Sub-Mod 1", slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0,
                modulePool.subMods1, moduleFilters[4], modulePool.cachedWidths[4]);
        if (!modulePool.subMods2.empty())
            RenderModuleCombo("Sub-Mod 2", slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980,
                modulePool.subMods2, moduleFilters[5], modulePool.cachedWidths[5]);
    }

    void RenderWeaponSlotSizes(SDK::FStr_WeaponParts& slot) {
        ImGui::TextDisabled("Sizes");
        RenderVectorDrag("Head##sz", slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA);
        RenderVectorDrag("Guard##sz", slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D);
        RenderVectorDrag("Pommel##sz", slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524);
    }

    static void RenderWeaponSlotMaterials(SDK::FStr_WeaponParts& slot) {
        auto& matMap = slot.MemberVar_40_43_0B501E224AC2292FC03A999C237C2C81;
        if (matMap.Num() == 0) return;

        ImGui::TextDisabled("Materials");
        for (auto it = begin(matMap); it != end(matMap); ++it) {
            const char* label = "Unknown";
            switch (it->Key()) {
                case SDK::Enum_Weapon_Material_Type::NewEnumerator0: label = "Steel##mat"; break;
                case SDK::Enum_Weapon_Material_Type::NewEnumerator1: label = "Colored Metal##mat"; break;
                case SDK::Enum_Weapon_Material_Type::NewEnumerator2: label = "Wood##mat"; break;
                case SDK::Enum_Weapon_Material_Type::NewEnumerator3: label = "Leather##mat"; break;
                default: continue;
            }
            GuiUtils::RenderMaterialCombo(label, it->Value());
        }

        auto& colorMap = slot.MemberVar_44_45_FF627FBE4FE882E7D295BFA0BB6716C0;
        for (auto it = begin(colorMap); it != end(colorMap); ++it) {
            const char* label = nullptr;
            switch (it->Key()) {
                case SDK::Enum_Weapon_Material_Type::NewEnumerator2: label = "Wood Color##col"; break;
                case SDK::Enum_Weapon_Material_Type::NewEnumerator3: label = "Leather Color##col"; break;
                default: continue;
            }
            if (!label) continue;
            float col[4] = {it->Value().R, it->Value().G, it->Value().B, it->Value().A};
            if (ImGui::ColorEdit4(label, col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                it->Value() = {col[0], col[1], col[2], col[3]};
        }
    }

    void RenderWeaponsTab() {
        ImGui::PushID("weapons");

        if (!ComponentValidator::Validate(player)) {
            ImGui::TextDisabled("Player not available");
            ImGui::PopID();
            return;
        }

        EnsureModulePool();

        if (ImGui::Button("Clear All Weapons", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
            ClearAllWeapons();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;

        for (int i = 0; i < 7; ++i) {
            auto& slot = LoadoutPresetSerializer::GetWeaponSlot(weapons, i);
            bool hasWeapon = slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 != nullptr;
            std::string className = hasWeapon
                ? slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066->GetName()
                : "(empty)";

            ImGui::PushID(i);

            bool open = ImGui::TreeNodeEx(WEAPON_SLOT_NAMES[i], 0, "%s - %s", WEAPON_SLOT_NAMES[i], className.c_str());

            if (open) {
                if (hasWeapon) {
                    RenderWeaponSlotModules(slot);

                    ImGui::Spacing();
                    RenderWeaponSlotSizes(slot);

                    ImGui::Spacing();
                    RenderWeaponSlotMaterials(slot);

                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
                    ImGui::InputInt("CoA##coa", &slot.COAInt_63_593665BE4EF020F95F7D1A92564C1239);

                    ImGui::Spacing();
                    float halfW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                    if (ImGui::Button("Apply", ImVec2(halfW, 0)))
                        ApplyWeaponToPlayer(i);
                    ImGui::SameLine();
                    if (ImGui::Button("Remove", ImVec2(halfW, 0))) {
                        slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 = nullptr;
                        slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F = nullptr;
                        slot.GuardModule_21_774015784EB0300D2671C894D57ED144 = nullptr;
                        slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 = nullptr;
                        slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 = nullptr;
                        slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 = nullptr;
                        slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 = nullptr;
                        if (cfg.livePreview && i <= 1)
                            ApplyWeaponToPlayer(i);
                    }
                } else {
                    ImGui::TextDisabled("(empty slot)");
                }

                if (ImGui::Button(hasWeapon ? "Regenerate" : "Generate", ImVec2(-1, 0)))
                    GenerateWeaponForSlot(i);

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::PopID();
    }

    void RenderPresetsTab() {
        ImGui::PushID("presets");
        status.Render();
        GuiUtils::PresetPanelState panelState{presetNameBuf, sizeof(presetNameBuf), presetListDirty, presetTree, status, ComponentValidator::Validate(player)};
        GuiUtils::RenderPresetPanel(panelState, LoadoutPresetSerializer::GetPresetsDir(),
            [this]() { RefreshPresetTree(); },
            [this](const char* name) {
                auto data = BuildPresetFromPlayer();
                data.name = name;
                if (LoadoutPresetSerializer::SavePresetByName(data)) {
                    status.Set("Saved: " + std::string(name));
                    presetListDirty = true;
                } else {
                    status.Set("Error saving preset", true);
                }
            },
            [this](const std::filesystem::path& path) {
                auto result = LoadoutPresetSerializer::LoadFromFile(path);
                if (result.success) {
                    strncpy_s(presetNameBuf, result.name.c_str(), _TRUNCATE);
                    std::string loadedName = std::move(result.name);
                    ApplyLoadoutPreset(std::move(result));
                    status.Set("Loaded: " + loadedName);
                } else {
                    status.Set("Error: " + result.error, true);
                }
            },
            [this](const std::filesystem::path& path) {
                PresetUtils::DeletePreset(path);
                PresetUtils::CleanEmptyDirectories(LoadoutPresetSerializer::GetPresetsDir());
                presetListDirty = true;
            });
        ImGui::PopID();
    }

public:
    EquipmentManagerSection() : CollapsibleSection("Equipment Manager") {
        Function("Apply Loadout")
            .WithKey(&cfg.applyKey)
            .WithTooltip("Reapply the current equipment to the player")
            .Action([this]() { ApplyArmorToPlayer(); }, player);

        Function("Randomize Equipment")
            .WithKey(&cfg.randomizeKey)
            .WithParams({
                Parameter("live_preview", "Live Preview", &cfg.livePreview, "Auto-apply changes to the player"),
                Parameter("tier", "Generate Tier", &cfg.generateTier, 0, 8, "Tier for generated equipment")
            })
            .WithTooltip("Generate random armor for all equipped slots")
            .Action([this]() { RandomizeAllArmor(); }, player, world);
    }

    void RenderContent() override {
        SectionStyle::StyleRAII style;

        for (auto& function : functions) {
            function->Render();
            ImGui::Spacing();
        }

        if (cfg.livePreview && pendingSlotApply) {
            static constexpr double APPLY_COOLDOWN = 0.3;
            if (ImGui::GetTime() - lastSlotApplyTime >= APPLY_COOLDOWN) {
                ReapplyArmorSlot(pendingSlot);
                lastSlotApplyTime = ImGui::GetTime();
                pendingSlotApply = false;
            }
        }

        if (staggeredIdx < staggeredOps.size() && !staggeredBusy.load(std::memory_order_acquire)) {
            staggeredBusy.store(true, std::memory_order_release);
            auto op = staggeredOps[staggeredIdx++];
            GameHook::QueueAction([op, flag = &staggeredBusy]() {
                op();
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
            case 0: RenderArmorTab();   break;
            case 1: RenderWeaponsTab(); break;
            case 2: RenderPresetsTab(); break;
        }
    }
};
