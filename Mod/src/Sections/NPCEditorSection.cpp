#include "Menu/Sections/Spawner/NPCEditorSection.h"

#include <algorithm>
#include <utility>

#include "Menu/Sections/Spawner/SpawnBindings.h"
#include "Menu/SectionStyle.h"
#include "ConfigManager.h"

#include "Utils/GuiUtils.h"
#include "Utils/SpawnWorkflow.h"

namespace {
    constexpr SpawnBindings::BindingConfig NPC_BINDING_CONFIG{
        .indexSection = "NPCSpawnBindings",
        .bindingPrefix = "NPCSpawnBinding_",
        .defaultName = "Spawn NPC",
        .addTooltip = "Save the current NPC setup as its own keybind",
        .emptyText = "No NPC spawn bindings saved",
        .updateTooltip = "Replace this binding with the current NPC setup",
        .deletePopupTitle = "Delete NPC Binding",
        .deletePrompt = "Delete NPC spawn binding?",
        .spawnParams =
            {
                .forwardLabel = "Distance Forward",
                .forwardMin = 100.0f,
                .forwardMax = 500.0f,
                .forwardTooltip = "How far in front the NPC appears",
                .upLabel = "Distance Up",
                .upMin = 0.0f,
                .upMax = 300.0f,
                .upTooltip = "Height offset for spawn position",
                .scaleMin = 0.1f,
                .scaleMax = 4.0f,
                .scaleTooltip =
                    "Size multiplier for the spawned NPC. Adjust the height offset to match the scale so the game "
                    "doesn't crash.",
            },
    };

    std::string OverrideKey(const char* group, const char* name, const char* suffix) {
        return std::string(group) + "_" + name + "_" + suffix;
    }

    void SaveOverrides(std::string_view section, NPCPresetData& npc) {
        auto& config = ConfigManager::Get();
        for (const auto& group : NPCPresetData::GetOverrideGroups(npc)) {
            for (const auto& field : group.fields) {
                config.SetBool(section, OverrideKey(group.section, field.name, "enabled"), *field.enabled);
                switch (field.type) {
                    case OverrideFieldType::Double:
                        config.SetFloat(
                            section, OverrideKey(group.section, field.name, "value"),
                            static_cast<float>(*static_cast<double*>(field.value))
                        );
                        break;
                    case OverrideFieldType::Int:
                        config.SetInt(section, OverrideKey(group.section, field.name, "value"), GetInt(field));
                        break;
                    case OverrideFieldType::Bool:
                        config.SetBool(section, OverrideKey(group.section, field.name, "value"), GetBool(field));
                        break;
                }
            }
        }
    }

    void LoadOverrides(std::string_view section, NPCPresetData& npc) {
        auto& config = ConfigManager::Get();
        for (const auto& group : NPCPresetData::GetOverrideGroups(npc)) {
            for (const auto& field : group.fields) {
                *field.enabled = config.GetBool(section, OverrideKey(group.section, field.name, "enabled"), false);
                switch (field.type) {
                    case OverrideFieldType::Double:
                        *static_cast<double*>(field.value) =
                            config.GetFloat(section, OverrideKey(group.section, field.name, "value"), 0.0f);
                        break;
                    case OverrideFieldType::Int:
                        *static_cast<int*>(field.value) =
                            config.GetInt(section, OverrideKey(group.section, field.name, "value"), 0);
                        break;
                    case OverrideFieldType::Bool:
                        *static_cast<bool*>(field.value) =
                            config.GetBool(section, OverrideKey(group.section, field.name, "value"), false);
                        break;
                }
            }
        }
    }
}

void NPCEditorSection::BuildDescriptors() {
    auto& o = overrides;

    physicalFields = {
        OverrideField("Height Rate", o.heightRate, 0.01f, "Character height multiplier (1.0 = normal)"),
        OverrideField("Muscle Rate", o.muscleRate, 0.01f, "Character muscle/bulk multiplier (1.0 = normal)"),
        OverrideField(
            "Scale Mutation Inhibitor", o.scaleMutationInhibitor, 0.01f,
            "Controls how much random scale variation is suppressed"
        ),
        OverrideField("Face Type", o.faceType, 0.1f, "Face mesh index"),
        OverrideField("Eye Color", o.eyeColor, 0.1f, "Eye color index"),
        OverrideField("Hair Length", o.hairLength, 0.01f, "Hair length (0 = bald, 1 = maximum)"),
        OverrideField("Hair Color", o.hairColor, 0.01f, "Hair melanin (0 = blonde, 0.5 = brown, 1 = black)"),
    };
    combatFields = {
        OverrideField("Damage Rate", o.damageRate, 0.1f, "Additional damage multiplier dealt by this NPC"),
        OverrideField("Limb Damage Rate", o.limbDamageRate, 0.1f, "Additional limb-specific damage multiplier"),
        OverrideField(
            "Dismember Threshold", o.dismemberThreshold, 0.1f, "Health threshold below which dismemberment can occur"
        ),
        OverrideField("Regen Rate", o.regenRate, 0.01f, "Health regeneration rate per tick"),
        OverrideField("AI Invincibility", o.aiInvincibility, 0.01f, "Rate at which AI ignores incoming damage"),
        OverrideField("AI Armor Invincibility", o.aiArmorInvincibility, 0.01f, "Rate at which AI armor ignores damage"),
        OverrideField("Body Skill", o.bodySkill, 0.1f, "Overall combat skill level affecting movement and reactions"),
    };
    behaviorFields = {
        OverrideField("Start Kneeled", o.startKneeled, "NPC spawns in a kneeling position"),
        OverrideField("Spawn in Pants", o.spawnInPants, "NPC spawns wearing only pants (no armor)"),
        OverrideField("Blossfechten Gear", o.blossfechtenGear, "NPC spawns with Blossfechten gear setup"),
        OverrideField("Clear Spawn Area", o.clearSpawnArea, "Clear objects around spawn point before spawning"),
        OverrideField("Drunk", o.drunk, 0.01f, "Drunkenness level (0 = sober, 1 = fully drunk)"),
        OverrideField("Bolts in Quiver", o.boltsInQuiver, 0.1f, "Number of crossbow bolts the NPC carries"),
    };
    bodyConditionFields = {
        OverrideField("Head", o.headHealth, 0.1f),
        OverrideField("Neck", o.neckHealth, 0.1f),
        OverrideField("Right Arm", o.armRHealth, 0.1f),
        OverrideField("Left Arm", o.armLHealth, 0.1f),
        OverrideField("Upper Body", o.bodyUpperHealth, 0.1f),
        OverrideField("Lower Body", o.bodyLowerHealth, 0.1f),
        OverrideField("Right Leg", o.legRHealth, 0.1f),
        OverrideField("Left Leg", o.legLHealth, 0.1f),
    };
}

int NPCEditorSection::CountAllActive() const {
    return CountActive(physicalFields) + CountActive(combatFields) + CountActive(behaviorFields) +
           CountActive(bodyConditionFields);
}

const char* NPCEditorSection::GetNPCClassName(int npcTypeIndex) const noexcept {
    if (npcTypeIndex >= 0 && npcTypeIndex < NPC_TYPES_COUNT) [[likely]]
        return NPC_TYPES[npcTypeIndex].className;
    return NPC_TYPES[0].className;
}

const char* NPCEditorSection::GetNPCClassName() const noexcept {
    return GetNPCClassName(cfg.npcTypeIndex);
}

void NPCEditorSection::SpawnNPC() {
    auto snapshot = RenderSnapshot();
    if (!snapshot.player || !snapshot.world) return;

    SpawnWorkflow::NPCSpawnParams request{
        .classPath = GetNPCClassName(),
        .nationality = static_cast<SDK::Enum_Nationalities>(cfg.npcNationality),
        .tier = static_cast<SDK::Enum_Ranks>(cfg.npcTier),
        .mercenary = cfg.npcMercenary,
        .bodyguard = cfg.bodyguard,
        .team = cfg.npcTeam,
        .overrides = overrides,
    };

    if (loadoutPicker.HasSelection()) {
        request.loadout = LoadoutPresetSerializer::LoadFromFile(loadoutPicker.SelectedPath());
        request.hasLoadout = request.loadout.success;
    }

    SpawnWorkflow::QueueNPCSpawn(snapshot, cfg.spawn, std::move(request));
}

void NPCEditorSection::SpawnBindingNPC(const SpawnBinding& binding, const RuntimeContextSnapshot& runtime) const {
    if (!runtime.world || !runtime.player) return;

    SpawnWorkflow::NPCSpawnParams request{
        .classPath = GetNPCClassName(binding.npc.npcTypeIndex),
        .nationality = static_cast<SDK::Enum_Nationalities>(binding.npc.nationality),
        .tier = static_cast<SDK::Enum_Ranks>(binding.npc.tier),
        .mercenary = binding.npc.mercenary,
        .bodyguard = binding.bodyguard,
        .team = binding.team,
        .overrides = binding.npc.overrides,
    };

    if (binding.hasLoadout && !binding.loadoutPath.empty()) {
        request.loadout = LoadoutPresetSerializer::LoadFromFile(binding.loadoutPath);
        request.hasLoadout = request.loadout.success;
    }

    SpawnWorkflow::SpawnNPC(runtime, binding.spawn, request);
}

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
    cfg.npcTypeIndex = std::clamp(d.npcTypeIndex, 0, NPC_TYPES_COUNT - 1);
    cfg.npcNationality = std::clamp(d.nationality, 0, NATIONALITY_COUNT - 1);
    cfg.npcTier = std::clamp(d.tier, 0, 8);
    cfg.npcMercenary = d.mercenary;
    overrides = d.overrides;
}

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
    RenderOverrideField(behaviorFields[4]);

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

NPCEditorSection::NPCEditorSection(ModContext& ctx) : Section(ctx, SECTION) {
    BuildDescriptors();
    LoadSpawnBindings();
}

struct NPCEditorSection::BindingOps {
    static constexpr size_t EXTRA_PARAM_COUNT = 3;

    NPCEditorSection& owner;

    bool Capture(SpawnBinding& binding) const {
        ReadSelection(binding);
        return true;
    }

    void LoadFields(SpawnBinding& binding, ConfigManager& config, std::string_view section) const {
        binding.bodyguard = config.GetBool(section, "bodyguard", false);
        binding.team = config.GetInt(section, "team", 0);
        binding.npc.npcTypeIndex = config.GetInt(section, "npc_type", 0);
        binding.npc.nationality = config.GetInt(section, "nationality", 0);
        binding.npc.tier = config.GetInt(section, "tier", 4);
        binding.npc.mercenary = config.GetBool(section, "mercenary", false);
        binding.hasLoadout = config.GetBool(section, "has_loadout", false);
        binding.loadoutPath = config.GetString(section, "loadout_path", "");
        LoadOverrides(section, binding.npc);
    }

    void SaveFields(const SpawnBinding& binding, ConfigManager& config, std::string_view section) const {
        config.SetBool(section, "bodyguard", binding.bodyguard);
        config.SetInt(section, "team", binding.team);
        config.SetInt(section, "npc_type", binding.npc.npcTypeIndex);
        config.SetInt(section, "nationality", binding.npc.nationality);
        config.SetInt(section, "tier", binding.npc.tier);
        config.SetBool(section, "mercenary", binding.npc.mercenary);
        config.SetBool(section, "has_loadout", binding.hasLoadout);
        config.SetString(section, "loadout_path", binding.loadoutPath);
        auto npc = binding.npc;
        SaveOverrides(section, npc);
    }

    void Spawn(const SpawnBinding& binding, const RuntimeContextSnapshot& runtime) const {
        owner.SpawnBindingNPC(binding, runtime);
    }

    void AppendLeadingParams(SpawnBinding& binding, std::vector<KeybindParam>& params) const {
        params.emplace_back("bodyguard", "Bodyguard", &binding.bodyguard, "Will join your team");
        params.emplace_back(
            "mercenary", "Mercenary", &binding.npc.mercenary, "Generate with mercenary color scheme"
        );
    }

    void AppendTrailingParams(SpawnBinding& binding, std::vector<KeybindParam>& params) const {
        params.emplace_back(
            "team", "Team", &binding.team, 0, 9,
            "Assign the NPC to a team number. 0-4 are the default teams. 0 means no team."
        );
    }

private:
    void ReadSelection(SpawnBinding& binding) const {
        binding.spawn = owner.cfg.spawn;
        binding.bodyguard = owner.cfg.bodyguard;
        binding.team = owner.cfg.npcTeam;
        binding.npc = owner.BuildPresetData();
        binding.hasLoadout = owner.loadoutPicker.HasSelection();
        binding.loadoutPath = binding.hasLoadout ? owner.loadoutPicker.SelectedPath().string() : "";
        binding.summary = NPC_TYPES[std::clamp(binding.npc.npcTypeIndex, 0, NPC_TYPES_COUNT - 1)].displayName;
        if (binding.hasLoadout) binding.summary += " + Loadout";
    }
};

void NPCEditorSection::LoadSpawnBindings() {
    SpawnBindings::BindingList<SpawnBinding, BindingOps>(
        spawnBindings, nextBindingId, pendingDeleteBindingId, NPC_BINDING_CONFIG, BindingOps{*this}
    )
        .Load();
}

void NPCEditorSection::RenderSpawnBindings() {
    SpawnBindings::BindingList<SpawnBinding, BindingOps>(
        spawnBindings, nextBindingId, pendingDeleteBindingId, NPC_BINDING_CONFIG, BindingOps{*this}
    )
        .Render();
}

void NPCEditorSection::Render() {
    const SectionStyle::StyleRAII style;

    RenderSpawnBindings();
    ImGui::Spacing();

    auto npcGetter = [](void* data, int idx) -> const char* {
        return static_cast<const NPCTypeInfo*>(data)[idx].displayName;
    };
    static float npcTypeComboW = GuiUtils::CalcComboWidth(npcGetter, (void*)NPC_TYPES, NPC_TYPES_COUNT);
    static float nationalityComboW = GuiUtils::CalcComboWidth(NATIONALITY_NAMES, NATIONALITY_COUNT);
    GuiUtils::PrepareNextCombo(npcTypeComboW);
    ImGui::Combo("##NPCTypeSelector", &cfg.npcTypeIndex, npcGetter, (void*)NPC_TYPES, NPC_TYPES_COUNT);
    TooltipHelper::ShowTooltip("Choose which NPC class to spawn");
    ImGui::SameLine();
    GuiUtils::PrepareNextCombo(nationalityComboW);
    ImGui::Combo("##NationalitySelector", &cfg.npcNationality, NATIONALITY_NAMES, NATIONALITY_COUNT);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(GuiUtils::CachedTierComboWidth());
    GuiUtils::DebouncedDragInt("##NPCTierSlider", &cfg.npcTier, 0.1f, 0, 8, "Tier %d");

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
                [this]() { return BuildPresetData(); }, [this](const NPCPresetData& d) { ApplyPresetData(d); }
            );
            break;
        default: break;
    }

    ImGui::EndChild();
}
