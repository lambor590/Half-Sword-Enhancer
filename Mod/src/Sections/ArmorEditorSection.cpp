#include "Menu/Sections/Equipment/ArmorEditorSection.h"

#include <utility>
#include "Hooks/GameHook.h"
#include "Utils/ArmorGenerationUi.h"
#include "Utils/BlueprintRegistry.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/GameClass.h"
#include "Utils/GuiUtils.h"
#include "Utils/PresetUtils.h"
#include "Utils/PresetApplication.h"
#include "Utils/Spawner.h"
#include "Utils/SpawnWorkflow.h"
#include "Utils/TierValidation.h"
#include "SDK/BP_Armor_Master_classes.hpp"
#include "SDK/BP_Armor_Modular_Core_Master_classes.hpp"

namespace {
    template <typename... OverrideTypes> bool HasAnyEnabledOverride(const OverrideTypes&... overrides) {
        return (... || overrides.enabled);
    }
}

void ArmorEditorSection::BuildDescriptors() {
    auto& rp = runtimeProps;

    protectionFields = {
        OverrideField("Blunt Protection", rp.protectionBlunt, 0.1f, "Protection against blunt/crushing damage"),
        OverrideField("Cut Protection", rp.protectionCut, 0.1f, "Protection against cutting/slashing damage"),
        OverrideField("Stab Protection", rp.protectionStab, 0.1f, "Protection against piercing/stabbing damage"),
    };
    physicsFields = {
        OverrideField(
            "Material Heaviness", rp.materialDensity, 0.1f,
            "Higher values make the armor heavier and absorb more impact"
        ),
        OverrideField("Weight", rp.massScale, 0.01f, "Overall armor weight"),
    };
    behaviorFields = {
        OverrideField("Gauntlet Grip", rp.handsRigidity, 0.1f, "Grip strength while wearing these gauntlets"),
        OverrideField("Helmet Security", rp.strapPower, 0.1f, "How securely the helmet stays on"),
        OverrideField(
            "NPC Damage Resistance", rp.aiInvincibilityRate, 0.01f, "Damage resistance for NPCs wearing this armor"
        ),
        OverrideField("Custom Price", rp.price, 1.0f, "Custom armor price"),
        OverrideField("Can Be Picked Up", rp.pickUp, "Allow this armor to be picked up from the ground"),
    };
}

int ArmorEditorSection::CountAllActive() const {
    return CountActive(protectionFields) + CountActive(physicsFields) + CountActive(behaviorFields);
}

bool ArmorEditorSection::IsModularCore() const {
    SDK::UClass* coreClass = armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
    if (!coreClass || !coreClass->ClassDefaultObject) return false;
    return GameClass::IsModularArmor(coreClass->ClassDefaultObject);
}

void ArmorEditorSection::PopulateModulePoolForCurrentCore() {
    SDK::UClass* coreClass = armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
    if (!coreClass || coreClass == armorModules.populatedForCore) return;

    armorModules = {};
    armorModules.populatedForCore = coreClass;

    if (!coreClass->ClassDefaultObject) return;
    if (!GameClass::IsModularArmor(coreClass->ClassDefaultObject)) return;

    auto* cdo = static_cast<SDK::ABP_Armor_Modular_Core_Master_C*>(coreClass->ClassDefaultObject);

    auto collect = [](std::vector<ModuleEntry>& out, const SDK::TArray<SDK::UClass*>& arr) {
        out.reserve(arr.Num());
        for (int i = 0; i < arr.Num(); ++i) {
            if (arr[i]) out.push_back({arr[i], BlueprintRegistry::CleanDisplayName(arr[i]->GetName())});
        }
    };
    collect(armorModules.modules1, cdo->Available_Modules_1);
    collect(armorModules.modules2, cdo->Available_Modules_2);
    collect(armorModules.modules3, cdo->Available_Modules_3);
    armorModules.populated = true;
}

void ArmorEditorSection::ResetArmorPassport() {
    renderDraftRevision = draftRevision.fetch_add(1, std::memory_order_acq_rel) + 1;
    armorGenerationPending.store(false, std::memory_order_release);
    {
        std::scoped_lock lock(pendingRenderMutex);
        pendingRenderUpdates.draft.reset();
    }
    armorPassport = {};
    armorCorePath.clear();
    armorPassport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393 = {0.5f, 0.5f, 0.5f, 1.0f};
    armorPassport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C = {0.5f, 0.5f, 0.5f, 1.0f};
    armorPassport.Tier_50_E497AE434B01B84C559DEE8A863BB42E = static_cast<SDK::Enum_Ranks>(4);
    armorPassport.Price_27_8E3ADD54484EFC4A59FE9381485AC192 = 50.0;
}

void ArmorEditorSection::QueueGeneration(
    SDK::EArmorSlots_Enum slot, SDK::Enum_Ranks tier, EquipmentGenerator::ArmorGenerationOptions options
) {
    const std::uint64_t revision = draftRevision.fetch_add(1, std::memory_order_acq_rel) + 1;
    armorGenerationPending.store(true, std::memory_order_release);
    {
        std::scoped_lock lock(pendingRenderMutex);
        pendingRenderUpdates.draft.reset();
    }
    const bool queued =
        GameHook::QueueAction([this, slot, tier, options, revision](const RuntimeContextSnapshot& runtime) {
            auto* world = runtime.world;
            if (!world) {
                if (draftRevision.load(std::memory_order_acquire) == revision)
                    armorGenerationPending.store(false, std::memory_order_release);
                return;
            }
            auto generated = EquipmentGenerator::GenerateArmor(world, tier, slot, options);
            auto snapshot = PresetApplication::SnapshotArmorPassport(generated);
            if (snapshot) {
                PendingDraftUpdate update;
                update.revision = revision;
                update.data = std::move(*snapshot);
                PublishDraftUpdate(std::move(update));
            } else {
                PublishStatus("Could not create an armor design for the selected slot and tier", true, revision);
            }
            if (draftRevision.load(std::memory_order_acquire) == revision)
                armorGenerationPending.store(false, std::memory_order_release);
        });
    if (!queued) {
        armorGenerationPending.store(false, std::memory_order_release);
        presets.status.Set("Could not create armor design", true);
    }
}

void ArmorEditorSection::GenerateArmorPassport() {
    uint16_t mask = TierValidation::VALID_ARMOR_TIER_MASKS[cfg.armorSlotIndex];
    cfg.armorTier = TierValidation::NearestValidTier(mask, cfg.armorTier);
    QueueGeneration(
        static_cast<SDK::EArmorSlots_Enum>(ARMOR_SLOTS[cfg.armorSlotIndex].slotEnum),
        static_cast<SDK::Enum_Ranks>(cfg.armorTier), cfg.armorOptions
    );
}

void ArmorEditorSection::RandomizeArmorPassport() {
    cfg.armorSlotIndex = GameConstants::RandomInt(0, ARMOR_SLOT_COUNT - 1);
    cfg.armorTier = TierValidation::RandomValidTier(TierValidation::VALID_ARMOR_TIER_MASKS[cfg.armorSlotIndex]);
    GenerateArmorPassport();
}

void ArmorEditorSection::SpawnPreview() {
    if (!armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) {
        preview.Destroy();
        return;
    }
    auto snapshot = RenderSnapshot();
    if (!snapshot.player || !snapshot.world) {
        preview.Destroy();
        return;
    }

    lastPreviewedPassport = armorPassport;
    lastPreviewedProps = runtimeProps;

    bool hasOverrides = CountAllActive() > 0;
    auto runtimeSnapshot = runtimeProps;
    auto preset = BuildPresetData();

    SpawnWorkflow::QueueArmorPreview(
        snapshot, preview, cfg.spawn, std::move(preset), [hasOverrides, runtimeSnapshot](SDK::AActor* actor) {
            if (hasOverrides) (void)PresetApplication::ApplyArmorRuntimeOverrides(actor, runtimeSnapshot);
        }
    );
}

bool ArmorEditorSection::PassportChanged(const SDK::FStr_Passport_Armor1& a, const SDK::FStr_Passport_Armor1& b) {
    return !PresetApplication::ArmorPassportsEqual(a, b);
}

void ArmorEditorSection::RenderArmorTierCombo() {
    uint16_t mask = TierValidation::VALID_ARMOR_TIER_MASKS[cfg.armorSlotIndex];
    cfg.armorTier = TierValidation::NearestValidTier(mask, cfg.armorTier);

    if (GuiUtils::BeginSizedCombo(
            "##GenTier", GuiUtils::TIER_LABELS[cfg.armorTier], GuiUtils::CachedTierComboWidth()
        )) {
        for (int t = 0; t <= 8; ++t) {
            if (!(mask & (1 << t))) continue;
            if (ImGui::Selectable(GuiUtils::TIER_LABELS[t], t == cfg.armorTier)) cfg.armorTier = t;
            if (t == cfg.armorTier) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void ArmorEditorSection::SpawnArmor() {
    if (!armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) return;
    auto snapshot = RenderSnapshot();
    if (!snapshot.player || !snapshot.world) return;

    if (cfg.preview.livePreview) preview.Disable();

    SpawnArmor(snapshot, BuildSpawnDraftSnapshot());
}

ArmorEditorSection::SpawnDraftSnapshot ArmorEditorSection::BuildSpawnDraftSnapshot() const {
    return {
        .spawn = cfg.spawn,
        .preset = BuildPresetData(),
    };
}

void ArmorEditorSection::PublishSpawnDraftSnapshot() {
    auto snapshot = BuildSpawnDraftSnapshot();
    std::scoped_lock lock(spawnDraftMutex);
    if (renderDraftRevision < publishedSpawnDraftRevision) return;
    publishedSpawnDraft = snapshot;
    publishedSpawnDraftRevision = renderDraftRevision;
}

bool ArmorEditorSection::PublishAppliedPresetSpawnSnapshot(const PendingDraftUpdate& update) {
    std::scoped_lock lock(spawnDraftMutex);
    if (draftRevision.load(std::memory_order_acquire) != update.revision ||
        update.revision < publishedSpawnDraftRevision)
        return false;

    auto snapshot = publishedSpawnDraft;
    snapshot.preset = update.data;
    publishedSpawnDraft = snapshot;
    publishedSpawnDraftRevision = update.revision;
    return true;
}

void ArmorEditorSection::SpawnArmor(const RuntimeContextSnapshot& runtime, SpawnDraftSnapshot draft) {
    if (draft.preset.armorCorePath.empty() || !runtime.player || !runtime.world) return;
    ItemSpawnPresetData data;
    data.source = ItemSpawnPresetSource::ArmorPreset;
    data.spawn = {
        .distanceForward = draft.spawn.distanceForward,
        .distanceUp = draft.spawn.distanceUp,
        .scale = draft.spawn.scale,
        .snapToGround = draft.spawn.snapToGround,
    };
    data.armorPreset = MakePresetCopyLink(std::move(draft.preset));
    (void)SpawnWorkflow::QueueItemPresetSpawn(runtime, data);
}

void ArmorEditorSection::RenderGenerationControls() {
    auto [world, player] = RenderPlayerWorld();

    ImGui::PushID("gen");

    static float slotComboW = 0;
    if (slotComboW == 0) {
        float maxW = 0;
        for (int i = 0; i < ARMOR_SLOT_COUNT; ++i) {
            float w = ImGui::CalcTextSize(ARMOR_SLOTS[i].name).x;
            if (w > maxW) maxW = w;
        }
        slotComboW = GuiUtils::ComboWidthFromText(maxW);
    }

    if (GuiUtils::BeginSizedCombo("##Slot", ARMOR_SLOTS[cfg.armorSlotIndex].name, slotComboW)) {
        for (int i = 0; i < ARMOR_SLOT_COUNT; ++i) {
            if (ImGui::Selectable(ARMOR_SLOTS[i].name, i == cfg.armorSlotIndex)) cfg.armorSlotIndex = i;
            if (i == cfg.armorSlotIndex) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    (void)GuiUtils::SameLineIfFits(GuiUtils::CachedTierComboWidth());
    RenderArmorTierCombo();

    ArmorGenerationUi::RenderOptions(cfg.armorOptions);

    ImGui::Spacing();
    if (GuiUtils::Button("Create Armor Design")) {
        if (player && world) GenerateArmorPassport();
    }
    GuiUtils::HelpTooltip("Create an armor design for the selected slot and tier");
    (void)GuiUtils::SameLineIfFitsButton("Random Armor Design");
    if (GuiUtils::Button("Random Armor Design")) {
        if (player && world) RandomizeArmorPassport();
    }
    GuiUtils::HelpTooltip("Create a random armor design");
    (void)GuiUtils::SameLineIfFitsButton("Clear Armor Design");
    if (GuiUtils::Button("Clear Armor Design")) ResetArmorPassport();
    GuiUtils::HelpTooltip("Return the editor to an empty armor design");

    if (armorGenerationPending) {
        (void)GuiUtils::SameLineIfFits(ImGui::CalcTextSize("Creating design...").x);
        ImGui::TextDisabled("Creating design...");
    }

    ImGui::PopID();
}

void ArmorEditorSection::RenderModulesTab() {
    ImGui::PushID("modules");

    if (!IsModularCore()) {
        ImGui::TextDisabled("This armor does not support custom parts");
        ImGui::PopID();
        return;
    }

    PopulateModulePoolForCurrentCore();

    if (!armorModules.populated) {
        ImGui::TextDisabled("Armor parts are unavailable");
        ImGui::PopID();
        return;
    }

    ImGui::Checkbox("Parts Only", &armorPassport.CoreRemoved_12_5CFF8F6D4A05C15812594CAF6771C66B);
    GuiUtils::HelpTooltip("Show only the attached armor parts");

    ImGui::Spacing();
    GuiUtils::RenderModuleIndexCombo(
        "Armor Part 1", armorPassport.Module1_5_46B7198E4341C93CBF6AE989EF9898E4, armorModules.modules1,
        moduleFilters[0], armorModules.cachedWidths[0]
    );
    GuiUtils::RenderModuleIndexCombo(
        "Armor Part 2", armorPassport.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0, armorModules.modules2,
        moduleFilters[1], armorModules.cachedWidths[1]
    );
    GuiUtils::RenderModuleIndexCombo(
        "Armor Part 3", armorPassport.Module3_9_E282C465414F6D4EF2A8039FBA847AD2, armorModules.modules3,
        moduleFilters[2], armorModules.cachedWidths[2]
    );

    ImGui::PopID();
}

void ArmorEditorSection::RenderColorsTab() {
    ImGui::PushID("colors");

    ImGui::SeparatorText("Fabric Colors");
    GuiUtils::RenderColorEditor("Fabric Color 1", armorPassport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393);
    GuiUtils::RenderColorEditor("Fabric Color 2", armorPassport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C);

    ImGui::PopID();
}

void ArmorEditorSection::RenderStatsTab() {
    ImGui::PushID("stats");

    ImGui::SeparatorText("Base Values");
    GuiUtils::RenderFreeTierCombo("Tier", armorPassport.Tier_50_E497AE434B01B84C559DEE8A863BB42E);
    GuiUtils::HelpTooltip("Base armor tier");
    GuiUtils::RenderPriceDrag("Price", armorPassport.Price_27_8E3ADD54484EFC4A59FE9381485AC192);
    GuiUtils::HelpTooltip("Base armor price");

    ImGui::SeparatorText("Custom Stats");
    GuiUtils::HelpTooltip("Enable only the armor values you want to change.");

    if (ImGui::Button("Clear Custom Stats")) runtimeProps = {};
    GuiUtils::HelpTooltip("Disable every custom armor value");
    GuiUtils::RenderOverrideCount(CountAllActive());

    ImGui::Spacing();
    if (ImGui::TreeNodeEx("Protection", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderOverrideGroup(protectionFields);
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Weight")) {
        RenderOverrideGroup(physicsFields);
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Other")) {
        RenderOverrideGroup({behaviorFields.data(), 4});
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Availability")) {
        RenderOverrideField(behaviorFields[4]);
        ImGui::TreePop();
    }

    ImGui::PopID();
}

ArmorPresetData ArmorEditorSection::BuildPresetData() const {
    ArmorPresetData d;
    d.passport = armorPassport;
    d.passport.SlotsBlocked_45_0807340240E57ACE5A59D39F5E998F51 = {};
    d.runtimeProps = runtimeProps;
    d.armorCorePath =
        armorCorePath.empty()
            ? PresetUtils::ObjectToAbsolutePath(armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43)
            : armorCorePath;
    return d;
}

PresetApplyDisposition ArmorEditorSection::ApplyPresetData(const ArmorPresetData& preset) {
    const std::uint64_t revision = draftRevision.fetch_add(1, std::memory_order_acq_rel) + 1;
    pendingPresetApplyRevision = revision;
    armorGenerationPending.store(false, std::memory_order_release);
    {
        std::scoped_lock lock(pendingRenderMutex);
        pendingRenderUpdates.draft.reset();
    }

    auto queuedData = preset;
    const bool queued =
        GameHook::QueueAction([this, data = std::move(queuedData), revision](const RuntimeContextSnapshot&) mutable {
            if (draftRevision.load(std::memory_order_acquire) != revision) {
                PublishStatus("Preset could not be loaded; your current edits were kept", true, revision, true);
                return;
            }

            std::string error;
            auto materialized = data;
            if (!PresetApplication::MaterializeArmorPreset(materialized, &error)) {
                PublishStatus("Could not load preset: " + error, true, revision, true);
                return;
            }
            auto flattened = PresetApplication::SnapshotArmorPassport(materialized.passport);
            if (!flattened) {
                PublishStatus("This armor preset is invalid", true, revision, true);
                return;
            }
            data.passport = flattened->passport;
            data.armorCorePath = std::move(flattened->armorCorePath);

            PendingDraftUpdate update;
            update.revision = revision;
            update.data = std::move(data);
            update.replaceAll = true;
            update.completesPresetApply = true;
            if (!PublishAppliedPresetSpawnSnapshot(update)) {
                PublishStatus("Preset could not be loaded; your current edits were kept", true, revision, true);
                return;
            }
            PublishDraftUpdate(std::move(update));
        });
    if (!queued) {
        pendingPresetApplyRevision = 0;
        presets.status.Set("Could not load preset", true);
        return PresetApplyDisposition::Rejected;
    }
    return PresetApplyDisposition::Pending;
}

void ArmorEditorSection::PublishDraftUpdate(PendingDraftUpdate update) {
    {
        std::scoped_lock lock(pendingRenderMutex);
        if (draftRevision.load(std::memory_order_acquire) != update.revision) {
            if (!update.completesPresetApply) return;
            pendingRenderUpdates.statuses.push_back(
                {"Preset could not be loaded; your current edits were kept", true, update.revision, true}
            );
        } else {
            pendingRenderUpdates.draft = std::move(update);
        }
    }
    pendingRenderReady.store(true, std::memory_order_release);
}

void ArmorEditorSection::PublishStatus(
    std::string message, bool isError, std::uint64_t revision, bool completesPresetApply
) {
    {
        std::scoped_lock lock(pendingRenderMutex);
        pendingRenderUpdates.statuses.push_back({std::move(message), isError, revision, completesPresetApply});
    }
    pendingRenderReady.store(true, std::memory_order_release);
}

void ArmorEditorSection::ApplyDraftUpdate(PendingDraftUpdate update) {
    renderDraftRevision = update.revision;
    armorPassport = update.data.passport;
    armorCorePath = std::move(update.data.armorCorePath);
    if (update.replaceAll) runtimeProps = update.data.runtimeProps;
    armorModules = {};
}

void ArmorEditorSection::DrainPendingRenderUpdates() {
    if (!pendingRenderReady.exchange(false, std::memory_order_acq_rel)) return;

    PendingRenderUpdates updates;
    {
        std::scoped_lock lock(pendingRenderMutex);
        updates = std::move(pendingRenderUpdates);
        pendingRenderUpdates = {};
    }

    const std::uint64_t currentRevision = draftRevision.load(std::memory_order_acquire);
    if (updates.draft) {
        const std::uint64_t updateRevision = updates.draft->revision;
        const bool completesPresetApply = updates.draft->completesPresetApply;
        if (updateRevision == currentRevision) {
            ApplyDraftUpdate(std::move(*updates.draft));
            if (completesPresetApply && updateRevision == pendingPresetApplyRevision) {
                presets.CompletePendingApply(true);
                pendingPresetApplyRevision = 0;
            }
        } else if (completesPresetApply && updateRevision == pendingPresetApplyRevision) {
            presets.CompletePendingApply(false, "Preset could not be loaded; your current edits were kept");
            pendingPresetApplyRevision = 0;
        }
    }
    for (auto& status : updates.statuses) {
        if (status.completesPresetApply) {
            if (status.revision != pendingPresetApplyRevision) continue;
            presets.CompletePendingApply(false, std::move(status.message));
            pendingPresetApplyRevision = 0;
            continue;
        }
        if (status.revision != 0 && status.revision != currentRevision) continue;
        presets.status.Set(status.message, status.isError);
    }
}

ArmorEditorSection::ArmorEditorSection(ModContext& ctx) : Section(ctx, SECTION) {
    ResetArmorPassport();
    BuildDescriptors();
    PublishSpawnDraftSnapshot();
    InitKeybinds();
}

void ArmorEditorSection::OnOpen() {
    DrainPendingRenderUpdates();
}

void ArmorEditorSection::InitKeybinds() {
    keybinds.Add({
        .name = "Spawn Armor",
        .tooltip = "Place the armor shown in the editor in front of you",
        .configSection = "SpawnArmor",
        .keyPtr = &cfg.spawnKey,
        .callback =
            [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                SpawnDraftSnapshot draft;
                {
                    std::scoped_lock lock(spawnDraftMutex);
                    draft = publishedSpawnDraft;
                }
                SpawnArmor(runtime, draft);
            },
        .params =
            {KeybindParam(
                 "snap_to_ground", "Place on Ground", &cfg.spawn.snapToGround, "Place spawned armor on the ground"
             ),
             KeybindParam(
                 "distance_forward", "Distance", &cfg.spawn.distanceForward, 50.0f, 300.0f,
                 "How far in front of the player the armor appears"
             ),
             KeybindParam("distance_up", "Height", &cfg.spawn.distanceUp, 0.0f, 200.0f, "How high the armor appears"),
             KeybindParam("scale", "Size", &cfg.spawn.scale, 0.1f, 5.0f, "Armor size"),
             KeybindParam(
                 "live_preview", "Preview Changes", &cfg.preview.livePreview, "Show your edits on preview armor"
             )},
    });
}

void ArmorEditorSection::Render() {
    auto [world, player] = RenderPlayerWorld();

    DrainPendingRenderUpdates();
    preview.InvalidateIfDead(player, world);
    preview.SyncToggleState();
    const bool presetApplyPending = presets.IsApplyPending();
    if (presetApplyPending) ImGui::BeginDisabled();

    keybinds.Render();
    ImGui::Spacing();

    RenderGenerationControls();

    GuiUtils::RenderPreviewControls(cfg.preview, "preview armor");

    presets.status.Render();

    GuiUtils::BeginScrollWithFooter("##armor_scroll");

    static constexpr const char* AE_TAB_LABELS[] = {"Parts", "Colors", "Stats", "Presets"};
    GuiUtils::RenderUnderlineTabs("##ArmorEditorTabs", activeTab, AE_TAB_LABELS, 4);
    switch (activeTab) {
        case 0: RenderModulesTab(); break;
        case 1: RenderColorsTab(); break;
        case 2: RenderStatsTab(); break;
        case 3:
            presets.RenderPresetsTab(
                [this]() { return BuildPresetData(); },
                [this](const ArmorPresetData& data) { return ApplyPresetData(data); }
            );
            break;
        default: break;
    }

    ImGui::EndChild();

    const bool canSpawn = armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 != nullptr && player && world;
    if (!canSpawn) ImGui::BeginDisabled();
    if (GuiUtils::Button("Spawn Armor", GuiUtils::ButtonTone::Primary)) SpawnArmor();
    if (!canSpawn) ImGui::EndDisabled();

    if (presetApplyPending) ImGui::EndDisabled();

    if (!presetApplyPending && cfg.preview.livePreview) {
        bool needsUpdate = PassportChanged(armorPassport, lastPreviewedPassport) || runtimeProps != lastPreviewedProps;
        preview.Update(needsUpdate, [this]() { SpawnPreview(); });
        preview.Rotate();
    }
    PublishSpawnDraftSnapshot();
}
