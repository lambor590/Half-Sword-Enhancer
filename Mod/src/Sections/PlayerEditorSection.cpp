#include "Menu/Sections/Player/PlayerEditorSection.h"
#include "Hooks/GameHook.h"
#include "SDK/Willie_BP_classes.hpp"
#include "Utils/GuiUtils.h"
#include "Utils/GameClass.h"
#include "Utils/PresetApplication.h"
#include "Utils/Spawner.h"

void PlayerEditorSection::BuildDescriptors() {
    auto& o = overrides;

    physicalFields = {
        OverrideField("Height", o.heightRate, 0.01f, "Player height (1.0 = normal)"),
        OverrideField("Body Build", o.muscleRate, 0.01f, "Character muscle and bulk (1.0 = normal)"),
        OverrideField(
            "Size Consistency", o.scaleMutationInhibitor, 0.01f, "Higher values make body proportions more even"
        ),
    };
    healthFields = {
        OverrideField("Health", o.health, 1.0f, "Overall health"),
        OverrideField("Head", o.headHealth, 1.0f),
        OverrideField("Neck", o.neckHealth, 1.0f),
        OverrideField("Right Arm##h", o.armRHealth, 1.0f),
        OverrideField("Left Arm##h", o.armLHealth, 1.0f),
        OverrideField("Upper Body", o.bodyUpperHealth, 1.0f),
        OverrideField("Lower Body", o.bodyLowerHealth, 1.0f),
        OverrideField("Right Leg##h", o.legRHealth, 1.0f),
        OverrideField("Left Leg##h", o.legLHealth, 1.0f),
        OverrideField("Back", o.backHealth, 1.0f),
        OverrideField("Consciousness", o.consciousness, 1.0f, "Consciousness level (0 = knocked out)"),
        OverrideField("Health Recovery", o.regenRate, 0.01f, "How quickly health returns"),
    };
    physicsFields = {
        OverrideField("Whole Body", o.allBodyTonus, 1.0f, "Overall ability to stay firm and upright (100 = normal)"),
        OverrideField("Head##t", o.headTonus, 0.01f),
        OverrideField("Right Arm##t", o.armRTonus, 0.01f),
        OverrideField("Left Arm##t", o.armLTonus, 0.01f),
        OverrideField("Right Leg##t", o.legRTonus, 0.01f),
        OverrideField("Left Leg##t", o.legLTonus, 0.01f),
        OverrideField("Strength", o.musclePower, 0.5f, "Overall physical strength (35 = default)"),
        OverrideField("Posture Stability", o.orientationStrength, 0.1f, "How strongly the body holds its posture"),
        OverrideField("Rotation Stability", o.angularStrength, 0.1f, "Resistance to unwanted body rotation"),
        OverrideField("Impact Stiffness", o.hitRigidity, 0.01f, "How stiff the body remains when hit"),
    };
    movementFields = {
        OverrideField("Run Speed", o.runningSpeedRate, 0.1f, "Running speed (1.5 = default)"),
        OverrideField("Walk Speed", o.walkSpeedRateRun, 0.1f, "Walking and aiming speed"),
        OverrideField("Jump Strength", o.jumpRate, 0.1f, "Jump height and power"),
        OverrideField("Dodge Speed", o.dodgeRate, 0.1f, "Dodge speed and distance"),
        OverrideField("Crawl Speed", o.crawlRate, 0.01f, "Crawling speed"),
        OverrideField("Get-Up Speed", o.getUpRate, 0.1f, "How quickly the character gets up"),
        OverrideField("Fall Recovery", o.fallenRate, 0.01f, "How quickly the character recovers after falling"),
    };
    combatFields = {
        OverrideField("Damage", o.damageRate, 0.1f, "Overall damage dealt"),
        OverrideField("Limb Damage", o.limbDamageRate, 0.1f, "Damage dealt to individual limbs"),
        OverrideField("Limb Severing", o.dismemberThreshold, 0.1f, "Higher values make limbs easier to sever"),
        OverrideField("Stamina", o.stamina, 1.0f, "Current stamina level (100 = full)"),
        OverrideField("Right Swing Cost", o.staminaBurnSwingR, 0.1f, "Stamina used by right-hand swings"),
        OverrideField("Left Swing Cost", o.staminaBurnSwingL, 0.1f, "Stamina used by left-hand swings"),
        OverrideField("Dodge Cost", o.staminaBurnDodge, 0.1f, "Stamina used by dodging"),
        OverrideField("Right Grip Strength", o.grabForceR, 100.0f, "Right-hand grip strength (10000 = default)"),
        OverrideField("Left Grip Strength", o.grabForceL, 100.0f, "Left-hand grip strength (10000 = default)"),
        OverrideField("Punch Power", o.handsRigidity, 0.01f, "How hard punches hit (0.666 = default)"),
        OverrideField("Combat Skill", o.bodySkill, 0.1f, "Overall combat ability"),
        OverrideField("Weapon Skill", o.weaponSkill, 0.1f, "Weapon handling skill level"),
    };
    skillFields = {
        OverrideField("Thrust", o.skillThrust),          OverrideField("Parry", o.skillParry),
        OverrideField("Alternate Grip", o.skillAltGrip), OverrideField("Alternate Stance", o.skillAltStance),
        OverrideField("Rotate", o.skillRotate),          OverrideField("Crouch", o.skillCrouch),
        OverrideField("Dodge##sk", o.skillDodge),        OverrideField("Kick", o.skillKick),
        OverrideField("Slow Motion", o.skillSlomo),
    };
    stateFields = {
        OverrideField("Exhaustion", o.exhaustion, 0.1f, "Physical exhaustion level"),
        OverrideField("Drunkenness", o.drunk, 0.01f, "0 = sober, 1 = fully drunk"),
        OverrideField("Fear", o.fear, 0.1f, "Fear level"),
        OverrideField("Invulnerable", o.invulnerable, "Immune to all damage"),
    };
}

int PlayerEditorSection::CountAllActive() const {
    return CountActive(physicalFields) + CountActive(healthFields) + CountActive(physicsFields) +
           CountActive(movementFields) + CountActive(combatFields) + CountActive(skillFields) +
           CountActive(stateFields);
}

void PlayerEditorSection::ApplyToPlayer(SDK::AWillie_BP_C* p) {
    PlayerEditorOverrides snapshot;
    {
        std::scoped_lock lock(publishedOverridesMutex);
        snapshot = publishedOverrides;
    }
    (void)PresetApplication::ApplyPlayerOverrides(p, snapshot);
}

void PlayerEditorSection::PublishOverrides() {
    std::scoped_lock lock(publishedOverridesMutex);
    publishedOverrides = overrides;
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

    presets.status.Set("Player stats copied");
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
                if (!GameClass::IsWillie(actor)) return;
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
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().CellPadding.y);
    RenderOverrideField(healthFields[9]);

    ImGui::PopID();
}

void PlayerEditorSection::RenderPhysicsTab() {
    ImGui::PushID("physics");

    ImGui::SeparatorText("Body Stability");
    RenderOverrideField(physicsFields[0]); // All Body Tonus

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().CellPadding.y);
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

    ImGui::SeparatorText("Grip & Combat Skill");
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
    PublishOverrides();
    InitKeybinds();
}

void PlayerEditorSection::InitKeybinds() {
    keybinds.Add({
        .name = "Custom Player Stats",
        .tooltip = "Use the enabled player values",
        .configSection = "EnforceOverrides",
        .keyPtr = &enforceKey,
        .callback =
            [this](bool active, const RuntimeContextSnapshot& runtime) {
                auto* player = runtime.player;
                if (active && player) ApplyToPlayer(player);
            },
        .kind = KeybindKind::State,
        .applyOnToggle = true,
        .events = {GameEvent::OffLedge},
    });
}

void PlayerEditorSection::Render() {
    auto [world, player] = RenderPlayerWorld();

    keybinds.Render();

    ImGui::Spacing();

    if (!player) ImGui::BeginDisabled();
    if (GuiUtils::Button("Copy Player Stats")) ReadFromPlayer();
    if (!player) ImGui::EndDisabled();
    GuiUtils::HelpTooltip("Copy the current player values without enabling them");
    (void)GuiUtils::SameLineIfFitsButton("Clear Custom Stats");
    if (GuiUtils::Button("Clear Custom Stats", GuiUtils::ButtonTone::Danger)) overrides = {};
    GuiUtils::HelpTooltip("Disable every custom player value");
    (void)GuiUtils::SameLineIfFitsButton("Spawn Player Clone");
    if (!player || !world) ImGui::BeginDisabled();
    if (GuiUtils::Button("Spawn Player Clone", GuiUtils::ButtonTone::Primary)) {
        ClonePlayer();
        presets.status.Set("Player cloned");
    }
    if (!player || !world) ImGui::EndDisabled();
    GuiUtils::HelpTooltip("Spawn a clone with the current body settings");

    GuiUtils::RenderOverrideCount(CountAllActive());
    presets.status.Render();

    ImGui::Spacing();
    ImGui::BeginChild("##playereditor_scroll", ImVec2(0, 0));

    static constexpr const char* TAB_LABELS[] = {"Body",     "Health", "Strength & Stability",
                                                 "Movement", "Combat", "Abilities & State",
                                                 "Presets"};
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
    PublishOverrides();
}
