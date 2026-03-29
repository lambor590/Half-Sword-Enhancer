#include "Menu/Sections/Player/PlayerEditorSection.h"
#include "Menu/SectionRegistry.h"
#include "Menu/SectionStyle.h"
#include "ComponentValidator.h"

REGISTER_SECTION(PlayerEditorSection, MenuTab::Player);
#include "Utils/GuiUtils.h"
#include "Utils/Spawner.h"

// ── Descriptor construction ───────────────────────────────────────────

void PlayerEditorSection::BuildDescriptors() {
    auto& o = overrides;

    physicalFields = {
        OverrideField("Height Rate", o.heightRate, 0.0, 0.0, 0.0, 0.01f),
        OverrideField("Muscle Rate", o.muscleRate, 0.0, 0.0, 0.0, 0.01f),
        OverrideField("Scale Mutation Inhibitor", o.scaleMutationInhibitor, 0.0, 0.0, 0.0, 0.01f),
    };
    healthFields = {
        OverrideField("Health", o.health, 0.0, 0.0, 0.0, 1.0f),
        OverrideField("Head", o.headHealth, 0.0, 0.0, 0.0, 1.0f),
        OverrideField("Neck", o.neckHealth, 0.0, 0.0, 0.0, 1.0f),
        OverrideField("Right Arm##h", o.armRHealth, 0.0, 0.0, 0.0, 1.0f),
        OverrideField("Left Arm##h", o.armLHealth, 0.0, 0.0, 0.0, 1.0f),
        OverrideField("Upper Body", o.bodyUpperHealth, 0.0, 0.0, 0.0, 1.0f),
        OverrideField("Lower Body", o.bodyLowerHealth, 0.0, 0.0, 0.0, 1.0f),
        OverrideField("Right Leg##h", o.legRHealth, 0.0, 0.0, 0.0, 1.0f),
        OverrideField("Left Leg##h", o.legLHealth, 0.0, 0.0, 0.0, 1.0f),
        OverrideField("Back", o.backHealth, 0.0, 0.0, 0.0, 1.0f),
        OverrideField("Consciousness", o.consciousness, 0.0, 0.0, 0.0, 1.0f),
        OverrideField("Regen Rate", o.regenRate, 0.0, 0.0, 0.0, 0.01f),
    };
    physicsFields = {
        OverrideField("All Body Tonus", o.allBodyTonus, 0.0, 0.0, 0.0, 1.0f),
        OverrideField("Head##t", o.headTonus, 0.0, 0.0, 0.0, 0.01f),
        OverrideField("Right Arm##t", o.armRTonus, 0.0, 0.0, 0.0, 0.01f),
        OverrideField("Left Arm##t", o.armLTonus, 0.0, 0.0, 0.0, 0.01f),
        OverrideField("Right Leg##t", o.legRTonus, 0.0, 0.0, 0.0, 0.01f),
        OverrideField("Left Leg##t", o.legLTonus, 0.0, 0.0, 0.0, 0.01f),
        OverrideField("Muscle Power", o.musclePower, 0.0, 0.0, 0.0, 0.5f),
        OverrideField("Orientation Strength", o.orientationStrength, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Angular Strength", o.angularStrength, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Hit Rigidity", o.hitRigidity, 0.0, 0.0, 0.0, 0.01f),
    };
    movementFields = {
        OverrideField("Running Speed Rate", o.runningSpeedRate, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Walk Speed Rate", o.walkSpeedRateRun, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Jump Rate", o.jumpRate, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Dodge Rate", o.dodgeRate, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Crawl Rate", o.crawlRate, 0.0, 0.0, 0.0, 0.01f),
        OverrideField("Get Up Rate", o.getUpRate, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Fallen Rate", o.fallenRate, 0.0, 0.0, 0.0, 0.01f),
    };
    combatFields = {
        OverrideField("Damage Rate", o.damageRate, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Limb Damage Rate", o.limbDamageRate, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Dismember Threshold", o.dismemberThreshold, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Stamina", o.stamina, 0.0, 0.0, 0.0, 1.0f),
        OverrideField("Swing R Burn", o.staminaBurnSwingR, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Swing L Burn", o.staminaBurnSwingL, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Dodge Burn", o.staminaBurnDodge, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Grab Force R", o.grabForceR, 0.0, 0.0, 0.0, 100.0f),
        OverrideField("Grab Force L", o.grabForceL, 0.0, 0.0, 0.0, 100.0f),
        OverrideField("Hands Rigidity", o.handsRigidity, 0.0, 0.0, 0.0, 0.01f),
        OverrideField("Body Skill", o.bodySkill, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Weapon Skill", o.weaponSkill, 0.0, 0.0, 0.0, 0.1f),
    };
    skillFields = {
        OverrideField("Thrust", o.skillThrust),     OverrideField("Parry", o.skillParry),
        OverrideField("Alt Grip", o.skillAltGrip),  OverrideField("Alt Stance", o.skillAltStance),
        OverrideField("Rotate", o.skillRotate),     OverrideField("Crouch", o.skillCrouch),
        OverrideField("Dodge##sk", o.skillDodge),   OverrideField("Kick", o.skillKick),
        OverrideField("Slow Motion", o.skillSlomo),
    };
    stateFields = {
        OverrideField("Exhaustion", o.exhaustion, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Drunk", o.drunk, 0.0, 0.0, 0.0, 0.01f),
        OverrideField("Fear", o.fear, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Invulnerable", o.invulnerable),
        OverrideField("Fearless", o.fearless),
    };
}

// ── Active override counting via descriptors ──────────────────────────

int PlayerEditorSection::CountAllActive() const {
    return CountActive(physicalFields) + CountActive(healthFields) + CountActive(physicsFields) +
           CountActive(movementFields) + CountActive(combatFields) + CountActive(skillFields) +
           CountActive(stateFields);
}

// ── Apply overrides per group ─────────────────────────────────────────
// Each group uses ApplyAll() to iterate only enabled descriptors.
// Pointer identity on the descriptor's value field dispatches to the correct
// SDK property. This replaces 55+ manual if(.enabled) chains with descriptor
// iteration. Special cases (heightRate → passport, invulnerable → BitPad)
// are handled inline in their respective group applier.

namespace {

    void ApplyPhysical(std::span<const OverrideDescriptor> fields, SDK::AWillie_BP_C* p, PlayerEditorOverrides& o) {
        ApplyAll(fields, [p, &o](const OverrideDescriptor& f) {
            double v = GetDouble(f);
            if (f.value == &o.heightRate.value) {
                p->Height_Rate = v;
                p->Character_Passport.Height_21_0EB204DF4978B92AD0ED188FD32EEC7B = v;
            } else if (f.value == &o.muscleRate.value) {
                p->Muscle_Rate = v;
            } else {
                p->Scale_Mutation_Inhibitor = v;
            }
        });
    }

    void ApplyHealth(std::span<const OverrideDescriptor> fields, SDK::AWillie_BP_C* p, PlayerEditorOverrides& o) {
        ApplyAll(fields, [p, &o](const OverrideDescriptor& f) {
            double v = GetDouble(f);
            if (f.value == &o.health.value)
                p->Health = v;
            else if (f.value == &o.headHealth.value)
                p->Head_Health = v;
            else if (f.value == &o.neckHealth.value)
                p->Neck_Health = v;
            else if (f.value == &o.armRHealth.value)
                p->Arm_R_Health = v;
            else if (f.value == &o.armLHealth.value)
                p->Arm_L_Health = v;
            else if (f.value == &o.bodyUpperHealth.value)
                p->Body_Upper_Health = v;
            else if (f.value == &o.bodyLowerHealth.value)
                p->Body_Lower_Health = v;
            else if (f.value == &o.legRHealth.value)
                p->Leg_R_Health = v;
            else if (f.value == &o.legLHealth.value)
                p->Leg_L_Health = v;
            else if (f.value == &o.backHealth.value)
                p->Back_Health = v;
            else if (f.value == &o.consciousness.value)
                p->Consciousness = v;
            else if (f.value == &o.regenRate.value)
                p->Regen_Rate = v;
        });
    }

    void ApplyPhysics(std::span<const OverrideDescriptor> fields, SDK::AWillie_BP_C* p, PlayerEditorOverrides& o) {
        ApplyAll(fields, [p, &o](const OverrideDescriptor& f) {
            double v = GetDouble(f);
            if (f.value == &o.allBodyTonus.value)
                p->All_Body_Tonus = v;
            else if (f.value == &o.headTonus.value)
                p->Head_Tonus = v;
            else if (f.value == &o.armRTonus.value)
                p->Arm_R_Tonus = v;
            else if (f.value == &o.armLTonus.value)
                p->Arm_L_Tonus = v;
            else if (f.value == &o.legRTonus.value)
                p->Leg_R_Tonus = v;
            else if (f.value == &o.legLTonus.value)
                p->Leg_L_Tonus = v;
            else if (f.value == &o.musclePower.value)
                p->Muscle_Power = v;
            else if (f.value == &o.orientationStrength.value)
                p->Orientation_Strength = v;
            else if (f.value == &o.angularStrength.value)
                p->Angular_Strength = v;
            else if (f.value == &o.hitRigidity.value)
                p->Hit_Rigidity = v;
        });
    }

    void ApplyMovement(std::span<const OverrideDescriptor> fields, SDK::AWillie_BP_C* p, PlayerEditorOverrides& o) {
        ApplyAll(fields, [p, &o](const OverrideDescriptor& f) {
            double v = GetDouble(f);
            if (f.value == &o.runningSpeedRate.value)
                p->Running_Speed_Rate = v;
            else if (f.value == &o.walkSpeedRateRun.value)
                p->Walk_Speed_Rate_Run = static_cast<float>(v);
            else if (f.value == &o.jumpRate.value)
                p->Jump_Rate = v;
            else if (f.value == &o.dodgeRate.value)
                p->Dodge_Rate = v;
            else if (f.value == &o.crawlRate.value)
                p->Crawl_Rate = v;
            else if (f.value == &o.getUpRate.value)
                p->Get_Up_Rate = v;
            else if (f.value == &o.fallenRate.value)
                p->Fallen_Rate = v;
        });
    }

    void ApplyCombat(std::span<const OverrideDescriptor> fields, SDK::AWillie_BP_C* p, PlayerEditorOverrides& o) {
        ApplyAll(fields, [p, &o](const OverrideDescriptor& f) {
            double v = GetDouble(f);
            if (f.value == &o.damageRate.value)
                p->Damage_Rate__Additional_ = v;
            else if (f.value == &o.limbDamageRate.value)
                p->Limb_Damage_Rate__Additional_ = v;
            else if (f.value == &o.dismemberThreshold.value)
                p->Health_Threshold_For_Dismemberment = v;
            else if (f.value == &o.stamina.value)
                p->Stamina = v;
            else if (f.value == &o.staminaBurnSwingR.value)
                p->Stamina_Burn_Swing_R = v;
            else if (f.value == &o.staminaBurnSwingL.value)
                p->Stamina_Burn_Swing_L = v;
            else if (f.value == &o.staminaBurnDodge.value)
                p->Stamina_Burn_Dodge = v;
            else if (f.value == &o.grabForceR.value)
                p->R_Grab_Force_Limit = v;
            else if (f.value == &o.grabForceL.value)
                p->L_Grab_Force_Limit = v;
            else if (f.value == &o.handsRigidity.value)
                p->Hands_Rigidity__Gauntlets_ = v;
            else if (f.value == &o.bodySkill.value)
                p->Body_Skill__Temp_ = v;
            else if (f.value == &o.weaponSkill.value)
                p->Weapon_Skill__Temp_ = v;
        });
    }

    void ApplySkills(std::span<const OverrideDescriptor> fields, SDK::AWillie_BP_C* p, PlayerEditorOverrides& o) {
        ApplyAll(fields, [p, &o](const OverrideDescriptor& f) {
            bool v = GetBool(f);
            if (f.value == &o.skillThrust.value)
                p->Skill_Unlock_Weapon_Thrust = v;
            else if (f.value == &o.skillParry.value)
                p->Skill_Unlock_Weapon_Parry = v;
            else if (f.value == &o.skillAltGrip.value)
                p->Skill_Unlock_Weapon_Alt_Grip = v;
            else if (f.value == &o.skillAltStance.value)
                p->Skill_Unlock_Weapon_Alt_Stance = v;
            else if (f.value == &o.skillRotate.value)
                p->Skill_Unlock_Weapon_Rotate = v;
            else if (f.value == &o.skillCrouch.value)
                p->Skill_Unlock_Body_Crouch = v;
            else if (f.value == &o.skillDodge.value)
                p->Skill_Unlock_Body_Dodge = v;
            else if (f.value == &o.skillKick.value)
                p->Skill_Unlock_Body_Kick = v;
            else if (f.value == &o.skillSlomo.value)
                p->Skill_Unlock_Body_Slomo = v;
        });
    }

    void ApplyState(std::span<const OverrideDescriptor> fields, SDK::AWillie_BP_C* p, PlayerEditorOverrides& o) {
        ApplyAll(fields, [p, &o](const OverrideDescriptor& f) {
            if (f.type == OverrideFieldType::Double) {
                double v = GetDouble(f);
                if (f.value == &o.exhaustion.value)
                    p->Exhaustion = v;
                else if (f.value == &o.drunk.value)
                    p->Drunk = v;
                else if (f.value == &o.fear.value)
                    p->Fear = v;
            } else {
                bool v = GetBool(f);
                if (f.value == &o.invulnerable.value) {
                    p->Invulnerable = v;
                    p->BitPad_5C_0 = v;
                } else {
                    p->Fearless = v;
                }
            }
        });
    }

} // namespace

void PlayerEditorSection::ApplyToPlayer(SDK::AWillie_BP_C* p) {
    ApplyPhysical(physicalFields, p, overrides);
    ApplyHealth(healthFields, p, overrides);
    ApplyPhysics(physicsFields, p, overrides);
    ApplyMovement(movementFields, p, overrides);
    ApplyCombat(combatFields, p, overrides);
    ApplySkills(skillFields, p, overrides);
    ApplyState(stateFields, p, overrides);
}

// ── Read current values from player into override fields ──────────────

void PlayerEditorSection::ReadFromPlayer() {
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

// ── Preset data conversion ────────────────────────────────────────────

PlayerPresetData PlayerEditorSection::BuildPresetData() const {
    PlayerPresetData d;
    d.overrides = overrides;
    return d;
}

void PlayerEditorSection::ApplyPresetData(const PlayerPresetData& d) {
    overrides = d.overrides;
}

// ── Clone player ──────────────────────────────────────────────────────

void PlayerEditorSection::ClonePlayer() {
    if (!player || !world) return;
    auto passport = player->Character_Passport;
    if (overrides.heightRate.enabled) passport.Height_21_0EB204DF4978B92AD0ED188FD32EEC7B = overrides.heightRate.value;
    if (overrides.muscleRate.enabled) passport.Weight_23_65E4C6534D14653F96EB739F159E58CD = overrides.muscleRate.value;

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

// ── Tab rendering (using RenderOverrideField from override system) ────

void PlayerEditorSection::RenderPhysicalTab() {
    ImGui::PushID("physical");
    ImGui::SeparatorText("Body");
    RenderOverrideField(physicalFields[0]);
    TooltipHelper::ShowTooltip("Character height multiplier (1.0 = normal). Only takes effect at spawn");
    RenderOverrideField(physicalFields[1]);
    TooltipHelper::ShowTooltip("Character muscle/bulk multiplier (1.0 = normal)");
    RenderOverrideField(physicalFields[2]);
    TooltipHelper::ShowTooltip("Controls how much random scale variation is suppressed");
    ImGui::PopID();
}

void PlayerEditorSection::RenderHealthTab() {
    ImGui::PushID("health");

    ImGui::SeparatorText("General");
    RenderOverrideField(healthFields[0]); // Health
    TooltipHelper::ShowTooltip("Overall health points");
    RenderOverrideField(healthFields[10]); // Consciousness
    TooltipHelper::ShowTooltip("Consciousness level (0 = knocked out)");
    RenderOverrideField(healthFields[11]); // Regen Rate
    TooltipHelper::ShowTooltip("Health regeneration rate per tick");

    ImGui::SeparatorText("Per-Limb Health");
    if (ImGui::BeginTable("##healthparts", 2, ImGuiTableFlags_None)) {
        ImGui::TableNextColumn();
        RenderOverrideField(healthFields[1]); // Head
        ImGui::TableNextColumn();
        RenderOverrideField(healthFields[2]); // Neck

        ImGui::TableNextColumn();
        RenderOverrideField(healthFields[3]); // Right Arm
        ImGui::TableNextColumn();
        RenderOverrideField(healthFields[4]); // Left Arm

        ImGui::TableNextColumn();
        RenderOverrideField(healthFields[5]); // Upper Body
        ImGui::TableNextColumn();
        RenderOverrideField(healthFields[6]); // Lower Body

        ImGui::TableNextColumn();
        RenderOverrideField(healthFields[7]); // Right Leg
        ImGui::TableNextColumn();
        RenderOverrideField(healthFields[8]); // Left Leg
        ImGui::EndTable();
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - SectionStyle::cellPadding.y);
    RenderOverrideField(healthFields[9]); // Back
    TooltipHelper::ShowTooltip("Back health");

    ImGui::PopID();
}

void PlayerEditorSection::RenderPhysicsTab() {
    ImGui::PushID("physics");

    ImGui::SeparatorText("Muscle Tonus");
    RenderOverrideField(physicsFields[0]); // All Body Tonus
    TooltipHelper::ShowTooltip("Master body muscle tension (100 = normal)");

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - SectionStyle::cellPadding.y);
    if (ImGui::BeginTable("##tonusparts", 2, ImGuiTableFlags_None)) {
        ImGui::TableNextColumn();
        RenderOverrideField(physicsFields[1]); // Head
        ImGui::TableNextColumn();
        ImGui::Dummy(ImVec2(0, 0));

        ImGui::TableNextColumn();
        RenderOverrideField(physicsFields[2]); // Right Arm
        ImGui::TableNextColumn();
        RenderOverrideField(physicsFields[3]); // Left Arm

        ImGui::TableNextColumn();
        RenderOverrideField(physicsFields[4]); // Right Leg
        ImGui::TableNextColumn();
        RenderOverrideField(physicsFields[5]); // Left Leg
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Strength");
    RenderOverrideField(physicsFields[6]); // Muscle Power
    TooltipHelper::ShowTooltip("Overall muscle force (35 = default)");
    RenderOverrideField(physicsFields[7]); // Orientation Strength
    RenderOverrideField(physicsFields[8]); // Angular Strength
    RenderOverrideField(physicsFields[9]); // Hit Rigidity
    TooltipHelper::ShowTooltip("How rigid the body stays when hit");

    ImGui::PopID();
}

void PlayerEditorSection::RenderMovementTab() {
    ImGui::PushID("movement");

    ImGui::SeparatorText("Speed");
    RenderOverrideField(movementFields[0]); // Running Speed Rate
    TooltipHelper::ShowTooltip("Running speed multiplier (1.5 = default)");
    RenderOverrideField(movementFields[1]); // Walk Speed Rate
    TooltipHelper::ShowTooltip("Walking/aiming speed rate");

    ImGui::SeparatorText("Actions");
    RenderOverrideField(movementFields[2]); // Jump Rate
    TooltipHelper::ShowTooltip("Jump power multiplier");
    RenderOverrideField(movementFields[3]); // Dodge Rate
    TooltipHelper::ShowTooltip("Dodge speed/distance multiplier");
    RenderOverrideField(movementFields[4]); // Crawl Rate
    TooltipHelper::ShowTooltip("Crawling speed multiplier");
    RenderOverrideField(movementFields[5]); // Get Up Rate
    TooltipHelper::ShowTooltip("Speed of getting up from the ground");
    RenderOverrideField(movementFields[6]); // Fallen Rate
    TooltipHelper::ShowTooltip("Rate at which the character recovers from falling");

    ImGui::PopID();
}

void PlayerEditorSection::RenderCombatTab() {
    ImGui::PushID("combat");

    ImGui::SeparatorText("Damage");
    RenderOverrideField(combatFields[0]); // Damage Rate
    TooltipHelper::ShowTooltip("Additional damage multiplier dealt");
    RenderOverrideField(combatFields[1]); // Limb Damage Rate
    TooltipHelper::ShowTooltip("Additional limb-specific damage multiplier");
    RenderOverrideField(combatFields[2]); // Dismember Threshold
    TooltipHelper::ShowTooltip("Health threshold below which dismemberment can occur");

    ImGui::SeparatorText("Stamina");
    RenderOverrideField(combatFields[3]); // Stamina
    TooltipHelper::ShowTooltip("Current stamina level (100 = full)");
    RenderOverrideField(combatFields[4]); // Swing R Burn
    TooltipHelper::ShowTooltip("Stamina cost for right-hand swings");
    RenderOverrideField(combatFields[5]); // Swing L Burn
    TooltipHelper::ShowTooltip("Stamina cost for left-hand swings");
    RenderOverrideField(combatFields[6]); // Dodge Burn
    TooltipHelper::ShowTooltip("Stamina cost for dodging");

    ImGui::SeparatorText("Grip & Skill");
    RenderOverrideField(combatFields[7]); // Grab Force R
    TooltipHelper::ShowTooltip("Right hand grip force limit (10000 = default)");
    RenderOverrideField(combatFields[8]); // Grab Force L
    TooltipHelper::ShowTooltip("Left hand grip force limit (10000 = default)");
    RenderOverrideField(combatFields[9]); // Hands Rigidity
    TooltipHelper::ShowTooltip("Punch impact force (0.666 = default)");
    RenderOverrideField(combatFields[10]); // Body Skill
    TooltipHelper::ShowTooltip("Overall combat skill level");
    RenderOverrideField(combatFields[11]); // Weapon Skill
    TooltipHelper::ShowTooltip("Weapon handling skill level");

    ImGui::PopID();
}

void PlayerEditorSection::RenderSkillsStateTab() {
    ImGui::PushID("skillstate");

    ImGui::SeparatorText("Weapon Skills");
    if (ImGui::BeginTable("##weaponskills", 2, ImGuiTableFlags_None)) {
        ImGui::TableNextColumn();
        RenderOverrideField(skillFields[0]); // Thrust
        ImGui::TableNextColumn();
        RenderOverrideField(skillFields[1]); // Parry

        ImGui::TableNextColumn();
        RenderOverrideField(skillFields[2]); // Alt Grip
        ImGui::TableNextColumn();
        RenderOverrideField(skillFields[3]); // Alt Stance

        ImGui::TableNextColumn();
        RenderOverrideField(skillFields[4]); // Rotate
        ImGui::TableNextColumn();
        ImGui::Dummy(ImVec2(0, 0));
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Body Skills");
    if (ImGui::BeginTable("##bodyskills", 2, ImGuiTableFlags_None)) {
        ImGui::TableNextColumn();
        RenderOverrideField(skillFields[5]); // Crouch
        ImGui::TableNextColumn();
        RenderOverrideField(skillFields[6]); // Dodge

        ImGui::TableNextColumn();
        RenderOverrideField(skillFields[7]); // Kick
        ImGui::TableNextColumn();
        RenderOverrideField(skillFields[8]); // Slow Motion
        ImGui::EndTable();
    }

    ImGui::SeparatorText("State");
    RenderOverrideField(stateFields[0]); // Exhaustion
    TooltipHelper::ShowTooltip("Physical exhaustion level");
    RenderOverrideField(stateFields[1]); // Drunk
    TooltipHelper::ShowTooltip("Drunkenness level (0 = sober, 1 = fully drunk)");
    RenderOverrideField(stateFields[2]); // Fear
    TooltipHelper::ShowTooltip("Fear level");
    RenderOverrideField(stateFields[3]); // Invulnerable
    TooltipHelper::ShowTooltip("Immune to all damage");
    RenderOverrideField(stateFields[4]); // Fearless
    TooltipHelper::ShowTooltip("Never flees from combat");

    ImGui::PopID();
}

// ── Constructor & main Render ─────────────────────────────────────────

PlayerEditorSection::PlayerEditorSection(ModContext& ctx) : Section(ctx, "Editor") {
    BuildDescriptors();
    InitKeybinds();
}

void PlayerEditorSection::InitKeybinds() {
    keybinds.push_back({
        .name = "Enforce Overrides",
        .tooltip = "Continuously applies all enabled overrides to the player character every game tick",
        .configSection = "EnforceOverrides",
        .keyPtr = &enforceKey,
        .callback =
            [this](bool active) {
                if (!ComponentValidator::Validate(player)) return;
                if (active) ApplyToPlayer(player);
            },
        .toggleable = true,
        .events = {GameEvent::OffLedge},
    });
    InitKeybindEntry(keybinds.back());
}

void PlayerEditorSection::Render() {
    const SectionStyle::StyleRAII style;
    ComponentValidator::Validate(player);
    ComponentValidator::Validate(controller);
    ComponentValidator::Validate(world);

    KeybindUI::RenderKeybindList(keybinds);

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

    GuiUtils::RenderOverrideCount(CountAllActive());
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
                [this]() { return BuildPresetData(); }, [this](PlayerPresetData d) { ApplyPresetData(std::move(d)); }
            );
            break;
    }

    ImGui::EndChild();
}
