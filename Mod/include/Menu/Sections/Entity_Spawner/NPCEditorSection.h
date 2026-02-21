#pragma once

#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Utils/Spawner.h"
#include "Utils/EquipmentGenerator.h"
#include "SDK/Willie_BP_classes.hpp"
#include "SDK/Str_Character_Body_Condition_structs.hpp"
#include "Utils/NPCPresetSerializer.h"
#include "Utils/GuiUtils.h"

#define WILLIE_PATH(s) "/Game/Character/Blueprints" s

struct NPCTypeInfo {
    const char* displayName;
    const char* className;
};

class NPCEditorSection : public CollapsibleSection {
private:
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
    static constexpr int npcTypesCount = sizeof(npcTypes) / sizeof(npcTypes[0]);

    static constexpr const char* nationalityNames[] = {
        "English", "French", "German", "Italian", "Spanish", "Slavic", "Nordic"
    };
    static constexpr int nationalityCount = 7;

    NPCOverrides overrides{};

    char presetNameBuf[128] = {};
    PresetUtils::PresetTreeNode presetTree;
    bool presetListDirty = true;
    std::string statusMessage;
    double statusMessageTime = 0.0;
    bool statusIsError = false;
    int activeTab = 0;

    const char* getNPCClassName() const noexcept {
        if (cfg.npcTypeIndex >= 0 && cfg.npcTypeIndex < npcTypesCount) [[likely]]
            return npcTypes[cfg.npcTypeIndex].className;
        return npcTypes[0].className;
    }

    int CountActiveOverrides() const {
        const bool flags[] = {
            overrides.heightRate.enabled, overrides.muscleRate.enabled,
            overrides.scaleMutationInhibitor.enabled, overrides.faceType.enabled,
            overrides.eyeColor.enabled, overrides.hairLength.enabled, overrides.hairColor.enabled,
            overrides.damageRate.enabled, overrides.limbDamageRate.enabled,
            overrides.dismemberThreshold.enabled, overrides.regenRate.enabled,
            overrides.aiInvincibility.enabled, overrides.aiArmorInvincibility.enabled,
            overrides.bodySkill.enabled, overrides.fearless.enabled,
            overrides.startKneeled.enabled, overrides.spawnInPants.enabled,
            overrides.clearSpawnArea.enabled, overrides.drunk.enabled,
            overrides.boltsInQuiver.enabled,
            overrides.headHealth.enabled, overrides.neckHealth.enabled,
            overrides.armRHealth.enabled, overrides.armLHealth.enabled,
            overrides.bodyUpperHealth.enabled, overrides.bodyLowerHealth.enabled,
            overrides.legRHealth.enabled, overrides.legLHealth.enabled
        };
        int count = 0;
        for (bool f : flags) count += f;
        return count;
    }

    bool HasAnyOverride() const {
        return CountActiveOverrides() > 0;
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
        bool hasOverrides = HasAnyOverride();

        SDK::FTransform spawnTransform = player->GetTransform();
        spawnTransform.Translation += player->GetActorForwardVector() * cfg.spawnDistanceForward;
        spawnTransform.Translation.Z += cfg.spawnDistanceUp;
        spawnTransform.Scale3D = SDK::FVector(cfg.spawnScale, cfg.spawnScale, cfg.spawnScale);

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
                ApplyPropertyOverrides(npc, ovr);
                npc->Set_Character_Height();
                if (ovr.hairColor.enabled && npc->Hair_Mat) {
                    auto melaninName = SDK::BasicFilesImpleUtils::StringToName(L"Melanin");
                    npc->Hair_Mat->SetScalarParameterValue(melaninName, static_cast<float>(ovr.hairColor.value));
                }
            };
        }

        Spawner::SpawnActor(world, className, spawnTransform, preCallback, cfg.snapToGround, 4, postCallback);
    }

    NPCPresetData BuildPresetData() const {
        NPCPresetData d;
        d.name = presetNameBuf;
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

    void SetStatus(std::string msg, bool isError = false) {
        statusMessage = std::move(msg);
        statusMessageTime = ImGui::GetTime();
        statusIsError = isError;
    }

    void RefreshPresetTree() {
        presetTree = NPCPresetSerializer::ListPresetsTree();
        presetListDirty = false;
    }

    void RenderPhysicalTab() {
        ImGui::PushID("physical");

        ImGui::TextDisabled("Body");
        GuiUtils::RenderOverrideDrag("Height Rate", overrides.heightRate, 0.01f, 0.1f, 3.0f);
        TooltipHelper::ShowTooltip("Character height multiplier (1.0 = normal)");
        GuiUtils::RenderOverrideDrag("Muscle Rate", overrides.muscleRate, 0.01f, 0.1f, 3.0f);
        TooltipHelper::ShowTooltip("Character muscle/bulk multiplier (1.0 = normal)");
        GuiUtils::RenderOverrideDrag("Scale Mutation Inhibitor", overrides.scaleMutationInhibitor, 0.01f);
        TooltipHelper::ShowTooltip("Controls how much random scale variation is suppressed");

        ImGui::Spacing();
        ImGui::TextDisabled("Appearance");
        GuiUtils::RenderOverrideInt("Face Type", overrides.faceType, 0, 20);
        TooltipHelper::ShowTooltip("Face mesh index");
        GuiUtils::RenderOverrideInt("Eye Color", overrides.eyeColor, 0, 10);
        TooltipHelper::ShowTooltip("Eye color index");
        GuiUtils::RenderOverrideDrag("Hair Length", overrides.hairLength, 0.01f, 0.0f, 1.0f);
        TooltipHelper::ShowTooltip("Hair length (0 = bald, 1 = maximum)");
        GuiUtils::RenderOverrideDrag("Hair Color", overrides.hairColor, 0.01f, 0.0f, 1.0f);
        TooltipHelper::ShowTooltip("Hair melanin (0 = blonde, 0.5 = brown, 1 = black)");

        ImGui::PopID();
    }

    void RenderCombatTab() {
        ImGui::PushID("combat");

        ImGui::TextDisabled("Damage");
        GuiUtils::RenderOverrideDrag("Damage Rate", overrides.damageRate, 0.1f);
        TooltipHelper::ShowTooltip("Additional damage multiplier dealt by this NPC");
        GuiUtils::RenderOverrideDrag("Limb Damage Rate", overrides.limbDamageRate, 0.1f);
        TooltipHelper::ShowTooltip("Additional limb-specific damage multiplier");
        GuiUtils::RenderOverrideDrag("Dismember Threshold", overrides.dismemberThreshold, 0.1f);
        TooltipHelper::ShowTooltip("Health threshold below which dismemberment can occur");

        ImGui::Spacing();
        ImGui::TextDisabled("Defense");
        GuiUtils::RenderOverrideDrag("Regen Rate", overrides.regenRate, 0.01f);
        TooltipHelper::ShowTooltip("Health regeneration rate per tick");
        GuiUtils::RenderOverrideDrag("AI Invincibility", overrides.aiInvincibility, 0.01f);
        TooltipHelper::ShowTooltip("Rate at which AI ignores incoming damage");
        GuiUtils::RenderOverrideDrag("AI Armor Invincibility", overrides.aiArmorInvincibility, 0.01f);
        TooltipHelper::ShowTooltip("Rate at which AI armor ignores damage");

        ImGui::Spacing();
        ImGui::TextDisabled("Skill");
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
        GuiUtils::RenderOverrideDrag("Drunk", overrides.drunk, 0.01f, 0.0f, 1.0f);
        TooltipHelper::ShowTooltip("Drunkenness level (0 = sober, 1 = fully drunk)");
        GuiUtils::RenderOverrideInt("Bolts in Quiver", overrides.boltsInQuiver, 0, 50);
        TooltipHelper::ShowTooltip("Number of crossbow bolts the NPC carries");

        ImGui::PopID();
    }

    void RenderBodyConditionTab() {
        ImGui::PushID("bodycond");

        ImGui::TextDisabled("Starting Health Per Limb");
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
        GuiUtils::RenderOverrideDrag("Head", overrides.headHealth, 0.1f);
        GuiUtils::RenderOverrideDrag("Neck", overrides.neckHealth, 0.1f);
        GuiUtils::RenderOverrideDrag("Right Arm", overrides.armRHealth, 0.1f);
        GuiUtils::RenderOverrideDrag("Left Arm", overrides.armLHealth, 0.1f);
        GuiUtils::RenderOverrideDrag("Upper Body", overrides.bodyUpperHealth, 0.1f);
        GuiUtils::RenderOverrideDrag("Lower Body", overrides.bodyLowerHealth, 0.1f);
        GuiUtils::RenderOverrideDrag("Right Leg", overrides.legRHealth, 0.1f);
        GuiUtils::RenderOverrideDrag("Left Leg", overrides.legLHealth, 0.1f);

        ImGui::PopID();
    }

    void RenderPresetsTab() {
        ImGui::PushID("presets");

        ImGui::TextDisabled("Save");
        float btnWidth = ImGui::CalcTextSize("Save").x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - btnWidth - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputTextWithHint("##PresetName", "folder/name...", presetNameBuf, sizeof(presetNameBuf));
        ImGui::SameLine();
        bool canSave = presetNameBuf[0] != '\0';
        if (!canSave) ImGui::BeginDisabled();
        if (ImGui::Button("Save")) {
            auto data = BuildPresetData();
            if (NPCPresetSerializer::SavePresetByName(presetNameBuf, data)) {
                SetStatus("Saved: " + std::string(presetNameBuf));
                presetListDirty = true;
            } else {
                SetStatus("Error saving preset", true);
            }
        }
        if (!canSave) ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("Presets");
        if (presetListDirty)
            RefreshPresetTree();

        if (presetTree.presets.empty() && presetTree.children.empty()) {
            ImGui::TextDisabled("No saved presets");
        } else {
            ImGui::BeginChild("##presetList", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 8), ImGuiChildFlags_Borders);
            auto action = GuiUtils::RenderPresetTree(presetTree);
            ImGui::EndChild();

            if (action.type == GuiUtils::PresetTreeAction::Load) {
                auto result = NPCPresetSerializer::LoadFromFile(action.path);
                if (result.success) {
                    ApplyPresetData(result);
                    strncpy_s(presetNameBuf, result.name.c_str(), _TRUNCATE);
                    SetStatus("Loaded: " + result.name);
                } else {
                    SetStatus("Error: " + result.error, true);
                }
            } else if (action.type == GuiUtils::PresetTreeAction::Delete) {
                NPCPresetSerializer::DeletePreset(action.path);
                PresetUtils::CleanEmptyDirectories(NPCPresetSerializer::GetPresetsDirectory());
                presetListDirty = true;
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Open Presets Folder", ImVec2(-1, 0))) {
            PresetUtils::OpenInExplorer(NPCPresetSerializer::GetPresetsDirectory());
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
                Parameter("snap_to_ground", "Snap to Ground", &cfg.snapToGround, "Automatically adjust height to touch the ground"),
                Parameter("distance_forward", "Distance Forward", &cfg.spawnDistanceForward, 100.0f, 500.0f, "How far in front the NPC appears"),
                Parameter("distance_up", "Distance Up", &cfg.spawnDistanceUp, 0.0f, 300.0f, "Height offset for spawn position"),
                Parameter("scale", "Scale", &cfg.spawnScale, 0.1f, 4.0f, "Size multiplier for the spawned NPC. Adjust the height offset to match the scale so the game doesn't crash."),
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

        ImGui::Text("NPC Type");
        TooltipHelper::ShowTooltip("Choose which NPC class to spawn");
        auto npcGetter = [](void* data, int idx) -> const char* {
            return static_cast<const NPCTypeInfo*>(data)[idx].displayName;
        };
        static float npcTypeComboW = GuiUtils::CalcComboWidth(npcGetter, (void*)npcTypes, npcTypesCount);
        ImGui::SetNextItemWidth(npcTypeComboW);
        ImGui::Combo("##NPCTypeSelector", &cfg.npcTypeIndex,
            npcGetter, (void*)npcTypes, npcTypesCount);

        ImGui::Spacing();
        ImGui::Text("Nationality");
        static float nationalityComboW = GuiUtils::CalcComboWidth(nationalityNames, nationalityCount);
        ImGui::SetNextItemWidth(nationalityComboW);
        ImGui::Combo("##NationalitySelector", &cfg.npcNationality,
            nationalityNames, nationalityCount);

        ImGui::Text("Equipment Tier");
        ImGui::SliderInt("##NPCTierSlider", &cfg.npcTier, 0, 8, "Tier %d");

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

        if (!statusMessage.empty()) {
            if (ImGui::GetTime() - statusMessageTime > 3.0)
                statusMessage.clear();
            else {
                auto color = statusIsError ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
                ImGui::TextColored(color, "%s", statusMessage.c_str());
            }
        }

        ImGui::Spacing();
        static constexpr const char* NPC_TAB_LABELS[] = {"Physical", "Combat", "Behavior", "Body Condition", "Presets"};
        GuiUtils::RenderFullWidthTabs("##NPCEditorTabs", activeTab, NPC_TAB_LABELS, 5);
        switch (activeTab) {
            case 0: RenderPhysicalTab();       break;
            case 1: RenderCombatTab();          break;
            case 2: RenderBehaviorTab();        break;
            case 3: RenderBodyConditionTab();   break;
            case 4: RenderPresetsTab();         break;
        }
    }
};
