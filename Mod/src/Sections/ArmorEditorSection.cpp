#include "Menu/Sections/Equipment/ArmorEditorSection.h"
#include <cstring>
#include "Hooks/GameHook.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/GuiUtils.h"
#include "Utils/Spawner.h"
#include "SDK/BP_Armor_Master_classes.hpp"
#include "SDK/BP_Armor_Modular_Core_Master_classes.hpp"

bool ArmorEditorSection::IsModularCore() const {
    SDK::UClass* coreClass = armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
    if (!coreClass || !coreClass->ClassDefaultObject) return false;
    return coreClass->ClassDefaultObject->IsA(SDK::ABP_Armor_Modular_Core_Master_C::StaticClass());
}

void ArmorEditorSection::PopulateModulePoolForCurrentCore() {
    SDK::UClass* coreClass = armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
    if (!coreClass || coreClass == armorModules.populatedForCore) return;

    armorModules = {};
    armorModules.populatedForCore = coreClass;

    if (!coreClass->ClassDefaultObject) return;
    if (!coreClass->ClassDefaultObject->IsA(SDK::ABP_Armor_Modular_Core_Master_C::StaticClass())) return;

    auto* cdo = static_cast<SDK::ABP_Armor_Modular_Core_Master_C*>(coreClass->ClassDefaultObject);

    auto collect = [](std::vector<ModuleEntry>& out, const SDK::TArray<SDK::UClass*>& arr) {
        out.reserve(arr.Num());
        for (int i = 0; i < arr.Num(); ++i) {
            if (arr[i]) out.push_back({arr[i], arr[i]->GetName()});
        }
    };
    collect(armorModules.modules1, cdo->Available_Modules_1);
    collect(armorModules.modules2, cdo->Available_Modules_2);
    collect(armorModules.modules3, cdo->Available_Modules_3);
    armorModules.populated = true;
}

void ArmorEditorSection::CreateBlankArmorPassport() {
    armorPassport = {};
    armorPassport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393 = {0.5f, 0.5f, 0.5f, 1.0f};
    armorPassport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C = {0.5f, 0.5f, 0.5f, 1.0f};
    armorPassport.Tier_50_E497AE434B01B84C559DEE8A863BB42E = static_cast<SDK::Enum_Ranks>(4);
    armorPassport.Price_27_8E3ADD54484EFC4A59FE9381485AC192 = 50.0;
}

void ArmorEditorSection::QueueGeneration(SDK::EArmorSlots_Enum slot, SDK::Enum_Ranks tier, double moduleChance) {
    armorGenerationPending = true;
    GameHook::QueueAction([this, slot, tier, moduleChance]() {
        EquipmentGenerator::Init(world);
        armorPassport = EquipmentGenerator::GenerateArmor(tier, slot, moduleChance);
        PopulateModulePoolForCurrentCore();
        armorGenerationPending = false;
    });
}

void ArmorEditorSection::GenerateArmorPassport() {
    QueueGeneration(
        static_cast<SDK::EArmorSlots_Enum>(ARMOR_SLOTS[cfg.armorSlotIndex].slotEnum),
        static_cast<SDK::Enum_Ranks>(cfg.armorTier), cfg.moduleChance
    );
}

void ArmorEditorSection::RandomizeArmorPassport() {
    cfg.armorSlotIndex = GameConstants::RandomInt(0, ARMOR_SLOT_COUNT - 1);
    cfg.armorTier = GameConstants::RandomInt(0, 8);
    GenerateArmorPassport();
}

void ArmorEditorSection::ApplyRuntimeProps(SDK::AActor* actor, const ArmorRuntimeProps& props) {
    if (!actor) return;
    auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(actor);

    if (props.protectionBlunt.enabled) armor->Protection_Blunt = props.protectionBlunt.value;
    if (props.protectionCut.enabled) armor->Protection_Cut = props.protectionCut.value;
    if (props.protectionStab.enabled) armor->Protection_Stab = props.protectionStab.value;
    if (props.materialDensity.enabled) armor->Material_Density = props.materialDensity.value;
    if (props.massScale.enabled) armor->Mass_Scale = props.massScale.value;
    if (props.handsRigidity.enabled) armor->Hands_Rigidity__Gauntlets_ = props.handsRigidity.value;
    if (props.strapPower.enabled) armor->Strap_Power__Helmet_ = props.strapPower.value;
    if (props.aiInvincibilityRate.enabled) armor->AI_Invinvcibility_Rate = props.aiInvincibilityRate.value;
    if (props.price.enabled) armor->Price = props.price.value;
    if (props.pickUp.enabled) armor->Pick_Up = props.pickUp.value;
}

int ArmorEditorSection::CountActiveOverrides() const {
    const bool flags[] = {runtimeProps.protectionBlunt.enabled, runtimeProps.protectionCut.enabled,
                          runtimeProps.protectionStab.enabled,  runtimeProps.materialDensity.enabled,
                          runtimeProps.massScale.enabled,       runtimeProps.handsRigidity.enabled,
                          runtimeProps.strapPower.enabled,      runtimeProps.aiInvincibilityRate.enabled,
                          runtimeProps.price.enabled,           runtimeProps.pickUp.enabled};
    int count = 0;
    for (bool f : flags)
        count += f;
    return count;
}

void ArmorEditorSection::SpawnPreview() {
    preview.Destroy();
    if (!armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) return;
    if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;

    lastPreviewedPassport = armorPassport;
    lastPreviewedProps = runtimeProps;

    auto props = runtimeProps;
    bool hasOverrides = CountActiveOverrides() > 0;

    Spawner::SpawnArmorFromPassport(
        world, armorPassport,
        Spawner::BuildSpawnTransform(player, cfg.spawn.distanceForward, cfg.spawn.distanceUp, cfg.spawn.scale),
        cfg.spawn.snapToGround,
        [this, props, hasOverrides](SDK::AActor* actor) {
            if (!cfg.preview.livePreview) {
                actor->K2_DestroyActor();
                return;
            }
            auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(actor);
            armor->Simulates_Physics = false;
            if (armor->Armor_Mesh_Static) armor->Armor_Mesh_Static->SetSimulatePhysics(false);
            if (armor->Armor_Mesh_Skeletal) armor->Armor_Mesh_Skeletal->SetAllBodiesSimulatePhysics(false);
            if (armor->Armor_Mesh_Primitive) armor->Armor_Mesh_Primitive->SetSimulatePhysics(false);
            actor->SetActorEnableCollision(false);
            if (hasOverrides) ApplyRuntimeProps(actor, props);
            preview.SetPreviewActor(actor);
            if (cfg.preview.autoRotate) actor->K2_SetActorRotation(SDK::FRotator{0.0, preview.GetYaw(), 0.0}, true);
        }
    );
}

bool ArmorEditorSection::PassportChanged(const SDK::FStr_Passport_Armor1& a, const SDK::FStr_Passport_Armor1& b) {
    static constexpr size_t BEFORE_TMAP =
        offsetof(SDK::FStr_Passport_Armor1, SlotsBlocked_45_0807340240E57ACE5A59D39F5E998F51);
    static constexpr size_t AFTER_TMAP =
        offsetof(SDK::FStr_Passport_Armor1, RequiresModuleHirarchy_47_9ED58E2C48514BE5153606977BE68B6A);
    static constexpr size_t TAIL_SIZE = sizeof(SDK::FStr_Passport_Armor1) - AFTER_TMAP;

    if (std::memcmp(&a, &b, BEFORE_TMAP) != 0) return true;
    return std::memcmp(
               reinterpret_cast<const char*>(&a) + AFTER_TMAP, reinterpret_cast<const char*>(&b) + AFTER_TMAP, TAIL_SIZE
           ) != 0;
}

void ArmorEditorSection::SpawnFromPassport() {
    if (!armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) return;

    if (cfg.preview.livePreview) {
        cfg.preview.livePreview = false;
        preview.Destroy();
    }

    std::function<void(SDK::AActor*)> callback = nullptr;
    bool hasOverrides = CountActiveOverrides() > 0;

    if (hasOverrides) {
        auto props = runtimeProps;
        callback = [props](SDK::AActor* actor) {
            ApplyRuntimeProps(actor, props);
        };
    }

    Spawner::SpawnArmorFromPassport(
        world, armorPassport,
        Spawner::BuildSpawnTransform(player, cfg.spawn.distanceForward, cfg.spawn.distanceUp, cfg.spawn.scale),
        cfg.spawn.snapToGround, callback
    );
}

void ArmorEditorSection::RenderGenerationControls() {
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

    ImGui::SetNextItemWidth(slotComboW);
    if (ImGui::BeginCombo("##Slot", ARMOR_SLOTS[cfg.armorSlotIndex].name)) {
        for (int i = 0; i < ARMOR_SLOT_COUNT; ++i) {
            if (ImGui::Selectable(ARMOR_SLOTS[i].name, i == cfg.armorSlotIndex)) cfg.armorSlotIndex = i;
            if (i == cfg.armorSlotIndex) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    GuiUtils::RenderFreeTierCombo("##GenTier", cfg.armorTier);

    ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
    ImGui::DragFloat("Module Chance", &cfg.moduleChance, 0.01f, 0.0f, 0.0f, "%.2f");
    TooltipHelper::ShowTooltip("Probability of generating modular armor vs built armor");

    ImGui::Spacing();
    if (ImGui::Button("Generate")) {
        if (ComponentValidator::Validate(player) && ComponentValidator::Validate(world)) GenerateArmorPassport();
    }
    ImGui::SameLine();
    if (ImGui::Button("Randomize")) {
        if (ComponentValidator::Validate(player) && ComponentValidator::Validate(world)) RandomizeArmorPassport();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) CreateBlankArmorPassport();

    if (armorGenerationPending) {
        ImGui::SameLine();
        ImGui::TextDisabled("Generating...");
    }

    ImGui::PopID();
}

void ArmorEditorSection::RenderModulesTab() {
    ImGui::PushID("modules");

    if (!IsModularCore()) {
        ImGui::TextDisabled("This armor does not support modules");
        ImGui::PopID();
        return;
    }

    PopulateModulePoolForCurrentCore();

    if (!armorModules.populated) {
        ImGui::TextDisabled("Module pool not loaded");
        ImGui::PopID();
        return;
    }

    ImGui::Checkbox("Core Removed", &armorPassport.CoreRemoved_12_5CFF8F6D4A05C15812594CAF6771C66B);
    TooltipHelper::ShowTooltip("Remove the core piece, keeping only attached modules");

    ImGui::Spacing();
    GuiUtils::RenderModuleIndexCombo(
        "Module 1", armorPassport.Module1_5_46B7198E4341C93CBF6AE989EF9898E4, armorModules.modules1, moduleFilters[0],
        armorModules.cachedWidths[0]
    );
    GuiUtils::RenderModuleIndexCombo(
        "Module 2", armorPassport.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0, armorModules.modules2, moduleFilters[1],
        armorModules.cachedWidths[1]
    );
    GuiUtils::RenderModuleIndexCombo(
        "Module 3", armorPassport.Module3_9_E282C465414F6D4EF2A8039FBA847AD2, armorModules.modules3, moduleFilters[2],
        armorModules.cachedWidths[2]
    );

    ImGui::PopID();
}

void ArmorEditorSection::RenderColorsTab() {
    ImGui::PushID("colors");

    ImGui::SeparatorText("Passport Fabric Colors");
    GuiUtils::RenderColorEditor("Fabric Color 1", armorPassport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393);
    GuiUtils::RenderColorEditor("Fabric Color 2", armorPassport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C);

    ImGui::PopID();
}

void ArmorEditorSection::RenderStatsTab() {
    ImGui::PushID("stats");

    ImGui::SeparatorText("Passport");
    GuiUtils::RenderFreeTierCombo("Tier", armorPassport.Tier_50_E497AE434B01B84C559DEE8A863BB42E);
    TooltipHelper::ShowTooltip("Stored tier value in the passport");
    GuiUtils::RenderPriceDrag("Price", armorPassport.Price_27_8E3ADD54484EFC4A59FE9381485AC192);
    TooltipHelper::ShowTooltip("Armor price value stored in the passport");

    ImGui::SeparatorText("Runtime Overrides");
    TooltipHelper::ShowTooltip("Override armor stats after spawning. Enable each to apply its value.");

    if (ImGui::Button("Reset All Overrides")) runtimeProps = {};
    TooltipHelper::ShowTooltip("Disable all runtime overrides");
    GuiUtils::RenderOverrideCount(CountActiveOverrides());

    ImGui::Spacing();
    if (ImGui::TreeNodeEx("Protection", ImGuiTreeNodeFlags_DefaultOpen)) {
        GuiUtils::RenderOverrideDrag("Blunt Protection", runtimeProps.protectionBlunt, 0.1f);
        TooltipHelper::ShowTooltip("Protection against blunt/crushing damage");
        GuiUtils::RenderOverrideDrag("Cut Protection", runtimeProps.protectionCut, 0.1f);
        TooltipHelper::ShowTooltip("Protection against cutting/slashing damage");
        GuiUtils::RenderOverrideDrag("Stab Protection", runtimeProps.protectionStab, 0.1f);
        TooltipHelper::ShowTooltip("Protection against piercing/stabbing damage");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Physics")) {
        GuiUtils::RenderOverrideDrag("Material Density", runtimeProps.materialDensity, 0.1f);
        TooltipHelper::ShowTooltip("Material density - affects weight and impact absorption");
        GuiUtils::RenderOverrideDrag("Mass Scale", runtimeProps.massScale, 0.01f);
        TooltipHelper::ShowTooltip("Overall mass multiplier for the armor piece");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Behavior")) {
        GuiUtils::RenderOverrideDrag("Hands Rigidity", runtimeProps.handsRigidity, 0.1f);
        TooltipHelper::ShowTooltip("Gauntlet hand rigidity - affects grip strength");
        GuiUtils::RenderOverrideDrag("Strap Power", runtimeProps.strapPower, 0.1f);
        TooltipHelper::ShowTooltip("Helmet strap force - affects how securely the helmet stays on");
        GuiUtils::RenderOverrideDrag("AI Invincibility Rate", runtimeProps.aiInvincibilityRate, 0.01f);
        TooltipHelper::ShowTooltip("Rate at which AI ignores damage when wearing this armor");
        GuiUtils::RenderOverrideDrag("Price Override", runtimeProps.price, 1.0f);
        TooltipHelper::ShowTooltip("Override the runtime price value on the actor");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Toggles")) {
        GuiUtils::RenderOverrideBool("Pick Up", runtimeProps.pickUp);
        TooltipHelper::ShowTooltip("Allow picking up this armor piece from the ground");
        ImGui::TreePop();
    }

    ImGui::PopID();
}

ArmorPresetData ArmorEditorSection::BuildPresetData() const {
    ArmorPresetData d;
    d.passport = armorPassport;
    d.runtimeProps = runtimeProps;
    return d;
}

void ArmorEditorSection::ApplyPresetData(ArmorPresetData d) {
    armorPassport = d.passport;
    runtimeProps = d.runtimeProps;
    armorModules = {};

    if (!d.armorCorePath.empty()) {
        GameHook::QueueAction([this, path = std::move(d.armorCorePath)]() {
            armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = Spawner::LoadClass(path);
        });
    }
}

void ArmorEditorSection::RenderSpawnFooter() {
    bool canSpawn = armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 != nullptr;
    if (!canSpawn) ImGui::BeginDisabled();
    if (ImGui::Button("Spawn Armor", ImVec2(-1, 0))) {
        if (ComponentValidator::Validate(player) && ComponentValidator::Validate(world)) SpawnFromPassport();
    }
    if (!canSpawn) ImGui::EndDisabled();
}

ArmorEditorSection::ArmorEditorSection(ModContext& ctx) : CollapsibleSection(ctx, "Armor Editor") {
    CreateBlankArmorPassport();

    Function("Spawn Armor")
        .WithKey(&cfg.spawnKey)
        .WithParams(
            {Parameter("snap_to_ground", "Snap to Ground", &cfg.spawn.snapToGround, "Snap spawned armor to the ground"),
             Parameter(
                 "distance_forward", "Forward Distance", &cfg.spawn.distanceForward, 50.0f, 300.0f,
                 "Spawn distance in front of player"
             ),
             Parameter("distance_up", "Up Distance", &cfg.spawn.distanceUp, 0.0f, 200.0f, "Spawn height offset"),
             Parameter("scale", "Scale", &cfg.spawn.scale, 0.1f, 5.0f, "Size multiplier"),
             Parameter(
                 "live_preview", "Live Preview", &cfg.preview.livePreview, "Auto-spawn preview armor as you edit"
             )}
        )
        .WithTooltip("Spawns the currently edited armor with runtime overrides applied")
        .Action([this]() { SpawnFromPassport(); }, player, world);
}

void ArmorEditorSection::Render() {
    SectionStyle::StyleRAII style;

    preview.InvalidateIfDead(player, world);
    preview.SyncToggleState();

    for (auto& function : functions) {
        function->Render();
        ImGui::Spacing();
    }

    RenderGenerationControls();

    (void)GuiUtils::CheckboxWithTooltip(
        "Live Preview", &cfg.preview.livePreview, "Auto-spawn a preview armor that updates as you edit"
    );
    if (cfg.preview.livePreview) {
        ImGui::SameLine();
        (void
        )GuiUtils::CheckboxWithTooltip("Auto-Rotate", &cfg.preview.autoRotate, "Continuously rotate the preview armor");
        if (cfg.preview.autoRotate) {
            ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
            ImGui::DragFloat("Rotation Speed", &cfg.preview.rotationSpeed, 1.0f, -360.0f, 360.0f, "%.0f deg/s");
            TooltipHelper::ShowTooltip("Rotation speed in degrees/second. Negative values reverse direction");
        }
    }

    presets.status.Render();

    float footerH = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    float scrollH = ImGui::GetContentRegionAvail().y - footerH;
    if (scrollH < 100.0f) scrollH = 100.0f;

    ImGui::BeginChild("##armor_scroll", ImVec2(0, scrollH));

    static constexpr const char* AE_TAB_LABELS[] = {"Modules", "Colors", "Stats", "Presets"};
    GuiUtils::RenderUnderlineTabs("##ArmorEditorTabs", activeTab, AE_TAB_LABELS, 4);
    switch (activeTab) {
        case 0: RenderModulesTab(); break;
        case 1: RenderColorsTab(); break;
        case 2: RenderStatsTab(); break;
        case 3:
            presets.RenderPresetsTab(
                [this]() { return BuildPresetData(); }, [this](ArmorPresetData d) { ApplyPresetData(std::move(d)); }
            );
            break;
    }

    ImGui::EndChild();

    RenderSpawnFooter();

    if (cfg.preview.livePreview) {
        bool needsUpdate = PassportChanged(armorPassport, lastPreviewedPassport) ||
                           std::memcmp(&runtimeProps, &lastPreviewedProps, sizeof(ArmorRuntimeProps)) != 0;
        preview.Update(needsUpdate, [this]() { SpawnPreview(); });
        preview.Rotate();
    }
}
