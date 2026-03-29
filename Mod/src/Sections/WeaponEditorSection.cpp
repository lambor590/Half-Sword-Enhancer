#include "Menu/Sections/Equipment/WeaponEditorSection.h"
#include "Menu/SectionRegistry.h"
#include "Menu/SectionStyle.h"
#include "ComponentValidator.h"

REGISTER_SECTION(WeaponEditorSection, MenuTab::Equipment);

#include <cstdio>
#include <cstring>
#include <algorithm>

#include "Hooks/GameHook.h"
#include "Utils/BlueprintRegistry.h"
#include "Utils/CustomizableWeapon.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/GuiUtils.h"
#include "Utils/Spawner.h"
#include "Utils/TierValidation.h"
#include "SDK/BP_GameWeapon_Customizable_Master_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/ModularWeaponBP_Customizable_classes.hpp"

void WeaponEditorSection::ClearWeaponPassportPadding(SDK::FStr_Passport_Weapon1& p) {
    std::memset(p.Pad_14, 0, sizeof(p.Pad_14));
    std::memset(p.Pad_EC, 0, sizeof(p.Pad_EC));
    std::memset(reinterpret_cast<uint8_t*>(&p) + 0xF9, 0, 7);
}

const char* WeaponEditorSection::ExtractCategory(const std::string& fullName) {
    if (fullName.find("/Weapons/") != std::string::npos) return "Weapon";
    if (fullName.find("/Armor/") != std::string::npos) return "Armor";
    if (fullName.find("/Props/") != std::string::npos) return "Prop";
    if (fullName.find("/Environments/") != std::string::npos) return "Env";
    if (fullName.find("/Clothing/") != std::string::npos) return "Cloth";
    if (fullName.find("/Character/") != std::string::npos) return "Char";
    if (fullName.find("/Traps/") != std::string::npos) return "Trap";
    if (fullName.find("/Effects/") != std::string::npos) return "FX";
    return "Other";
}

bool WeaponEditorSection::HasExcludedPath(const std::string& fullName) {
    static constexpr const char* PATTERNS[] = {"/Engine/",          "/Effects/",
                                               "/UltraDynamicSky/", "/MetaHumans/",
                                               "/Tests/",           "/Character/Material",
                                               "/Collisions/",      "/Niagara/",
                                               "/Debug/",           "/Editor",
                                               "/NavMesh/",         "/Plugins/",
                                               "/Developer/",       "/BasicShapes/",
                                               "/EditorMeshes/",    "/Geometry/",
                                               "/MaterialEditor/",  "/PCG/",
                                               "/FieldSystem/",     "/GeometryCollection/",
                                               "/ChaosFlesh/",      "/ChaosVehicles/"};
    for (auto* p : PATTERNS)
        if (fullName.find(p) != std::string::npos) return true;
    return false;
}

bool WeaponEditorSection::HasExcludedName(std::string_view name) {
    static constexpr const char* PREFIXES[] = {"UCX_", "UBX_", "USP_", "UCP_", "SM_Preview", "SM_Template"};
    for (auto* prefix : PREFIXES)
        if (name.compare(0, std::strlen(prefix), prefix) == 0) return true;

    static constexpr const char* SUBSTRINGS[] = {"_Collision", "Proxy", "Placeholder", "NavMesh"};
    for (auto* sub : SUBSTRINGS)
        if (name.find(sub) != std::string_view::npos) return true;

    return false;
}

bool WeaponEditorSection::IsStaticMeshInvalid(SDK::UStaticMesh* sm) {
    if (sm->StaticMaterials.Num() == 0) return true;

    auto& bounds = sm->ExtendedBounds;
    if (bounds.SphereRadius < 0.5 || bounds.SphereRadius > 50000.0) return true;

    double minDim = (std::min)({bounds.BoxExtent.X, bounds.BoxExtent.Y, bounds.BoxExtent.Z});
    if (minDim < 0.01 && bounds.SphereRadius > 100.0) return true;

    return false;
}

bool WeaponEditorSection::IsSkeletalMeshInvalid(SDK::USkeletalMesh* sk) {
    if (sk->Materials.Num() == 0) return true;

    auto bounds = sk->GetBounds();
    if (bounds.SphereRadius < 0.5 || bounds.SphereRadius > 50000.0) return true;

    return false;
}

void WeaponEditorSection::CollectMeshesFromWeapon(SDK::AModularWeaponBP_C* weapon) {
    SDK::UStaticMeshComponent* comps[] = {weapon->Head, weapon->Guard, weapon->Grip, weapon->Pommel};
    bool added = false;
    for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
        if (!comps[i]) continue;
        auto* mesh = comps[i]->StaticMesh;
        if (!mesh || !meshSeen.insert(mesh).second) continue;
        std::string fullName = mesh->GetFullName();
        pendingMeshEntries.push_back(
            {mesh, mesh->GetName(), PresetUtils::ObjectToAbsolutePath(mesh), ExtractCategory(fullName),
             MeshType::Static}
        );
        added = true;
    }
    if (added) {
        meshPendingIsFullReplace = false;
        meshPendingReady.store(true, std::memory_order_release);
    }
}

SDK::UObject* WeaponEditorSection::LoadAssetByPath(const char* pathStr) {
    std::string path(pathStr);
    if (path.empty()) return nullptr;

    if (path[0] != '/') path = "/Game/" + path;

    if (path.find('.') == std::string::npos) {
        size_t lastSlash = path.rfind('/');
        if (lastSlash != std::string::npos) path += "." + path.substr(lastSlash + 1);
    }

    std::wstring widePath(path.begin(), path.end());
    SDK::FString fstr(widePath.c_str());

    auto softPath = SDK::UKismetSystemLibrary::MakeSoftObjectPath(fstr);
    auto softRef = SDK::UKismetSystemLibrary::Conv_SoftObjPathToSoftObjRef(softPath);
    auto* loaded = SDK::UKismetSystemLibrary::LoadAsset_Blocking(softRef);
    if (!loaded) return nullptr;

    auto* staticMeshClass = SDK::UStaticMesh::StaticClass();
    auto* skeletalMeshClass = SDK::USkeletalMesh::StaticClass();

    MeshType type;
    if (loaded->IsA(skeletalMeshClass))
        type = MeshType::Skeletal;
    else if (loaded->IsA(staticMeshClass))
        type = MeshType::Static;
    else
        return nullptr;

    if (meshSeen.insert(loaded).second) {
        std::string fullName = loaded->GetFullName();
        pendingMeshEntries.push_back(
            {loaded, loaded->GetName(), PresetUtils::ObjectToAbsolutePath(loaded), ExtractCategory(fullName), type}
        );
        meshPendingIsFullReplace = false;
        meshPendingReady.store(true, std::memory_order_release);
    }
    return loaded;
}

void WeaponEditorSection::ScanAllMeshes() {
    auto* staticClass = SDK::UStaticMesh::StaticClass();
    auto* skeletalClass = SDK::USkeletalMesh::StaticClass();
    int count = SDK::UObject::GObjects->Num();

    std::vector<MeshPoolEntry> scanned;
    std::unordered_set<SDK::UObject*> seen;
    scanned.reserve(2048);
    seen.reserve(4096);

    for (int i = 0; i < count; ++i) {
        auto* obj = SDK::UObject::GObjects->GetByIndex(i);
        if (!obj) continue;

        MeshType type;
        if (obj->Class == staticClass)
            type = MeshType::Static;
        else if (obj->Class == skeletalClass)
            type = MeshType::Skeletal;
        else
            continue;

        if (obj->IsDefaultObject()) continue;
        if (!seen.insert(obj).second) continue;

        std::string fullName = obj->GetFullName();
        if (HasExcludedPath(fullName)) continue;

        auto dotPos = fullName.rfind('.');
        std::string_view meshNameView =
            (dotPos != std::string::npos) ? std::string_view(fullName).substr(dotPos + 1) : std::string_view(fullName);
        if (HasExcludedName(meshNameView)) continue;

        if (type == MeshType::Static) {
            if (IsStaticMeshInvalid(static_cast<SDK::UStaticMesh*>(obj))) continue;
        } else {
            if (IsSkeletalMeshInvalid(static_cast<SDK::USkeletalMesh*>(obj))) continue;
        }

        scanned.push_back(
            {obj, std::string(meshNameView), PresetUtils::ObjectToAbsolutePath(obj), ExtractCategory(fullName), type}
        );
    }

    pendingMeshEntries = std::move(scanned);
    meshPendingIsFullReplace = true;
    meshPendingReady.store(true, std::memory_order_release);
}

bool WeaponEditorSection::HasAnyMeshOverride() const {
    for (int i = 0; i < MODULE_SLOT_COUNT; ++i)
        if (meshOverrides[i].enabled && meshOverrides[i].mesh) return true;
    return false;
}

void WeaponEditorSection::ApplyMeshOverrides(
    SDK::AModularWeaponBP_C* weapon, const MeshSnapshot& snap, SDK::USkeletalMeshComponent** outSkeletalComps,
    bool enableSkeletalCollision
) {
    SDK::UStaticMeshComponent* comps[] = {weapon->Head, weapon->Guard, weapon->Grip, weapon->Pommel};
    for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
        if (!comps[i]) continue;
        const auto& slot = snap.slots[i];

        if (!slot.enabled || !slot.mesh) {
            comps[i]->SetVisibility(true, true);
            continue;
        }

        if (slot.meshType == MeshType::Static) {
            comps[i]->SetVisibility(true, true);
            comps[i]->SetStaticMesh(static_cast<SDK::UStaticMesh*>(slot.mesh));
            comps[i]->SetRelativeScale3D(slot.scale);
            comps[i]->K2_SetRelativeRotation(slot.rotation, false, nullptr, true);
            comps[i]->K2_SetRelativeLocation(slot.offset, false, nullptr, true);
            continue;
        }

        comps[i]->SetVisibility(false, true);

        auto* skMesh = static_cast<SDK::USkeletalMesh*>(slot.mesh);

        auto* added =
            weapon->AddComponentByClass(SDK::USkeletalMeshComponent::StaticClass(), false, SDK::FTransform{}, false);
        if (!added) continue;

        auto* skelComp = static_cast<SDK::USkeletalMeshComponent*>(added);
        skelComp->SetSkeletalMeshAsset(skMesh);
        skelComp->SetAnimationMode(SDK::EAnimationMode::AnimationCustomMode, false);
        skelComp->SetRenderStatic(true);
        skelComp->SetSimulatePhysics(false);
        skelComp->SetEnableGravity(false);
        skelComp->SetComponentTickEnabled(false);
        skelComp->SetCollisionEnabled(SDK::ECollisionEnabled::NoCollision);

        if (enableSkeletalCollision) {
            comps[i]->SetCollisionEnabled(SDK::ECollisionEnabled::NoCollision);
            skelComp->bAlwaysCreatePhysicsState = true;
            skelComp->bEnablePerPolyCollision = true;
            skelComp->SetCollisionProfileName(comps[i]->GetCollisionProfileName(), true);
            skelComp->SetCollisionEnabled(SDK::ECollisionEnabled::QueryAndPhysics);
            skelComp->SetSimulatePhysics(false);
            skelComp->SetEnableGravity(false);
        }

        skelComp->K2_AttachToComponent(
            comps[i], SDK::FName(), SDK::EAttachmentRule::SnapToTarget, SDK::EAttachmentRule::SnapToTarget,
            SDK::EAttachmentRule::SnapToTarget, !enableSkeletalCollision
        );

        skelComp->SetRelativeScale3D(slot.scale);
        skelComp->K2_SetRelativeRotation(slot.rotation, false, nullptr, true);
        skelComp->K2_SetRelativeLocation(slot.offset, false, nullptr, true);

        if (outSkeletalComps) outSkeletalComps[i] = skelComp;
    }
}

WeaponEditorSection::MeshSnapshot WeaponEditorSection::BuildMeshSnapshot() const {
    MeshSnapshot snap;
    std::copy(std::begin(meshOverrides), std::end(meshOverrides), snap.slots);
    return snap;
}

void WeaponEditorSection::ApplyMeshToPreview() {
    if (!preview.GetPreviewActor()) return;
    for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
        if (skeletalPreviewComps[i]) {
            skeletalPreviewComps[i]->K2_DestroyComponent(skeletalPreviewComps[i]);
            skeletalPreviewComps[i] = nullptr;
        }
    }
    auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(preview.GetPreviewActor());
    ApplyMeshOverrides(weapon, BuildMeshSnapshot(), skeletalPreviewComps);
}

int WeaponEditorSection::RandomValidTier(uint16_t mask) {
    int validTiers[9];
    int count = 0;
    for (int t = 0; t <= 8; ++t)
        if (mask & (1 << t)) validTiers[count++] = t;
    if (count == 0) return 4;
    return validTiers[GameConstants::RandomInt(0, count - 1)];
}

void WeaponEditorSection::CreateBlankWeaponPassport() {
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
}

void WeaponEditorSection::QueueGeneration(CustomizableWeapon type, SDK::Enum_Ranks tier) {
    weaponGenerationPending = true;
    GameHook::QueueAction([this, type, tier]() {
        EquipmentGenerator::Init(world);
        weaponPassport = EquipmentGenerator::GenerateCustomizableWeapon(type, tier);
        ClearWeaponPassportPadding(weaponPassport);
        if (!EquipmentGenerator::IsPassportValid(weaponPassport))
            SetStatus("Generation failed for this type/tier", true);
        globalModules.Populate();
        weaponGenerationPending = false;
    });
}

void WeaponEditorSection::GenerateWeaponPassport() {
    QueueGeneration(static_cast<CustomizableWeapon>(cfg.weaponType), static_cast<SDK::Enum_Ranks>(cfg.weaponTier));
}

void WeaponEditorSection::RandomizeWeaponPassport() {
    cfg.weaponType = GameConstants::RandomInt(1, WEAPON_TYPE_COUNT);
    uint16_t mask = TierValidation::VALID_TIER_MASKS[cfg.weaponType];
    cfg.weaponTier = RandomValidTier(mask);
    GenerateWeaponPassport();
}

void WeaponEditorSection::ApplyRuntimeProps(SDK::AActor* actor, const WeaponRuntimeProps& props) {
    if (!actor) return;
    auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);

    if (props.rigidity.enabled) weapon->Rigidity = props.rigidity.value;
    if (props.edgeSharpness.enabled) weapon->Edge_Sharpness = props.edgeSharpness.value;
    if (props.rawDamage.enabled) weapon->Raw_Damage = props.rawDamage.value;
    if (props.cuttingRate.enabled) weapon->Cutting_Rate = props.cuttingRate.value;
    if (props.stabRate.enabled) weapon->Stab_Rate = props.stabRate.value;
    if (props.defRating.enabled) weapon->Def_Rating = props.defRating.value;
    if (props.gripRate.enabled) weapon->Grip_Rate = props.gripRate.value;
    if (props.drawCutRate.enabled) weapon->Draw_Cut_Rate = props.drawCutRate.value;
    if (props.tipSharpness.enabled) weapon->Tip_Sharpness = props.tipSharpness.value;
    if (props.kickPower.enabled) weapon->Kick_Power = props.kickPower.value;
    if (props.matDensity.enabled) weapon->Mat_Density = props.matDensity.value;
    if (props.dismemberSharp.enabled) weapon->Dismemberment_Level_Sharp = props.dismemberSharp.value;
    if (props.dismemberBlunt.enabled) weapon->Dismemberment_Level_Blunt = props.dismemberBlunt.value;
    if (props.doubleEdged.enabled) weapon->Double_Edged = props.doubleEdged.value;
    if (props.piercing.enabled) weapon->Piercing = props.piercing.value;
    if (props.noStab.enabled) weapon->NoStab = props.noStab.value;
    if (props.staminaBurnR.enabled) weapon->R_Hand_Stamina_Burn_Rate = props.staminaBurnR.value;
    if (props.staminaBurnL.enabled) weapon->L_Hand_Stamina_Burn_Rate = props.staminaBurnL.value;
    if (props.staminaBurn2H.enabled) weapon->TwoH_Default_Stamina_Burn_Rate = props.staminaBurn2H.value;
    if (props.staminaBurn2HAlt.enabled) weapon->TwoH_Alt_Stamina_Burn_Rate = props.staminaBurn2HAlt.value;
}

int WeaponEditorSection::CountActiveOverrides() const {
    const bool flags[] = {runtimeProps.rigidity.enabled,       runtimeProps.edgeSharpness.enabled,
                          runtimeProps.rawDamage.enabled,      runtimeProps.cuttingRate.enabled,
                          runtimeProps.stabRate.enabled,       runtimeProps.defRating.enabled,
                          runtimeProps.gripRate.enabled,       runtimeProps.drawCutRate.enabled,
                          runtimeProps.tipSharpness.enabled,   runtimeProps.kickPower.enabled,
                          runtimeProps.matDensity.enabled,     runtimeProps.dismemberSharp.enabled,
                          runtimeProps.dismemberBlunt.enabled, runtimeProps.doubleEdged.enabled,
                          runtimeProps.piercing.enabled,       runtimeProps.noStab.enabled,
                          runtimeProps.staminaBurnR.enabled,   runtimeProps.staminaBurnL.enabled,
                          runtimeProps.staminaBurn2H.enabled,  runtimeProps.staminaBurn2HAlt.enabled};
    int count = 0;
    for (bool f : flags)
        count += f;
    return count;
}

void WeaponEditorSection::SpawnPreview() {
    preview.Destroy();
    if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;

    lastPreviewedPassport = weaponPassport;
    ClearWeaponPassportPadding(lastPreviewedPassport);
    lastPreviewedProps = runtimeProps;

    auto props = runtimeProps;
    bool hasOverrides = CountActiveOverrides() > 0;
    bool hasMesh = HasAnyMeshOverride();
    auto meshSnap = hasMesh ? BuildMeshSnapshot() : MeshSnapshot{};

    Spawner::SpawnCustomizableFromPassport(
        world, weaponPassport,
        Spawner::BuildSpawnTransform(player, cfg.spawn.distanceForward, cfg.spawn.distanceUp, cfg.spawn.scale),
        cfg.spawn.snapToGround,
        [this, props, hasOverrides, hasMesh, meshSnap](SDK::AActor* actor) {
            auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
            CollectMeshesFromWeapon(weapon);
            if (!cfg.preview.livePreview) {
                actor->K2_DestroyActor();
                return;
            }
            weapon->Simulates_Physics = false;
            weapon->Turn_Off_Collision();
            actor->SetActorEnableCollision(false);
            if (hasOverrides) ApplyRuntimeProps(actor, props);
            if (hasMesh) ApplyMeshOverrides(weapon, meshSnap, skeletalPreviewComps);
            preview.SetPreviewActor(actor);
            if (cfg.preview.autoRotate) actor->K2_SetActorRotation(SDK::FRotator{0.0, preview.GetYaw(), 0.0}, true);
        }
    );
}

void WeaponEditorSection::SpawnFromPassport() {
    if (cfg.preview.livePreview) {
        cfg.preview.livePreview = false;
        preview.Destroy();
    }

    bool hasOverrides = CountActiveOverrides() > 0;
    bool hasMesh = HasAnyMeshOverride();

    auto props = runtimeProps;
    auto meshSnap = hasMesh ? BuildMeshSnapshot() : MeshSnapshot{};

    auto callback = [this, props, hasOverrides, hasMesh, meshSnap](SDK::AActor* actor) {
        auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
        CollectMeshesFromWeapon(weapon);
        if (hasOverrides) ApplyRuntimeProps(actor, props);
        if (hasMesh) ApplyMeshOverrides(weapon, meshSnap, nullptr, true);
    };

    Spawner::SpawnCustomizableFromPassport(
        world, weaponPassport,
        Spawner::BuildSpawnTransform(player, cfg.spawn.distanceForward, cfg.spawn.distanceUp, cfg.spawn.scale),
        cfg.spawn.snapToGround, callback
    );
}

void WeaponEditorSection::RenderVectorDrag(const char* label, SDK::FVector& vec, float speed) {
    float v[3] = {static_cast<float>(vec.X), static_cast<float>(vec.Y), static_cast<float>(vec.Z)};
    if (ImGui::DragFloat3(label, v, speed, 0.0f, 0.0f, "%.3f")) {
        vec.X = v[0];
        vec.Y = v[1];
        vec.Z = v[2];
    }
}

void WeaponEditorSection::RenderMassDrag(const char* label, double& mass, float speed) {
    float val = static_cast<float>(mass);
    if (ImGui::DragFloat(label, &val, speed, 0.0f, 0.0f, "%.3f")) mass = val;
}

void WeaponEditorSection::RenderValidatedTierCombo(const char* label, int& tier, uint16_t validMask) {
    tier = TierValidation::NearestValidTier(validMask, tier);

    ImGui::SetNextItemWidth(GuiUtils::CachedTierComboWidth());
    if (ImGui::BeginCombo(label, GuiUtils::TIER_LABELS[tier])) {
        for (int t = 0; t <= 8; ++t) {
            if (!(validMask & (1 << t))) continue;
            if (ImGui::Selectable(GuiUtils::TIER_LABELS[t], t == tier)) tier = t;
            if (t == tier) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void WeaponEditorSection::RenderSizeMassRow(const char* label, SDK::FVector& size, double& mass) {
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

void WeaponEditorSection::RenderGenerationControls() {
    BlueprintRegistry::Get().EnsureTiersScanned();
    ImGui::PushID("gen");

    static float weaponTypeComboW = GuiUtils::CalcComboWidth(WEAPON_TYPE_NAMES, WEAPON_TYPE_COUNT);
    ImGui::SetNextItemWidth(weaponTypeComboW);
    int typeIdx = cfg.weaponType - 1;
    if (ImGui::Combo("##Type", &typeIdx, WEAPON_TYPE_NAMES, WEAPON_TYPE_COUNT)) cfg.weaponType = typeIdx + 1;
    TooltipHelper::ShowTooltip("Base weapon archetype that determines available modules and valid tiers");

    ImGui::SameLine();
    uint16_t weaponMask = TierValidation::VALID_TIER_MASKS[cfg.weaponType];
    RenderValidatedTierCombo("##GenTier", cfg.weaponTier, weaponMask);
    TooltipHelper::ShowTooltip("Quality tier - affects generated module selection and weapon stats");

    bool canGenerate = ComponentValidator::Validate(player) && ComponentValidator::Validate(world);

    ImGui::Spacing();
    if (!canGenerate) ImGui::BeginDisabled();
    if (ImGui::Button("Generate")) GenerateWeaponPassport();
    TooltipHelper::ShowTooltip("Generate weapon passport using selected type and tier");
    ImGui::SameLine();
    if (ImGui::Button("Randomize")) RandomizeWeaponPassport();
    TooltipHelper::ShowTooltip("Pick random type and tier, then generate");
    if (!canGenerate) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Reset")) CreateBlankWeaponPassport();
    TooltipHelper::ShowTooltip("Clear all passport data to blank defaults");

    if (weaponGenerationPending) {
        ImGui::SameLine();
        ImGui::TextDisabled("Generating...");
    }

    ImGui::PopID();
}

void WeaponEditorSection::RenderModulesTab() {
    ImGui::PushID("modules");

    if (!globalModules.populated.load(std::memory_order_acquire)) {
        ImGui::TextDisabled("Module pool not loaded yet");
        ImGui::PopID();
        return;
    }

    GuiUtils::RenderGlobalModuleCombo(
        "Head", weaponPassport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139, globalModules.heads, moduleFilters[0],
        globalModules.cachedWidths[0]
    );
    GuiUtils::RenderGlobalModuleCombo(
        "Guard", weaponPassport.GuardModule_13_6DD2B06245505E53B529D090333012F0, globalModules.guards, moduleFilters[1],
        globalModules.cachedWidths[1]
    );
    GuiUtils::RenderGlobalModuleCombo(
        "Grip", weaponPassport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4, globalModules.grips, moduleFilters[2],
        globalModules.cachedWidths[2]
    );
    GuiUtils::RenderGlobalModuleCombo(
        "Pommel", weaponPassport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6, globalModules.pommels,
        moduleFilters[3], globalModules.cachedWidths[3]
    );
    if (!globalModules.subMods1.empty()) {
        GuiUtils::RenderGlobalModuleCombo(
            "Sub-Mod 1", weaponPassport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D, globalModules.subMods1,
            moduleFilters[4], globalModules.cachedWidths[4]
        );
    }
    if (!globalModules.subMods2.empty()) {
        GuiUtils::RenderGlobalModuleCombo(
            "Sub-Mod 2", weaponPassport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9, globalModules.subMods2,
            moduleFilters[5], globalModules.cachedWidths[5]
        );
    }
    if (globalModules.subMods1.empty() && globalModules.subMods2.empty())
        ImGui::TextDisabled("No sub-modules available for this weapon type");

    ImGui::PopID();
}

void WeaponEditorSection::RenderGeometryTab() {
    ImGui::PushID("geometry");

    float headerAvail = ImGui::GetContentRegionAvail().x;
    ImGui::AlignTextToFramePadding();
    ImGui::Dummy(ImVec2(0, 0));
    ImGui::SameLine(65.0f);
    ImGui::TextDisabled("Size (XYZ)");
    ImGui::SameLine(headerAvail - 70.0f + ImGui::GetStyle().ItemSpacing.x);
    ImGui::TextDisabled("Mass");
    RenderSizeMassRow(
        "Head", weaponPassport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57,
        weaponPassport.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7
    );
    TooltipHelper::ShowTooltip("Scale and weight of the head component");
    RenderSizeMassRow(
        "Guard", weaponPassport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704,
        weaponPassport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927
    );
    TooltipHelper::ShowTooltip("Scale and weight of the guard component");
    RenderSizeMassRow(
        "Grip", weaponPassport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF,
        weaponPassport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10
    );
    TooltipHelper::ShowTooltip("Scale and weight of the grip component");
    RenderSizeMassRow(
        "Pommel", weaponPassport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E,
        weaponPassport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173
    );
    TooltipHelper::ShowTooltip("Scale and weight of the pommel component");

    ImGui::Spacing();
    if (ImGui::Button("Reset Geometry")) {
        weaponPassport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57 = {1.0, 1.0, 1.0};
        weaponPassport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 = {1.0, 1.0, 1.0};
        weaponPassport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF = {1.0, 1.0, 1.0};
        weaponPassport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E = {1.0, 1.0, 1.0};
        weaponPassport.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7 = 1.0;
        weaponPassport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927 = 1.0;
        weaponPassport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10 = 1.0;
        weaponPassport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173 = 1.0;
    }
    TooltipHelper::ShowTooltip("Reset all component sizes to 1.0 and masses to 1.0");

    ImGui::PopID();
}

void WeaponEditorSection::RenderAppearanceTab() {
    ImGui::PushID("appearance");

    ImGui::SeparatorText("Metal");
    GuiUtils::RenderMaterialCombo("Steel", weaponPassport.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36);
    TooltipHelper::ShowTooltip("Primary metal surface finish");
    GuiUtils::RenderMaterialCombo("Colored", weaponPassport.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2);
    TooltipHelper::ShowTooltip("Secondary metallic accent layer");

    ImGui::SeparatorText("Organic");
    GuiUtils::RenderMaterialCombo("Wood", weaponPassport.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A);
    TooltipHelper::ShowTooltip("Wood grain pattern for handle and wooden parts");
    GuiUtils::RenderColorEditor("Wood Color", weaponPassport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743);
    TooltipHelper::ShowTooltip("Tint color applied to wooden surfaces");

    ImGui::Spacing();
    GuiUtils::RenderMaterialCombo("Leather", weaponPassport.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB);
    TooltipHelper::ShowTooltip("Leather wrap style for grip sections");
    GuiUtils::RenderColorEditor("Leather Color", weaponPassport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638);
    TooltipHelper::ShowTooltip("Tint color applied to leather wrapping");

    ImGui::Spacing();
    if (ImGui::Button("Reset Appearance")) {
        weaponPassport.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36 = static_cast<SDK::Enum_MaterialLayer>(3);
        weaponPassport.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2 =
            static_cast<SDK::Enum_MaterialLayer>(0);
        weaponPassport.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A = static_cast<SDK::Enum_MaterialLayer>(14);
        weaponPassport.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB = static_cast<SDK::Enum_MaterialLayer>(10);
        weaponPassport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 = {0.4f, 0.26f, 0.13f, 1.0f};
        weaponPassport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 = {0.3f, 0.18f, 0.08f, 1.0f};
    }
    TooltipHelper::ShowTooltip("Reset all materials and colors to defaults");

    ImGui::PopID();
}

void WeaponEditorSection::RenderMeshTransformControls(MeshOverride& ovr) {
    float s[3] = {static_cast<float>(ovr.scale.X), static_cast<float>(ovr.scale.Y), static_cast<float>(ovr.scale.Z)};
    ImGui::SetNextItemWidth(meshComboWidth * 0.6f);
    if (ImGui::DragFloat3("Scale", s, 0.01f, 0.0f, 0.0f, "%.2f")) {
        ovr.scale = {s[0], s[1], s[2]};
        if (preview.GetPreviewActor()) GameHook::QueueAction([this]() { ApplyMeshToPreview(); });
    }

    float r[3] = {
        static_cast<float>(ovr.rotation.Pitch), static_cast<float>(ovr.rotation.Yaw),
        static_cast<float>(ovr.rotation.Roll)};
    ImGui::SetNextItemWidth(meshComboWidth * 0.6f);
    if (ImGui::DragFloat3("Rotation", r, 1.0f, -180.0f, 180.0f, "%.1f")) {
        ovr.rotation = {r[0], r[1], r[2]};
        if (preview.GetPreviewActor()) GameHook::QueueAction([this]() { ApplyMeshToPreview(); });
    }

    float o[3] = {static_cast<float>(ovr.offset.X), static_cast<float>(ovr.offset.Y), static_cast<float>(ovr.offset.Z)};
    ImGui::SetNextItemWidth(meshComboWidth * 0.6f);
    if (ImGui::DragFloat3("Offset", o, 0.1f, 0.0f, 0.0f, "%.1f")) {
        ovr.offset = {o[0], o[1], o[2]};
        if (preview.GetPreviewActor()) GameHook::QueueAction([this]() { ApplyMeshToPreview(); });
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) {
        ovr.scale = {1.0, 1.0, 1.0};
        ovr.rotation = {0.0, 0.0, 0.0};
        ovr.offset = {0.0, 0.0, 0.0};
        if (preview.GetPreviewActor()) GameHook::QueueAction([this]() { ApplyMeshToPreview(); });
    }
}

void WeaponEditorSection::RenderMeshCombo(int slotIdx) {
    if (meshPool.empty()) return;

    if (meshComboWidth == 0.0f) {
        float maxW = 0;
        for (const auto& e : meshPool) {
            char buf[128];
            const char* tag = e.type == MeshType::Skeletal ? "SK" : "SM";
            std::snprintf(buf, sizeof(buf), "%-36s [%s][%s]", e.name.c_str(), e.category, tag);
            float w = ImGui::CalcTextSize(buf).x;
            if (w > maxW) maxW = w;
        }
        meshComboWidth = GuiUtils::ComboWidthFromText(maxW);
    }

    auto& ovr = meshOverrides[slotIdx];
    ImGui::Checkbox("##meshEn", &ovr.enabled);
    ImGui::SameLine();
    if (!ovr.enabled) ImGui::BeginDisabled();

    const char* comboPreview = (ovr.poolIndex >= 0 && ovr.poolIndex < static_cast<int>(meshPool.size()))
                                   ? meshPool[ovr.poolIndex].name.c_str()
                                   : "None";

    ImGui::SetNextItemWidth(meshComboWidth);
    if (ImGui::BeginCombo("Mesh", comboPreview)) {
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##mf", "Search meshes...", meshFilters[slotIdx], 64);

        const size_t filterLen = std::strlen(meshFilters[slotIdx]);
        const bool hasFilter = filterLen > 0;

        if (hasFilter) {
            int visible = 0;
            for (const auto& e : meshPool)
                if (GuiUtils::MatchesFilter(e.name.c_str(), e.name.size(), meshFilters[slotIdx], filterLen)) ++visible;
            ImGui::TextDisabled("Showing %d of %d", visible, static_cast<int>(meshPool.size()));
        }
        ImGui::Separator();

        if (ImGui::Selectable("None", ovr.poolIndex < 0)) {
            ovr.poolIndex = -1;
            ovr.mesh = nullptr;
        }

        char display[128];
        for (int i = 0; i < static_cast<int>(meshPool.size()); ++i) {
            if (hasFilter && !GuiUtils::MatchesFilter(
                                 meshPool[i].name.c_str(), meshPool[i].name.size(), meshFilters[slotIdx], filterLen
                             ))
                continue;
            ImGui::PushID(i);
            const char* tag = meshPool[i].type == MeshType::Skeletal ? "SK" : "SM";
            std::snprintf(
                display, sizeof(display), "%-36s [%s][%s]", meshPool[i].name.c_str(), meshPool[i].category, tag
            );
            if (ImGui::Selectable(display, ovr.poolIndex == i)) {
                ovr.poolIndex = i;
                ovr.mesh = meshPool[i].mesh;
                ovr.meshType = meshPool[i].type;
                if (preview.GetPreviewActor()) {
                    GameHook::QueueAction([this]() { ApplyMeshToPreview(); });
                }
            }
            if (ovr.poolIndex == i) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    if (ovr.mesh) {
        RenderMeshTransformControls(ovr);
    }

    if (!ovr.enabled) ImGui::EndDisabled();
}

void WeaponEditorSection::DrainPendingMeshEntries() {
    if (!meshPendingReady.exchange(false, std::memory_order_acquire)) return;

    if (meshPendingIsFullReplace) {
        meshPool = std::move(pendingMeshEntries);
        meshSeen.clear();
        for (auto& e : meshPool)
            meshSeen.insert(e.mesh);
        meshScanQueued = false;
    } else {
        for (auto& e : pendingMeshEntries)
            meshPool.push_back(std::move(e));
        pendingMeshEntries.clear();
    }
    meshComboWidth = 0.0f;
}

void WeaponEditorSection::RenderMeshTab() {
    ImGui::PushID("mesh");

    DrainPendingMeshEntries();

    if (meshPool.empty() && !meshScanQueued) {
        meshScanQueued = true;
        GameHook::QueueAction([this]() { ScanAllMeshes(); });
    }

    if (meshPool.empty()) {
        ImGui::TextDisabled("Scanning meshes...");
        ImGui::PopID();
        return;
    }

    if (ImGui::Button("Refresh")) {
        meshScanQueued = true;
        GameHook::QueueAction([this]() { ScanAllMeshes(); });
    }
    TooltipHelper::ShowTooltip("Rescan all loaded meshes from memory. Custom-loaded assets will need to be reloaded");
    ImGui::SameLine();
    int smCount = 0, skCount = 0;
    for (const auto& e : meshPool)
        (e.type == MeshType::Skeletal ? skCount : smCount)++;
    ImGui::TextDisabled("(%d SM, %d SK)", smCount, skCount);
    ImGui::SameLine();
    if (ImGui::Button("Reset All Meshes")) {
        for (int i = 0; i < MODULE_SLOT_COUNT; ++i)
            meshOverrides[i] = {};
        std::memset(meshFilters, 0, sizeof(meshFilters));
    }
    TooltipHelper::ShowTooltip("Clear all mesh overrides and restore original weapon meshes");

    ImGui::SetNextItemWidth(
        ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Load").x - ImGui::GetStyle().FramePadding.x * 2 -
        ImGui::GetStyle().ItemSpacing.x
    );
    ImGui::InputTextWithHint(
        "##assetPath", "Asset path (e.g. Assets/Animals/Horse_001/SkeletalMeshes/SK_Animal_Horse_002)", assetPathBuf,
        sizeof(assetPathBuf)
    );
    TooltipHelper::ShowTooltip(
        "Full or partial UE asset path. Prefix /Game/ and suffix .AssetName are added automatically if missing"
    );
    ImGui::SameLine();
    if (ImGui::Button("Load") && assetPathBuf[0]) {
        auto pathCopy = std::string(assetPathBuf);
        GameHook::QueueAction([this, pathCopy]() {
            auto* result = LoadAssetByPath(pathCopy.c_str());
            if (result)
                SetStatus("Loaded: " + std::string(result->GetName()));
            else
                SetStatus("Failed to load asset", true);
        });
    }
    TooltipHelper::ShowTooltip("Force-load an asset from disk into memory");

    for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
        ImGuiTreeNodeFlags flags = (i == 0) ? ImGuiTreeNodeFlags_DefaultOpen : 0;
        if (!ImGui::TreeNodeEx(MODULE_SLOT_NAMES[i], flags)) continue;

        ImGui::PushID(i);
        RenderMeshCombo(i);
        ImGui::PopID();
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void WeaponEditorSection::RenderStatsTab() {
    ImGui::PushID("stats");

    ImGui::SeparatorText("Passport");
    GuiUtils::RenderFreeTierCombo("Tier", weaponPassport.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461);
    TooltipHelper::ShowTooltip("Stored tier value in the passport, independent of generation tier");
    GuiUtils::RenderPriceDrag("Price", weaponPassport.Price_60_83FE5A624EA188485BBE4E9C8606AEE5);
    TooltipHelper::ShowTooltip("Weapon price value stored in the passport");

    ImGui::SeparatorText("Runtime Overrides");
    TooltipHelper::ShowTooltip("Override weapon stats after spawning. Enable each to apply its value.");

    if (ImGui::Button("Reset All Overrides")) runtimeProps = {};
    TooltipHelper::ShowTooltip("Disable all runtime overrides");
    GuiUtils::RenderOverrideCount(CountActiveOverrides());

    ImGui::Spacing();
    if (ImGui::TreeNodeEx("Combat", ImGuiTreeNodeFlags_DefaultOpen)) {
        GuiUtils::RenderOverrideDrag("Rigidity", runtimeProps.rigidity, 0.1f);
        TooltipHelper::ShowTooltip("Structural stiffness - affects impact resistance and damage transfer");
        GuiUtils::RenderOverrideDrag("Edge Sharpness", runtimeProps.edgeSharpness, 0.1f);
        TooltipHelper::ShowTooltip("Cutting edge quality - determines slashing effectiveness");
        GuiUtils::RenderOverrideDrag("Raw Damage", runtimeProps.rawDamage, 0.1f);
        TooltipHelper::ShowTooltip("Base damage multiplier before other modifiers");
        GuiUtils::RenderOverrideDrag("Cutting Rate", runtimeProps.cuttingRate, 0.01f);
        TooltipHelper::ShowTooltip("Slashing damage multiplier for cutting attacks");
        GuiUtils::RenderOverrideDrag("Stab Rate", runtimeProps.stabRate, 0.01f);
        TooltipHelper::ShowTooltip("Thrusting damage multiplier for stab attacks");
        GuiUtils::RenderOverrideDrag("Def Rating", runtimeProps.defRating, 0.01f);
        TooltipHelper::ShowTooltip("Defensive effectiveness when blocking or parrying");
        GuiUtils::RenderOverrideDrag("Grip Rate", runtimeProps.gripRate, 0.01f);
        TooltipHelper::ShowTooltip("Weapon handling and control precision");
        GuiUtils::RenderOverrideDrag("Draw Cut Rate", runtimeProps.drawCutRate, 0.01f);
        TooltipHelper::ShowTooltip("Damage bonus for drawing/slicing motions");
        GuiUtils::RenderOverrideDrag("Tip Sharpness", runtimeProps.tipSharpness, 0.1f);
        TooltipHelper::ShowTooltip("Point sharpness - affects piercing on thrust attacks");
        GuiUtils::RenderOverrideDrag("Kick Power", runtimeProps.kickPower, 0.1f);
        TooltipHelper::ShowTooltip("Knockback force applied on impact");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Physics")) {
        GuiUtils::RenderOverrideDrag("Mat Density", runtimeProps.matDensity, 0.1f);
        TooltipHelper::ShowTooltip("Material density - affects momentum and swing weight");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Dismemberment")) {
        GuiUtils::RenderOverrideInt("Sharp Level", runtimeProps.dismemberSharp);
        TooltipHelper::ShowTooltip("Sharp dismemberment threshold (higher = easier to sever)");
        GuiUtils::RenderOverrideInt("Blunt Level", runtimeProps.dismemberBlunt);
        TooltipHelper::ShowTooltip("Blunt dismemberment threshold (higher = easier to crush)");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Toggles")) {
        GuiUtils::RenderOverrideBool("Double Edged", runtimeProps.doubleEdged);
        TooltipHelper::ShowTooltip("Both edges can cut (swords vs single-edge weapons)");
        GuiUtils::RenderOverrideBool("Piercing", runtimeProps.piercing);
        TooltipHelper::ShowTooltip("Weapon can pierce through armor");
        GuiUtils::RenderOverrideBool("No Stab", runtimeProps.noStab);
        TooltipHelper::ShowTooltip("Disables thrust attacks (for blunt weapons)");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Stamina")) {
        GuiUtils::RenderOverrideDrag("R Hand Burn", runtimeProps.staminaBurnR, 0.01f);
        TooltipHelper::ShowTooltip("Stamina drain rate when wielding in right hand");
        GuiUtils::RenderOverrideDrag("L Hand Burn", runtimeProps.staminaBurnL, 0.01f);
        TooltipHelper::ShowTooltip("Stamina drain rate when wielding in left hand");
        GuiUtils::RenderOverrideDrag("2H Burn", runtimeProps.staminaBurn2H, 0.01f);
        TooltipHelper::ShowTooltip("Stamina drain rate for two-handed default grip");
        GuiUtils::RenderOverrideDrag("2H Alt Burn", runtimeProps.staminaBurn2HAlt, 0.01f);
        TooltipHelper::ShowTooltip("Stamina drain for alternate two-handed grip (half-sword, mordschlag)");
        ImGui::TreePop();
    }

    ImGui::PopID();
}

WeaponPresetData WeaponEditorSection::BuildPresetData() const {
    WeaponPresetData d;
    d.passport = weaponPassport;
    d.runtimeProps = runtimeProps;

    for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
        d.meshPresets[i].enabled = meshOverrides[i].enabled;
        if (meshOverrides[i].enabled && meshOverrides[i].poolIndex >= 0 &&
            meshOverrides[i].poolIndex < static_cast<int>(meshPool.size()))
            d.meshPresets[i].meshPath = meshPool[meshOverrides[i].poolIndex].path;
        d.meshPresets[i].meshType = meshOverrides[i].meshType;
        d.meshPresets[i].scale = meshOverrides[i].scale;
        d.meshPresets[i].rotation = meshOverrides[i].rotation;
        d.meshPresets[i].offset = meshOverrides[i].offset;
    }

    return d;
}

void WeaponEditorSection::ApplyPresetData(WeaponPresetData d) {
    weaponPassport = d.passport;
    ClearWeaponPassportPadding(weaponPassport);
    runtimeProps = d.runtimeProps;

    GameHook::QueueAction([this, paths = std::move(d.classPaths)]() {
        auto load = [](SDK::UClass*& target, const std::string& path) {
            if (!path.empty()) target = Spawner::LoadClass(path);
        };
        load(weaponPassport.WeaponClass_54_B478ECF7499977809745A3973AD678EC, paths.weaponClass);
        load(weaponPassport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139, paths.headModule);
        load(weaponPassport.GuardModule_13_6DD2B06245505E53B529D090333012F0, paths.guardModule);
        load(weaponPassport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4, paths.gripModule);
        load(weaponPassport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6, paths.pommelModule);
        load(weaponPassport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D, paths.subModule1);
        load(weaponPassport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9, paths.subModule2);
    });

    struct PendingMeshLoad {
        int slot;
        std::string path;
        MeshType meshType;
    };
    std::vector<PendingMeshLoad> pending;

    for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
        meshOverrides[i].enabled = d.meshPresets[i].enabled;
        meshOverrides[i].mesh = nullptr;
        meshOverrides[i].poolIndex = -1;
        meshOverrides[i].meshType = d.meshPresets[i].meshType;
        meshOverrides[i].scale = d.meshPresets[i].scale;
        meshOverrides[i].rotation = d.meshPresets[i].rotation;
        meshOverrides[i].offset = d.meshPresets[i].offset;

        if (!d.meshPresets[i].enabled || d.meshPresets[i].meshPath.empty()) continue;

        bool found = false;
        for (int j = 0; j < static_cast<int>(meshPool.size()); ++j) {
            if (meshPool[j].path == d.meshPresets[i].meshPath) {
                meshOverrides[i].poolIndex = j;
                meshOverrides[i].mesh = meshPool[j].mesh;
                found = true;
                break;
            }
        }
        if (!found) pending.push_back({i, std::move(d.meshPresets[i].meshPath), d.meshPresets[i].meshType});
    }

    if (!pending.empty()) {
        GameHook::QueueAction([this, pending = std::move(pending)]() {
            for (const auto& pl : pending) {
                auto* loaded = LoadAssetByPath(pl.path.c_str());
                if (!loaded) continue;
                for (int j = 0; j < static_cast<int>(meshPool.size()); ++j) {
                    if (meshPool[j].mesh == loaded) {
                        meshOverrides[pl.slot].poolIndex = j;
                        meshOverrides[pl.slot].mesh = loaded;
                        break;
                    }
                }
            }
        });
    }
}

void WeaponEditorSection::SetStatus(std::string msg, bool isError) {
    presets.status.Set(std::move(msg), isError);
}

void WeaponEditorSection::RenderSpawnFooter() {
    bool canSpawn = ComponentValidator::Validate(player) && ComponentValidator::Validate(world);
    if (!canSpawn) ImGui::BeginDisabled();
    if (ImGui::Button("Spawn Weapon", ImVec2(-1, 0))) SpawnFromPassport();
    TooltipHelper::ShowTooltip("Spawn the weapon with current settings. Disables live preview");
    if (!canSpawn) ImGui::EndDisabled();
}

WeaponEditorSection::WeaponEditorSection(ModContext& ctx) : Section(ctx, "Weapon Editor") {
    CreateBlankWeaponPassport();
    InitKeybinds();

    preview.SetCleanupCallback([this]() {
        for (int i = 0; i < MODULE_SLOT_COUNT; ++i)
            skeletalPreviewComps[i] = nullptr;
    });
}

void WeaponEditorSection::InitKeybinds() {
    keybinds.push_back({
        .name = "Spawn Weapon",
        .tooltip = "Spawns the currently edited weapon with runtime overrides applied",
        .configSection = "SpawnWeapon",
        .keyPtr = &cfg.spawnKey,
        .callback =
            [this]([[maybe_unused]] bool) {
                if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;
                SpawnFromPassport();
            },
        .params =
            {KeybindParam(
                 "snap_to_ground", "Snap to Ground", &cfg.spawn.snapToGround, "Snap spawned weapon to the ground"
             ),
             KeybindParam(
                 "distance_forward", "Forward Distance", &cfg.spawn.distanceForward, 50.0f, 300.0f,
                 "Spawn distance in front of player"
             ),
             KeybindParam("distance_up", "Up Distance", &cfg.spawn.distanceUp, 0.0f, 200.0f, "Spawn height offset"),
             KeybindParam("scale", "Scale", &cfg.spawn.scale, 0.1f, 5.0f, "Size multiplier"),
             KeybindParam(
                 "live_preview", "Live Preview", &cfg.preview.livePreview, "Auto-spawn preview weapon as you edit"
             )},
    });
    InitKeybindEntry(keybinds.back());
}

void WeaponEditorSection::Render() {
    SectionStyle::StyleRAII style;

    preview.InvalidateIfDead(player, world);
    preview.SyncToggleState();

    KeybindUI::RenderKeybindList(keybinds);
    ImGui::Spacing();

    RenderGenerationControls();

    if (!globalModules.populated.load(std::memory_order_acquire) && !modulePoolQueued) {
        modulePoolQueued = true;
        GameHook::QueueAction([this]() { globalModules.Populate(); });
    }

    GuiUtils::RenderPreviewControls(cfg.preview, "preview weapon");

    presets.status.Render();

    GuiUtils::BeginScrollWithFooter("##weapon_scroll");

    static constexpr const char* WE_TAB_LABELS[] = {"Modules", "Geometry", "Appearance", "Mesh", "Stats", "Presets"};
    GuiUtils::RenderUnderlineTabs("##WeaponEditorTabs", activeTab, WE_TAB_LABELS, 6);
    switch (activeTab) {
        case 0: RenderModulesTab(); break;
        case 1: RenderGeometryTab(); break;
        case 2: RenderAppearanceTab(); break;
        case 3: RenderMeshTab(); break;
        case 4: RenderStatsTab(); break;
        case 5:
            presets.RenderPresetsTab(
                [this]() { return BuildPresetData(); }, [this](WeaponPresetData d) { ApplyPresetData(std::move(d)); }
            );
            break;
    }

    ImGui::EndChild();

    RenderSpawnFooter();

    if (cfg.preview.livePreview) {
        bool needsUpdate =
            std::memcmp(&weaponPassport, &lastPreviewedPassport, sizeof(SDK::FStr_Passport_Weapon1)) != 0 ||
            std::memcmp(&runtimeProps, &lastPreviewedProps, sizeof(WeaponRuntimeProps)) != 0;
        preview.Update(needsUpdate, [this]() { SpawnPreview(); });
        preview.Rotate();
    }
}
