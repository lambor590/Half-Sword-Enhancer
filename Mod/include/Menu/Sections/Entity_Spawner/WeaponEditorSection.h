#pragma once

#include <array>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <random>
#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Hooks/GameHook.h"
#include "Utils/Spawner.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/CustomizableWeapon.h"
#include "Utils/TierValidation.h"
#include "SDK/BP_GameWeapon_Customizable_Master_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/ModularWeaponBP_Customizable_classes.hpp"
#include "Utils/WeaponPresetSerializer.h"
#include "Utils/GuiUtils.h"

class WeaponEditorSection : public CollapsibleSection {
private:
    SectionConfig::WeaponEditorConfig& cfg = SectionConfig::weaponEditor;

    static constexpr const char* WEAPON_TYPE_NAMES[] = {
        "Arming Sword", "Short Sword", "Long Sword",
        "Short Mace", "Mace", "Long Mace",
        "Short Hafted", "Hafted", "Long Hafted",
        "Short Polearm", "Polearm", "Long Polearm",
        "Short Pollaxe", "Pollaxe", "Long Pollaxe",
        "Short Casted", "Casted", "Long Casted",
        "Messer"
    };
    static constexpr int WEAPON_TYPE_COUNT = 19;

    static constexpr const char* MATERIAL_LAYER_NAMES[] = {
        "Brushed Steel 1", "Brushed Steel 2", "Brushed Steel 3", "Steel",
        "Iron", "Gilded", "Copper", "Brass",
        "Bronze", "Gold", "Leather", "Turned Leather 1",
        "Turned Leather 2", "Turned Leather 3", "Wood", "Old Wood",
    };

    SDK::FStr_Passport_Weapon1 weaponPassport{};
    bool weaponGenerated = false;
    bool weaponGenerationPending = false;

    using RuntimeOverride = WeaponPresetData::RuntimeOverride;
    using BoolOverride = WeaponPresetData::BoolOverride;
    using IntOverride = WeaponPresetData::IntOverride;
    using WeaponRuntimeProps = decltype(WeaponPresetData::runtimeProps);

    WeaponRuntimeProps runtimeProps{};

    SDK::AActor* previewActor = nullptr;
    double lastChangeTime = 0.0;
    SDK::FStr_Passport_Weapon1 lastPreviewedPassport{};
    WeaponRuntimeProps lastPreviewedProps{};

    struct GlobalModuleEntry {
        SDK::UClass* cls;
        std::string name;
        const char* sourceType;
    };

    struct GlobalModulePool {
        std::vector<GlobalModuleEntry> heads, guards, grips, pommels, subMods1, subMods2;
        float cachedWidths[6] = {};
        bool populated = false;
    } globalModules;

    char moduleFilters[6][64] = {};

    char presetNameBuf[128] = {};
    std::vector<PresetListEntry> presetList;
    bool presetListDirty = true;
    std::string statusMessage;
    double statusMessageTime = 0.0;

    static bool ContainsClass(const std::vector<GlobalModuleEntry>& vec, SDK::UClass* cls) {
        for (const auto& e : vec)
            if (e.cls == cls) return true;
        return false;
    }

    static void CollectEntries(std::vector<GlobalModuleEntry>& out,
        const SDK::TArray<SDK::UClass*>& arr, const char* sourceType)
    {
        for (int i = 0; i < arr.Num(); ++i) {
            if (arr[i] && !ContainsClass(out, arr[i]))
                out.push_back({arr[i], arr[i]->GetName(), sourceType});
        }
    }

    void PopulateGlobalModulePool() {
        if (globalModules.populated) return;

        for (int i = 1; i <= WEAPON_TYPE_COUNT; ++i) {
            auto type = static_cast<CustomizableWeapon>(i);
            SDK::UClass* masterClass = EquipmentGenerator::GetCustomizableModulesClass(type);
            if (!masterClass || !masterClass->ClassDefaultObject) continue;

            auto* cdo = reinterpret_cast<SDK::UBP_GameWeapon_Customizable_Master_C*>(
                masterClass->ClassDefaultObject);
            const char* typeName = WEAPON_TYPE_NAMES[i - 1];

            CollectEntries(globalModules.heads, cdo->Module_Heads_Array, typeName);
            CollectEntries(globalModules.guards, cdo->Module_Guards_Array, typeName);
            CollectEntries(globalModules.grips, cdo->Module_Grips_Array, typeName);
            CollectEntries(globalModules.pommels, cdo->Module_Pommels_Array, typeName);
            CollectEntries(globalModules.subMods1, cdo->Head_Sub_Module_1_Array, typeName);
            CollectEntries(globalModules.subMods2, cdo->Head_Sub_Module_2_Array, typeName);
        }
        globalModules.populated = true;
    }

    static int RandomInt(int min, int max) {
        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }

    static int RandomValidTier(uint16_t mask) {
        int validTiers[9];
        int count = 0;
        for (int t = 0; t <= 8; ++t)
            if (mask & (1 << t)) validTiers[count++] = t;
        if (count == 0) return 4;
        return validTiers[RandomInt(0, count - 1)];
    }

    void CreateBlankWeaponPassport() {
        weaponPassport = {};
        weaponPassport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57 = {1.0, 1.0, 1.0};
        weaponPassport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 = {1.0, 1.0, 1.0};
        weaponPassport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF = {1.0, 1.0, 1.0};
        weaponPassport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E = {1.0, 1.0, 1.0};
        weaponPassport.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7 = 1.0;
        weaponPassport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927 = 1.0;
        weaponPassport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10 = 1.0;
        weaponPassport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173 = 1.0;
        weaponPassport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 = {0.4f, 0.26f, 0.13f, 1.0f};
        weaponPassport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 = {0.3f, 0.18f, 0.08f, 1.0f};
        weaponPassport.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461 = static_cast<SDK::Enum_Ranks>(4);
        weaponPassport.Price_60_83FE5A624EA188485BBE4E9C8606AEE5 = 100.0;
        weaponGenerated = true;
    }

    void QueueGeneration(CustomizableWeapon type, SDK::Enum_Ranks tier) {
        weaponGenerationPending = true;
        GameHook::QueueAction([this, type, tier]() {
            EquipmentGenerator::Init(world);
            weaponPassport = EquipmentGenerator::GenerateCustomizableWeapon(type, tier);
            PopulateGlobalModulePool();
            weaponGenerated = true;
            weaponGenerationPending = false;
        });
    }

    void GenerateWeaponPassport() {
        QueueGeneration(
            static_cast<CustomizableWeapon>(cfg.weaponType),
            static_cast<SDK::Enum_Ranks>(cfg.weaponTier));
    }

    void RandomizeWeaponPassport() {
        cfg.weaponType = RandomInt(1, WEAPON_TYPE_COUNT);
        uint16_t mask = TierValidation::VALID_TIER_MASKS[cfg.weaponType];
        cfg.weaponTier = RandomValidTier(mask);
        GenerateWeaponPassport();
    }

    static void ApplyRuntimeProps(SDK::AActor* actor, const WeaponRuntimeProps& props) {
        if (!actor) return;
        auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);

        if (props.rigidity.enabled)       weapon->Rigidity = props.rigidity.value;
        if (props.edgeSharpness.enabled)   weapon->Edge_Sharpness = props.edgeSharpness.value;
        if (props.rawDamage.enabled)       weapon->Raw_Damage = props.rawDamage.value;
        if (props.cuttingRate.enabled)     weapon->Cutting_Rate = props.cuttingRate.value;
        if (props.stabRate.enabled)        weapon->Stab_Rate = props.stabRate.value;
        if (props.defRating.enabled)       weapon->Def_Rating = props.defRating.value;
        if (props.gripRate.enabled)        weapon->Grip_Rate = props.gripRate.value;
        if (props.drawCutRate.enabled)     weapon->Draw_Cut_Rate = props.drawCutRate.value;
        if (props.tipSharpness.enabled)    weapon->Tip_Sharpness = props.tipSharpness.value;
        if (props.kickPower.enabled)       weapon->Kick_Power = props.kickPower.value;
        if (props.matDensity.enabled)      weapon->Mat_Density = props.matDensity.value;
        if (props.dismemberSharp.enabled)  weapon->Dismemberment_Level_Sharp = props.dismemberSharp.value;
        if (props.dismemberBlunt.enabled)  weapon->Dismemberment_Level_Blunt = props.dismemberBlunt.value;
        if (props.doubleEdged.enabled)     weapon->Double_Edged = props.doubleEdged.value;
        if (props.piercing.enabled)        weapon->Piercing = props.piercing.value;
        if (props.noStab.enabled)          weapon->NoStab = props.noStab.value;
        if (props.staminaBurnR.enabled)    weapon->R_Hand_Stamina_Burn_Rate = props.staminaBurnR.value;
        if (props.staminaBurnL.enabled)    weapon->L_Hand_Stamina_Burn_Rate = props.staminaBurnL.value;
        if (props.staminaBurn2H.enabled)   weapon->TwoH_Default_Stamina_Burn_Rate = props.staminaBurn2H.value;
        if (props.staminaBurn2HAlt.enabled) weapon->TwoH_Alt_Stamina_Burn_Rate = props.staminaBurn2HAlt.value;
    }

    bool HasAnyRuntimeOverride() const {
        return runtimeProps.rigidity.enabled || runtimeProps.edgeSharpness.enabled ||
               runtimeProps.rawDamage.enabled || runtimeProps.cuttingRate.enabled ||
               runtimeProps.stabRate.enabled || runtimeProps.defRating.enabled ||
               runtimeProps.gripRate.enabled || runtimeProps.drawCutRate.enabled ||
               runtimeProps.tipSharpness.enabled || runtimeProps.kickPower.enabled ||
               runtimeProps.matDensity.enabled || runtimeProps.dismemberSharp.enabled ||
               runtimeProps.dismemberBlunt.enabled || runtimeProps.doubleEdged.enabled ||
               runtimeProps.piercing.enabled || runtimeProps.noStab.enabled ||
               runtimeProps.staminaBurnR.enabled || runtimeProps.staminaBurnL.enabled ||
               runtimeProps.staminaBurn2H.enabled || runtimeProps.staminaBurn2HAlt.enabled;
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
        if (!weaponGenerated) return;
        if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;

        lastPreviewedPassport = weaponPassport;
        lastPreviewedProps = runtimeProps;

        auto props = runtimeProps;
        bool hasOverrides = HasAnyRuntimeOverride();

        Spawner::SpawnCustomizableFromPassport(world, weaponPassport, BuildSpawnTransform(), cfg.snapToGround,
            [this, props, hasOverrides](SDK::AActor* actor) {
                if (!cfg.livePreview) {
                    actor->K2_DestroyActor();
                    return;
                }
                auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
                weapon->Simulates_Physics = false;
                weapon->Turn_Off_Collision();
                actor->SetActorEnableCollision(false);
                if (hasOverrides) ApplyRuntimeProps(actor, props);
                previewActor = actor;
            });
    }

    void UpdatePreview() {
        static constexpr double REFRESH_COOLDOWN = 0.2;

        bool needsUpdate = std::memcmp(&weaponPassport, &lastPreviewedPassport, sizeof(SDK::FStr_Passport_Weapon1)) != 0
                        || std::memcmp(&runtimeProps, &lastPreviewedProps, sizeof(WeaponRuntimeProps)) != 0;

        if (!needsUpdate) return;
        if (previewActor && (ImGui::GetTime() - lastChangeTime < REFRESH_COOLDOWN)) return;

        lastChangeTime = ImGui::GetTime();
        SpawnPreview();
    }

    void SpawnFromPassport() {
        if (!weaponGenerated) return;

        if (cfg.livePreview) {
            cfg.livePreview = false;
            DestroyPreview();
        }

        std::function<void(SDK::AActor*)> callback = nullptr;
        if (HasAnyRuntimeOverride()) {
            auto props = runtimeProps;
            callback = [props](SDK::AActor* actor) { ApplyRuntimeProps(actor, props); };
        }

        Spawner::SpawnCustomizableFromPassport(world, weaponPassport, BuildSpawnTransform(), cfg.snapToGround, callback);
    }

    static constexpr auto LOWER_TABLE = [] {
        std::array<char, 256> t{};
        for (int i = 0; i < 256; ++i) t[i] = static_cast<char>(i);
        for (int i = 'A'; i <= 'Z'; ++i) t[i] = static_cast<char>(i + 32);
        return t;
    }();

    static bool MatchesFilter(const char* name, size_t nameLen, const char* filter, size_t filterLen) {
        if (filterLen == 0) return true;
        if (filterLen > nameLen) return false;
        for (size_t i = 0; i <= nameLen - filterLen; ++i) {
            size_t j = 0;
            while (j < filterLen &&
                   LOWER_TABLE[static_cast<unsigned char>(name[i + j])] ==
                   LOWER_TABLE[static_cast<unsigned char>(filter[j])])
                ++j;
            if (j == filterLen) return true;
        }
        return false;
    }

    static void RenderFilteredModuleCombo(const char* label,
        SDK::UClass*& current, const std::vector<GlobalModuleEntry>& options,
        char* filterBuf, float& cachedWidth, bool allowNone = true)
    {
        const char* preview = "None";
        for (const auto& e : options) {
            if (e.cls == current) { preview = e.name.c_str(); break; }
        }

        if (cachedWidth == 0.0f) {
            float maxModW = 0;
            for (const auto& e : options) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%-36s [%s]", e.name.c_str(), e.sourceType);
                float w = ImGui::CalcTextSize(buf).x;
                if (w > maxModW) maxModW = w;
            }
            cachedWidth = GuiUtils::ComboWidthFromText(maxModW);
        }

        ImGui::SetNextItemWidth(cachedWidth);
        if (!ImGui::BeginCombo(label, preview)) return;

        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##filter", "Search modules...", filterBuf, 64);

        const size_t filterLen = std::strlen(filterBuf);
        const bool hasFilter = filterLen > 0;

        if (hasFilter) {
            int visible = 0;
            for (const auto& e : options)
                if (MatchesFilter(e.name.c_str(), e.name.size(), filterBuf, filterLen)) ++visible;
            ImGui::TextDisabled("Showing %d of %d", visible, static_cast<int>(options.size()));
        }

        ImGui::Separator();

        if (allowNone && ImGui::Selectable("None", current == nullptr))
            current = nullptr;

        char display[128];
        for (const auto& e : options) {
            if (hasFilter && !MatchesFilter(e.name.c_str(), e.name.size(), filterBuf, filterLen))
                continue;
            std::snprintf(display, sizeof(display), "%-36s [%s]", e.name.c_str(), e.sourceType);
            if (ImGui::Selectable(display, e.cls == current))
                current = e.cls;
            if (e.cls == current) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    static void RenderMaterialCombo(const char* label, SDK::Enum_MaterialLayer& mat) {
        static float materialComboW = 0;
        if (materialComboW == 0) materialComboW = GuiUtils::CalcComboWidth(MATERIAL_LAYER_NAMES, 16);

        int val = static_cast<int>(mat);
        const char* preview = (val >= 0 && val < 16) ? MATERIAL_LAYER_NAMES[val] : "Unknown";
        ImGui::SetNextItemWidth(materialComboW);
        if (ImGui::BeginCombo(label, preview)) {
            for (int i = 0; i < 16; ++i) {
                if (ImGui::Selectable(MATERIAL_LAYER_NAMES[i], val == i))
                    mat = static_cast<SDK::Enum_MaterialLayer>(i);
                if (val == i) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    static void RenderColorEditor(const char* label, SDK::FLinearColor& color) {
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.75f);
        float col[4] = {color.R, color.G, color.B, color.A};
        if (ImGui::ColorEdit4(label, col)) {
            color.R = col[0]; color.G = col[1]; color.B = col[2]; color.A = col[3];
        }
        ImGui::PopItemWidth();
    }

    static void RenderVectorDrag(const char* label, SDK::FVector& vec, float speed = 0.01f) {
        float v[3] = {static_cast<float>(vec.X), static_cast<float>(vec.Y), static_cast<float>(vec.Z)};
        if (ImGui::DragFloat3(label, v, speed, 0.01f, 10.0f, "%.3f")) {
            vec.X = v[0]; vec.Y = v[1]; vec.Z = v[2];
        }
    }

    static void RenderMassDrag(const char* label, double& mass, float speed = 0.01f) {
        float val = static_cast<float>(mass);
        if (ImGui::DragFloat(label, &val, speed, 0.01f, 100.0f, "%.3f"))
            mass = val;
    }

    static void RenderPriceDrag(const char* label, double& price, float speed = 1.0f) {
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.75f);
        float val = static_cast<float>(price);
        if (ImGui::DragFloat(label, &val, speed, 0.0f, 0.0f, "%.1f"))
            price = val;
        ImGui::PopItemWidth();
    }

    static constexpr const char* TIER_LABELS[] = {
        "Tier 0", "Tier 1", "Tier 2", "Tier 3", "Tier 4",
        "Tier 5", "Tier 6", "Tier 7", "Tier 8"
    };

    static float CachedTierComboWidth() {
        static float w = GuiUtils::CalcComboWidth(TIER_LABELS, 9);
        return w;
    }

    static void RenderFreeTierCombo(const char* label, int& tier) {
        ImGui::SetNextItemWidth(CachedTierComboWidth());
        if (ImGui::BeginCombo(label, TIER_LABELS[tier])) {
            for (int t = 0; t <= 8; ++t) {
                if (ImGui::Selectable(TIER_LABELS[t], t == tier))
                    tier = t;
                if (t == tier) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    static void RenderFreeTierCombo(const char* label, SDK::Enum_Ranks& tier) {
        int val = static_cast<int>(tier);
        RenderFreeTierCombo(label, val);
        tier = static_cast<SDK::Enum_Ranks>(val);
    }

    static void RenderValidatedTierCombo(const char* label, int& tier, uint16_t validMask) {
        tier = TierValidation::NearestValidTier(validMask, tier);

        ImGui::SetNextItemWidth(CachedTierComboWidth());
        if (ImGui::BeginCombo(label, TIER_LABELS[tier])) {
            for (int t = 0; t <= 8; ++t) {
                if (!(validMask & (1 << t))) continue;
                if (ImGui::Selectable(TIER_LABELS[t], t == tier))
                    tier = t;
                if (t == tier) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    static void RenderSizeMassRow(const char* label, SDK::FVector& size, double& mass) {
        float massWidth = 70.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(65.0f);

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - massWidth - spacing);
        char sizeId[32];
        std::snprintf(sizeId, sizeof(sizeId), "##size_%s", label);
        RenderVectorDrag(sizeId, size);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(massWidth);
        char massId[32];
        std::snprintf(massId, sizeof(massId), "##mass_%s", label);
        RenderMassDrag(massId, mass);
    }

    static void RenderOverrideDrag(const char* label, RuntimeOverride& ovr, float speed = 0.1f, float min = 0.0f, float max = 0.0f) {
        ImGui::PushID(label);
        ImGui::Checkbox("##en", &ovr.enabled);
        ImGui::SameLine();
        if (!ovr.enabled) ImGui::BeginDisabled();
        float val = static_cast<float>(ovr.value);
        if (ImGui::DragFloat(label, &val, speed, min, max, "%.3f"))
            ovr.value = val;
        if (!ovr.enabled) ImGui::EndDisabled();
        ImGui::PopID();
    }

    static void RenderOverrideInt(const char* label, IntOverride& ovr, int min = 0, int max = 10) {
        ImGui::PushID(label);
        ImGui::Checkbox("##en", &ovr.enabled);
        ImGui::SameLine();
        if (!ovr.enabled) ImGui::BeginDisabled();
        ImGui::SliderInt(label, &ovr.value, min, max);
        if (!ovr.enabled) ImGui::EndDisabled();
        ImGui::PopID();
    }

    static void RenderOverrideBool(const char* label, BoolOverride& ovr) {
        ImGui::PushID(label);
        ImGui::Checkbox("##en", &ovr.enabled);
        ImGui::SameLine();
        if (!ovr.enabled) ImGui::BeginDisabled();
        ImGui::Checkbox(label, &ovr.value);
        if (!ovr.enabled) ImGui::EndDisabled();
        ImGui::PopID();
    }

    void RenderGenerationControls() {
        ImGui::PushID("gen");

        static float weaponTypeComboW = GuiUtils::CalcComboWidth(WEAPON_TYPE_NAMES, WEAPON_TYPE_COUNT);
        ImGui::SetNextItemWidth(weaponTypeComboW);
        int typeIdx = cfg.weaponType - 1;
        if (ImGui::Combo("##Type", &typeIdx, WEAPON_TYPE_NAMES, WEAPON_TYPE_COUNT))
            cfg.weaponType = typeIdx + 1;

        ImGui::SameLine();
        uint16_t weaponMask = TierValidation::VALID_TIER_MASKS[cfg.weaponType];
        RenderValidatedTierCombo("##GenTier", cfg.weaponTier, weaponMask);

        ImGui::Spacing();
        if (ImGui::Button("New")) {
            GameHook::QueueAction([this]() {
                PopulateGlobalModulePool();
                CreateBlankWeaponPassport();
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Generate")) {
            if (ComponentValidator::Validate(player) && ComponentValidator::Validate(world))
                GenerateWeaponPassport();
        }
        ImGui::SameLine();
        if (ImGui::Button("Randomize")) {
            if (ComponentValidator::Validate(player) && ComponentValidator::Validate(world))
                RandomizeWeaponPassport();
        }
        if (weaponGenerated) {
            ImGui::SameLine();
            if (ImGui::Button("Reset"))
                CreateBlankWeaponPassport();
        }

        if (weaponGenerationPending) {
            ImGui::SameLine();
            ImGui::TextDisabled("Generating...");
        }

        ImGui::PopID();
    }

    void RenderModulesTab() {
        ImGui::PushID("modules");

        if (!globalModules.populated) {
            ImGui::TextDisabled("Module pool not loaded yet");
            ImGui::PopID();
            return;
        }

        RenderFilteredModuleCombo("Head",
            weaponPassport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139,
            globalModules.heads, moduleFilters[0], globalModules.cachedWidths[0]);
        RenderFilteredModuleCombo("Guard",
            weaponPassport.GuardModule_13_6DD2B06245505E53B529D090333012F0,
            globalModules.guards, moduleFilters[1], globalModules.cachedWidths[1]);
        RenderFilteredModuleCombo("Grip",
            weaponPassport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4,
            globalModules.grips, moduleFilters[2], globalModules.cachedWidths[2]);
        RenderFilteredModuleCombo("Pommel",
            weaponPassport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6,
            globalModules.pommels, moduleFilters[3], globalModules.cachedWidths[3]);
        if (!globalModules.subMods1.empty()) {
            RenderFilteredModuleCombo("Sub-Mod 1",
                weaponPassport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D,
                globalModules.subMods1, moduleFilters[4], globalModules.cachedWidths[4]);
        }
        if (!globalModules.subMods2.empty()) {
            RenderFilteredModuleCombo("Sub-Mod 2",
                weaponPassport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9,
                globalModules.subMods2, moduleFilters[5], globalModules.cachedWidths[5]);
        }

        ImGui::PopID();
    }

    void RenderGeometryTab() {
        ImGui::PushID("geometry");

        float headerAvail = ImGui::GetContentRegionAvail().x;
        ImGui::AlignTextToFramePadding();
        ImGui::Dummy(ImVec2(0, 0));
        ImGui::SameLine(65.0f);
        ImGui::TextDisabled("Size (XYZ)");
        ImGui::SameLine(headerAvail - 70.0f + ImGui::GetStyle().ItemSpacing.x);
        ImGui::TextDisabled("Mass");
        RenderSizeMassRow("Head",
            weaponPassport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57,
            weaponPassport.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7);
        RenderSizeMassRow("Guard",
            weaponPassport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704,
            weaponPassport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927);
        RenderSizeMassRow("Grip",
            weaponPassport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF,
            weaponPassport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10);
        RenderSizeMassRow("Pommel",
            weaponPassport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E,
            weaponPassport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173);

        ImGui::Spacing();
        if (ImGui::Button("Reset Sizes")) {
            weaponPassport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57 = {1.0, 1.0, 1.0};
            weaponPassport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 = {1.0, 1.0, 1.0};
            weaponPassport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF = {1.0, 1.0, 1.0};
            weaponPassport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E = {1.0, 1.0, 1.0};
            weaponPassport.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7 = 1.0;
            weaponPassport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927 = 1.0;
            weaponPassport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10 = 1.0;
            weaponPassport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173 = 1.0;
        }

        ImGui::PopID();
    }

    void RenderAppearanceTab() {
        ImGui::PushID("appearance");

        ImGui::TextDisabled("Metal");
        RenderMaterialCombo("Steel", weaponPassport.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36);
        RenderMaterialCombo("Colored", weaponPassport.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2);

        ImGui::Spacing();
        ImGui::TextDisabled("Organic");
        RenderMaterialCombo("Wood", weaponPassport.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A);
        RenderColorEditor("Wood Color", weaponPassport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743);

        ImGui::Spacing();
        RenderMaterialCombo("Leather", weaponPassport.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB);
        RenderColorEditor("Leather Color", weaponPassport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638);

        ImGui::PopID();
    }

    void RenderStatsTab() {
        ImGui::PushID("stats");

        ImGui::TextDisabled("Passport");
        RenderFreeTierCombo("Tier", weaponPassport.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461);
        TooltipHelper::ShowTooltip("Stored tier value in the passport, independent of generation tier");
        RenderPriceDrag("Price", weaponPassport.Price_60_83FE5A624EA188485BBE4E9C8606AEE5);
        TooltipHelper::ShowTooltip("Weapon price value stored in the passport");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("Runtime Overrides");
        TooltipHelper::ShowTooltip("Override weapon stats after spawning. Enable each to apply its value.");

        if (ImGui::Button("Reset All Overrides"))
            runtimeProps = {};
        TooltipHelper::ShowTooltip("Disable all runtime overrides");

        ImGui::Spacing();
        if (ImGui::TreeNodeEx("Combat", ImGuiTreeNodeFlags_DefaultOpen)) {
            RenderOverrideDrag("Rigidity", runtimeProps.rigidity, 0.1f);
            TooltipHelper::ShowTooltip("Structural stiffness - affects impact resistance and damage transfer");
            RenderOverrideDrag("Edge Sharpness", runtimeProps.edgeSharpness, 0.1f);
            TooltipHelper::ShowTooltip("Cutting edge quality - determines slashing effectiveness");
            RenderOverrideDrag("Raw Damage", runtimeProps.rawDamage, 0.1f);
            TooltipHelper::ShowTooltip("Base damage multiplier before other modifiers");
            RenderOverrideDrag("Cutting Rate", runtimeProps.cuttingRate, 0.01f);
            TooltipHelper::ShowTooltip("Slashing damage multiplier for cutting attacks");
            RenderOverrideDrag("Stab Rate", runtimeProps.stabRate, 0.01f);
            TooltipHelper::ShowTooltip("Thrusting damage multiplier for stab attacks");
            RenderOverrideDrag("Def Rating", runtimeProps.defRating, 0.01f);
            TooltipHelper::ShowTooltip("Defensive effectiveness when blocking or parrying");
            RenderOverrideDrag("Grip Rate", runtimeProps.gripRate, 0.01f);
            TooltipHelper::ShowTooltip("Weapon handling and control precision");
            RenderOverrideDrag("Draw Cut Rate", runtimeProps.drawCutRate, 0.01f);
            TooltipHelper::ShowTooltip("Damage bonus for drawing/slicing motions");
            RenderOverrideDrag("Tip Sharpness", runtimeProps.tipSharpness, 0.1f);
            TooltipHelper::ShowTooltip("Point sharpness - affects piercing on thrust attacks");
            RenderOverrideDrag("Kick Power", runtimeProps.kickPower, 0.1f);
            TooltipHelper::ShowTooltip("Knockback force applied on impact");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Physics")) {
            RenderOverrideDrag("Mat Density", runtimeProps.matDensity, 0.1f);
            TooltipHelper::ShowTooltip("Material density - affects momentum and swing weight");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Dismemberment")) {
            RenderOverrideInt("Sharp Level", runtimeProps.dismemberSharp, 0, 10);
            TooltipHelper::ShowTooltip("Sharp dismemberment threshold (higher = easier to sever)");
            RenderOverrideInt("Blunt Level", runtimeProps.dismemberBlunt, 0, 10);
            TooltipHelper::ShowTooltip("Blunt dismemberment threshold (higher = easier to crush)");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Toggles")) {
            RenderOverrideBool("Double Edged", runtimeProps.doubleEdged);
            TooltipHelper::ShowTooltip("Both edges can cut (swords vs single-edge weapons)");
            RenderOverrideBool("Piercing", runtimeProps.piercing);
            TooltipHelper::ShowTooltip("Weapon can pierce through armor");
            RenderOverrideBool("No Stab", runtimeProps.noStab);
            TooltipHelper::ShowTooltip("Disables thrust attacks (for blunt weapons)");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Stamina")) {
            RenderOverrideDrag("R Hand Burn", runtimeProps.staminaBurnR, 0.01f);
            TooltipHelper::ShowTooltip("Stamina drain rate when wielding in right hand");
            RenderOverrideDrag("L Hand Burn", runtimeProps.staminaBurnL, 0.01f);
            TooltipHelper::ShowTooltip("Stamina drain rate when wielding in left hand");
            RenderOverrideDrag("2H Burn", runtimeProps.staminaBurn2H, 0.01f);
            TooltipHelper::ShowTooltip("Stamina drain rate for two-handed default grip");
            RenderOverrideDrag("2H Alt Burn", runtimeProps.staminaBurn2HAlt, 0.01f);
            TooltipHelper::ShowTooltip("Stamina drain for alternate two-handed grip (half-sword, mordschlag)");
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    WeaponPresetData BuildPresetData() const {
        WeaponPresetData d;
        d.name = presetNameBuf;
        d.passport = weaponPassport;
        d.runtimeProps = runtimeProps;
        return d;
    }

    void ApplyPresetData(const WeaponPresetData& d) {
        weaponPassport = d.passport;
        runtimeProps = d.runtimeProps;
        weaponGenerated = true;
    }

    void SetStatus(std::string msg) {
        statusMessage = std::move(msg);
        statusMessageTime = ImGui::GetTime();
    }

    void RefreshPresetList() {
        presetList = WeaponPresetSerializer::ListPresets();
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
            if (WeaponPresetSerializer::SavePresetByName(presetNameBuf, weaponPassport, data)) {
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
                    auto result = WeaponPresetSerializer::LoadFromFile(presetList[i].path);
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
                    WeaponPresetSerializer::DeletePreset(presetList[i].path);
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
            std::string encoded = WeaponPresetSerializer::EncodeForClipboard(weaponPassport, data);
            ImGui::SetClipboardText(encoded.c_str());
            SetStatus("Copied to clipboard");
        }
        ImGui::SameLine();
        if (ImGui::Button("Paste from Clipboard", ImVec2(halfWidth, 0))) {
            const char* clip = ImGui::GetClipboardText();
            if (clip && clip[0]) {
                auto result = WeaponPresetSerializer::DecodeFromClipboard(clip);
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

        if (!weaponGenerated) ImGui::BeginDisabled();
        if (ImGui::Button("Spawn Weapon", ImVec2(-1, 0))) {
            if (ComponentValidator::Validate(player) && ComponentValidator::Validate(world))
                SpawnFromPassport();
        }
        if (!weaponGenerated) ImGui::EndDisabled();
    }

public:
    WeaponEditorSection() : CollapsibleSection("Weapon Editor") {
        Function("Spawn Weapon")
            .WithKey(&cfg.spawnKey)
            .WithParams({
                Parameter("snap_to_ground", "Snap to Ground", &cfg.snapToGround, "Snap spawned weapon to the ground"),
                Parameter("distance_forward", "Forward Distance", &cfg.spawnDistanceForward, 50.0f, 300.0f, "Spawn distance in front of player"),
                Parameter("distance_up", "Up Distance", &cfg.spawnDistanceUp, 0.0f, 200.0f, "Spawn height offset"),
                Parameter("scale", "Scale", &cfg.spawnScale, 0.1f, 5.0f, "Size multiplier"),
                Parameter("live_preview", "Live Preview", &cfg.livePreview, "Auto-spawn preview weapon as you edit")
            })
            .WithTooltip("Spawns the currently edited weapon with runtime overrides applied")
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

        if (weaponGenerated) {
            if (ImGui::Checkbox("Live Preview", &cfg.livePreview)) {
                if (!cfg.livePreview)
                    DestroyPreview();
            }

            ImGui::Spacing();
            if (ImGui::BeginTabBar("##WeaponEditorTabs")) {
                if (ImGui::BeginTabItem("Modules"))    { RenderModulesTab();    ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Geometry"))    { RenderGeometryTab();   ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Appearance"))  { RenderAppearanceTab(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Stats"))       { RenderStatsTab();      ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem("Presets"))     { RenderPresetsTab();    ImGui::EndTabItem(); }
                ImGui::EndTabBar();
            }
        }

        RenderSpawnFooter();

        if (cfg.livePreview && weaponGenerated)
            UpdatePreview();
    }
};
