#pragma once

#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Utils/Spawner.h"
#include "Utils/EquipmentGenerator.h"
#include "SDK/Willie_BP_classes.hpp"
#include "Utils/NPCPresetSerializer.h"
#include "Utils/NPCSpawnHelpers.h"
#include "Utils/PresetSectionState.h"
#include "Utils/PresetPickerState.h"
#include "Utils/LoadoutPresetSerializer.h"
#include "Utils/WeaponPassportBuilder.h"
#include "Utils/GuiUtils.h"

#define WILLIE_PATH(s) "/Game/Character/Blueprints" s

class NPCEditorSection : public CollapsibleSection {
private:
    struct NPCTypeInfo {
        const char* displayName;
        const char* className;
    };

    static constexpr int SPECIAL_TEAM_ID = 1337;

    SectionConfig::NPCConfig& cfg = SectionConfig::npc;

    static constexpr NPCTypeInfo npcTypes[] =
        {{"Regular", WILLIE_PATH("/Willie_BP.Willie_BP_C")},
         {"No Brain", WILLIE_PATH("/Willie_BP_NoBrain.Willie_BP_NoBrain_C")},
         {"Zombie", WILLIE_PATH("/Willie_BP_Zombie.Willie_BP_Zombie_C")},
         {"DressUp", WILLIE_PATH("/Willie_BP_DressUp.Willie_BP_DressUp_C")},
         {"Torso", WILLIE_PATH("/Willie_Torso_BP.Willie_Torso_BP_C")},
         {"Falcon Boss", WILLIE_PATH("/Unique/Willie_BP_FalconBoss.Willie_BP_FalconBoss_C")},
         {"Grim Reaper", WILLIE_PATH("/Unique/Willie_BP_GrimReaper.Willie_BP_GrimReaper_C")}};
#undef WILLIE_PATH
    static constexpr int npcTypesCount = sizeof(npcTypes) / sizeof(npcTypes[0]);

    static constexpr const char* nationalityNames[] = {"English", "French", "German", "Italian",
                                                       "Spanish", "Slavic", "Nordic"};
    static constexpr int nationalityCount = 7;

    NPCOverrides overrides{};

    PresetSectionState<NPCPresetSerializer> presets;
    PresetPickerState<LoadoutPresetSerializer> loadoutPicker;
    int activeTab = 0;

    const char* getNPCClassName() const noexcept {
        if (cfg.npcTypeIndex >= 0 && cfg.npcTypeIndex < npcTypesCount) [[likely]]
            return npcTypes[cfg.npcTypeIndex].className;
        return npcTypes[0].className;
    }

    int CountActiveOverrides() const {
        return overrides.heightRate.enabled + overrides.muscleRate.enabled + overrides.scaleMutationInhibitor.enabled +
               overrides.faceType.enabled + overrides.eyeColor.enabled + overrides.hairLength.enabled +
               overrides.hairColor.enabled + overrides.damageRate.enabled + overrides.limbDamageRate.enabled +
               overrides.dismemberThreshold.enabled + overrides.regenRate.enabled + overrides.aiInvincibility.enabled +
               overrides.aiArmorInvincibility.enabled + overrides.bodySkill.enabled + overrides.fearless.enabled +
               overrides.startKneeled.enabled + overrides.spawnInPants.enabled + overrides.clearSpawnArea.enabled +
               overrides.drunk.enabled + overrides.boltsInQuiver.enabled + overrides.headHealth.enabled +
               overrides.neckHealth.enabled + overrides.armRHealth.enabled + overrides.armLHealth.enabled +
               overrides.bodyUpperHealth.enabled + overrides.bodyLowerHealth.enabled + overrides.legRHealth.enabled +
               overrides.legLHealth.enabled;
    }


    void SpawnNPC() {
        auto className = std::string(getNPCClassName());
        auto nationality = static_cast<SDK::Enum_Nationalities>(cfg.npcNationality);
        auto tier = static_cast<SDK::Enum_Ranks>(cfg.npcTier);
        bool mercenary = cfg.npcMercenary;
        bool bodyguard = cfg.bodyguard;
        int team = cfg.npcTeam;
        auto ovr = overrides;
        bool hasOverrides = CountActiveOverrides() > 0;
        bool hasLoadout = loadoutPicker.HasSelection();

        LoadoutPresetData loadoutData;
        if (hasLoadout) {
            loadoutData = LoadoutPresetSerializer::LoadFromFile(loadoutPicker.SelectedPath());
            if (!loadoutData.success) hasLoadout = false;
        }

        double spawnScale = ovr.heightRate.enabled ? 0.875 + ovr.heightRate.value * 0.125 : cfg.spawn.scale;
        auto spawnTransform = Spawner::BuildSpawnTransform(
            player, cfg.spawn.distanceForward, cfg.spawn.distanceUp, static_cast<float>(spawnScale)
        );

        auto preCallback = [this, nationality, tier, mercenary, bodyguard, team, ovr, hasOverrides,
                            hasLoadout](SDK::AActor* actor) {
            auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);
            if (!npc) return;

            if (bodyguard) {
                npc->Team_Int = player->Team_Int;
            } else {
                npc->Team_Int = team;
            }

            EquipmentGenerator::Init(world);
            auto passport = EquipmentGenerator::GenerateCharacter(npc->Class, nationality, tier, mercenary);
            NPCSpawnHelpers::ApplyPassportOverrides(passport, ovr);
            npc->Character_Passport = passport;

            if (hasLoadout) npc->Spawn_in_Pants = true;

            if (hasOverrides) NPCSpawnHelpers::ApplyPropertyOverrides(npc, ovr);
        };

        auto postCallback = [w = world, ovr, hasOverrides, hasLoadout,
                             loadout = std::move(loadoutData)](SDK::AActor* actor) {
            auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);
            if (!npc) return;

            if (hasOverrides) NPCSpawnHelpers::ApplyHairColor(npc, ovr);

            if (hasLoadout) {
                for (const auto& sd : loadout.armorSlots) {
                    SDK::UClass* cls = sd.armorClass.empty() ? nullptr : Spawner::LoadClass(sd.armorClass);
                    if (!cls) continue;

                    SDK::FStr_Passport_Armor1 armorPassport{};
                    armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = cls;
                    armorPassport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393 = sd.color1;
                    armorPassport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C = sd.color2;
                    armorPassport.Slot_30_7561CB484566A4512003EA96ED44F88D = sd.slot;
                    Spawner::SpawnAndEquipArmor(w, npc, armorPassport);
                }

                auto& weapons = npc->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
                for (int i = 0; i < 7; ++i) {
                    const auto& wd = loadout.weaponSlots[i];
                    if (wd.weaponClass.empty()) continue;
                    auto& slot = LoadoutPresetSerializer::GetWeaponSlot(weapons, i);
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

                auto& slot0 = LoadoutPresetSerializer::GetWeaponSlot(weapons, 0);
                if (slot0.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066) {
                    auto p = WeaponPassportBuilder::FromWeaponParts(slot0);
                    npc->Set_Up_Right_Hand_Weapon(
                        slot0.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066, npc->Weapon_R, false, true, p
                    );
                }
                auto& slot1 = LoadoutPresetSerializer::GetWeaponSlot(weapons, 1);
                if (slot1.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066) {
                    auto p = WeaponPassportBuilder::FromWeaponParts(slot1);
                    npc->Set_Up_Left_Hand_Weapon(
                        slot1.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066, npc->Weapon_L, false, true, p
                    );
                }
            }
        };

        Spawner::SpawnActor(world, className, spawnTransform, preCallback, cfg.spawn.snapToGround, 4, postCallback);
    }

    NPCPresetData BuildPresetData() const {
        NPCPresetData d;
        d.npcTypeIndex = cfg.npcTypeIndex;
        d.nationality = cfg.npcNationality;
        d.tier = cfg.npcTier;
        d.mercenary = cfg.npcMercenary;
        d.overrides = overrides;
        return d;
    }

    void ApplyPresetData(const NPCPresetData& d) {
        cfg.npcTypeIndex = std::clamp(d.npcTypeIndex, 0, npcTypesCount - 1);
        cfg.npcNationality = std::clamp(d.nationality, 0, nationalityCount - 1);
        cfg.npcTier = std::clamp(d.tier, 0, 8);
        cfg.npcMercenary = d.mercenary;
        overrides = d.overrides;
    }

    void RenderPhysicalTab() {
        ImGui::PushID("physical");

        ImGui::SeparatorText("Body");
        GuiUtils::RenderOverrideDrag("Height Rate", overrides.heightRate, 0.01f);
        TooltipHelper::ShowTooltip("Character height multiplier (1.0 = normal)");
        GuiUtils::RenderOverrideDrag("Muscle Rate", overrides.muscleRate, 0.01f);
        TooltipHelper::ShowTooltip("Character muscle/bulk multiplier (1.0 = normal)");
        GuiUtils::RenderOverrideDrag("Scale Mutation Inhibitor", overrides.scaleMutationInhibitor, 0.01f);
        TooltipHelper::ShowTooltip("Controls how much random scale variation is suppressed");

        ImGui::SeparatorText("Appearance");
        GuiUtils::RenderOverrideInt("Face Type", overrides.faceType);
        TooltipHelper::ShowTooltip("Face mesh index");
        GuiUtils::RenderOverrideInt("Eye Color", overrides.eyeColor);
        TooltipHelper::ShowTooltip("Eye color index");
        GuiUtils::RenderOverrideDrag("Hair Length", overrides.hairLength, 0.01f);
        TooltipHelper::ShowTooltip("Hair length (0 = bald, 1 = maximum)");
        GuiUtils::RenderOverrideDrag("Hair Color", overrides.hairColor, 0.01f);
        TooltipHelper::ShowTooltip("Hair melanin (0 = blonde, 0.5 = brown, 1 = black)");

        ImGui::PopID();
    }

    void RenderCombatTab() {
        ImGui::PushID("combat");

        ImGui::SeparatorText("Damage");
        GuiUtils::RenderOverrideDrag("Damage Rate", overrides.damageRate, 0.1f);
        TooltipHelper::ShowTooltip("Additional damage multiplier dealt by this NPC");
        GuiUtils::RenderOverrideDrag("Limb Damage Rate", overrides.limbDamageRate, 0.1f);
        TooltipHelper::ShowTooltip("Additional limb-specific damage multiplier");
        GuiUtils::RenderOverrideDrag("Dismember Threshold", overrides.dismemberThreshold, 0.1f);
        TooltipHelper::ShowTooltip("Health threshold below which dismemberment can occur");

        ImGui::SeparatorText("Defense");
        GuiUtils::RenderOverrideDrag("Regen Rate", overrides.regenRate, 0.01f);
        TooltipHelper::ShowTooltip("Health regeneration rate per tick");
        GuiUtils::RenderOverrideDrag("AI Invincibility", overrides.aiInvincibility, 0.01f);
        TooltipHelper::ShowTooltip("Rate at which AI ignores incoming damage");
        GuiUtils::RenderOverrideDrag("AI Armor Invincibility", overrides.aiArmorInvincibility, 0.01f);
        TooltipHelper::ShowTooltip("Rate at which AI armor ignores damage");

        ImGui::SeparatorText("Skill");
        GuiUtils::RenderOverrideDrag("Body Skill", overrides.bodySkill, 0.1f);
        TooltipHelper::ShowTooltip("Overall combat skill level affecting movement and reactions");

        ImGui::PopID();
    }

    void RenderBehaviorTab() {
        ImGui::PushID("behavior");

        GuiUtils::RenderOverrideBool("Fearless", overrides.fearless);
        TooltipHelper::ShowTooltip("NPC never flees from combat");
        GuiUtils::RenderOverrideBool("Start Kneeled", overrides.startKneeled);
        TooltipHelper::ShowTooltip("NPC spawns in a kneeling position");
        GuiUtils::RenderOverrideBool("Spawn in Pants", overrides.spawnInPants);
        TooltipHelper::ShowTooltip("NPC spawns wearing only pants (no armor)");
        GuiUtils::RenderOverrideBool("Clear Spawn Area", overrides.clearSpawnArea);
        TooltipHelper::ShowTooltip("Clear objects around spawn point before spawning");

        ImGui::Spacing();
        GuiUtils::RenderOverrideDrag("Drunk", overrides.drunk, 0.01f);
        TooltipHelper::ShowTooltip("Drunkenness level (0 = sober, 1 = fully drunk)");
        GuiUtils::RenderOverrideInt("Bolts in Quiver", overrides.boltsInQuiver);
        TooltipHelper::ShowTooltip("Number of crossbow bolts the NPC carries");

        ImGui::PopID();
    }

    void RenderBodyConditionTab() {
        ImGui::PushID("bodycond");

        ImGui::SeparatorText("Starting Health Per Limb");
        TooltipHelper::ShowTooltip("Override starting health for each body part (0 = default game value)");

        if (ImGui::Button("Reset All")) {
            overrides.headHealth = {};
            overrides.neckHealth = {};
            overrides.armRHealth = {};
            overrides.armLHealth = {};
            overrides.bodyUpperHealth = {};
            overrides.bodyLowerHealth = {};
            overrides.legRHealth = {};
            overrides.legLHealth = {};
        }
        TooltipHelper::ShowTooltip("Disable all body condition overrides");

        ImGui::Spacing();
        if (ImGui::BeginTable("##bodyparts", 2, ImGuiTableFlags_None)) {
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Head", overrides.headHealth, 0.1f);
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Neck", overrides.neckHealth, 0.1f);

            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Right Arm", overrides.armRHealth, 0.1f);
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Left Arm", overrides.armLHealth, 0.1f);

            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Upper Body", overrides.bodyUpperHealth, 0.1f);
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Lower Body", overrides.bodyLowerHealth, 0.1f);

            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Right Leg", overrides.legRHealth, 0.1f);
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Left Leg", overrides.legLHealth, 0.1f);
            ImGui::EndTable();
        }

        ImGui::PopID();
    }

public:
    NPCEditorSection() : CollapsibleSection("NPC Editor") {
        Function("Spawn NPC")
            .WithKey(&cfg.spawnEnemyKey)
            .WithParams(
                {Parameter("bodyguard", "Bodyguard", &cfg.bodyguard, "Will join your team"),
                 Parameter("mercenary", "Mercenary", &cfg.npcMercenary, "Generate with mercenary color scheme"),
                 Parameter(
                     "snap_to_ground", "Snap to Ground", &cfg.spawn.snapToGround,
                     "Automatically adjust height to touch the ground"
                 ),
                 Parameter(
                     "distance_forward", "Distance Forward", &cfg.spawn.distanceForward, 100.0f, 500.0f,
                     "How far in front the NPC appears"
                 ),
                 Parameter(
                     "distance_up", "Distance Up", &cfg.spawn.distanceUp, 0.0f, 300.0f,
                     "Height offset for spawn position"
                 ),
                 Parameter(
                     "scale", "Scale", &cfg.spawn.scale, 0.1f, 4.0f,
                     "Size multiplier for the spawned NPC. Adjust the height offset to match the scale "
                     "so the game doesn't crash."
                 ),
                 Parameter(
                     "team", "Team", &cfg.npcTeam, 0, 9,
                     "Assign the NPC to a team number. 0-4 are the default teams. 0 means no team."
                 )}
            )
            .WithTooltip("Spawns an NPC with randomly generated equipment and applied overrides")
            .Action([this]() { SpawnNPC(); }, player, world);
    }

    void RenderContent() override {
        const SectionStyle::StyleRAII style;

        for (auto& function : functions) {
            function->Render();
            ImGui::Spacing();
        }

        auto npcGetter = [](void* data, int idx) -> const char* {
            return static_cast<const NPCTypeInfo*>(data)[idx].displayName;
        };
        static float npcTypeComboW = GuiUtils::CalcComboWidth(npcGetter, (void*)npcTypes, npcTypesCount);
        static float nationalityComboW = GuiUtils::CalcComboWidth(nationalityNames, nationalityCount);
        ImGui::SetNextItemWidth(npcTypeComboW);
        ImGui::Combo("##NPCTypeSelector", &cfg.npcTypeIndex, npcGetter, (void*)npcTypes, npcTypesCount);
        TooltipHelper::ShowTooltip("Choose which NPC class to spawn");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(nationalityComboW);
        ImGui::Combo("##NationalitySelector", &cfg.npcNationality, nationalityNames, nationalityCount);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(GuiUtils::CachedTierComboWidth());
        ImGui::DragInt("##NPCTierSlider", &cfg.npcTier, 0.1f, 0, 8, "Tier %d");

        ImGui::Spacing();
        if (ImGui::Button("Reset All Overrides")) {
            overrides = {};
        }
        TooltipHelper::ShowTooltip("Disable all NPC property overrides");
        GuiUtils::RenderOverrideCount(CountActiveOverrides());
        presets.status.Render();

        ImGui::Spacing();
        ImGui::BeginChild("##npc_scroll", ImVec2(0, 0));

        static constexpr const char* NPC_TAB_LABELS[] = {"Physical",       "Combat",    "Behavior",
                                                         "Body Condition", "Equipment", "Presets"};
        GuiUtils::RenderUnderlineTabs("##NPCEditorTabs", activeTab, NPC_TAB_LABELS, 6);
        switch (activeTab) {
            case 0: RenderPhysicalTab(); break;
            case 1: RenderCombatTab(); break;
            case 2: RenderBehaviorTab(); break;
            case 3: RenderBodyConditionTab(); break;
            case 4:
                ImGui::PushID("equipment");
                loadoutPicker.Render("Loadout Preset");
                if (loadoutPicker.HasSelection())
                    ImGui::TextColored(
                        ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "Equipment from preset will replace generated equipment"
                    );
                else
                    ImGui::TextDisabled("No preset selected — NPC will use randomly generated equipment");
                ImGui::PopID();
                break;
            case 5:
                presets.RenderPresetsTab(
                    [this]() { return BuildPresetData(); }, [this](NPCPresetData d) { ApplyPresetData(std::move(d)); }
                );
                break;
        }

        ImGui::EndChild();
    }
};
