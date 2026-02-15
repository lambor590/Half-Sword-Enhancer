#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <random>
#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Hooks/GameHook.h"
#include "Utils/Spawner.h"
#include "Utils/EquipmentGenerator.h"
#include "SDK/BP_Armor_Master_classes.hpp"
#include "SDK/BP_Armor_Modular_Core_Master_classes.hpp"
#include "Utils/ArmorPresetSerializer.h"
#include "Utils/GuiUtils.h"

class ArmorEditorSection : public CollapsibleSection {
private:
    SectionConfig::ArmorEditorConfig& cfg = SectionConfig::armorEditor;

    static constexpr struct { const char* name; int slotEnum; } ARMOR_SLOTS[] = {
        {"Head", 0}, {"Hands", 1}, {"Neck (Bevor)", 4}, {"Neck (Standard)", 5},
        {"Arms", 6}, {"Shoulders", 7}, {"Tabard", 8}, {"Chest (Plate)", 9},
        {"Hauberk", 10}, {"Cuisses", 11}, {"Body (Clothing)", 12},
        {"Waist", 13}, {"Legs (Greaves)", 14}, {"Feet", 15}, {"Hosen", 16}
    };
    static constexpr int ARMOR_SLOT_COUNT = sizeof(ARMOR_SLOTS) / sizeof(ARMOR_SLOTS[0]);

    SDK::FStr_Passport_Armor1 armorPassport{};
    bool armorGenerated = false;
    bool armorGenerationPending = false;

    SDK::UClass* cachedCoreClass = nullptr;
    std::string cachedCoreName;

    using ArmorRuntimeProps = decltype(ArmorPresetData::runtimeProps);
    using ArmorRuntimeColors = decltype(ArmorPresetData::runtimeColors);

    ArmorRuntimeProps runtimeProps{};
    ArmorRuntimeColors runtimeColors{};

    SDK::AActor* previewActor = nullptr;
    double lastChangeTime = 0.0;
    SDK::FStr_Passport_Armor1 lastPreviewedPassport{};
    ArmorRuntimeProps lastPreviewedProps{};
    ArmorRuntimeColors lastPreviewedColors{};

    struct ModuleEntry {
        SDK::UClass* cls;
        std::string name;
    };

    struct ArmorModulePool {
        std::vector<ModuleEntry> modules1, modules2, modules3;
        float cachedWidths[3] = {};
        bool populated = false;
        SDK::UClass* populatedForCore = nullptr;
    } armorModules;

    char moduleFilters[3][64] = {};

    char presetNameBuf[128] = {};
    std::vector<PresetListEntry> presetList;
    bool presetListDirty = true;
    std::string statusMessage;
    double statusMessageTime = 0.0;

    static int RandomInt(int min, int max) {
        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }

    bool IsModularCore() const {
        SDK::UClass* coreClass = armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
        if (!coreClass || !coreClass->ClassDefaultObject) return false;
        return coreClass->ClassDefaultObject->IsA(SDK::ABP_Armor_Modular_Core_Master_C::StaticClass());
    }

    void PopulateModulePoolForCurrentCore() {
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

    void CreateBlankArmorPassport() {
        armorPassport = {};
        armorPassport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393 = {0.5f, 0.5f, 0.5f, 1.0f};
        armorPassport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C = {0.5f, 0.5f, 0.5f, 1.0f};
        armorPassport.Tier_50_E497AE434B01B84C559DEE8A863BB42E = static_cast<SDK::Enum_Ranks>(4);
        armorPassport.Price_27_8E3ADD54484EFC4A59FE9381485AC192 = 50.0;
        armorGenerated = true;
    }

    void QueueGeneration(SDK::EArmorSlots_Enum slot, SDK::Enum_Ranks tier, double moduleChance) {
        armorGenerationPending = true;
        GameHook::QueueAction([this, slot, tier, moduleChance]() {
            EquipmentGenerator::Init(world);
            armorPassport = EquipmentGenerator::GenerateArmor(tier, slot, moduleChance);
            PopulateModulePoolForCurrentCore();
            armorGenerated = true;
            armorGenerationPending = false;
        });
    }

    void GenerateArmorPassport() {
        QueueGeneration(
            static_cast<SDK::EArmorSlots_Enum>(ARMOR_SLOTS[cfg.armorSlotIndex].slotEnum),
            static_cast<SDK::Enum_Ranks>(cfg.armorTier),
            cfg.moduleChance);
    }

    void RandomizeArmorPassport() {
        cfg.armorSlotIndex = RandomInt(0, ARMOR_SLOT_COUNT - 1);
        cfg.armorTier = RandomInt(0, 8);
        GenerateArmorPassport();
    }

    static void ApplyRuntimeProps(SDK::AActor* actor, const ArmorRuntimeProps& props,
                                  const ArmorRuntimeColors& colors) {
        if (!actor) return;
        auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(actor);

        if (props.protectionBlunt.enabled)     armor->Protection_Blunt = props.protectionBlunt.value;
        if (props.protectionCut.enabled)       armor->Protection_Cut = props.protectionCut.value;
        if (props.protectionStab.enabled)      armor->Protection_Stab = props.protectionStab.value;
        if (props.materialDensity.enabled)     armor->Material_Density = props.materialDensity.value;
        if (props.massScale.enabled)           armor->Mass_Scale = props.massScale.value;
        if (props.handsRigidity.enabled)       armor->Hands_Rigidity__Gauntlets_ = props.handsRigidity.value;
        if (props.strapPower.enabled)          armor->Strap_Power__Helmet_ = props.strapPower.value;
        if (props.aiInvincibilityRate.enabled) armor->AI_Invinvcibility_Rate = props.aiInvincibilityRate.value;
        if (props.price.enabled)               armor->Price = props.price.value;
        if (props.dynamicColor.enabled)        armor->Dynamic_Color = props.dynamicColor.value;
        if (props.fixedColor.enabled)          armor->Fixed_Color = props.fixedColor.value;
        if (props.metal.enabled)               armor->Metal_ = props.metal.value;
        if (props.simulatesPhysics.enabled)    armor->Simulates_Physics = props.simulatesPhysics.value;
        if (props.pickUp.enabled)              armor->Pick_Up = props.pickUp.value;

        if (colors.enabled) {
            armor->C_1 = colors.c1;
            armor->C_2 = colors.c2;
            armor->C_3 = colors.c3;
        }
    }

    bool HasAnyOverride() const {
        return runtimeProps.protectionBlunt.enabled || runtimeProps.protectionCut.enabled ||
               runtimeProps.protectionStab.enabled || runtimeProps.materialDensity.enabled ||
               runtimeProps.massScale.enabled || runtimeProps.handsRigidity.enabled ||
               runtimeProps.strapPower.enabled || runtimeProps.aiInvincibilityRate.enabled ||
               runtimeProps.price.enabled || runtimeProps.dynamicColor.enabled ||
               runtimeProps.fixedColor.enabled || runtimeProps.metal.enabled ||
               runtimeProps.simulatesPhysics.enabled || runtimeProps.pickUp.enabled ||
               runtimeColors.enabled;
    }

    void DestroyPreview() {
        if (!previewActor) return;
        SDK::AActor* actor = previewActor;
        previewActor = nullptr;
        GameHook::QueueAction([actor]() {
            if (actor) actor->K2_DestroyActor();
        });
    }

    SDK::FTransform BuildSpawnTransform() const {
        auto transform = player->GetTransform();
        const auto forward = player->GetActorForwardVector();
        transform.Translation.X += forward.X * cfg.spawnDistanceForward;
        transform.Translation.Y += forward.Y * cfg.spawnDistanceForward;
        transform.Translation.Z += cfg.spawnDistanceUp;
        transform.Scale3D = {cfg.spawnScale, cfg.spawnScale, cfg.spawnScale};
        return transform;
    }

    void SpawnPreview() {
        DestroyPreview();
        if (!armorGenerated) return;
        if (!armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) return;
        if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;

        lastPreviewedPassport = armorPassport;
        lastPreviewedProps = runtimeProps;
        lastPreviewedColors = runtimeColors;

        auto props = runtimeProps;
        auto colors = runtimeColors;
        bool hasOverrides = HasAnyOverride();

        Spawner::SpawnArmorFromPassport(world, armorPassport, BuildSpawnTransform(), cfg.snapToGround,
            [this, props, colors, hasOverrides](SDK::AActor* actor) {
                if (!cfg.livePreview) {
                    actor->K2_DestroyActor();
                    return;
                }
                auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(actor);
                armor->Simulates_Physics = false;
                actor->SetActorEnableCollision(false);
                if (hasOverrides) ApplyRuntimeProps(actor, props, colors);
                previewActor = actor;
            });
    }

    static bool PassportChanged(const SDK::FStr_Passport_Armor1& a, const SDK::FStr_Passport_Armor1& b) {
        static constexpr size_t BEFORE_TMAP = offsetof(SDK::FStr_Passport_Armor1, SlotsBlocked_45_0807340240E57ACE5A59D39F5E998F51);
        static constexpr size_t AFTER_TMAP = offsetof(SDK::FStr_Passport_Armor1, RequiresModuleHirarchy_47_9ED58E2C48514BE5153606977BE68B6A);
        static constexpr size_t TAIL_SIZE = sizeof(SDK::FStr_Passport_Armor1) - AFTER_TMAP;

        if (std::memcmp(&a, &b, BEFORE_TMAP) != 0) return true;
        return std::memcmp(
            reinterpret_cast<const char*>(&a) + AFTER_TMAP,
            reinterpret_cast<const char*>(&b) + AFTER_TMAP,
            TAIL_SIZE) != 0;
    }

    void UpdatePreview() {
        static constexpr double REFRESH_COOLDOWN = 0.2;

        bool needsUpdate = PassportChanged(armorPassport, lastPreviewedPassport)
                        || std::memcmp(&runtimeProps, &lastPreviewedProps, sizeof(ArmorRuntimeProps)) != 0
                        || std::memcmp(&runtimeColors, &lastPreviewedColors, sizeof(ArmorRuntimeColors)) != 0;

        if (!needsUpdate) return;
        if (previewActor && (ImGui::GetTime() - lastChangeTime < REFRESH_COOLDOWN)) return;

        lastChangeTime = ImGui::GetTime();
        SpawnPreview();
    }

    void SpawnFromPassport() {
        if (!armorGenerated) return;
        if (!armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) return;

        if (cfg.livePreview) {
            cfg.livePreview = false;
            DestroyPreview();
        }

        std::function<void(SDK::AActor*)> callback = nullptr;
        if (HasAnyOverride()) {
            auto props = runtimeProps;
            auto colors = runtimeColors;
            callback = [props, colors](SDK::AActor* actor) { ApplyRuntimeProps(actor, props, colors); };
        }

        Spawner::SpawnArmorFromPassport(world, armorPassport, BuildSpawnTransform(), cfg.snapToGround, callback);
    }

    void RenderModuleIndexCombo(const char* label, int32_t& moduleIndex,
        const std::vector<ModuleEntry>& available, char* filterBuf, float& cachedWidth)
    {
        if (available.empty()) {
            ImGui::TextDisabled("No %s modules available", label);
            return;
        }

        const char* preview = "None";
        if (moduleIndex > 0 && moduleIndex <= static_cast<int32_t>(available.size()))
            preview = available[moduleIndex - 1].name.c_str();

        if (cachedWidth == 0.0f) {
            float maxW = 0;
            for (const auto& e : available) {
                float w = ImGui::CalcTextSize(e.name.c_str()).x;
                if (w > maxW) maxW = w;
            }
            cachedWidth = GuiUtils::ComboWidthFromText(maxW);
        }

        ImGui::SetNextItemWidth(cachedWidth);
        if (!ImGui::BeginCombo(label, preview)) return;

        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##filter", "Search modules...", filterBuf, 64);

        const size_t filterLen = std::strlen(filterBuf);
        const bool hasFilter = filterLen > 0;

        if (hasFilter) {
            int visible = 0;
            for (const auto& e : available)
                if (GuiUtils::MatchesFilter(e.name.c_str(), e.name.size(), filterBuf, filterLen)) ++visible;
            ImGui::TextDisabled("Showing %d of %d", visible, static_cast<int>(available.size()));
        }

        ImGui::Separator();

        if (ImGui::Selectable("None", moduleIndex <= 0))
            moduleIndex = 0;

        for (int i = 0; i < static_cast<int>(available.size()); ++i) {
            if (hasFilter && !GuiUtils::MatchesFilter(available[i].name.c_str(), available[i].name.size(), filterBuf, filterLen))
                continue;
            bool selected = (moduleIndex == i + 1);
            if (ImGui::Selectable(available[i].name.c_str(), selected))
                moduleIndex = i + 1;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    static void RenderColorEditor(const char* label, SDK::FLinearColor& color) {
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.75f);
        float col[4] = {color.R, color.G, color.B, color.A};
        if (ImGui::ColorEdit4(label, col)) {
            color.R = col[0]; color.G = col[1]; color.B = col[2]; color.A = col[3];
        }
        ImGui::PopItemWidth();
    }

    static void RenderFreeTierCombo(const char* label, SDK::Enum_Ranks& tier) {
        int val = static_cast<int>(tier);
        GuiUtils::RenderFreeTierCombo(label, val);
        tier = static_cast<SDK::Enum_Ranks>(val);
    }

    void RenderGenerationControls() {
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
                if (ImGui::Selectable(ARMOR_SLOTS[i].name, i == cfg.armorSlotIndex))
                    cfg.armorSlotIndex = i;
                if (i == cfg.armorSlotIndex) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        GuiUtils::RenderFreeTierCombo("##GenTier", cfg.armorTier);

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        ImGui::SliderFloat("Module Chance", &cfg.moduleChance, 0.0f, 1.0f, "%.2f");
        TooltipHelper::ShowTooltip("Probability of generating modular armor vs built armor");

        ImGui::Spacing();
        if (ImGui::Button("New"))
            CreateBlankArmorPassport();
        ImGui::SameLine();
        if (ImGui::Button("Generate")) {
            if (ComponentValidator::Validate(player) && ComponentValidator::Validate(world))
                GenerateArmorPassport();
        }
        ImGui::SameLine();
        if (ImGui::Button("Randomize")) {
            if (ComponentValidator::Validate(player) && ComponentValidator::Validate(world))
                RandomizeArmorPassport();
        }
        if (armorGenerated) {
            ImGui::SameLine();
            if (ImGui::Button("Reset"))
                CreateBlankArmorPassport();
        }

        if (armorGenerationPending) {
            ImGui::SameLine();
            ImGui::TextDisabled("Generating...");
        }

        if (armorGenerated && armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43) {
            SDK::UClass* core = armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
            if (core != cachedCoreClass) {
                cachedCoreClass = core;
                cachedCoreName = core->GetName();
            }
            ImGui::TextDisabled("Core: %s", cachedCoreName.c_str());
        }

        ImGui::PopID();
    }

    void RenderModulesTab() {
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

        ImGui::Checkbox("Core Removed",
            &armorPassport.CoreRemoved_12_5CFF8F6D4A05C15812594CAF6771C66B);
        TooltipHelper::ShowTooltip("Remove the core piece, keeping only attached modules");

        ImGui::Spacing();
        RenderModuleIndexCombo("Module 1",
            armorPassport.Module1_5_46B7198E4341C93CBF6AE989EF9898E4,
            armorModules.modules1, moduleFilters[0], armorModules.cachedWidths[0]);
        RenderModuleIndexCombo("Module 2",
            armorPassport.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0,
            armorModules.modules2, moduleFilters[1], armorModules.cachedWidths[1]);
        RenderModuleIndexCombo("Module 3",
            armorPassport.Module3_9_E282C465414F6D4EF2A8039FBA847AD2,
            armorModules.modules3, moduleFilters[2], armorModules.cachedWidths[2]);

        ImGui::PopID();
    }

    void RenderColorsTab() {
        ImGui::PushID("colors");

        ImGui::TextDisabled("Passport Fabric Colors");
        RenderColorEditor("Fabric Color 1",
            armorPassport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393);
        RenderColorEditor("Fabric Color 2",
            armorPassport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("Runtime Colors");
        TooltipHelper::ShowTooltip("Applied to the armor actor after spawning (C_1, C_2, C_3)");
        ImGui::Checkbox("Enable Runtime Colors", &runtimeColors.enabled);
        if (!runtimeColors.enabled) ImGui::BeginDisabled();
        RenderColorEditor("Color 1 (C_1)", runtimeColors.c1);
        RenderColorEditor("Color 2 (C_2)", runtimeColors.c2);
        RenderColorEditor("Color 3 (C_3)", runtimeColors.c3);
        if (!runtimeColors.enabled) ImGui::EndDisabled();

        ImGui::PopID();
    }

    void RenderStatsTab() {
        ImGui::PushID("stats");

        ImGui::TextDisabled("Passport");
        RenderFreeTierCombo("Tier", armorPassport.Tier_50_E497AE434B01B84C559DEE8A863BB42E);
        TooltipHelper::ShowTooltip("Stored tier value in the passport");
        GuiUtils::RenderPriceDrag("Price", armorPassport.Price_27_8E3ADD54484EFC4A59FE9381485AC192);
        TooltipHelper::ShowTooltip("Armor price value stored in the passport");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("Runtime Overrides");
        TooltipHelper::ShowTooltip("Override armor stats after spawning. Enable each to apply its value.");

        if (ImGui::Button("Reset All Overrides"))
            runtimeProps = {};
        TooltipHelper::ShowTooltip("Disable all runtime overrides");

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
            GuiUtils::RenderOverrideBool("Dynamic Color", runtimeProps.dynamicColor);
            TooltipHelper::ShowTooltip("Enable dynamic material coloring on this armor");
            GuiUtils::RenderOverrideBool("Fixed Color", runtimeProps.fixedColor);
            TooltipHelper::ShowTooltip("Lock colors to prevent dynamic changes");
            GuiUtils::RenderOverrideBool("Metal", runtimeProps.metal);
            TooltipHelper::ShowTooltip("Mark armor as metallic (affects sound and visuals)");
            GuiUtils::RenderOverrideBool("Simulates Physics", runtimeProps.simulatesPhysics);
            TooltipHelper::ShowTooltip("Enable physics simulation on the armor mesh");
            GuiUtils::RenderOverrideBool("Pick Up", runtimeProps.pickUp);
            TooltipHelper::ShowTooltip("Allow picking up this armor piece from the ground");
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ArmorPresetData BuildPresetData() const {
        ArmorPresetData d;
        d.name = presetNameBuf;
        d.passport = armorPassport;
        d.runtimeProps = runtimeProps;
        d.runtimeColors = runtimeColors;
        return d;
    }

    void ApplyPresetData(const ArmorPresetData& d) {
        armorPassport = d.passport;
        runtimeProps = d.runtimeProps;
        runtimeColors = d.runtimeColors;
        armorGenerated = true;
        armorModules = {};
    }

    void SetStatus(std::string msg) {
        statusMessage = std::move(msg);
        statusMessageTime = ImGui::GetTime();
    }

    void RefreshPresetList() {
        presetList = ArmorPresetSerializer::ListPresets();
        presetListDirty = false;
    }

    void RenderPresetsTab() {
        ImGui::PushID("presets");

        if (!statusMessage.empty()) {
            if (ImGui::GetTime() - statusMessageTime > 3.0)
                statusMessage.clear();
            else
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", statusMessage.c_str());
        }

        ImGui::TextDisabled("Save");
        float btnWidth = ImGui::CalcTextSize("Save").x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - btnWidth - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputTextWithHint("##PresetName", "Preset name...", presetNameBuf, sizeof(presetNameBuf));
        ImGui::SameLine();
        bool canSave = presetNameBuf[0] != '\0';
        if (!canSave) ImGui::BeginDisabled();
        if (ImGui::Button("Save")) {
            auto data = BuildPresetData();
            if (ArmorPresetSerializer::SavePresetByName(presetNameBuf, armorPassport, data)) {
                SetStatus("Saved: " + std::string(presetNameBuf));
                presetListDirty = true;
            } else {
                SetStatus("Error saving preset");
            }
        }
        if (!canSave) ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("Presets");
        if (presetListDirty)
            RefreshPresetList();

        if (presetList.empty()) {
            ImGui::TextDisabled("No saved presets");
        } else {
            const float framePadX2 = ImGui::GetStyle().FramePadding.x * 2;
            const float loadW = ImGui::CalcTextSize("Load").x + framePadX2;
            const float delW = ImGui::CalcTextSize("Del").x + framePadX2;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float buttonsWidth = loadW + delW + spacing * 2;

            for (size_t i = 0; i < presetList.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                float textW = ImGui::GetContentRegionAvail().x - buttonsWidth;

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(presetList[i].name.c_str());
                if (textW > 0) {
                    ImGui::SameLine(textW);
                }
                if (ImGui::Button("Load")) {
                    auto result = ArmorPresetSerializer::LoadFromFile(presetList[i].path);
                    if (result.success) {
                        ApplyPresetData(result);
                        strncpy_s(presetNameBuf, result.name.c_str(), _TRUNCATE);
                        SetStatus("Loaded: " + result.name);
                    } else {
                        SetStatus("Error: " + result.error);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Del")) {
                    ArmorPresetSerializer::DeletePreset(presetList[i].path);
                    SetStatus("Deleted: " + presetList[i].name);
                    presetListDirty = true;
                }
                ImGui::PopID();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("Share");
        float halfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Copy to Clipboard", ImVec2(halfWidth, 0))) {
            auto data = BuildPresetData();
            std::string encoded = ArmorPresetSerializer::EncodeForClipboard(armorPassport, data);
            ImGui::SetClipboardText(encoded.c_str());
            SetStatus("Copied to clipboard");
        }
        ImGui::SameLine();
        if (ImGui::Button("Paste from Clipboard", ImVec2(halfWidth, 0))) {
            const char* clip = ImGui::GetClipboardText();
            if (clip && clip[0]) {
                auto result = ArmorPresetSerializer::DecodeFromClipboard(clip);
                if (result.success) {
                    ApplyPresetData(result);
                    strncpy_s(presetNameBuf, result.name.c_str(), _TRUNCATE);
                    SetStatus("Pasted: " + result.name);
                } else {
                    SetStatus("Error: " + result.error);
                }
            } else {
                SetStatus("Clipboard is empty");
            }
        }

        ImGui::PopID();
    }

    void RenderSpawnFooter() {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool canSpawn = armorGenerated && armorPassport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
        if (!canSpawn) ImGui::BeginDisabled();
        if (ImGui::Button("Spawn Armor", ImVec2(-1, 0))) {
            if (ComponentValidator::Validate(player) && ComponentValidator::Validate(world))
                SpawnFromPassport();
        }
        if (!canSpawn) ImGui::EndDisabled();
    }

public:
    ArmorEditorSection() : CollapsibleSection("Armor Editor") {
        Function("Spawn Armor")
            .WithKey(&cfg.spawnKey)
            .WithParams({
                Parameter("snap_to_ground", "Snap to Ground", &cfg.snapToGround, "Snap spawned armor to the ground"),
                Parameter("distance_forward", "Forward Distance", &cfg.spawnDistanceForward, 50.0f, 300.0f, "Spawn distance in front of player"),
                Parameter("distance_up", "Up Distance", &cfg.spawnDistanceUp, 0.0f, 200.0f, "Spawn height offset"),
                Parameter("scale", "Scale", &cfg.spawnScale, 0.1f, 5.0f, "Size multiplier"),
                Parameter("live_preview", "Live Preview", &cfg.livePreview, "Auto-spawn preview armor as you edit")
            })
            .WithTooltip("Spawns the currently edited armor with runtime overrides applied")
            .Action([this]() { SpawnFromPassport(); }, player, world);
    }

    void RenderContent() override {
        SectionStyle::StyleRAII style;

        if (previewActor && (!player || !world))
            previewActor = nullptr;

        for (auto& function : functions) {
            function->Render();
            ImGui::Spacing();
        }

        RenderGenerationControls();

        if (armorGenerated) {
            if (ImGui::Checkbox("Live Preview", &cfg.livePreview)) {
                if (!cfg.livePreview)
                    DestroyPreview();
            }

            ImGui::Spacing();
            if (ImGui::BeginTabBar("##ArmorEditorTabs")) {
                if (ImGui::BeginTabItem("Modules"))  { RenderModulesTab();  ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Colors"))    { RenderColorsTab();   ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Stats"))     { RenderStatsTab();    ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Presets"))   { RenderPresetsTab();  ImGui::EndTabItem(); }
                ImGui::EndTabBar();
            }
        }

        RenderSpawnFooter();

        if (cfg.livePreview && armorGenerated)
            UpdatePreview();
    }
};
