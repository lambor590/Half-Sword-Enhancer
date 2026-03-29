#include "Menu/Sections/Spawner/NPCEditorSection.h"
#include "Menu/SectionRegistry.h"
#include "Menu/SectionStyle.h"
#include "Utils/Spawner.h"

REGISTER_SECTION(NPCEditorSection, MenuTab::Spawner);
#include "Utils/EquipmentGenerator.h"
#include "Utils/NPCSpawnHelpers.h"
#include "Utils/GuiUtils.h"

// ── Descriptor construction ───────────────────────────────────────────

void NPCEditorSection::BuildDescriptors() {
    auto& o = overrides;

    physicalFields = {
        OverrideField("Height Rate", o.heightRate, 0.0, 0.0, 0.0, 0.01f, "Character height multiplier (1.0 = normal)"),
        OverrideField(
            "Muscle Rate", o.muscleRate, 0.0, 0.0, 0.0, 0.01f, "Character muscle/bulk multiplier (1.0 = normal)"
        ),
        OverrideField(
            "Scale Mutation Inhibitor", o.scaleMutationInhibitor, 0.0, 0.0, 0.0, 0.01f,
            "Controls how much random scale variation is suppressed"
        ),
        OverrideField("Face Type", o.faceType, 0, 0, 0, 0.1f, "Face mesh index"),
        OverrideField("Eye Color", o.eyeColor, 0, 0, 0, 0.1f, "Eye color index"),
        OverrideField("Hair Length", o.hairLength, 0.0, 0.0, 0.0, 0.01f, "Hair length (0 = bald, 1 = maximum)"),
        OverrideField(
            "Hair Color", o.hairColor, 0.0, 0.0, 0.0, 0.01f, "Hair melanin (0 = blonde, 0.5 = brown, 1 = black)"
        ),
    };
    combatFields = {
        OverrideField(
            "Damage Rate", o.damageRate, 0.0, 0.0, 0.0, 0.1f, "Additional damage multiplier dealt by this NPC"
        ),
        OverrideField(
            "Limb Damage Rate", o.limbDamageRate, 0.0, 0.0, 0.0, 0.1f, "Additional limb-specific damage multiplier"
        ),
        OverrideField(
            "Dismember Threshold", o.dismemberThreshold, 0.0, 0.0, 0.0, 0.1f,
            "Health threshold below which dismemberment can occur"
        ),
        OverrideField("Regen Rate", o.regenRate, 0.0, 0.0, 0.0, 0.01f, "Health regeneration rate per tick"),
        OverrideField(
            "AI Invincibility", o.aiInvincibility, 0.0, 0.0, 0.0, 0.01f, "Rate at which AI ignores incoming damage"
        ),
        OverrideField(
            "AI Armor Invincibility", o.aiArmorInvincibility, 0.0, 0.0, 0.0, 0.01f,
            "Rate at which AI armor ignores damage"
        ),
        OverrideField(
            "Body Skill", o.bodySkill, 0.0, 0.0, 0.0, 0.1f,
            "Overall combat skill level affecting movement and reactions"
        ),
    };
    behaviorFields = {
        OverrideField("Fearless", o.fearless, false, "NPC never flees from combat"),
        OverrideField("Start Kneeled", o.startKneeled, false, "NPC spawns in a kneeling position"),
        OverrideField("Spawn in Pants", o.spawnInPants, false, "NPC spawns wearing only pants (no armor)"),
        OverrideField("Clear Spawn Area", o.clearSpawnArea, false, "Clear objects around spawn point before spawning"),
        OverrideField("Drunk", o.drunk, 0.0, 0.0, 0.0, 0.01f, "Drunkenness level (0 = sober, 1 = fully drunk)"),
        OverrideField("Bolts in Quiver", o.boltsInQuiver, 0, 0, 0, 0.1f, "Number of crossbow bolts the NPC carries"),
    };
    bodyConditionFields = {
        OverrideField("Head", o.headHealth, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Neck", o.neckHealth, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Right Arm", o.armRHealth, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Left Arm", o.armLHealth, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Upper Body", o.bodyUpperHealth, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Lower Body", o.bodyLowerHealth, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Right Leg", o.legRHealth, 0.0, 0.0, 0.0, 0.1f),
        OverrideField("Left Leg", o.legLHealth, 0.0, 0.0, 0.0, 0.1f),
    };
}

// ── Active override counting via descriptors ──────────────────────────

int NPCEditorSection::CountAllActive() const {
    return CountActive(physicalFields) + CountActive(combatFields) + CountActive(behaviorFields) +
           CountActive(bodyConditionFields);
}

// ── Helpers ───────────────────────────────────────────────────────────

const char* NPCEditorSection::getNPCClassName() const noexcept {
    if (cfg.npcTypeIndex >= 0 && cfg.npcTypeIndex < npcTypesCount) [[likely]]
        return npcTypes[cfg.npcTypeIndex].className;
    return npcTypes[0].className;
}

// ── Spawn NPC ─────────────────────────────────────────────────────────

void NPCEditorSection::SpawnNPC() {
    auto className = std::string(getNPCClassName());
    auto nationality = static_cast<SDK::Enum_Nationalities>(cfg.npcNationality);
    auto tier = static_cast<SDK::Enum_Ranks>(cfg.npcTier);
    bool mercenary = cfg.npcMercenary;
    bool bodyguard = cfg.bodyguard;
    int team = cfg.npcTeam;
    auto ovr = overrides;
    bool hasOverrides = CountAllActive() > 0;
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

        auto passport = EquipmentGenerator::GenerateCharacter(world, npc->Class, nationality, tier, mercenary);
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
        if (hasLoadout) NPCSpawnHelpers::ApplyNPCLoadout(w, npc, loadout);
    };

    Spawner::SpawnActor(world, className, spawnTransform, preCallback, cfg.spawn.snapToGround, 4, postCallback);
}

// ── Preset data conversion ────────────────────────────────────────────

NPCPresetData NPCEditorSection::BuildPresetData() const {
    NPCPresetData d;
    d.npcTypeIndex = cfg.npcTypeIndex;
    d.nationality = cfg.npcNationality;
    d.tier = cfg.npcTier;
    d.mercenary = cfg.npcMercenary;
    d.overrides = overrides;
    return d;
}

void NPCEditorSection::ApplyPresetData(const NPCPresetData& d) {
    cfg.npcTypeIndex = std::clamp(d.npcTypeIndex, 0, npcTypesCount - 1);
    cfg.npcNationality = std::clamp(d.nationality, 0, nationalityCount - 1);
    cfg.npcTier = std::clamp(d.tier, 0, 8);
    cfg.npcMercenary = d.mercenary;
    overrides = d.overrides;
}

// ── Tab rendering (using RenderOverrideField from override system) ────

void NPCEditorSection::RenderPhysicalTab() {
    ImGui::PushID("physical");

    ImGui::SeparatorText("Body");
    RenderOverrideGroup({physicalFields.data(), 3});

    ImGui::SeparatorText("Appearance");
    RenderOverrideGroup({physicalFields.data() + 3, 4});

    ImGui::PopID();
}

void NPCEditorSection::RenderCombatTab() {
    ImGui::PushID("combat");

    ImGui::SeparatorText("Damage");
    RenderOverrideGroup({combatFields.data(), 3});

    ImGui::SeparatorText("Defense");
    RenderOverrideGroup({combatFields.data() + 3, 3});

    ImGui::SeparatorText("Skill");
    RenderOverrideField(combatFields[6]);

    ImGui::PopID();
}

void NPCEditorSection::RenderBehaviorTab() {
    ImGui::PushID("behavior");

    RenderOverrideGroup({behaviorFields.data(), 4});

    ImGui::Spacing();
    RenderOverrideGroup({behaviorFields.data() + 4, 2});

    ImGui::PopID();
}

void NPCEditorSection::RenderBodyConditionTab() {
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
        RenderOverrideField(bodyConditionFields[0]); // Head
        ImGui::TableNextColumn();
        RenderOverrideField(bodyConditionFields[1]); // Neck

        ImGui::TableNextColumn();
        RenderOverrideField(bodyConditionFields[2]); // Right Arm
        ImGui::TableNextColumn();
        RenderOverrideField(bodyConditionFields[3]); // Left Arm

        ImGui::TableNextColumn();
        RenderOverrideField(bodyConditionFields[4]); // Upper Body
        ImGui::TableNextColumn();
        RenderOverrideField(bodyConditionFields[5]); // Lower Body

        ImGui::TableNextColumn();
        RenderOverrideField(bodyConditionFields[6]); // Right Leg
        ImGui::TableNextColumn();
        RenderOverrideField(bodyConditionFields[7]); // Left Leg
        ImGui::EndTable();
    }

    ImGui::PopID();
}

// ── Constructor & keybinds ────────────────────────────────────────────

NPCEditorSection::NPCEditorSection(ModContext& ctx) : Section(ctx, "NPC Editor") {
    BuildDescriptors();
    InitKeybinds();
}

void NPCEditorSection::InitKeybinds() {
    AddKeybind(
        keybinds,
        {
            .name = "Spawn NPC",
            .tooltip = "Spawns an NPC with randomly generated equipment and applied overrides",
            .configSection = "SpawnNPC",
            .keyPtr = &cfg.spawnEnemyKey,
            .callback =
                [this]([[maybe_unused]] bool) {
                    if (!player || !world) return;
                    SpawnNPC();
                },
            .params =
                {KeybindParam("bodyguard", "Bodyguard", &cfg.bodyguard, "Will join your team"),
                 KeybindParam("mercenary", "Mercenary", &cfg.npcMercenary, "Generate with mercenary color scheme"),
                 KeybindParam(
                     "snap_to_ground", "Snap to Ground", &cfg.spawn.snapToGround,
                     "Automatically adjust height to touch the ground"
                 ),
                 KeybindParam(
                     "distance_forward", "Distance Forward", &cfg.spawn.distanceForward, 100.0f, 500.0f,
                     "How far in front the NPC appears"
                 ),
                 KeybindParam(
                     "distance_up", "Distance Up", &cfg.spawn.distanceUp, 0.0f, 300.0f,
                     "Height offset for spawn position"
                 ),
                 KeybindParam(
                     "scale", "Scale", &cfg.spawn.scale, 0.1f, 4.0f,
                     "Size multiplier for the spawned NPC. Adjust the height offset to match the scale "
                     "so the game doesn't crash."
                 ),
                 KeybindParam(
                     "team", "Team", &cfg.npcTeam, 0, 9,
                     "Assign the NPC to a team number. 0-4 are the default teams. 0 means no team."
                 )},
        }
    );
}

// ── Main Render ───────────────────────────────────────────────────────

void NPCEditorSection::Render() {
    const SectionStyle::StyleRAII style;

    KeybindUI::RenderKeybindList(keybinds);
    ImGui::Spacing();

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
    GuiUtils::RenderOverrideCount(CountAllActive());
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
