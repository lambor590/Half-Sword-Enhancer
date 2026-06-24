#include "Menu/Sections/Player/PlayerEditorSection.h"
#include "Menu/SectionStyle.h"
#include "Hooks/GameHook.h"
#include "SDK/Willie_BP_classes.hpp"
#include "Utils/GuiUtils.h"
#include "Utils/Spawner.h"

void PlayerEditorSection::BuildDescriptors() {
    auto& o = overrides;

    physicalFields = {
        OverrideField(
            "Height Rate", o.heightRate, 0.01f, "Character height multiplier (1.0 = normal). Only takes effect at spawn"
        ),
        OverrideField("Muscle Rate", o.muscleRate, 0.01f, "Character muscle/bulk multiplier (1.0 = normal)"),
        OverrideField(
            "Scale Mutation Inhibitor", o.scaleMutationInhibitor, 0.01f,
            "Controls how much random scale variation is suppressed"
        ),
    };
    healthFields = {
        OverrideField("Health", o.health, 1.0f, "Overall health points"),
        OverrideField("Head", o.headHealth, 1.0f),
        OverrideField("Neck", o.neckHealth, 1.0f),
        OverrideField("Right Arm##h", o.armRHealth, 1.0f),
        OverrideField("Left Arm##h", o.armLHealth, 1.0f),
        OverrideField("Upper Body", o.bodyUpperHealth, 1.0f),
        OverrideField("Lower Body", o.bodyLowerHealth, 1.0f),
        OverrideField("Right Leg##h", o.legRHealth, 1.0f),
        OverrideField("Left Leg##h", o.legLHealth, 1.0f),
        OverrideField("Back", o.backHealth, 1.0f, "Back health"),
        OverrideField("Consciousness", o.consciousness, 1.0f, "Consciousness level (0 = knocked out)"),
        OverrideField("Regen Rate", o.regenRate, 0.01f, "Health regeneration rate per tick"),
    };
    physicsFields = {
        OverrideField("All Body Tonus", o.allBodyTonus, 1.0f, "Master body muscle tension (100 = normal)"),
        OverrideField("Head##t", o.headTonus, 0.01f),
        OverrideField("Right Arm##t", o.armRTonus, 0.01f),
        OverrideField("Left Arm##t", o.armLTonus, 0.01f),
        OverrideField("Right Leg##t", o.legRTonus, 0.01f),
        OverrideField("Left Leg##t", o.legLTonus, 0.01f),
        OverrideField("Muscle Power", o.musclePower, 0.5f, "Overall muscle force (35 = default)"),
        OverrideField("Orientation Strength", o.orientationStrength, 0.1f),
        OverrideField("Angular Strength", o.angularStrength, 0.1f),
        OverrideField("Hit Rigidity", o.hitRigidity, 0.01f, "How rigid the body stays when hit"),
    };
    movementFields = {
        OverrideField("Running Speed Rate", o.runningSpeedRate, 0.1f, "Running speed multiplier (1.5 = default)"),
        OverrideField("Walk Speed Rate", o.walkSpeedRateRun, 0.1f, "Walking/aiming speed rate"),
        OverrideField("Jump Rate", o.jumpRate, 0.1f, "Jump power multiplier"),
        OverrideField("Dodge Rate", o.dodgeRate, 0.1f, "Dodge speed/distance multiplier"),
        OverrideField("Crawl Rate", o.crawlRate, 0.01f, "Crawling speed multiplier"),
        OverrideField("Get Up Rate", o.getUpRate, 0.1f, "Speed of getting up from the ground"),
        OverrideField("Fallen Rate", o.fallenRate, 0.01f, "Rate at which the character recovers from falling"),
    };
    combatFields = {
        OverrideField("Damage Rate", o.damageRate, 0.1f, "Additional damage multiplier dealt"),
        OverrideField("Limb Damage Rate", o.limbDamageRate, 0.1f, "Additional limb-specific damage multiplier"),
        OverrideField(
            "Dismember Threshold", o.dismemberThreshold, 0.1f, "Health threshold below which dismemberment can occur"
        ),
        OverrideField("Stamina", o.stamina, 1.0f, "Current stamina level (100 = full)"),
        OverrideField("Swing R Burn", o.staminaBurnSwingR, 0.1f, "Stamina cost for right-hand swings"),
        OverrideField("Swing L Burn", o.staminaBurnSwingL, 0.1f, "Stamina cost for left-hand swings"),
        OverrideField("Dodge Burn", o.staminaBurnDodge, 0.1f, "Stamina cost for dodging"),
        OverrideField("Grab Force R", o.grabForceR, 100.0f, "Right hand grip force limit (10000 = default)"),
        OverrideField("Grab Force L", o.grabForceL, 100.0f, "Left hand grip force limit (10000 = default)"),
        OverrideField("Hands Rigidity", o.handsRigidity, 0.01f, "Punch impact force (0.666 = default)"),
        OverrideField("Body Skill", o.bodySkill, 0.1f, "Overall combat skill level"),
        OverrideField("Weapon Skill", o.weaponSkill, 0.1f, "Weapon handling skill level"),
    };
    skillFields = {
        OverrideField("Thrust", o.skillThrust),     OverrideField("Parry", o.skillParry),
        OverrideField("Alt Grip", o.skillAltGrip),  OverrideField("Alt Stance", o.skillAltStance),
        OverrideField("Rotate", o.skillRotate),     OverrideField("Crouch", o.skillCrouch),
        OverrideField("Dodge##sk", o.skillDodge),   OverrideField("Kick", o.skillKick),
        OverrideField("Slow Motion", o.skillSlomo),
    };
    stateFields = {
        OverrideField("Exhaustion", o.exhaustion, 0.1f, "Physical exhaustion level"),
        OverrideField("Drunk", o.drunk, 0.01f, "Drunkenness level (0 = sober, 1 = fully drunk)"),
        OverrideField("Fear", o.fear, 0.1f, "Fear level"),
        OverrideField("Invulnerable", o.invulnerable, "Immune to all damage"),
    };
}

int PlayerEditorSection::CountAllActive() const {
    return CountActive(physicalFields) + CountActive(healthFields) + CountActive(physicsFields) +
           CountActive(movementFields) + CountActive(combatFields) + CountActive(skillFields) +
           CountActive(stateFields);
}

// Each group has a parallel array of setter function pointers, indexed to
// match the descriptor order from BuildDescriptors(). ApplyWithSetters
// iterates enabled fields and dispatches through the table.

namespace {

    using P = SDK::AWillie_BP_C;

    static constexpr OverrideSetter PHYSICAL_SETTERS[] = {
        [](void* a, const OverrideDescriptor& f) {
            auto* p = static_cast<P*>(a);
            double v = GetDouble(f);
            p->Height_Rate = v;
            p->Character_Passport.Height_21_0EB204DF4978B92AD0ED188FD32EEC7B = v;
        },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Muscle_Rate = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Scale_Mutation_Inhibitor = GetDouble(f); },
    };

    static constexpr OverrideSetter HEALTH_SETTERS[] = {
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Health = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Head_Health = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Neck_Health = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Arm_R_Health = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Arm_L_Health = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Body_Upper_Health = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Body_Lower_Health = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Leg_R_Health = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Leg_L_Health = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Back_Health = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Consciousness = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Regen_Rate = GetDouble(f); },
    };

    static constexpr OverrideSetter PHYSICS_SETTERS[] = {
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->All_Body_Tonus = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Head_Tonus = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Arm_R_Tonus = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Arm_L_Tonus = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Leg_R_Tonus = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Leg_L_Tonus = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Muscle_Power = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Orientation_Strength = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Angular_Strength = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Hit_Rigidity = GetDouble(f); },
    };

    static constexpr OverrideSetter MOVEMENT_SETTERS[] = {
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Running_Speed_Rate = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) {
            static_cast<P*>(a)->Walk_Speed_Rate_Run = static_cast<float>(GetDouble(f));
        },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Jump_Rate = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Dodge_Rate = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Crawl_Rate = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Get_Up_Rate = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Fallen_Rate = GetDouble(f); },
    };

    static constexpr OverrideSetter COMBAT_SETTERS[] = {
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Damage_Rate__Additional_ = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Limb_Damage_Rate__Additional_ = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) {
            static_cast<P*>(a)->Health_Threshold_For_Dismemberment = GetDouble(f);
        },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Stamina = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Stamina_Burn_Swing_R = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Stamina_Burn_Swing_L = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Stamina_Burn_Dodge = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->R_Grab_Force_Limit = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->L_Grab_Force_Limit = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Hands_Rigidity__Gauntlets_ = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Body_Skill__Temp_ = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Weapon_Skill__Temp_ = GetDouble(f); },
    };

    static constexpr OverrideSetter SKILL_SETTERS[] = {
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Skill_Unlock_Weapon_Thrust = GetBool(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Skill_Unlock_Weapon_Parry = GetBool(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Skill_Unlock_Weapon_Alt_Grip = GetBool(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Skill_Unlock_Weapon_Alt_Stance = GetBool(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Skill_Unlock_Weapon_Rotate = GetBool(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Skill_Unlock_Body_Crouch = GetBool(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Skill_Unlock_Body_Dodge = GetBool(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Skill_Unlock_Body_Kick = GetBool(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Skill_Unlock_Body_Slomo = GetBool(f); },
    };

    static constexpr OverrideSetter STATE_SETTERS[] = {
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Exhaustion = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Drunk = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<P*>(a)->Fear = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) {
            auto* p = static_cast<P*>(a);
            bool v = GetBool(f);
            p->Invulnerable = v;
            p->BitPad_5C_0 = v;
        },
    };

} // namespace

void PlayerEditorSection::ApplyToPlayer(SDK::AWillie_BP_C* p) {
    auto* target = static_cast<void*>(p);
    ApplyWithSetters(physicalFields, target, PHYSICAL_SETTERS);
    ApplyWithSetters(healthFields, target, HEALTH_SETTERS);
    ApplyWithSetters(physicsFields, target, PHYSICS_SETTERS);
    ApplyWithSetters(movementFields, target, MOVEMENT_SETTERS);
    ApplyWithSetters(combatFields, target, COMBAT_SETTERS);
    ApplyWithSetters(skillFields, target, SKILL_SETTERS);
    ApplyWithSetters(stateFields, target, STATE_SETTERS);
}

void PlayerEditorSection::ReadFromPlayer() {
    auto* player = RenderPlayer();
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

    presets.status.Set("Values read from player");
}

PlayerPresetData PlayerEditorSection::BuildPresetData() const {
    PlayerPresetData d;
    d.overrides = overrides;
    return d;
}

void PlayerEditorSection::ApplyPresetData(const PlayerPresetData& d) {
    overrides = d.overrides;
}

void PlayerEditorSection::ClonePlayer() {
    auto [world, player] = RenderPlayerWorld();
    if (!player || !world) return;
    auto passport = player->Character_Passport;
    if (overrides.heightRate.enabled) passport.Height_21_0EB204DF4978B92AD0ED188FD32EEC7B = overrides.heightRate.value;
    if (overrides.muscleRate.enabled) passport.Weight_23_65E4C6534D14653F96EB739F159E58CD = overrides.muscleRate.value;

    double heightRate = passport.Height_21_0EB204DF4978B92AD0ED188FD32EEC7B;
    double muscleRate = passport.Weight_23_65E4C6534D14653F96EB739F159E58CD;
    auto spawnScale = static_cast<float>(0.875 + heightRate * 0.125);

    auto transform = Spawner::BuildSpawnTransform(player, 150.0f, 0.0f, spawnScale);

    GameHook::QueueAction([passport, heightRate, muscleRate, transform](const RuntimeContextSnapshot& runtime) {
        if (!runtime.world) return;
        Spawner::SpawnActor(
            runtime.world, GameConstants::WILLIE_BP_PATH, transform,
            [passport, heightRate, muscleRate](SDK::AActor* actor) {
                auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);
                npc->Character_Passport = passport;
                npc->Height_Rate = heightRate;
                npc->Muscle_Rate = muscleRate;
                npc->Team_Int = 1;
            }
        );
    });
}

void PlayerEditorSection::RenderPhysicalTab() {
    ImGui::PushID("physical");
    ImGui::SeparatorText("Body");
    RenderOverrideGroup(physicalFields);
    ImGui::PopID();
}

void PlayerEditorSection::RenderHealthTab() {
    ImGui::PushID("health");

    ImGui::SeparatorText("General");
    RenderOverrideField(healthFields[0]);
    RenderOverrideField(healthFields[10]);
    RenderOverrideField(healthFields[11]);

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
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - SectionStyle::CELL_PADDING.y);
    RenderOverrideField(healthFields[9]);

    ImGui::PopID();
}

void PlayerEditorSection::RenderPhysicsTab() {
    ImGui::PushID("physics");

    ImGui::SeparatorText("Muscle Tonus");
    RenderOverrideField(physicsFields[0]); // All Body Tonus

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - SectionStyle::CELL_PADDING.y);
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
    RenderOverrideField(physicsFields[7]); // Orientation Strength
    RenderOverrideField(physicsFields[8]); // Angular Strength
    RenderOverrideField(physicsFields[9]); // Hit Rigidity

    ImGui::PopID();
}

void PlayerEditorSection::RenderMovementTab() {
    ImGui::PushID("movement");

    ImGui::SeparatorText("Speed");
    RenderOverrideGroup({movementFields.data(), 2});

    ImGui::SeparatorText("Actions");
    RenderOverrideGroup({movementFields.data() + 2, 5});

    ImGui::PopID();
}

void PlayerEditorSection::RenderCombatTab() {
    ImGui::PushID("combat");

    ImGui::SeparatorText("Damage");
    RenderOverrideGroup({combatFields.data(), 3});

    ImGui::SeparatorText("Stamina");
    RenderOverrideGroup({combatFields.data() + 3, 4});

    ImGui::SeparatorText("Grip & Skill");
    RenderOverrideGroup({combatFields.data() + 7, 5});

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
    RenderOverrideGroup(stateFields);

    ImGui::PopID();
}

PlayerEditorSection::PlayerEditorSection(ModContext& ctx) : Section(ctx, SECTION) {
    BuildDescriptors();
    InitKeybinds();
}

void PlayerEditorSection::InitKeybinds() {
    keybinds.Add({
        .name = "Enforce Overrides",
        .tooltip = "Continuously applies all enabled overrides to the player character every game tick",
        .configSection = "EnforceOverrides",
        .keyPtr = &enforceKey,
        .callback =
            [this](bool active, const RuntimeContextSnapshot& runtime) {
                auto* player = runtime.player;
                if (active && player) ApplyToPlayer(player);
            },
        .runOnToggle = true,
        .events = {GameEvent::OffLedge},
    });
}

void PlayerEditorSection::Render() {
    const SectionStyle::StyleRAII style;
    auto [world, player] = RenderPlayerWorld();

    keybinds.Render();

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
                [this]() { return BuildPresetData(); }, [this](const PlayerPresetData& d) { ApplyPresetData(d); }
            );
            break;
        default: break;
    }

    ImGui::EndChild();
}
