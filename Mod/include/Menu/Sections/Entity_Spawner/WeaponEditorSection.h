#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
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
#include "SDK/Str_Passport_Weapon1_structs.hpp"
#include "SDK/Enum_MaterialLayer_structs.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/ModularWeaponBP_Customizable_classes.hpp"

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

    // Runtime property overrides

    struct RuntimeOverride {
        bool enabled = false;
        double value = 0.0;
    };

    struct BoolOverride {
        bool enabled = false;
        bool value = false;
    };

    struct IntOverride {
        bool enabled = false;
        int value = 0;
    };

    struct WeaponRuntimeProps {
        RuntimeOverride rigidity;
        RuntimeOverride edgeSharpness;
        RuntimeOverride rawDamage;
        RuntimeOverride cuttingRate;
        RuntimeOverride stabRate;
        RuntimeOverride defRating;
        RuntimeOverride gripRate;
        RuntimeOverride drawCutRate;
        RuntimeOverride tipSharpness;
        RuntimeOverride kickPower;
        RuntimeOverride matDensity;
        IntOverride dismemberSharp;
        IntOverride dismemberBlunt;
        BoolOverride doubleEdged;
        BoolOverride piercing;
        BoolOverride noStab;
        RuntimeOverride staminaBurnR;
        RuntimeOverride staminaBurnL;
        RuntimeOverride staminaBurn2H;
        RuntimeOverride staminaBurn2HAlt;
    } runtimeProps;

    SDK::AActor* previewActor = nullptr;
    double lastChangeTime = 0.0;
    SDK::FStr_Passport_Weapon1 lastPreviewedPassport{};
    WeaponRuntimeProps lastPreviewedProps{};

    // Global Module Pool

    struct GlobalModuleEntry {
        SDK::UClass* cls;
        std::string name;
        const char* sourceType;
    };

    struct GlobalModulePool {
        std::vector<GlobalModuleEntry> heads, guards, grips, pommels, subMods1, subMods2;
        bool populated = false;
    } globalModules;

    char moduleFilters[6][64] = {};

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

    // Random helpers

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

    // Passport creation

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

    void GenerateWeaponPassport() {
        auto type = static_cast<CustomizableWeapon>(cfg.weaponType);
        auto tier = static_cast<SDK::Enum_Ranks>(cfg.weaponTier);
        weaponGenerationPending = true;
        GameHook::QueueAction([this, type, tier]() {
            EquipmentGenerator::Init(world);
            weaponPassport = EquipmentGenerator::GenerateCustomizableWeapon(type, tier);
            PopulateGlobalModulePool();
            weaponGenerated = true;
            weaponGenerationPending = false;
        });
    }

    void RandomizeWeaponPassport() {
        int randomType = RandomInt(1, WEAPON_TYPE_COUNT);
        uint16_t mask = TierValidation::VALID_TIER_MASKS[randomType];
        int randomTier = RandomValidTier(mask);

        cfg.weaponType = randomType;
        cfg.weaponTier = randomTier;

        auto type = static_cast<CustomizableWeapon>(randomType);
        auto tier = static_cast<SDK::Enum_Ranks>(randomTier);
        weaponGenerationPending = true;
        GameHook::QueueAction([this, type, tier]() {
            EquipmentGenerator::Init(world);
            weaponPassport = EquipmentGenerator::GenerateCustomizableWeapon(type, tier);
            PopulateGlobalModulePool();
            weaponGenerated = true;
            weaponGenerationPending = false;
        });
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

    void SpawnPreview() {
        DestroyPreview();
        if (!weaponGenerated) return;
        if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;

        lastPreviewedPassport = weaponPassport;
        lastPreviewedProps = runtimeProps;

        auto spawnTransform = player->GetTransform();
        const auto forward = player->GetActorForwardVector();
        spawnTransform.Translation.X += forward.X * cfg.spawnDistanceForward;
        spawnTransform.Translation.Y += forward.Y * cfg.spawnDistanceForward;
        spawnTransform.Translation.Z += cfg.spawnDistanceUp;
        spawnTransform.Scale3D = {cfg.spawnScale, cfg.spawnScale, cfg.spawnScale};

        auto props = runtimeProps;
        bool hasOverrides = HasAnyRuntimeOverride();

        Spawner::SpawnCustomizableFromPassport(world, weaponPassport, spawnTransform, cfg.snapToGround,
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

        auto spawnTransform = player->GetTransform();
        const auto forward = player->GetActorForwardVector();
        spawnTransform.Translation.X += forward.X * cfg.spawnDistanceForward;
        spawnTransform.Translation.Y += forward.Y * cfg.spawnDistanceForward;
        spawnTransform.Translation.Z += cfg.spawnDistanceUp;
        spawnTransform.Scale3D = {cfg.spawnScale, cfg.spawnScale, cfg.spawnScale};

        std::function<void(SDK::AActor*)> callback = nullptr;
        if (HasAnyRuntimeOverride()) {
            auto props = runtimeProps;
            callback = [props](SDK::AActor* actor) { ApplyRuntimeProps(actor, props); };
        }

        Spawner::SpawnCustomizableFromPassport(world, weaponPassport, spawnTransform, cfg.snapToGround, callback);
    }

    // UI helpers

    static bool MatchesFilter(const std::string& name, const char* filter) {
        if (!filter[0]) return true;
        auto toLower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
        std::string lowerName(name.size(), '\0');
        std::transform(name.begin(), name.end(), lowerName.begin(), toLower);
        std::string lowerFilter;
        for (const char* p = filter; *p; ++p)
            lowerFilter += toLower(*p);
        return lowerName.find(lowerFilter) != std::string::npos;
    }

    static void RenderFilteredModuleCombo(const char* label,
        SDK::UClass*& current, const std::vector<GlobalModuleEntry>& options,
        char* filterBuf, bool allowNone = true)
    {
        const char* preview = "None";
        for (const auto& e : options)
            if (e.cls == current) { preview = e.name.c_str(); break; }

        if (ImGui::BeginCombo(label, preview)) {
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##filter", "Search modules...", filterBuf, 64);

            if (filterBuf[0]) {
                int visible = 0, total = static_cast<int>(options.size());
                for (const auto& e : options)
                    if (MatchesFilter(e.name, filterBuf)) ++visible;
                ImGui::TextDisabled("Showing %d of %d", visible, total);
            }

            ImGui::Separator();

            if (allowNone && ImGui::Selectable("None", current == nullptr))
                current = nullptr;

            char display[128];
            for (const auto& e : options) {
                if (!MatchesFilter(e.name, filterBuf)) continue;
                std::snprintf(display, sizeof(display), "%-36s [%s]", e.name.c_str(), e.sourceType);
                if (ImGui::Selectable(display, e.cls == current))
                    current = e.cls;
                if (e.cls == current) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    static void RenderMaterialCombo(const char* label, SDK::Enum_MaterialLayer& mat) {
        int val = static_cast<int>(mat);
        const char* preview = (val >= 0 && val < 16) ? MATERIAL_LAYER_NAMES[val] : "Unknown";
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
        float col[4] = {color.R, color.G, color.B, color.A};
        if (ImGui::ColorEdit4(label, col)) {
            color.R = col[0]; color.G = col[1]; color.B = col[2]; color.A = col[3];
        }
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
        float val = static_cast<float>(price);
        if (ImGui::DragFloat(label, &val, speed, 0.0f, 0.0f, "%.1f"))
            price = val;
    }

    static void RenderFreeTierCombo(const char* label, int& tier) {
        char preview[16];
        std::snprintf(preview, sizeof(preview), "Tier %d", tier);
        if (ImGui::BeginCombo(label, preview)) {
            for (int t = 0; t <= 8; ++t) {
                char lbl[16];
                std::snprintf(lbl, sizeof(lbl), "Tier %d", t);
                if (ImGui::Selectable(lbl, t == tier))
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

        char preview[16];
        std::snprintf(preview, sizeof(preview), "Tier %d", tier);
        if (ImGui::BeginCombo(label, preview)) {
            for (int t = 0; t <= 8; ++t) {
                if (!(validMask & (1 << t))) continue;
                char lbl[16];
                std::snprintf(lbl, sizeof(lbl), "Tier %d", t);
                if (ImGui::Selectable(lbl, t == tier))
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

    // Runtime property UI helpers

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

    // Tab renderers

    void RenderGenerationControls() {
        ImGui::PushID("gen");

        float avail = ImGui::GetContentRegionAvail().x;
        float spacing = ImGui::GetStyle().ItemSpacing.x;

        ImGui::SetNextItemWidth(avail * 0.62f - spacing * 0.5f);
        int typeIdx = cfg.weaponType - 1;
        if (ImGui::Combo("##Type", &typeIdx, WEAPON_TYPE_NAMES, WEAPON_TYPE_COUNT))
            cfg.weaponType = typeIdx + 1;

        ImGui::SameLine();
        uint16_t weaponMask = TierValidation::VALID_TIER_MASKS[cfg.weaponType];
        ImGui::SetNextItemWidth(avail * 0.38f - spacing * 0.5f);
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
            globalModules.heads, moduleFilters[0]);
        RenderFilteredModuleCombo("Guard",
            weaponPassport.GuardModule_13_6DD2B06245505E53B529D090333012F0,
            globalModules.guards, moduleFilters[1]);
        RenderFilteredModuleCombo("Grip",
            weaponPassport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4,
            globalModules.grips, moduleFilters[2]);
        RenderFilteredModuleCombo("Pommel",
            weaponPassport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6,
            globalModules.pommels, moduleFilters[3]);
        if (!globalModules.subMods1.empty()) {
            RenderFilteredModuleCombo("Sub-Mod 1",
                weaponPassport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D,
                globalModules.subMods1, moduleFilters[4]);
        }
        if (!globalModules.subMods2.empty()) {
            RenderFilteredModuleCombo("Sub-Mod 2",
                weaponPassport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9,
                globalModules.subMods2, moduleFilters[5]);
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
                ImGui::EndTabBar();
            }
        }

        RenderSpawnFooter();

        if (cfg.livePreview && weaponGenerated)
            UpdatePreview();
    }
};
