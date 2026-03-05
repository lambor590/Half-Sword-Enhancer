#pragma once

#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Utils/Spawner.h"
#include "Utils/EquipmentGenerator.h"
#include "SDK/Willie_BP_classes.hpp"
#include "SDK/Str_Character_Body_Condition_structs.hpp"
#include "Utils/NPCPresetSerializer.h"
#include "Utils/PresetSectionState.h"
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

    static constexpr NPCTypeInfo npcTypes[] = {
        { "Regular", WILLIE_PATH("/Willie_BP.Willie_BP_C") },
        { "No Brain", WILLIE_PATH("/Willie_BP_NoBrain.Willie_BP_NoBrain_C") },
        { "Zombie", WILLIE_PATH("/Willie_BP_Zombie.Willie_BP_Zombie_C") },
        { "DressUp", WILLIE_PATH("/Willie_BP_DressUp.Willie_BP_DressUp_C") },
        { "Torso", WILLIE_PATH("/Willie_Torso_BP.Willie_Torso_BP_C") },
        { "Falcon Boss", WILLIE_PATH("/Unique/Willie_BP_FalconBoss.Willie_BP_FalconBoss_C") },
        { "Grim Reaper", WILLIE_PATH("/Unique/Willie_BP_GrimReaper.Willie_BP_GrimReaper_C") }
    };
#undef WILLIE_PATH
    static constexpr int npcTypesCount = sizeof(npcTypes) / sizeof(npcTypes[0]);

    static constexpr const char* nationalityNames[] = {
        "English", "French", "German", "Italian", "Spanish", "Slavic", "Nordic"
    };
    static constexpr int nationalityCount = 7;

    NPCOverrides overrides{};

    PresetSectionState<NPCPresetSerializer> presets;
    int activeTab = 0;

    const char* getNPCClassName() const noexcept {
        if (cfg.npcTypeIndex >= 0 && cfg.npcTypeIndex < npcTypesCount) [[likely]]
            return npcTypes[cfg.npcTypeIndex].className;
        return npcTypes[0].className;
    }

    int CountActiveOverrides() const {
        return overrides.heightRate.enabled + overrides.muscleRate.enabled
            + overrides.scaleMutationInhibitor.enabled + overrides.faceType.enabled
            + overrides.eyeColor.enabled + overrides.hairLength.enabled
            + overrides.hairColor.enabled + overrides.damageRate.enabled
            + overrides.limbDamageRate.enabled + overrides.dismemberThreshold.enabled
            + overrides.regenRate.enabled + overrides.aiInvincibility.enabled
            + overrides.aiArmorInvincibility.enabled + overrides.bodySkill.enabled
            + overrides.fearless.enabled + overrides.startKneeled.enabled
            + overrides.spawnInPants.enabled + overrides.clearSpawnArea.enabled
            + overrides.drunk.enabled + overrides.boltsInQuiver.enabled
            + overrides.headHealth.enabled + overrides.neckHealth.enabled
            + overrides.armRHealth.enabled + overrides.armLHealth.enabled
            + overrides.bodyUpperHealth.enabled + overrides.bodyLowerHealth.enabled
            + overrides.legRHealth.enabled + overrides.legLHealth.enabled;
    }

    static bool HasAnyBodyConditionOverride(const NPCOverrides& ovr) {
        return ovr.headHealth.enabled || ovr.neckHealth.enabled ||
               ovr.armRHealth.enabled || ovr.armLHealth.enabled ||
               ovr.bodyUpperHealth.enabled || ovr.bodyLowerHealth.enabled ||
               ovr.legRHealth.enabled || ovr.legLHealth.enabled;
    }

    static SDK::FLinearColor MelaninToColor(float melanin) {
        float m = std::clamp(melanin, 0.0f, 1.0f);
        float inv = 1.0f - m;
        return { inv * inv, inv * inv * inv, inv * inv * inv * inv, 1.0f };
    }

    static void ApplyPassportOverrides(SDK::FStr_Passport_Character1& passport,
                                       const NPCOverrides& ovr)
    {
        if (ovr.heightRate.enabled)
            passport.Height_21_0EB204DF4978B92AD0ED188FD32EEC7B = ovr.heightRate.value;
        if (ovr.muscleRate.enabled)
            passport.Weight_23_65E4C6534D14653F96EB739F159E58CD = ovr.muscleRate.value;
        if (ovr.bodySkill.enabled)
            passport.Skill_43_4CF5DCC248424BFADCCD6AB9F5F39CC9 = ovr.bodySkill.value;
        if (ovr.faceType.enabled)
            passport.FaceType_34_FB5E4D464B2A5CF6406C3CB19051FCE3 = ovr.faceType.value;
        if (ovr.eyeColor.enabled)
            passport.EyeColor_46_826504294B0D51C1343D848E8B1AB4C6 = ovr.eyeColor.value;
        if (ovr.hairLength.enabled)
            passport.HairLength_41_9295B3CF41DF9BED0FEDB9AE02E7FC16 = ovr.hairLength.value;
        if (ovr.hairColor.enabled)
            passport.HairColor_38_CBDC51B043E6816A062799A9A96EB232 = MelaninToColor(static_cast<float>(ovr.hairColor.value));
    }

    static void ApplyPropertyOverrides(SDK::AWillie_BP_C* npc, const NPCOverrides& ovr) {
        if (ovr.heightRate.enabled)           npc->Height_Rate = ovr.heightRate.value;
        if (ovr.muscleRate.enabled)           npc->Muscle_Rate = ovr.muscleRate.value;
        if (ovr.scaleMutationInhibitor.enabled) npc->Scale_Mutation_Inhibitor = ovr.scaleMutationInhibitor.value;

        if (ovr.damageRate.enabled)           npc->Damage_Rate__Additional_ = ovr.damageRate.value;
        if (ovr.limbDamageRate.enabled)       npc->Limb_Damage_Rate__Additional_ = ovr.limbDamageRate.value;
        if (ovr.dismemberThreshold.enabled)   npc->Health_Threshold_For_Dismemberment = ovr.dismemberThreshold.value;
        if (ovr.regenRate.enabled)            npc->Regen_Rate = ovr.regenRate.value;
        if (ovr.aiInvincibility.enabled)      npc->AI_Invincibility_Rate = ovr.aiInvincibility.value;
        if (ovr.aiArmorInvincibility.enabled) npc->AI_Armor_Invincibility_Rate = ovr.aiArmorInvincibility.value;
        if (ovr.bodySkill.enabled)            npc->Body_Skill__Temp_ = ovr.bodySkill.value;

        if (ovr.fearless.enabled)             npc->Fearless = ovr.fearless.value;
        if (ovr.startKneeled.enabled)         npc->Start_Kneeled = ovr.startKneeled.value;
        if (ovr.spawnInPants.enabled)         npc->Spawn_in_Pants = ovr.spawnInPants.value;
        if (ovr.clearSpawnArea.enabled)       npc->Clear_Spawn_Area = ovr.clearSpawnArea.value;
        if (ovr.drunk.enabled)                npc->Drunk = ovr.drunk.value;
        if (ovr.boltsInQuiver.enabled)        npc->Bolts_in_Quiver = ovr.boltsInQuiver.value;

        if (HasAnyBodyConditionOverride(ovr)) {
            SDK::FStr_Character_Body_Condition condition{};
            if (ovr.headHealth.enabled)      condition.HeadHealth_2_61859BB444171EF8952E0FA5DD8628EE = ovr.headHealth.value;
            if (ovr.neckHealth.enabled)      condition.NeckHealth_4_C658DC6A4BD1988C40F1A5B3C4F8F4EE = ovr.neckHealth.value;
            if (ovr.armRHealth.enabled)      condition.ArmRHealth_9_A65DD4C14ACBF6030A2B3AAD90FD0CFD = ovr.armRHealth.value;
            if (ovr.armLHealth.enabled)      condition.ArmLHealth_11_32345C31454A51B3CDE618918B9574F6 = ovr.armLHealth.value;
            if (ovr.bodyUpperHealth.enabled) condition.BodyUpperHealth_16_F71EA0C742135DC3B4F71EA3FEF07C46 = ovr.bodyUpperHealth.value;
            if (ovr.bodyLowerHealth.enabled) condition.BodyLowerHealth_18_37C008FF4FA0C0E5F5E09C9F0C174FE3 = ovr.bodyLowerHealth.value;
            if (ovr.legRHealth.enabled)      condition.LegRHealth_13_D50D4E174859A541DBEA66963D162E12 = ovr.legRHealth.value;
            if (ovr.legLHealth.enabled)      condition.LegLHealth_15_41C766B5460596C0804EA5B4B8F8EB36 = ovr.legLHealth.value;
            npc->Start_Body_Condition = condition;
        }
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

        double spawnScale = ovr.heightRate.enabled
            ? 0.875 + ovr.heightRate.value * 0.125
            : cfg.spawn.scale;
        auto spawnTransform = Spawner::BuildSpawnTransform(player, cfg.spawn.distanceForward, cfg.spawn.distanceUp, static_cast<float>(spawnScale));

        auto preCallback = [this, nationality, tier, mercenary, bodyguard, team, ovr, hasOverrides](SDK::AActor* actor) {
            auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);
            if (!npc) return;

            if (bodyguard) {
                player->Team_Int = SPECIAL_TEAM_ID;
                npc->Team_Int = player->Team_Int;
            } else {
                npc->Team_Int = team;
            }

            EquipmentGenerator::Init(world);
            auto passport = EquipmentGenerator::GenerateCharacter(
                npc->Class, nationality, tier, mercenary);
            ApplyPassportOverrides(passport, ovr);
            npc->Character_Passport = passport;

            if (hasOverrides)
                ApplyPropertyOverrides(npc, ovr);
        };

        std::function<void(SDK::AActor*)> postCallback = nullptr;
        if (hasOverrides) {
            postCallback = [ovr](SDK::AActor* actor) {
                auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);
                if (!npc) return;
                if (ovr.hairColor.enabled && npc->Hair_Mat) {
                    auto melaninName = SDK::BasicFilesImpleUtils::StringToName(L"Melanin");
                    npc->Hair_Mat->SetScalarParameterValue(melaninName, static_cast<float>(ovr.hairColor.value));
                }
            };
        }

        Spawner::SpawnActor(world, className, spawnTransform, preCallback, cfg.spawn.snapToGround, 4, postCallback);
    }

    NPCPresetData BuildPresetData() const {
        NPCPresetData d;
        d.name = presets.presetNameBuf;
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
    const char* GetGroup() const noexcept override { return "Editors"; }

    NPCEditorSection() : CollapsibleSection("NPC Editor") {
        Function("Spawn NPC")
            .WithKey(&cfg.spawnEnemyKey)
            .WithParams({
                Parameter("bodyguard", "Bodyguard", &cfg.bodyguard, "Will join your team"),
                Parameter("mercenary", "Mercenary", &cfg.npcMercenary, "Generate with mercenary color scheme"),
                Parameter("snap_to_ground", "Snap to Ground", &cfg.spawn.snapToGround, "Automatically adjust height to touch the ground"),
                Parameter("distance_forward", "Distance Forward", &cfg.spawn.distanceForward, 100.0f, 500.0f, "How far in front the NPC appears"),
                Parameter("distance_up", "Distance Up", &cfg.spawn.distanceUp, 0.0f, 300.0f, "Height offset for spawn position"),
                Parameter("scale", "Scale", &cfg.spawn.scale, 0.1f, 4.0f, "Size multiplier for the spawned NPC. Adjust the height offset to match the scale so the game doesn't crash."),
                Parameter("team", "Team", &cfg.npcTeam, 0, 9, "Assign the NPC to a team number. 0-4 are the default teams. 0 means no team.")
            })
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
        float spacing = ImGui::GetStyle().ItemSpacing.x;

        ImGui::SetNextItemWidth(npcTypeComboW);
        ImGui::Combo("##NPCTypeSelector", &cfg.npcTypeIndex,
            npcGetter, (void*)npcTypes, npcTypesCount);
        TooltipHelper::ShowTooltip("Choose which NPC class to spawn");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(nationalityComboW);
        ImGui::Combo("##NationalitySelector", &cfg.npcNationality,
            nationalityNames, nationalityCount);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(GuiUtils::CachedTierComboWidth());
        ImGui::DragInt("##NPCTierSlider", &cfg.npcTier, 0.1f, 0, 8, "Tier %d");

        ImGui::Spacing();
        if (ImGui::Button("Reset All Overrides")) {
            overrides = {};
        }
        TooltipHelper::ShowTooltip("Disable all NPC property overrides");
        int activeCount = CountActiveOverrides();
        if (activeCount > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%d active)", activeCount);
        }
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
        presets.status.Render();

        ImGui::Spacing();
        ImGui::BeginChild("##npc_scroll", ImVec2(0, 0));

        static constexpr const char* NPC_TAB_LABELS[] = {"Physical", "Combat", "Behavior", "Body Condition", "Presets"};
        GuiUtils::RenderUnderlineTabs("##NPCEditorTabs", activeTab, NPC_TAB_LABELS, 5);
        switch (activeTab) {
            case 0: RenderPhysicalTab();       break;
            case 1: RenderCombatTab();          break;
            case 2: RenderBehaviorTab();        break;
            case 3: RenderBodyConditionTab();   break;
            case 4: presets.RenderPresetsTab(
                        [this]() { return BuildPresetData(); },
                        [this](NPCPresetData d) { ApplyPresetData(std::move(d)); });
                    break;
        }

        ImGui::EndChild();
    }
};
