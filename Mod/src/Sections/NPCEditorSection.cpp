#include "Menu/Sections/Spawner/NPCEditorSection.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "Menu/Sections/Spawner/SpawnBindings.h"
#include "ConfigManager.h"

#include "Utils/GuiUtils.h"
#include "Utils/PresetLinkResolution.h"
#include "Utils/SpawnWorkflow.h"

namespace {
    constexpr SpawnBindings::BindingConfig NPC_BINDING_CONFIG{
        .indexSection = "NPCSpawnBindings",
        .bindingPrefix = "NPCSpawnBinding_",
        .defaultName = "Spawn NPC",
        .addTooltip = "Create a shortcut for the current NPC and placement",
        .emptyText = "No NPC spawn shortcuts saved",
        .updateTooltip = "Use the current NPC and placement for this shortcut",
        .deletePopupTitle = "Delete NPC Shortcut",
        .deletePrompt = "Delete this NPC spawn shortcut?",
        .spawnParams = {
            .forwardLabel = "Distance",
            .forwardMin = static_cast<float>(NPCPresetData::K_MIN_SPAWN_DISTANCE_FORWARD),
            .forwardMax = static_cast<float>(NPCPresetData::K_MAX_SPAWN_DISTANCE_FORWARD),
            .forwardTooltip = "How far in front the NPC appears",
            .upLabel = "Height",
            .upMin = static_cast<float>(NPCPresetData::K_MIN_SPAWN_DISTANCE_UP),
            .upMax = static_cast<float>(NPCPresetData::K_MAX_SPAWN_DISTANCE_UP),
            .upTooltip = "How high above the player the NPC appears",
            .scaleMin = static_cast<float>(NPCPresetData::K_MIN_SPAWN_SCALE),
            .scaleMax = static_cast<float>(NPCPresetData::K_MAX_SPAWN_SCALE),
            .scaleTooltip = "NPC size; larger NPCs may need more height",
        },
    };

    SpawnConfig NPCSpawnConfig(const NPCPresetData& npc) {
        return {
            .distanceForward = static_cast<float>(npc.spawnDistanceForward),
            .distanceUp = static_cast<float>(npc.spawnDistanceUp),
            .scale = static_cast<float>(npc.spawnScale),
            .snapToGround = npc.snapToGround,
        };
    }

    bool ResolveLoadoutLink(
        const PresetLink<LoadoutPresetData>& link, std::optional<ResolvedLoadoutPresetData>& resolved,
        std::string& error
    ) {
        resolved.reset();
        error.clear();
        if (IsEmptyPresetLink(link)) return true;

        auto resolution = LoadoutPresetResolver{}.Resolve(link);
        if (!resolution.success || !resolution.value) {
            error = PresetLinkResolution::FormatDiagnostic(resolution);
            if (error.empty()) error = "Loadout preset is unavailable";
            return false;
        }

        resolved = std::move(*resolution.value);
        return true;
    }

    std::array<std::uint64_t, 3> BindingCatalogRevisions() {
        return {
            LoadoutPresetSerializer::GetCatalogRevision(),
            WeaponPresetSerializer::GetCatalogRevision(),
            ArmorPresetSerializer::GetCatalogRevision(),
        };
    }
}

void NPCEditorSection::BuildDescriptors() {
    auto& o = overrides;

    physicalFields = {
        OverrideField("Height", o.heightRate, 0.01f, "Character height (1.0 = normal)"),
        OverrideField("Body Build", o.muscleRate, 0.01f, "Character muscle and bulk (1.0 = normal)"),
        OverrideField(
            "Size Consistency", o.scaleMutationInhibitor, 0.01f, "Higher values make body proportions more even"
        ),
        OverrideField("Face", o.faceType, 0.1f, "Choose a face style"),
        OverrideField("Eye Color", o.eyeColor, 0.1f, "Choose an eye color"),
        OverrideField("Hair Length", o.hairLength, 0.01f, "Hair length (0 = bald, 1 = maximum)"),
        OverrideField("Hair Color", o.hairColor, 0.01f, "0 = blonde, 0.5 = brown, 1 = black"),
    };
    combatFields = {
        OverrideField("Damage", o.damageRate, 0.1f, "Overall damage dealt by this NPC"),
        OverrideField("Limb Damage", o.limbDamageRate, 0.1f, "Damage dealt to individual limbs"),
        OverrideField("Limb Severing", o.dismemberThreshold, 0.1f, "Higher values make limbs easier to sever"),
        OverrideField("Health Recovery", o.regenRate, 0.01f, "How quickly health returns"),
        OverrideField("Damage Resistance", o.aiInvincibility, 0.01f, "Resistance to incoming damage"),
        OverrideField("Armor Resistance", o.aiArmorInvincibility, 0.01f, "How much damage the NPC's armor ignores"),
        OverrideField("Combat Skill", o.bodySkill, 0.1f, "Overall combat ability, movement, and reactions"),
    };
    behaviorFields = {
        OverrideField("Start Kneeling", o.startKneeled, "NPC starts in a kneeling position"),
        OverrideField("Pants Only", o.spawnInPants, "NPC wears pants without armor"),
        OverrideField("Blossfechten Gear", o.blossfechtenGear, "NPC wears Blossfechten gear"),
        OverrideField("Clear Nearby Objects", o.clearSpawnArea, "Remove objects around the NPC"),
        OverrideField("Drunkenness", o.drunk, 0.01f, "0 = sober, 1 = fully drunk"),
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

SpawnWorkflow::SpawnCompletion NPCEditorSection::MakeSpawnCompletion(
    SpawnTarget target, GuiUtils::StatusMessage::Token token
) const {
    return [this, target, token](const SpawnWorkflow::SpawnResult& result) {
        if (result.success) {
            StoreSpawnResult(target, {.token = token});
            return;
        }

        const std::string_view prefix =
            target == SpawnTarget::Shortcuts ? "NPC shortcut failed: " : "NPC failed: ";
        std::string error;
        error.reserve(prefix.size() + result.error.size());
        error.append(prefix).append(result.error);
        StoreSpawnResult(target, {.token = token, .error = std::move(error)});
    };
}

void NPCEditorSection::StoreSpawnResult(SpawnTarget target, PendingSpawnResult result) const {
    std::lock_guard lock(spawnFeedbackMutex);
    auto& pending = pendingSpawnResults[static_cast<std::size_t>(target)];
    if (pending && result.token < pending->token) return;
    pending = std::move(result);
}

bool NPCEditorSection::ResolveNPCLoadout(std::optional<ResolvedLoadoutPresetData>& resolved, std::string& error) {
    if (!loadoutPresetLink.HasLink()) {
        resolved.reset();
        error.clear();
        return true;
    }
    if (!ResolveLoadoutLink(loadoutPresetLink.GetLink(), resolved, error)) {
        loadoutPresetLink.MarkBroken(error);
        return false;
    }

    loadoutPresetLink.MarkHealthy();
    return true;
}

void NPCEditorSection::SpawnNPC() {
    constexpr SpawnTarget TARGET = SpawnTarget::Main;
    auto snapshot = RenderSnapshot();
    if (!snapshot.player || !snapshot.world) {
        SpawnStatus(TARGET).SetError("Open a map before spawning an NPC");
        return;
    }

    std::optional<ResolvedLoadoutPresetData> resolved;
    if (loadoutPresetLink.HasLink()) {
        std::string validationError;
        if (!ResolveNPCLoadout(resolved, validationError) || !resolved) {
            SpawnStatus(TARGET).SetError("Loadout preset: " + validationError);
            return;
        }
    }

    const auto preset = BuildPresetData();
    auto requestResult = SpawnWorkflow::BuildNPCSpawnParams(preset, std::move(resolved));
    if (!requestResult) {
        SpawnStatus(TARGET).SetError("NPC preset: " + requestResult.error());
        return;
    }
    auto request = std::move(*requestResult);
    const auto token = SpawnStatus(TARGET).SetInfo("Spawning NPC...");
    request.onComplete = MakeSpawnCompletion(TARGET, token);
    (void)SpawnWorkflow::QueueNPCSpawn(snapshot, NPCSpawnConfig(preset), std::move(request));
}

void NPCEditorSection::SpawnBindingNPC(const NPCPresetData& npc, const RuntimeContextSnapshot& runtime) const {
    constexpr SpawnTarget TARGET = SpawnTarget::Shortcuts;
    if (!runtime.world || !runtime.player) {
        StoreSpawnResult(TARGET, {.error = "NPC shortcut failed: open a map first"});
        return;
    }

    std::optional<ResolvedLoadoutPresetData> loadout;
    std::string resolutionError;
    if (!ResolveLoadoutLink(npc.loadout, loadout, resolutionError)) {
        StoreSpawnResult(TARGET, {.error = "NPC shortcut failed: " + resolutionError});
        return;
    }
    auto requestResult = SpawnWorkflow::BuildNPCSpawnParams(npc, std::move(loadout));
    if (!requestResult) {
        StoreSpawnResult(TARGET, {.error = "NPC shortcut failed: " + requestResult.error()});
        return;
    }
    auto request = std::move(*requestResult);
    request.onComplete = MakeSpawnCompletion(TARGET, 0);
    (void)SpawnWorkflow::SpawnNPC(runtime, NPCSpawnConfig(npc), request);
}

void NPCEditorSection::ConsumeSpawnFeedback() {
    std::array<std::optional<PendingSpawnResult>, SPAWN_ROUTE_COUNT> results;
    {
        std::lock_guard lock(spawnFeedbackMutex);
        results.swap(pendingSpawnResults);
    }

    for (std::size_t index = 0; index < results.size(); ++index) {
        auto& result = results[index];
        if (!result) continue;

        auto& status = spawnStatuses[index];
        if (result->token != 0 && result->token != status.revision) continue;
        if (!result->error.empty()) {
            status.SetError(std::move(result->error));
            continue;
        }

        if (result->token == 0)
            status.ClearText();
        else
            status.ClearText(result->token);
    }
}

NPCPresetData NPCEditorSection::BuildPresetData() const {
    NPCPresetData d;
    d.npcTypeIndex = cfg.npcTypeIndex;
    d.nationality = cfg.npcNationality;
    d.tier = cfg.npcTier;
    d.mercenary = cfg.npcMercenary;
    d.bodyguard = cfg.bodyguard;
    d.team = cfg.npcTeam;
    d.spawnDistanceForward = cfg.spawn.distanceForward;
    d.spawnDistanceUp = cfg.spawn.distanceUp;
    d.spawnScale = cfg.spawn.scale;
    d.snapToGround = cfg.spawn.snapToGround;
    d.overrides = overrides;
    d.loadout = loadoutPresetLink.GetLink();
    return d;
}

void NPCEditorSection::ApplyPresetData(const NPCPresetData& d) {
    cfg.npcTypeIndex = std::clamp(d.npcTypeIndex, 0, static_cast<int>(NPCPresetData::K_TYPES.size()) - 1);
    cfg.npcNationality = std::clamp(d.nationality, 0, static_cast<int>(NPCPresetData::K_NATIONALITY_NAMES.size()) - 1);
    cfg.npcTier = std::clamp(d.tier, NPCPresetData::K_MIN_TIER, NPCPresetData::K_MAX_TIER);
    cfg.npcMercenary = d.mercenary;
    cfg.bodyguard = d.bodyguard;
    cfg.npcTeam = d.team;
    cfg.spawn.distanceForward = static_cast<float>(d.spawnDistanceForward);
    cfg.spawn.distanceUp = static_cast<float>(d.spawnDistanceUp);
    cfg.spawn.scale = static_cast<float>(d.spawnScale);
    cfg.spawn.snapToGround = d.snapToGround;
    overrides = d.overrides;
    loadoutPresetLink.SetLink(d.loadout);
    std::optional<ResolvedLoadoutPresetData> resolved;
    std::string loadoutError;
    if (!ResolveNPCLoadout(resolved, loadoutError))
        presets.status.SetError("This NPC preset uses an unavailable loadout: " + loadoutError);
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

    ImGui::SeparatorText("Starting Limb Health");
    GuiUtils::HelpTooltip("Set the starting health of each body part. 0 uses the default value.");

    if (ImGui::Button("Clear Limb Health")) {
        overrides.headHealth = {};
        overrides.neckHealth = {};
        overrides.armRHealth = {};
        overrides.armLHealth = {};
        overrides.bodyUpperHealth = {};
        overrides.bodyLowerHealth = {};
        overrides.legRHealth = {};
        overrides.legLHealth = {};
    }
    GuiUtils::HelpTooltip("Use the default starting health for every limb");

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
    NPCEditorSection& owner;

    static void UpdateSummary(SpawnBinding& binding) {
        if (binding.npc.id.empty()) {
            binding.summary = "Unavailable NPC shortcut";
            return;
        }
        binding.summary =
            NPCPresetData::K_TYPES
                [std::clamp(binding.npc.npcTypeIndex, 0, static_cast<int>(NPCPresetData::K_TYPES.size()) - 1)]
                    .displayName;
        if (!binding.resolutionError.empty())
            binding.summary += " + Unavailable";
        else if (!IsEmptyPresetLink(binding.npc.loadout))
            binding.summary += " + Loadout";
    }

    static bool Refresh(SpawnBinding& binding) {
        if (binding.npc.id.empty()) {
            if (binding.resolutionError.empty()) binding.resolutionError = "saved shortcut is unavailable";
            UpdateSummary(binding);
            return false;
        }

        binding.resolutionError.clear();
        if (auto validation = binding.npc.ValidateForSave(); !validation) {
            binding.resolutionError = std::move(validation.error);
            UpdateSummary(binding);
            return false;
        }
        std::optional<ResolvedLoadoutPresetData> resolved;
        (void)ResolveLoadoutLink(binding.npc.loadout, resolved, binding.resolutionError);
        UpdateSummary(binding);
        return binding.resolutionError.empty();
    }

    bool Capture(SpawnBinding& binding) const {
        SpawnBinding candidate;
        candidate.id = binding.id;
        candidate.npc = owner.BuildPresetData();
        candidate.npc.name = "NPC shortcut";
        candidate.npc.id = "npc-binding-" + std::to_string(binding.id);
        if (!Refresh(candidate)) {
            owner.presets.status.SetError("Cannot create NPC shortcut: " + candidate.resolutionError);
            return false;
        }

        binding.npc = std::move(candidate.npc);
        binding.resolutionError = std::move(candidate.resolutionError);
        binding.summary = std::move(candidate.summary);
        return true;
    }

    void LoadFields(SpawnBinding& binding, ConfigManager& config, const char* section) const {
        std::string serialized;
        if (!SpawnBindings::DecodeData(config.GetString(section, "data_hex", ""), serialized)) {
            binding.resolutionError = "saved shortcut is unavailable";
            UpdateSummary(binding);
            return;
        }
        auto loaded = NPCPresetSerializer::DeserializeFromIniResult(serialized);
        if (!loaded.success) {
            binding.resolutionError = "saved shortcut is unavailable";
            UpdateSummary(binding);
            return;
        }
        binding.npc = std::move(loaded.value);
        Refresh(binding);
    }

    void SaveFields(const SpawnBinding& binding, ConfigManager& config, const char* section) const {
        if (binding.npc.id.empty()) return;
        const auto encoded = SpawnBindings::EncodeData(NPCPresetSerializer::SerializeToIni(binding.npc));
        config.SetString(section, "data_hex", encoded.c_str());
    }

    std::shared_ptr<const NPCPresetData> MakeSnapshot(const SpawnBinding& binding) const {
        return std::make_shared<const NPCPresetData>(binding.npc);
    }

    void Spawn(const NPCPresetData& npc, const RuntimeContextSnapshot& runtime) const {
        owner.SpawnBindingNPC(npc, runtime);
    }

    void AppendParams(
        SpawnBinding& binding, std::vector<KeybindParam>& params, const SpawnBindings::SpawnParamConfig& config
    ) const {
        params.emplace_back("bodyguard", "Bodyguard", &binding.npc.bodyguard, "Will fight on your side");
        params.emplace_back("mercenary", "Mercenary", &binding.npc.mercenary, "Use mercenary colors");
        SpawnBindings::AppendSpawnParams(
            params, binding.npc.spawnDistanceForward, binding.npc.spawnDistanceUp, binding.npc.spawnScale,
            binding.npc.snapToGround, config
        );
        params.emplace_back(
            "team", "Alliance", &binding.npc.team, NPCPresetData::K_MIN_TEAM, NPCPresetData::K_MAX_TEAM,
            "Choose which side the NPC fights for. 0 means no alliance; 1-4 are the standard alliances."
        );
    }
};

void NPCEditorSection::LoadSpawnBindings() {
    SpawnBindings::BindingList<SpawnBinding, BindingOps>(
        spawnBindings, nextBindingId, pendingDeleteBindingId, NPC_BINDING_CONFIG, BindingOps{*this}
    )
        .Load();
    spawnBindingCatalogRevisions = BindingCatalogRevisions();
}

void NPCEditorSection::RenderSpawnBindings() {
    auto bindings = SpawnBindings::BindingList<SpawnBinding, BindingOps>(
        spawnBindings, nextBindingId, pendingDeleteBindingId, NPC_BINDING_CONFIG, BindingOps{*this}
    );
    const auto revisions = BindingCatalogRevisions();
    if (revisions != spawnBindingCatalogRevisions) {
        spawnBindingCatalogRevisions = revisions;
        bindings.PublishSnapshots();
    }

    const bool hasLoadoutLinks = std::ranges::any_of(spawnBindings, [](const auto& binding) {
        return binding && !IsEmptyPresetLink(binding->npc.loadout);
    });
    if (hasLoadoutLinks) {
        if (ImGui::SmallButton("Check Saved Shortcuts")) {
            bindings.PublishSnapshots();
            const auto brokenCount = std::ranges::count_if(spawnBindings, [](const auto& binding) {
                return binding && !IsEmptyPresetLink(binding->npc.loadout) && !binding->resolutionError.empty();
            });
            if (brokenCount == 0)
                presets.status.Notify("All NPC shortcuts are ready");
            else
                presets.status.SetError("Unavailable NPC shortcuts: " + std::to_string(brokenCount));
        }
        presets.status.RenderResult();
    }

    bindings.Render();
}

void NPCEditorSection::Render() {
    ConsumeSpawnFeedback();
    ImGui::SeparatorText("New NPC");

    auto npcGetter = [](void* data, int idx) -> const char* {
        return static_cast<const NPCPresetData::TypeInfo*>(data)[idx].displayName;
    };
    constexpr int NPC_TYPE_COUNT = static_cast<int>(NPCPresetData::K_TYPES.size());
    constexpr int NATIONALITY_COUNT = static_cast<int>(NPCPresetData::K_NATIONALITY_NAMES.size());
    static float npcTypeComboW =
        GuiUtils::CalcComboWidth(npcGetter, (void*)NPCPresetData::K_TYPES.data(), NPC_TYPE_COUNT);
    static float nationalityComboW =
        GuiUtils::CalcComboWidth(NPCPresetData::K_NATIONALITY_NAMES.data(), NATIONALITY_COUNT);
    static float setupControlWidth =
        (std::
             max)({npcTypeComboW, nationalityComboW, GuiUtils::K_DRAG_WIDTH, GuiUtils::CheckboxNaturalWidth("Place on Ground")});
    constexpr ImGuiTableFlags SETUP_TABLE_FLAGS = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;
    if (ImGui::BeginTable("##NPCSpawnSetup", 3, SETUP_TABLE_FLAGS)) {
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, setupControlWidth);
        ImGui::TableSetupColumn("Nationality", ImGuiTableColumnFlags_WidthFixed, setupControlWidth);
        ImGui::TableSetupColumn("Tier", ImGuiTableColumnFlags_WidthFixed, setupControlWidth);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("Type");

        ImGui::TableNextColumn();
        ImGui::TextDisabled("Nationality");

        ImGui::TableNextColumn();
        ImGui::TextDisabled("Tier");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        GuiUtils::PrepareNextCombo(setupControlWidth);
        ImGui::Combo(
            "##NPCTypeSelector", &cfg.npcTypeIndex, npcGetter, (void*)NPCPresetData::K_TYPES.data(), NPC_TYPE_COUNT
        );
        GuiUtils::HelpTooltip("Choose the kind of NPC to spawn");

        ImGui::TableNextColumn();
        GuiUtils::PrepareNextCombo(setupControlWidth);
        ImGui::Combo(
            "##NationalitySelector", &cfg.npcNationality, NPCPresetData::K_NATIONALITY_NAMES.data(), NATIONALITY_COUNT
        );

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(setupControlWidth);
        GuiUtils::DebouncedDragInt(
            "##NPCTierSlider", &cfg.npcTier, 0.1f, NPCPresetData::K_MIN_TIER, NPCPresetData::K_MAX_TIER, "%d"
        );

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Checkbox("Mercenary", &cfg.npcMercenary);
        ImGui::TableNextColumn();
        ImGui::Checkbox("Bodyguard", &cfg.bodyguard);
        ImGui::TableNextColumn();
        ImGui::Checkbox("Place on Ground", &cfg.spawn.snapToGround);
        ImGui::EndTable();
    }
    if (ImGui::TreeNode("Placement")) {
        ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
        ImGui::DragFloat(
            "Distance", &cfg.spawn.distanceForward, 1.0f,
            static_cast<float>(NPCPresetData::K_MIN_SPAWN_DISTANCE_FORWARD),
            static_cast<float>(NPCPresetData::K_MAX_SPAWN_DISTANCE_FORWARD), "%.0f"
        );
        ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
        ImGui::DragFloat(
            "Height", &cfg.spawn.distanceUp, 1.0f, static_cast<float>(NPCPresetData::K_MIN_SPAWN_DISTANCE_UP),
            static_cast<float>(NPCPresetData::K_MAX_SPAWN_DISTANCE_UP), "%.0f"
        );
        ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
        ImGui::DragFloat(
            "Size", &cfg.spawn.scale, 0.01f, static_cast<float>(NPCPresetData::K_MIN_SPAWN_SCALE),
            static_cast<float>(NPCPresetData::K_MAX_SPAWN_SCALE), "%.2f"
        );
        ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
        ImGui::DragInt("Alliance", &cfg.npcTeam, 0.1f, NPCPresetData::K_MIN_TEAM, NPCPresetData::K_MAX_TEAM);
        ImGui::TreePop();
    }

    const bool canSpawn = RenderWorld() != nullptr;
    if (!canSpawn) ImGui::BeginDisabled();
    if (GuiUtils::Button("Spawn NPC", GuiUtils::ButtonTone::Primary)) SpawnNPC();
    if (!canSpawn) ImGui::EndDisabled();
    if (!canSpawn && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetItemTooltip("Open a map before spawning an NPC");
    SpawnStatus(SpawnTarget::Main).Render();

    SpawnStatus(SpawnTarget::Shortcuts).Render();
    if (ImGui::TreeNode("Spawn Shortcuts")) {
        RenderSpawnBindings();
        ImGui::TreePop();
    }

    ImGui::Spacing();
    if (GuiUtils::Button("Clear Custom NPC Stats", GuiUtils::ButtonTone::Danger)) {
        overrides = {};
    }
    GuiUtils::HelpTooltip("Disable every custom NPC value");
    GuiUtils::RenderOverrideCount(
        CountActive(physicalFields) + CountActive(combatFields) + CountActive(behaviorFields) +
        CountActive(bodyConditionFields)
    );
    presets.status.Render();

    ImGui::Spacing();
    ImGui::BeginChild("##npc_scroll", ImVec2(0, 0));

    static constexpr const char* NPC_TAB_LABELS[] = {"Body",        "Combat",    "Behavior",
                                                     "Limb Health", "Equipment", "Presets"};
    GuiUtils::RenderUnderlineTabs("##NPCEditorTabs", activeTab, NPC_TAB_LABELS, 6);
    switch (activeTab) {
        case 0: RenderPhysicalTab(); break;
        case 1: RenderCombatTab(); break;
        case 2: RenderBehaviorTab(); break;
        case 3: RenderBodyConditionTab(); break;
        case 4: {
            ImGui::PushID("equipment");
            const bool refreshed = loadoutPresetLink.RefreshIfCatalogChanged();
            const bool changed = loadoutPresetLink.Render("Loadout Preset");
            if (loadoutPresetLink.HasLink() && (refreshed || changed)) {
                std::optional<ResolvedLoadoutPresetData> resolved;
                std::string error;
                if (!ResolveNPCLoadout(resolved, error)) presets.status.SetError("NPC loadout: " + error);
            }
            if (loadoutPresetLink.HasLink() && !loadoutPresetLink.IsBroken())
                ImGui::TextColored(
                    ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "Selected loadout will be used instead of random equipment"
                );
            else if (loadoutPresetLink.HasLink())
                ImGui::TextDisabled("Choose an available loadout before spawning");
            else
                ImGui::TextDisabled("No preset selected - NPC will use random equipment");
            ImGui::PopID();
            break;
        }
        case 5:
            if (loadoutPresetLink.RefreshIfCatalogChanged() && loadoutPresetLink.HasLink()) {
                std::optional<ResolvedLoadoutPresetData> resolved;
                std::string error;
                if (!ResolveNPCLoadout(resolved, error)) presets.status.SetError("NPC loadout: " + error);
            }
            if (loadoutPresetLink.IsBroken()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                ImGui::TextWrapped("This loadout is unavailable: %s", loadoutPresetLink.GetDiagnostic().c_str());
                ImGui::PopStyleColor();
            }
            presets.RenderPresetsTab(
                [this](const char*, bool) {
                    auto data = BuildPresetData();
                    return PresetBuildResult<NPCPresetData>::Success(std::move(data));
                },
                [this](const NPCPresetData& d) {
                    ApplyPresetData(d);
                    return PresetApplyDisposition::Applied;
                }
            );
            break;
        default: break;
    }

    ImGui::EndChild();
}
