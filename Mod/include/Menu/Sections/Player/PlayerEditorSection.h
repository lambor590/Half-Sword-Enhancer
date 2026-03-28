#pragma once

#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "SDK/Willie_BP_classes.hpp"
#include "Utils/PlayerPresetSerializer.h"
#include "Utils/PresetSectionState.h"
#include "Utils/GuiUtils.h"
#include "Utils/GameConstants.h"
#include "Utils/Spawner.h"

class PlayerEditorSection : public CollapsibleSection {
private:
    int enforceKey = -1;
    PlayerEditorOverrides overrides{};

    PresetSectionState<PlayerPresetSerializer> presets;
    int activeTab = 0;

    int CountActiveOverrides() const {
        return overrides.heightRate.enabled + overrides.muscleRate.enabled + overrides.scaleMutationInhibitor.enabled +
               overrides.health.enabled + overrides.headHealth.enabled + overrides.neckHealth.enabled +
               overrides.armRHealth.enabled + overrides.armLHealth.enabled + overrides.bodyUpperHealth.enabled +
               overrides.bodyLowerHealth.enabled + overrides.legRHealth.enabled + overrides.legLHealth.enabled +
               overrides.backHealth.enabled + overrides.consciousness.enabled + overrides.regenRate.enabled +
               overrides.allBodyTonus.enabled + overrides.headTonus.enabled + overrides.armRTonus.enabled +
               overrides.armLTonus.enabled + overrides.legRTonus.enabled + overrides.legLTonus.enabled +
               overrides.musclePower.enabled + overrides.orientationStrength.enabled +
               overrides.angularStrength.enabled + overrides.hitRigidity.enabled + overrides.runningSpeedRate.enabled +
               overrides.walkSpeedRateRun.enabled + overrides.jumpRate.enabled + overrides.dodgeRate.enabled +
               overrides.crawlRate.enabled + overrides.getUpRate.enabled + overrides.fallenRate.enabled +
               overrides.damageRate.enabled + overrides.limbDamageRate.enabled + overrides.dismemberThreshold.enabled +
               overrides.stamina.enabled + overrides.staminaBurnSwingR.enabled + overrides.staminaBurnSwingL.enabled +
               overrides.staminaBurnDodge.enabled + overrides.grabForceR.enabled + overrides.grabForceL.enabled +
               overrides.handsRigidity.enabled + overrides.bodySkill.enabled + overrides.weaponSkill.enabled +
               overrides.skillThrust.enabled + overrides.skillParry.enabled + overrides.skillAltGrip.enabled +
               overrides.skillAltStance.enabled + overrides.skillRotate.enabled + overrides.skillCrouch.enabled +
               overrides.skillDodge.enabled + overrides.skillKick.enabled + overrides.skillSlomo.enabled +
               overrides.exhaustion.enabled + overrides.drunk.enabled + overrides.fear.enabled +
               overrides.invulnerable.enabled + overrides.fearless.enabled;
    }

    static void ApplyActiveOverrides(SDK::AWillie_BP_C* p, PlayerEditorOverrides& ovr) {
        if (ovr.heightRate.enabled) {
            p->Height_Rate = ovr.heightRate.value;
            p->Character_Passport.Height_21_0EB204DF4978B92AD0ED188FD32EEC7B = ovr.heightRate.value;
        }
        if (ovr.muscleRate.enabled) p->Muscle_Rate = ovr.muscleRate.value;
        if (ovr.scaleMutationInhibitor.enabled) p->Scale_Mutation_Inhibitor = ovr.scaleMutationInhibitor.value;

        if (ovr.health.enabled) p->Health = ovr.health.value;
        if (ovr.headHealth.enabled) p->Head_Health = ovr.headHealth.value;
        if (ovr.neckHealth.enabled) p->Neck_Health = ovr.neckHealth.value;
        if (ovr.armRHealth.enabled) p->Arm_R_Health = ovr.armRHealth.value;
        if (ovr.armLHealth.enabled) p->Arm_L_Health = ovr.armLHealth.value;
        if (ovr.bodyUpperHealth.enabled) p->Body_Upper_Health = ovr.bodyUpperHealth.value;
        if (ovr.bodyLowerHealth.enabled) p->Body_Lower_Health = ovr.bodyLowerHealth.value;
        if (ovr.legRHealth.enabled) p->Leg_R_Health = ovr.legRHealth.value;
        if (ovr.legLHealth.enabled) p->Leg_L_Health = ovr.legLHealth.value;
        if (ovr.backHealth.enabled) p->Back_Health = ovr.backHealth.value;
        if (ovr.consciousness.enabled) p->Consciousness = ovr.consciousness.value;
        if (ovr.regenRate.enabled) p->Regen_Rate = ovr.regenRate.value;

        if (ovr.allBodyTonus.enabled) p->All_Body_Tonus = ovr.allBodyTonus.value;
        if (ovr.headTonus.enabled) p->Head_Tonus = ovr.headTonus.value;
        if (ovr.armRTonus.enabled) p->Arm_R_Tonus = ovr.armRTonus.value;
        if (ovr.armLTonus.enabled) p->Arm_L_Tonus = ovr.armLTonus.value;
        if (ovr.legRTonus.enabled) p->Leg_R_Tonus = ovr.legRTonus.value;
        if (ovr.legLTonus.enabled) p->Leg_L_Tonus = ovr.legLTonus.value;
        if (ovr.musclePower.enabled) p->Muscle_Power = ovr.musclePower.value;
        if (ovr.orientationStrength.enabled) p->Orientation_Strength = ovr.orientationStrength.value;
        if (ovr.angularStrength.enabled) p->Angular_Strength = ovr.angularStrength.value;
        if (ovr.hitRigidity.enabled) p->Hit_Rigidity = ovr.hitRigidity.value;

        if (ovr.runningSpeedRate.enabled) p->Running_Speed_Rate = ovr.runningSpeedRate.value;
        if (ovr.walkSpeedRateRun.enabled) p->Walk_Speed_Rate_Run = static_cast<float>(ovr.walkSpeedRateRun.value);
        if (ovr.jumpRate.enabled) p->Jump_Rate = ovr.jumpRate.value;
        if (ovr.dodgeRate.enabled) p->Dodge_Rate = ovr.dodgeRate.value;
        if (ovr.crawlRate.enabled) p->Crawl_Rate = ovr.crawlRate.value;
        if (ovr.getUpRate.enabled) p->Get_Up_Rate = ovr.getUpRate.value;
        if (ovr.fallenRate.enabled) p->Fallen_Rate = ovr.fallenRate.value;

        if (ovr.damageRate.enabled) p->Damage_Rate__Additional_ = ovr.damageRate.value;
        if (ovr.limbDamageRate.enabled) p->Limb_Damage_Rate__Additional_ = ovr.limbDamageRate.value;
        if (ovr.dismemberThreshold.enabled) p->Health_Threshold_For_Dismemberment = ovr.dismemberThreshold.value;
        if (ovr.stamina.enabled) p->Stamina = ovr.stamina.value;
        if (ovr.staminaBurnSwingR.enabled) p->Stamina_Burn_Swing_R = ovr.staminaBurnSwingR.value;
        if (ovr.staminaBurnSwingL.enabled) p->Stamina_Burn_Swing_L = ovr.staminaBurnSwingL.value;
        if (ovr.staminaBurnDodge.enabled) p->Stamina_Burn_Dodge = ovr.staminaBurnDodge.value;
        if (ovr.grabForceR.enabled) p->R_Grab_Force_Limit = ovr.grabForceR.value;
        if (ovr.grabForceL.enabled) p->L_Grab_Force_Limit = ovr.grabForceL.value;
        if (ovr.handsRigidity.enabled) p->Hands_Rigidity__Gauntlets_ = ovr.handsRigidity.value;
        if (ovr.bodySkill.enabled) p->Body_Skill__Temp_ = ovr.bodySkill.value;
        if (ovr.weaponSkill.enabled) p->Weapon_Skill__Temp_ = ovr.weaponSkill.value;

        if (ovr.skillThrust.enabled) p->Skill_Unlock_Weapon_Thrust = ovr.skillThrust.value;
        if (ovr.skillParry.enabled) p->Skill_Unlock_Weapon_Parry = ovr.skillParry.value;
        if (ovr.skillAltGrip.enabled) p->Skill_Unlock_Weapon_Alt_Grip = ovr.skillAltGrip.value;
        if (ovr.skillAltStance.enabled) p->Skill_Unlock_Weapon_Alt_Stance = ovr.skillAltStance.value;
        if (ovr.skillRotate.enabled) p->Skill_Unlock_Weapon_Rotate = ovr.skillRotate.value;
        if (ovr.skillCrouch.enabled) p->Skill_Unlock_Body_Crouch = ovr.skillCrouch.value;
        if (ovr.skillDodge.enabled) p->Skill_Unlock_Body_Dodge = ovr.skillDodge.value;
        if (ovr.skillKick.enabled) p->Skill_Unlock_Body_Kick = ovr.skillKick.value;
        if (ovr.skillSlomo.enabled) p->Skill_Unlock_Body_Slomo = ovr.skillSlomo.value;

        if (ovr.exhaustion.enabled) p->Exhaustion = ovr.exhaustion.value;
        if (ovr.drunk.enabled) p->Drunk = ovr.drunk.value;
        if (ovr.fear.enabled) p->Fear = ovr.fear.value;
        if (ovr.invulnerable.enabled) {
            p->Invulnerable = ovr.invulnerable.value;
            p->BitPad_5C_0 = ovr.invulnerable.value;
        }
        if (ovr.fearless.enabled) p->Fearless = ovr.fearless.value;
    }

    void ReadFromPlayer() {
        if (!player) return;

        overrides.heightRate.value = player->Height_Rate;
        overrides.muscleRate.value = player->Muscle_Rate;
        overrides.scaleMutationInhibitor.value = player->Scale_Mutation_Inhibitor;

        overrides.health.value = player->Health;
        overrides.headHealth.value = player->Head_Health;
        overrides.neckHealth.value = player->Neck_Health;
        overrides.armRHealth.value = player->Arm_R_Health;
        overrides.armLHealth.value = player->Arm_L_Health;
        overrides.bodyUpperHealth.value = player->Body_Upper_Health;
        overrides.bodyLowerHealth.value = player->Body_Lower_Health;
        overrides.legRHealth.value = player->Leg_R_Health;
        overrides.legLHealth.value = player->Leg_L_Health;
        overrides.backHealth.value = player->Back_Health;
        overrides.consciousness.value = player->Consciousness;
        overrides.regenRate.value = player->Regen_Rate;

        overrides.allBodyTonus.value = player->All_Body_Tonus;
        overrides.headTonus.value = player->Head_Tonus;
        overrides.armRTonus.value = player->Arm_R_Tonus;
        overrides.armLTonus.value = player->Arm_L_Tonus;
        overrides.legRTonus.value = player->Leg_R_Tonus;
        overrides.legLTonus.value = player->Leg_L_Tonus;
        overrides.musclePower.value = player->Muscle_Power;
        overrides.orientationStrength.value = player->Orientation_Strength;
        overrides.angularStrength.value = player->Angular_Strength;
        overrides.hitRigidity.value = player->Hit_Rigidity;

        overrides.runningSpeedRate.value = player->Running_Speed_Rate;
        overrides.walkSpeedRateRun.value = static_cast<double>(player->Walk_Speed_Rate_Run);
        overrides.jumpRate.value = player->Jump_Rate;
        overrides.dodgeRate.value = player->Dodge_Rate;
        overrides.crawlRate.value = player->Crawl_Rate;
        overrides.getUpRate.value = player->Get_Up_Rate;
        overrides.fallenRate.value = player->Fallen_Rate;

        overrides.damageRate.value = player->Damage_Rate__Additional_;
        overrides.limbDamageRate.value = player->Limb_Damage_Rate__Additional_;
        overrides.dismemberThreshold.value = player->Health_Threshold_For_Dismemberment;
        overrides.stamina.value = player->Stamina;
        overrides.staminaBurnSwingR.value = player->Stamina_Burn_Swing_R;
        overrides.staminaBurnSwingL.value = player->Stamina_Burn_Swing_L;
        overrides.staminaBurnDodge.value = player->Stamina_Burn_Dodge;
        overrides.grabForceR.value = player->R_Grab_Force_Limit;
        overrides.grabForceL.value = player->L_Grab_Force_Limit;
        overrides.handsRigidity.value = player->Hands_Rigidity__Gauntlets_;
        overrides.bodySkill.value = player->Body_Skill__Temp_;
        overrides.weaponSkill.value = player->Weapon_Skill__Temp_;

        overrides.skillThrust.value = player->Skill_Unlock_Weapon_Thrust;
        overrides.skillParry.value = player->Skill_Unlock_Weapon_Parry;
        overrides.skillAltGrip.value = player->Skill_Unlock_Weapon_Alt_Grip;
        overrides.skillAltStance.value = player->Skill_Unlock_Weapon_Alt_Stance;
        overrides.skillRotate.value = player->Skill_Unlock_Weapon_Rotate;
        overrides.skillCrouch.value = player->Skill_Unlock_Body_Crouch;
        overrides.skillDodge.value = player->Skill_Unlock_Body_Dodge;
        overrides.skillKick.value = player->Skill_Unlock_Body_Kick;
        overrides.skillSlomo.value = player->Skill_Unlock_Body_Slomo;

        overrides.exhaustion.value = player->Exhaustion;
        overrides.drunk.value = player->Drunk;
        overrides.fear.value = player->Fear;
        overrides.invulnerable.value = player->Invulnerable;
        overrides.fearless.value = player->Fearless;

        presets.status.Set("Values read from player");
    }

    PlayerPresetData BuildPresetData() const {
        PlayerPresetData d;
        d.overrides = overrides;
        return d;
    }

    void ApplyPresetData(const PlayerPresetData& d) { overrides = d.overrides; }

    void ClonePlayer() {
        if (!player || !world) return;
        auto passport = player->Character_Passport;
        if (overrides.heightRate.enabled)
            passport.Height_21_0EB204DF4978B92AD0ED188FD32EEC7B = overrides.heightRate.value;
        if (overrides.muscleRate.enabled)
            passport.Weight_23_65E4C6534D14653F96EB739F159E58CD = overrides.muscleRate.value;

        double heightRate = passport.Height_21_0EB204DF4978B92AD0ED188FD32EEC7B;
        double muscleRate = passport.Weight_23_65E4C6534D14653F96EB739F159E58CD;
        float spawnScale = static_cast<float>(0.875 + heightRate * 0.125);

        Spawner::SpawnActor(
            world, GameConstants::WILLIE_BP_PATH, Spawner::BuildSpawnTransform(player, 150.0f, 0.0f, spawnScale),
            [passport, heightRate, muscleRate](SDK::AActor* actor) {
                auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);
                npc->Character_Passport = passport;
                npc->Height_Rate = heightRate;
                npc->Muscle_Rate = muscleRate;
                npc->Team_Int = 1;
            }
        );
    }

    void RenderPhysicalTab() {
        ImGui::PushID("physical");

        ImGui::SeparatorText("Body");
        GuiUtils::RenderOverrideDrag("Height Rate", overrides.heightRate, 0.01f);
        TooltipHelper::ShowTooltip("Character height multiplier (1.0 = normal). Only takes effect at spawn");
        GuiUtils::RenderOverrideDrag("Muscle Rate", overrides.muscleRate, 0.01f);
        TooltipHelper::ShowTooltip("Character muscle/bulk multiplier (1.0 = normal)");
        GuiUtils::RenderOverrideDrag("Scale Mutation Inhibitor", overrides.scaleMutationInhibitor, 0.01f);
        TooltipHelper::ShowTooltip("Controls how much random scale variation is suppressed");

        ImGui::PopID();
    }

    void RenderHealthTab() {
        ImGui::PushID("health");

        ImGui::SeparatorText("General");
        GuiUtils::RenderOverrideDrag("Health", overrides.health, 1.0f);
        TooltipHelper::ShowTooltip("Overall health points");
        GuiUtils::RenderOverrideDrag("Consciousness", overrides.consciousness, 1.0f);
        TooltipHelper::ShowTooltip("Consciousness level (0 = knocked out)");
        GuiUtils::RenderOverrideDrag("Regen Rate", overrides.regenRate, 0.01f);
        TooltipHelper::ShowTooltip("Health regeneration rate per tick");

        ImGui::SeparatorText("Per-Limb Health");
        if (ImGui::BeginTable("##healthparts", 2, ImGuiTableFlags_None)) {
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Head", overrides.headHealth, 1.0f);
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Neck", overrides.neckHealth, 1.0f);

            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Right Arm##h", overrides.armRHealth, 1.0f);
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Left Arm##h", overrides.armLHealth, 1.0f);

            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Upper Body", overrides.bodyUpperHealth, 1.0f);
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Lower Body", overrides.bodyLowerHealth, 1.0f);

            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Right Leg##h", overrides.legRHealth, 1.0f);
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Left Leg##h", overrides.legLHealth, 1.0f);
            ImGui::EndTable();
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - SectionStyle::cellPadding.y);
        GuiUtils::RenderOverrideDrag("Back", overrides.backHealth, 1.0f);
        TooltipHelper::ShowTooltip("Back health");

        ImGui::PopID();
    }

    void RenderPhysicsTab() {
        ImGui::PushID("physics");

        ImGui::SeparatorText("Muscle Tonus");
        GuiUtils::RenderOverrideDrag("All Body Tonus", overrides.allBodyTonus, 1.0f);
        TooltipHelper::ShowTooltip("Master body muscle tension (100 = normal)");

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - SectionStyle::cellPadding.y);
        if (ImGui::BeginTable("##tonusparts", 2, ImGuiTableFlags_None)) {
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Head##t", overrides.headTonus, 0.01f);
            ImGui::TableNextColumn();
            ImGui::Dummy(ImVec2(0, 0));

            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Right Arm##t", overrides.armRTonus, 0.01f);
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Left Arm##t", overrides.armLTonus, 0.01f);

            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Right Leg##t", overrides.legRTonus, 0.01f);
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideDrag("Left Leg##t", overrides.legLTonus, 0.01f);
            ImGui::EndTable();
        }

        ImGui::SeparatorText("Strength");
        GuiUtils::RenderOverrideDrag("Muscle Power", overrides.musclePower, 0.5f);
        TooltipHelper::ShowTooltip("Overall muscle force (35 = default)");
        GuiUtils::RenderOverrideDrag("Orientation Strength", overrides.orientationStrength, 0.1f);
        GuiUtils::RenderOverrideDrag("Angular Strength", overrides.angularStrength, 0.1f);
        GuiUtils::RenderOverrideDrag("Hit Rigidity", overrides.hitRigidity, 0.01f);
        TooltipHelper::ShowTooltip("How rigid the body stays when hit");

        ImGui::PopID();
    }

    void RenderMovementTab() {
        ImGui::PushID("movement");

        ImGui::SeparatorText("Speed");
        GuiUtils::RenderOverrideDrag("Running Speed Rate", overrides.runningSpeedRate, 0.1f);
        TooltipHelper::ShowTooltip("Running speed multiplier (1.5 = default)");
        GuiUtils::RenderOverrideDrag("Walk Speed Rate", overrides.walkSpeedRateRun, 0.1f);
        TooltipHelper::ShowTooltip("Walking/aiming speed rate");

        ImGui::SeparatorText("Actions");
        GuiUtils::RenderOverrideDrag("Jump Rate", overrides.jumpRate, 0.1f);
        TooltipHelper::ShowTooltip("Jump power multiplier");
        GuiUtils::RenderOverrideDrag("Dodge Rate", overrides.dodgeRate, 0.1f);
        TooltipHelper::ShowTooltip("Dodge speed/distance multiplier");
        GuiUtils::RenderOverrideDrag("Crawl Rate", overrides.crawlRate, 0.01f);
        TooltipHelper::ShowTooltip("Crawling speed multiplier");
        GuiUtils::RenderOverrideDrag("Get Up Rate", overrides.getUpRate, 0.1f);
        TooltipHelper::ShowTooltip("Speed of getting up from the ground");
        GuiUtils::RenderOverrideDrag("Fallen Rate", overrides.fallenRate, 0.01f);
        TooltipHelper::ShowTooltip("Rate at which the character recovers from falling");

        ImGui::PopID();
    }

    void RenderCombatTab() {
        ImGui::PushID("combat");

        ImGui::SeparatorText("Damage");
        GuiUtils::RenderOverrideDrag("Damage Rate", overrides.damageRate, 0.1f);
        TooltipHelper::ShowTooltip("Additional damage multiplier dealt");
        GuiUtils::RenderOverrideDrag("Limb Damage Rate", overrides.limbDamageRate, 0.1f);
        TooltipHelper::ShowTooltip("Additional limb-specific damage multiplier");
        GuiUtils::RenderOverrideDrag("Dismember Threshold", overrides.dismemberThreshold, 0.1f);
        TooltipHelper::ShowTooltip("Health threshold below which dismemberment can occur");

        ImGui::SeparatorText("Stamina");
        GuiUtils::RenderOverrideDrag("Stamina", overrides.stamina, 1.0f);
        TooltipHelper::ShowTooltip("Current stamina level (100 = full)");
        GuiUtils::RenderOverrideDrag("Swing R Burn", overrides.staminaBurnSwingR, 0.1f);
        TooltipHelper::ShowTooltip("Stamina cost for right-hand swings");
        GuiUtils::RenderOverrideDrag("Swing L Burn", overrides.staminaBurnSwingL, 0.1f);
        TooltipHelper::ShowTooltip("Stamina cost for left-hand swings");
        GuiUtils::RenderOverrideDrag("Dodge Burn", overrides.staminaBurnDodge, 0.1f);
        TooltipHelper::ShowTooltip("Stamina cost for dodging");

        ImGui::SeparatorText("Grip & Skill");
        GuiUtils::RenderOverrideDrag("Grab Force R", overrides.grabForceR, 100.0f);
        TooltipHelper::ShowTooltip("Right hand grip force limit (10000 = default)");
        GuiUtils::RenderOverrideDrag("Grab Force L", overrides.grabForceL, 100.0f);
        TooltipHelper::ShowTooltip("Left hand grip force limit (10000 = default)");
        GuiUtils::RenderOverrideDrag("Hands Rigidity", overrides.handsRigidity, 0.01f);
        TooltipHelper::ShowTooltip("Punch impact force (0.666 = default)");
        GuiUtils::RenderOverrideDrag("Body Skill", overrides.bodySkill, 0.1f);
        TooltipHelper::ShowTooltip("Overall combat skill level");
        GuiUtils::RenderOverrideDrag("Weapon Skill", overrides.weaponSkill, 0.1f);
        TooltipHelper::ShowTooltip("Weapon handling skill level");

        ImGui::PopID();
    }

    void RenderSkillsStateTab() {
        ImGui::PushID("skillstate");

        ImGui::SeparatorText("Weapon Skills");
        if (ImGui::BeginTable("##weaponskills", 2, ImGuiTableFlags_None)) {
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideBool("Thrust", overrides.skillThrust);
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideBool("Parry", overrides.skillParry);

            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideBool("Alt Grip", overrides.skillAltGrip);
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideBool("Alt Stance", overrides.skillAltStance);

            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideBool("Rotate", overrides.skillRotate);
            ImGui::TableNextColumn();
            ImGui::Dummy(ImVec2(0, 0));
            ImGui::EndTable();
        }

        ImGui::SeparatorText("Body Skills");
        if (ImGui::BeginTable("##bodyskills", 2, ImGuiTableFlags_None)) {
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideBool("Crouch", overrides.skillCrouch);
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideBool("Dodge##sk", overrides.skillDodge);

            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideBool("Kick", overrides.skillKick);
            ImGui::TableNextColumn();
            GuiUtils::RenderOverrideBool("Slow Motion", overrides.skillSlomo);
            ImGui::EndTable();
        }

        ImGui::SeparatorText("State");
        GuiUtils::RenderOverrideDrag("Exhaustion", overrides.exhaustion, 0.1f);
        TooltipHelper::ShowTooltip("Physical exhaustion level");
        GuiUtils::RenderOverrideDrag("Drunk", overrides.drunk, 0.01f);
        TooltipHelper::ShowTooltip("Drunkenness level (0 = sober, 1 = fully drunk)");
        GuiUtils::RenderOverrideDrag("Fear", overrides.fear, 0.1f);
        TooltipHelper::ShowTooltip("Fear level");
        GuiUtils::RenderOverrideBool("Invulnerable", overrides.invulnerable);
        TooltipHelper::ShowTooltip("Immune to all damage");
        GuiUtils::RenderOverrideBool("Fearless", overrides.fearless);
        TooltipHelper::ShowTooltip("Never flees from combat");

        ImGui::PopID();
    }

public:
    PlayerEditorSection() : CollapsibleSection("Editor") {
        Function("Enforce Overrides")
            .OnEvent(GameHook::GameEvent::OffLedge)
            .WithKey(&enforceKey)
            .Toggle()
            .WithTooltip("Continuously applies all enabled overrides to the player character every game tick")
            .Action(
                [this](bool active) {
                    if (active) ApplyActiveOverrides(player, overrides);
                },
                player
            );
    }

    void RenderContent() override {
        const SectionStyle::StyleRAII style;
        ComponentValidator::Validate(player);
        ComponentValidator::Validate(controller);
        ComponentValidator::Validate(world);

        for (auto& function : functions) {
            function->Render();
        }

        ImGui::Spacing();

        if (ImGui::Button("Read from Player")) {
            ReadFromPlayer();
        }
        TooltipHelper::ShowTooltip("Reads current player values into override fields (without enabling them)");
        ImGui::SameLine();
        if (ImGui::Button("Reset All Overrides")) {
            overrides = {};
        }
        TooltipHelper::ShowTooltip("Disable all property overrides");
        ImGui::SameLine();
        if (ImGui::Button("Clone Player")) {
            if (player && world) {
                ClonePlayer();
                presets.status.Set("Player cloned");
            }
        }
        TooltipHelper::ShowTooltip("Spawns a clone of the player with the current physical overrides");

        GuiUtils::RenderOverrideCount(CountActiveOverrides());
        presets.status.Render();

        ImGui::Spacing();
        ImGui::BeginChild("##playereditor_scroll", ImVec2(0, 0));

        static constexpr const char* TAB_LABELS[] = {"Physical", "Health",         "Physics", "Movement",
                                                     "Combat",   "Skills & State", "Presets"};
        GuiUtils::RenderUnderlineTabs("##PlayerEditorTabs", activeTab, TAB_LABELS, 7);
        switch (activeTab) {
            case 0: RenderPhysicalTab(); break;
            case 1: RenderHealthTab(); break;
            case 2: RenderPhysicsTab(); break;
            case 3: RenderMovementTab(); break;
            case 4: RenderCombatTab(); break;
            case 5: RenderSkillsStateTab(); break;
            case 6:
                presets.RenderPresetsTab(
                    [this]() { return BuildPresetData(); },
                    [this](PlayerPresetData d) { ApplyPresetData(std::move(d)); }
                );
                break;
        }

        ImGui::EndChild();
    }
};
