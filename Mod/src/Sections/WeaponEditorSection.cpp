#include "Menu/Sections/Equipment/WeaponEditorSection.h"

#include <cstdio>
#include <cstring>
#include <expected>
#include <tuple>
#include <algorithm>
#include <utility>

#include "Hooks/GameHook.h"
#include "Utils/BlueprintRegistry.h"
#include "Utils/CustomizableWeapon.h"
#include "Utils/EquipmentApplication.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/GameConstants.h"
#include "Utils/GuiUtils.h"
#include "Utils/PresetApplication.h"
#include "Utils/PresetUtils.h"
#include "Utils/SpawnWorkflow.h"
#include "Utils/TierValidation.h"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/ModularWeaponBP_Customizable_classes.hpp"

namespace {

    template <typename... OverrideTypes> bool HasAnyEnabledOverride(const OverrideTypes&... overrides) {
        return (... || overrides.enabled);
    }

    bool ColorEquals(const SDK::FLinearColor& a, const SDK::FLinearColor& b) {
        return a.R == b.R && a.G == b.G && a.B == b.B && a.A == b.A;
    }

    auto PassportFields(const SDK::FStr_Passport_Weapon1& p) {
        return std::tie(
            p.ID_70_C02CF656483647A1933EEA96314B78A6, p.Name_57_3729B51148E846FE8DD336B9419BCEE1,
            p.HeadSize_21_2D425E61473B8F64FBAB51B223459D57, p.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704,
            p.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF, p.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E,
            p.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7,
            p.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927,
            p.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10,
            p.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173,
            p.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36,
            p.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2,
            p.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A, p.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB,
            p.Price_60_83FE5A624EA188485BBE4E9C8606AEE5, p.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461
        );
    }

    bool WeaponPassportEquals(const SDK::FStr_Passport_Weapon1& a, const SDK::FStr_Passport_Weapon1& b) {
        return PassportFields(a) == PassportFields(b) &&
               ColorEquals(
                   a.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743, b.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743
               ) &&
               ColorEquals(
                   a.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638,
                   b.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638
               );
    }

} // anonymous namespace

const char* WeaponEditorSection::ExtractCategory(const std::string& fullName) {
    if (fullName.find("/Weapons/") != std::string::npos) return "Weapon";
    if (fullName.find("/Armor/") != std::string::npos) return "Armor";
    if (fullName.find("/Props/") != std::string::npos) return "Prop";
    if (fullName.find("/Environments/") != std::string::npos) return "Environment";
    if (fullName.find("/Clothing/") != std::string::npos) return "Clothing";
    if (fullName.find("/Character/") != std::string::npos) return "Character";
    if (fullName.find("/Traps/") != std::string::npos) return "Trap";
    if (fullName.find("/Effects/") != std::string::npos) return "Effects";
    return "Other";
}

bool WeaponEditorSection::HasExcludedPath(const std::string& fullName) {
    static constexpr std::string_view PATTERNS[] = {
        "/Engine/",          "/Effects/",        "/UltraDynamicSky/", "/MetaHumans/",
        "/Tests/",           "/Character/Material", "/Collisions/",   "/Niagara/",
        "/Debug/",           "/Editor",          "/NavMesh/",         "/Plugins/",
        "/Developer/",       "/BasicShapes/",     "/EditorMeshes/",   "/Geometry/",
        "/MaterialEditor/",  "/PCG/",            "/FieldSystem/",     "/GeometryCollection/",
        "/ChaosFlesh/",      "/ChaosVehicles/",
    };
    for (const auto p : PATTERNS)
        if (fullName.find(p) != std::string::npos) return true;
    return false;
}

bool WeaponEditorSection::HasExcludedName(std::string_view name) {
    static constexpr std::string_view PREFIXES[] = {"UCX_", "UBX_", "USP_", "UCP_", "SM_Preview", "SM_Template"};
    for (const auto prefix : PREFIXES)
        if (name.starts_with(prefix)) return true;

    static constexpr std::string_view SUBSTRINGS[] = {"_Collision", "Proxy", "Placeholder", "NavMesh"};
    for (const auto sub : SUBSTRINGS)
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

namespace {
    std::string MeshDisplayLabel(std::string_view name, const char* category, MeshType type) {
        char display[128];
        std::snprintf(
            display, sizeof(display), "%-36.*s [%s] [%s]", static_cast<int>(name.size()), name.data(), category,
            type == MeshType::Skeletal ? "Animated" : "Standard"
        );
        return display;
    }
}

void WeaponEditorSection::CollectMeshesFromWeapon(SDK::AModularWeaponBP_C* weapon) {
    SDK::UStaticMeshComponent* comps[] = {weapon->Head, weapon->Guard, weapon->Grip, weapon->Pommel};
    std::vector<MeshPoolEntry> entries;
    entries.reserve(MODULE_SLOT_COUNT);
    for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
        if (!comps[i]) continue;
        auto* mesh = comps[i]->StaticMesh;
        if (!mesh) continue;
        std::string fullName = mesh->GetFullName();
        std::string meshName = BlueprintRegistry::CleanDisplayName(mesh->GetName());
        const char* category = ExtractCategory(fullName);
        entries.push_back({mesh, meshName, MeshDisplayLabel(meshName, category, MeshType::Static), MeshType::Static});
    }
    PublishMeshEntries(std::move(entries), false);
}

SDK::UObject* WeaponEditorSection::LoadAssetByPath(const char* pathStr) {
    std::string path(pathStr);
    if (path.empty()) return nullptr;

    if (path[0] != '/') path = "/Game/" + path;

    if (path.find('.') == std::string::npos) {
        size_t lastSlash = path.rfind('/');
        if (lastSlash != std::string::npos) path += "." + path.substr(lastSlash + 1);
    }

    std::wstring widePath;
    if (!PresetUtils::TryUtf8ToWide(path, widePath)) return nullptr;
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

    std::string fullName = loaded->GetFullName();
    std::string meshName = BlueprintRegistry::CleanDisplayName(loaded->GetName());
    const char* category = ExtractCategory(fullName);
    std::vector<MeshPoolEntry> entries;
    entries.push_back({loaded, meshName, MeshDisplayLabel(meshName, category, type), type});
    PublishMeshEntries(std::move(entries), false);
    return loaded;
}

void WeaponEditorSection::ScanAllMeshes() {
    auto* staticClass = SDK::UStaticMesh::StaticClass();
    auto* skeletalClass = SDK::USkeletalMesh::StaticClass();
    int count = SDK::UObject::GObjects->Num();

    std::vector<MeshPoolEntry> scanned;
    scanned.reserve(2048);

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

        std::string meshName = BlueprintRegistry::CleanDisplayName(meshNameView);
        const char* category = ExtractCategory(fullName);
        scanned.push_back({obj, meshName, MeshDisplayLabel(meshName, category, type), type});
    }

    PublishMeshEntries(std::move(scanned), true);
}

void WeaponEditorSection::QueueMeshScan() {
    if (meshScanQueued) return;
    meshScanQueued = true;
    if (!GameHook::QueueAction([this](const RuntimeContextSnapshot&) { ScanAllMeshes(); })) meshScanQueued = false;
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
        const auto& slot = snap[i];

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
    MeshSnapshot snapshot;
    for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
        const auto& source = meshOverrides[i];
        auto& target = snapshot[i];
        static_cast<MeshOverrideSettings&>(target) = source;
        if (source.enabled) target.mesh = source.mesh;
    }
    return snapshot;
}

WeaponEditorSection::SpawnDraftSnapshot WeaponEditorSection::BuildSpawnDraftSnapshot() const {
    SpawnDraftSnapshot snapshot;
    snapshot.spawn = cfg.spawn;
    snapshot.passport = weaponPassport;
    snapshot.classPaths = weaponPaths;
    snapshot.deferredName = deferredWeaponName;
    snapshot.runtime = runtimeProps;
    snapshot.meshes = BuildMeshSnapshot();
    return snapshot;
}

bool WeaponEditorSection::SpawnDraftMatchesCurrent(const SpawnDraftSnapshot& snapshot) const {
    if (!WeaponPassportEquals(snapshot.passport, weaponPassport) ||
        std::tie(
            snapshot.spawn.distanceForward, snapshot.spawn.distanceUp, snapshot.spawn.scale,
            snapshot.spawn.snapToGround, snapshot.classPaths, snapshot.deferredName, snapshot.runtime
        ) != std::tie(
            cfg.spawn.distanceForward, cfg.spawn.distanceUp, cfg.spawn.scale, cfg.spawn.snapToGround, weaponPaths,
            deferredWeaponName, runtimeProps
        ))
        return false;

    for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
        const auto& source = meshOverrides[i];
        const auto& target = snapshot.meshes[i];
        auto* mesh = source.enabled ? source.mesh : nullptr;
        if (target.mesh != mesh) return false;
        if (!mesh) continue;
        if (target.meshType != source.meshType || target.scale != source.scale || target.rotation != source.rotation ||
            target.offset != source.offset)
            return false;
    }
    return true;
}

void WeaponEditorSection::PublishSpawnDraftSnapshot() {
    std::scoped_lock lock(spawnDraftMutex);
    if (renderDraftRevision < publishedSpawnDraftRevision) return;
    if (SpawnDraftMatchesCurrent(publishedSpawnDraft)) {
        publishedSpawnDraftRevision = renderDraftRevision;
        return;
    }
    publishedSpawnDraft = BuildSpawnDraftSnapshot();
    publishedSpawnDraftRevision = renderDraftRevision;
}

bool WeaponEditorSection::PublishAppliedPresetSpawnSnapshot(const PendingDraftUpdate& update) {
    std::scoped_lock lock(spawnDraftMutex);
    if (draftRevision.load(std::memory_order_acquire) != update.revision ||
        update.revision < publishedSpawnDraftRevision)
        return false;

    publishedSpawnDraft.passport = update.data.passport;
    publishedSpawnDraft.classPaths = update.data.classPaths;
    publishedSpawnDraft.deferredName = update.data.deferredWeaponName;
    publishedSpawnDraft.runtime = update.data.runtimeProps;
    for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
        const auto& source = update.data.meshPresets[i];
        auto& target = publishedSpawnDraft.meshes[i];
        target = {};
        static_cast<MeshOverrideSettings&>(target) = source;
        if (source.enabled) target.mesh = update.loadedMeshes[i];
    }

    publishedSpawnDraftRevision = update.revision;
    return true;
}

void WeaponEditorSection::ApplyMeshToPreview(const MeshSnapshot& snapshot) {
    if (!preview.GetPreviewActor()) return;
    std::scoped_lock lock(skeletalPreviewMutex);
    for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
        if (skeletalPreviewComps[i]) {
            skeletalPreviewComps[i]->K2_DestroyComponent(skeletalPreviewComps[i]);
            skeletalPreviewComps[i] = nullptr;
        }
    }
    auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(preview.GetPreviewActor());
    ApplyMeshOverrides(weapon, snapshot, skeletalPreviewComps);
}

void WeaponEditorSection::ResetWeaponPassport() {
    renderDraftRevision = draftRevision.fetch_add(1, std::memory_order_acq_rel) + 1;
    weaponGenerationPending.store(false, std::memory_order_release);
    {
        std::scoped_lock lock(pendingRenderMutex);
        pendingRenderUpdates.draft.reset();
    }
    weaponPaths = {};
    gripMeshPath.clear();
    coaInt = 0;
    deferredWeaponName.clear();
    weaponPassport = EquipmentApplication::DefaultWeaponPassport();
}

void WeaponEditorSection::QueueGeneration(CustomizableWeapon type, SDK::Enum_Ranks tier) {
    const std::uint64_t revision = draftRevision.fetch_add(1, std::memory_order_acq_rel) + 1;
    weaponGenerationPending.store(true, std::memory_order_release);
    {
        std::scoped_lock lock(pendingRenderMutex);
        pendingRenderUpdates.draft.reset();
    }
    const bool queued = GameHook::QueueAction([this, type, tier, revision](const RuntimeContextSnapshot& runtime) {
        auto* world = runtime.world;
        if (!world) {
            if (draftRevision.load(std::memory_order_acquire) == revision)
                weaponGenerationPending.store(false, std::memory_order_release);
            return;
        }
        auto generated = EquipmentGenerator::GenerateCustomizableWeapon(world, type, tier);
        PresetApplication::NormalizeWeaponPassport(generated);
        if (!EquipmentGenerator::IsPassportValid(generated)) {
            PublishFeedback(
                FeedbackOrigin::Generation, "Could not create a weapon design for the selected type and tier", 0,
                revision
            );
        } else {
            auto path = [](SDK::UObject* obj) {
                return PresetUtils::ObjectToAbsolutePath(obj);
            };
            PendingDraftUpdate update;
            update.revision = revision;
            update.data.passport = generated;
            update.data.classPaths = {
                path(generated.WeaponClass_54_B478ECF7499977809745A3973AD678EC),
                path(generated.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139),
                path(generated.GuardModule_13_6DD2B06245505E53B529D090333012F0),
                path(generated.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4),
                path(generated.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6),
                path(generated.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D),
                path(generated.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9),
            };
            PublishDraftUpdate(std::move(update));
            PublishFeedback(FeedbackOrigin::Generation, {}, 0, revision);
        }
        globalModules.Populate();
        if (draftRevision.load(std::memory_order_acquire) == revision)
            weaponGenerationPending.store(false, std::memory_order_release);
    });
    if (!queued) {
        weaponGenerationPending.store(false, std::memory_order_release);
        PublishFeedback(FeedbackOrigin::Generation, "Could not create weapon design", 0, revision);
    }
}

void WeaponEditorSection::GenerateWeaponPassport() {
    QueueGeneration(static_cast<CustomizableWeapon>(cfg.weaponType), static_cast<SDK::Enum_Ranks>(cfg.weaponTier));
}

void WeaponEditorSection::RandomizeWeaponPassport() {
    int validTypes[WEAPON_TYPE_COUNT];
    int count = 0;
    for (int i = 1; i <= WEAPON_TYPE_COUNT; ++i)
        if (TierValidation::VALID_TIER_MASKS[i] != 0) validTypes[count++] = i;
    if (count == 0) {
        PublishFeedback(FeedbackOrigin::Generation, "Weapon types are still loading");
        return;
    }
    cfg.weaponType = validTypes[GameConstants::RandomInt(0, count - 1)];
    uint16_t mask = TierValidation::VALID_TIER_MASKS[cfg.weaponType];
    cfg.weaponTier = TierValidation::RandomValidTier(mask);
    GenerateWeaponPassport();
}

void WeaponEditorSection::BuildDescriptors() {
    auto& rp = runtimeProps;

    combatFields = {
        OverrideField("Impact Resistance", rp.rigidity, 0.1f, "Resistance to bending and impact damage"),
        OverrideField("Edge Sharpness", rp.edgeSharpness, 0.1f, "Cutting effectiveness of the blade edge"),
        OverrideField("Damage", rp.rawDamage, 0.1f, "Overall weapon damage"),
        OverrideField("Cut Damage", rp.cuttingRate, 0.01f, "Damage from slashing attacks"),
        OverrideField("Stab Damage", rp.stabRate, 0.01f, "Damage from thrust attacks"),
        OverrideField("Block Strength", rp.defRating, 0.01f, "Effectiveness when blocking or parrying"),
        OverrideField("Handling", rp.gripRate, 0.01f, "Ease and precision of weapon control"),
        OverrideField("Draw Cut Damage", rp.drawCutRate, 0.01f, "Damage from drawing and slicing motions"),
        OverrideField("Tip Sharpness", rp.tipSharpness, 0.1f, "Piercing effectiveness of the weapon point"),
        OverrideField("Knockback", rp.kickPower, 0.1f, "How far targets are pushed on impact"),
    };
    physicsFields = {
        OverrideField("Swing Weight", rp.matDensity, 0.1f, "Weapon momentum and perceived weight when swinging"),
    };
    dismemberFields = {
        OverrideField("Severing Power", rp.dismemberSharp, 0.1f, "Higher values sever limbs more easily"),
        OverrideField("Crushing Power", rp.dismemberBlunt, 0.1f, "Higher values crush limbs more easily"),
    };
    toggleFields = {
        OverrideField("Double Edged", rp.doubleEdged, "Allow both edges to cut"),
        OverrideField("Armor Piercing", rp.piercing, "Allow the weapon to pierce armor"),
        OverrideField("Disable Stabbing", rp.noStab, "Prevent thrust attacks"),
    };
    staminaFields = {
        OverrideField("Right-Hand Cost", rp.staminaBurnR, 0.01f, "Stamina used while wielding in the right hand"),
        OverrideField("Left-Hand Cost", rp.staminaBurnL, 0.01f, "Stamina used while wielding in the left hand"),
        OverrideField("Two-Handed Cost", rp.staminaBurn2H, 0.01f, "Stamina used with the normal two-handed grip"),
        OverrideField(
            "Alternate Grip Cost", rp.staminaBurn2HAlt, 0.01f,
            "Stamina used with alternate two-handed grips such as half-sword and mordschlag"
        ),
    };
}

int WeaponEditorSection::CountAllActive() const {
    return CountActive(combatFields) + CountActive(physicsFields) + CountActive(dismemberFields) +
           CountActive(toggleFields) + CountActive(staminaFields);
}

namespace {

    template <typename Entry>
    const Entry* FindModulePath(const std::string& currentPath, const std::vector<Entry>& options) {
        if (currentPath.empty()) return nullptr;
        for (const auto& e : options) {
            if (e.path == currentPath) return &e;
        }
        return nullptr;
    }

} // namespace

void WeaponEditorSection::SpawnPreview() {
    auto snapshot = RenderSnapshot();
    if (!snapshot.player || !snapshot.world) {
        preview.Destroy();
        return;
    }

    const bool hasOverrides = CountAllActive() > 0;
    const bool hasMesh = HasAnyMeshOverride();
    auto meshSnapshot = hasMesh ? BuildMeshSnapshot() : MeshSnapshot{};
    auto runtimeSnapshot = runtimeProps;
    SpawnWorkflow::ActorCallback onPreviewReady;
    if (hasOverrides || hasMesh) {
        onPreviewReady =
            [this, hasOverrides, hasMesh, meshSnapshot = std::move(meshSnapshot),
             runtimeSnapshot](SDK::AActor* actor) {
                auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
                if (hasOverrides) (void)PresetApplication::ApplyWeaponRuntimeOverrides(actor, runtimeSnapshot);
                if (hasMesh) {
                    std::scoped_lock lock(skeletalPreviewMutex);
                    ApplyMeshOverrides(weapon, meshSnapshot, skeletalPreviewComps);
                }
            };
    }

    if (SpawnWorkflow::QueueWeaponPreview(
        snapshot, preview, cfg.spawn, weaponPassport, weaponPaths,
        [this](SDK::AActor* actor) {
            auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
            CollectMeshesFromWeapon(weapon);
        },
        std::move(onPreviewReady),
        deferredWeaponName
    )) {
        lastPreviewedPassport = weaponPassport;
        PresetApplication::NormalizeWeaponPassport(lastPreviewedPassport);
        lastPreviewedPaths = weaponPaths;
        lastPreviewedProps = runtimeProps;
    }
}

void WeaponEditorSection::SpawnWeapon() {
    auto snapshot = RenderSnapshot();
    if (!snapshot.player || !snapshot.world) return;

    if (cfg.preview.livePreview) preview.Disable();

    SpawnWeapon(snapshot, BuildSpawnDraftSnapshot());
}

void WeaponEditorSection::SpawnWeapon(const RuntimeContextSnapshot& runtime, SpawnDraftSnapshot draft) {
    if (!runtime.player || !runtime.world) return;

    const std::uint64_t request = BeginFeedbackRequest(FeedbackOrigin::Spawn);
    const auto& props = draft.runtime;
    const bool hasRuntimeOverrides = HasAnyEnabledOverride(
        props.rigidity, props.edgeSharpness, props.rawDamage, props.cuttingRate, props.stabRate, props.defRating,
        props.gripRate, props.drawCutRate, props.tipSharpness, props.kickPower, props.matDensity, props.dismemberSharp,
        props.dismemberBlunt, props.doubleEdged, props.piercing, props.noStab, props.staminaBurnR, props.staminaBurnL,
        props.staminaBurn2H, props.staminaBurn2HAlt
    );
    auto callback = [this, runtimeProps = draft.runtime, meshes = std::move(draft.meshes),
                     hasRuntimeOverrides](SDK::AActor* actor) -> std::expected<void, std::string> {
        if (!actor) return std::unexpected("Weapon could not be created");
        auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
        CollectMeshesFromWeapon(weapon);
        if (hasRuntimeOverrides && !PresetApplication::ApplyWeaponRuntimeOverrides(actor, runtimeProps))
            return std::unexpected("Custom weapon stats could not be applied");
        if (std::any_of(meshes.begin(), meshes.end(), [](const auto& slot) { return slot.mesh != nullptr; }))
            ApplyMeshOverrides(weapon, meshes, nullptr, true);
        return {};
    };

    SpawnWorkflow::QueueWeaponSpawn(
        runtime, draft.spawn, draft.passport, std::move(draft.classPaths), std::move(callback),
        std::move(draft.deferredName), [this, request](SpawnWorkflow::SpawnResult result) {
            if (result.success) {
                PublishFeedback(FeedbackOrigin::Spawn, {}, request);
                return;
            }
            PublishFeedback(
                FeedbackOrigin::Spawn,
                result.error.empty() ? "Could not spawn weapon"
                                     : "Could not spawn weapon: " + std::move(result.error),
                request
            );
        }
    );
}

void WeaponEditorSection::RenderVectorDrag(const char* label, SDK::FVector& vec, float speed) {
    float v[3] = {static_cast<float>(vec.X), static_cast<float>(vec.Y), static_cast<float>(vec.Z)};
    GuiUtils::DebouncedDragFloat3(label, v, speed, 0.0f, 0.0f, "%.3f");
    GuiUtils::StoreEdited(vec, v);
}

void WeaponEditorSection::RenderMassDrag(const char* label, double& mass, float speed) {
    auto val = static_cast<float>(mass);
    GuiUtils::DebouncedDragFloat(label, &val, speed, 0.0f, 0.0f, "%.3f");
    GuiUtils::StoreEdited(mass, val);
}

void WeaponEditorSection::RenderWeaponTypeCombo() {
    static float weaponTypeComboW = GuiUtils::CalcComboWidth(WEAPON_TYPE_NAMES, WEAPON_TYPE_COUNT);
    const int selectedIdx = cfg.weaponType - 1;
    const char* previewText =
        (selectedIdx >= 0 && selectedIdx < WEAPON_TYPE_COUNT) ? WEAPON_TYPE_NAMES[selectedIdx] : "Select weapon";

    if (!GuiUtils::BeginSizedCombo("##Type", previewText, weaponTypeComboW)) return;

    GuiUtils::SetComboSearchWidth(weaponTypeComboW);
    ImGui::InputTextWithHint("##WeaponTypeFilter", "Search weapons...", weaponTypeFilter, sizeof(weaponTypeFilter));

    const size_t filterLen = std::strlen(weaponTypeFilter);
    const bool hasFilter = filterLen > 0;
    if (hasFilter) {
        int visible = 0;
        for (auto* weaponName : WEAPON_TYPE_NAMES)
            if (GuiUtils::MatchesFilter(weaponName, std::strlen(weaponName), weaponTypeFilter, filterLen)) ++visible;
        ImGui::TextDisabled("Showing %d of %d", visible, WEAPON_TYPE_COUNT);
    }
    ImGui::Separator();

    for (int i = 0; i < WEAPON_TYPE_COUNT; ++i) {
        auto* weaponName = WEAPON_TYPE_NAMES[i];
        if (hasFilter && !GuiUtils::MatchesFilter(weaponName, std::strlen(weaponName), weaponTypeFilter, filterLen))
            continue;
        if (ImGui::Selectable(weaponName, i == selectedIdx)) cfg.weaponType = i + 1;
        if (i == selectedIdx) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
}

void WeaponEditorSection::RenderValidatedTierCombo(const char* label, int& tier, uint16_t validMask) {
    if (validMask == 0) {
        ImGui::BeginDisabled();
        if (GuiUtils::BeginSizedCombo(label, "No valid tiers", GuiUtils::CachedTierComboWidth())) ImGui::EndCombo();
        ImGui::EndDisabled();
        return;
    }

    tier = TierValidation::NearestValidTier(validMask, tier);

    if (GuiUtils::BeginSizedCombo(label, GuiUtils::TIER_LABELS[tier], GuiUtils::CachedTierComboWidth())) {
        for (int t = 0; t <= 8; ++t) {
            if (!(validMask & (1 << t))) continue;
            if (ImGui::Selectable(GuiUtils::TIER_LABELS[t], t == tier)) tier = t;
            if (t == tier) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void WeaponEditorSection::RenderSizeMassRow(const char* label, SDK::FVector& size, double& mass) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1.0f);
    char sizeId[32];
    std::snprintf(sizeId, sizeof(sizeId), "##size_%s", label);
    RenderVectorDrag(sizeId, size);

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1.0f);
    char massId[32];
    std::snprintf(massId, sizeof(massId), "##mass_%s", label);
    RenderMassDrag(massId, mass);
}

void WeaponEditorSection::RenderGenerationControls() {
    auto [world, player] = RenderPlayerWorld();

    BlueprintRegistry::Get().EnsureTiersScanned();
    ImGui::PushID("gen");

    RenderWeaponTypeCombo();
    GuiUtils::HelpTooltip("Choose the weapon style and its available parts");

    (void)GuiUtils::SameLineIfFits(GuiUtils::CachedTierComboWidth());
    uint16_t weaponMask = TierValidation::VALID_TIER_MASKS[cfg.weaponType];
    RenderValidatedTierCombo("##GenTier", cfg.weaponTier, weaponMask);
    GuiUtils::HelpTooltip("Choose the overall weapon quality");

    ImGui::Spacing();
    if (!player || !world || weaponMask == 0) ImGui::BeginDisabled();
    if (ImGui::Button("Create Weapon Design")) GenerateWeaponPassport();
    GuiUtils::HelpTooltip("Create a weapon design with the selected type and tier");
    if (!player || !world || weaponMask == 0) ImGui::EndDisabled();

    (void)GuiUtils::SameLineIfFitsButton("Random Weapon Design");
    if (!player || !world) ImGui::BeginDisabled();
    if (ImGui::Button("Random Weapon Design")) RandomizeWeaponPassport();
    GuiUtils::HelpTooltip("Create a random weapon design");
    if (!player || !world) ImGui::EndDisabled();

    (void)GuiUtils::SameLineIfFitsButton("Clear Weapon Design");
    if (ImGui::Button("Clear Weapon Design")) ResetWeaponPassport();
    GuiUtils::HelpTooltip("Return the editor to an empty weapon design");

    if (weaponGenerationPending) {
        (void)GuiUtils::SameLineIfFits(ImGui::CalcTextSize("Creating design...").x);
        ImGui::TextDisabled("Creating design...");
    }

    ImGui::PopID();
}

void WeaponEditorSection::RenderModulesTab() {
    ImGui::PushID("modules");

    if (!globalModules.populated.load(std::memory_order_acquire)) {
        ImGui::TextDisabled("Weapon parts are still loading...");
        ImGui::PopID();
        return;
    }

    auto modulePathFits = [](const std::string& path, const auto& options) {
        return path.empty() || FindModulePath(path, options);
    };
    auto pathsFit = [&](const GlobalModuleSet& set) {
        return modulePathFits(weaponPaths.headModule, set.heads) &&
               modulePathFits(weaponPaths.guardModule, set.guards) &&
               modulePathFits(weaponPaths.gripModule, set.grips) &&
               modulePathFits(weaponPaths.pommelModule, set.pommels) &&
               modulePathFits(weaponPaths.subModule1, set.subMods1) &&
               modulePathFits(weaponPaths.subModule2, set.subMods2);
    };
    int moduleType = cfg.weaponType;
    if (!pathsFit(globalModules.ForType(moduleType))) {
        for (int type = 1; type <= GameConstants::WEAPON_TYPE_COUNT; ++type) {
            if (pathsFit(globalModules.ForType(type))) {
                moduleType = type;
                break;
            }
        }
    }

    auto& modules = globalModules.ForType(moduleType);
    auto syncPath = [](SDK::UClass*& current, std::string& path, const auto& options) {
        if (const auto* entry = FindModulePath(path, options)) {
            current = entry->cls;
        } else if (!path.empty()) {
            path.clear();
            current = nullptr;
        }
    };

    syncPath(weaponPassport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139, weaponPaths.headModule, modules.heads);
    syncPath(weaponPassport.GuardModule_13_6DD2B06245505E53B529D090333012F0, weaponPaths.guardModule, modules.guards);
    syncPath(weaponPassport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4, weaponPaths.gripModule, modules.grips);
    syncPath(
        weaponPassport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6, weaponPaths.pommelModule, modules.pommels
    );
    syncPath(
        weaponPassport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D, weaponPaths.subModule1, modules.subMods1
    );
    syncPath(
        weaponPassport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9, weaponPaths.subModule2, modules.subMods2
    );

    GuiUtils::RenderGlobalModuleCombo(
        "Head", weaponPassport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139, modules.heads, moduleFilters[0],
        modules.cachedWidths[0], true, &weaponPaths.headModule
    );
    GuiUtils::RenderGlobalModuleCombo(
        "Guard", weaponPassport.GuardModule_13_6DD2B06245505E53B529D090333012F0, modules.guards, moduleFilters[1],
        modules.cachedWidths[1], true, &weaponPaths.guardModule
    );
    GuiUtils::RenderGlobalModuleCombo(
        "Grip", weaponPassport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4, modules.grips, moduleFilters[2],
        modules.cachedWidths[2], true, &weaponPaths.gripModule
    );
    GuiUtils::RenderGlobalModuleCombo(
        "Pommel", weaponPassport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6, modules.pommels, moduleFilters[3],
        modules.cachedWidths[3], true, &weaponPaths.pommelModule
    );
    if (!modules.subMods1.empty()) {
        GuiUtils::RenderGlobalModuleCombo(
            "Extra Head Part 1", weaponPassport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D, modules.subMods1,
            moduleFilters[4], modules.cachedWidths[4], true, &weaponPaths.subModule1
        );
    }
    if (!modules.subMods2.empty()) {
        GuiUtils::RenderGlobalModuleCombo(
            "Extra Head Part 2", weaponPassport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9, modules.subMods2,
            moduleFilters[5], modules.cachedWidths[5], true, &weaponPaths.subModule2
        );
    }
    if (modules.subMods1.empty() && modules.subMods2.empty())
        ImGui::TextDisabled("No extra head parts are available for this weapon type");

    const bool hasModules = !weaponPaths.headModule.empty() || !weaponPaths.guardModule.empty() ||
                            !weaponPaths.gripModule.empty() || !weaponPaths.pommelModule.empty() ||
                            !weaponPaths.subModule1.empty() || !weaponPaths.subModule2.empty();
    if (hasModules) {
        weaponPaths.weaponClass = GameConstants::MODULAR_WEAPON_BP_PATH;
        weaponPassport.WeaponClass_54_B478ECF7499977809745A3973AD678EC = nullptr;
    } else {
        weaponPaths.weaponClass.clear();
        weaponPassport.WeaponClass_54_B478ECF7499977809745A3973AD678EC = nullptr;
    }

    ImGui::PopID();
}

void WeaponEditorSection::RenderGeometryTab() {
    ImGui::PushID("geometry");

    constexpr ImGuiTableFlags TABLE_FLAGS = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    if (ImGui::BeginTable("##Geometry", 3, TABLE_FLAGS)) {
        ImGui::TableSetupColumn("Part", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Size (XYZ)", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthStretch, 1.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("Part");
        ImGui::TableNextColumn();
        ImGui::TextDisabled("Size (XYZ)");
        ImGui::TableNextColumn();
        ImGui::TextDisabled("Weight");

        RenderSizeMassRow(
            "Head", weaponPassport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57,
            weaponPassport.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7
        );
        GuiUtils::HelpTooltip("Size and weight of the head");
        RenderSizeMassRow(
            "Guard", weaponPassport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704,
            weaponPassport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927
        );
        GuiUtils::HelpTooltip("Size and weight of the guard");
        RenderSizeMassRow(
            "Grip", weaponPassport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF,
            weaponPassport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10
        );
        GuiUtils::HelpTooltip("Size and weight of the grip");
        RenderSizeMassRow(
            "Pommel", weaponPassport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E,
            weaponPassport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173
        );
        GuiUtils::HelpTooltip("Size and weight of the pommel");
        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (ImGui::Button("Restore Default Size & Weight")) {
        weaponPassport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57 = {1.0, 1.0, 1.0};
        weaponPassport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 = {1.0, 1.0, 1.0};
        weaponPassport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF = {1.0, 1.0, 1.0};
        weaponPassport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E = {1.0, 1.0, 1.0};
        weaponPassport.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7 = 1.0;
        weaponPassport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927 = 1.0;
        weaponPassport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10 = 1.0;
        weaponPassport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173 = 1.0;
    }
    GuiUtils::HelpTooltip("Restore the original part sizes and weights");

    ImGui::PopID();
}

void WeaponEditorSection::RenderAppearanceTab() {
    ImGui::PushID("appearance");

    ImGui::SeparatorText("Metal");
    GuiUtils::RenderMaterialCombo("Steel", weaponPassport.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36);
    GuiUtils::HelpTooltip("Primary metal surface finish");
    GuiUtils::RenderMaterialCombo("Colored", weaponPassport.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2);
    GuiUtils::HelpTooltip("Secondary metallic accent layer");

    ImGui::SeparatorText("Wood & Leather");
    GuiUtils::RenderMaterialCombo("Wood", weaponPassport.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A);
    GuiUtils::HelpTooltip("Wood grain pattern for handle and wooden parts");
    GuiUtils::RenderColorEditor("Wood Color", weaponPassport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743);
    GuiUtils::HelpTooltip("Tint color applied to wooden surfaces");

    ImGui::Spacing();
    GuiUtils::RenderMaterialCombo("Leather", weaponPassport.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB);
    GuiUtils::HelpTooltip("Leather wrap style for grip sections");
    GuiUtils::RenderColorEditor("Leather Color", weaponPassport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638);
    GuiUtils::HelpTooltip("Tint color applied to leather wrapping");

    ImGui::Spacing();
    if (ImGui::Button("Restore Default Appearance")) {
        weaponPassport.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36 = static_cast<SDK::Enum_MaterialLayer>(3);
        weaponPassport.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2 =
            static_cast<SDK::Enum_MaterialLayer>(0);
        weaponPassport.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A = static_cast<SDK::Enum_MaterialLayer>(14);
        weaponPassport.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB = static_cast<SDK::Enum_MaterialLayer>(10);
        weaponPassport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 = {0.4f, 0.26f, 0.13f, 1.0f};
        weaponPassport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 = {0.3f, 0.18f, 0.08f, 1.0f};
    }
    GuiUtils::HelpTooltip("Restore the original materials and colors");

    ImGui::PopID();
}

void WeaponEditorSection::RenderMeshTransformControls(MeshOverride& ovr) {
    float s[3] = {static_cast<float>(ovr.scale.X), static_cast<float>(ovr.scale.Y), static_cast<float>(ovr.scale.Z)};
    bool scaleCommitted = GuiUtils::DebouncedDragFloat3("Size", s, 0.01f, 0.0f, 0.0f, "%.2f");
    GuiUtils::StoreEdited(ovr.scale, s);
    if (scaleCommitted && preview.GetPreviewActor()) {
        auto snapshot = BuildMeshSnapshot();
        GameHook::QueueAction([this, snapshot](const RuntimeContextSnapshot&) { ApplyMeshToPreview(snapshot); });
    }

    float r[3] =
        {static_cast<float>(ovr.rotation.Pitch), static_cast<float>(ovr.rotation.Yaw),
         static_cast<float>(ovr.rotation.Roll)};
    bool rotationCommitted = GuiUtils::DebouncedDragFloat3("Rotation", r, 1.0f, -180.0f, 180.0f, "%.1f");
    GuiUtils::StoreEdited(ovr.rotation, r);
    if (rotationCommitted && preview.GetPreviewActor()) {
        auto snapshot = BuildMeshSnapshot();
        GameHook::QueueAction([this, snapshot](const RuntimeContextSnapshot&) { ApplyMeshToPreview(snapshot); });
    }

    float o[3] = {static_cast<float>(ovr.offset.X), static_cast<float>(ovr.offset.Y), static_cast<float>(ovr.offset.Z)};
    bool offsetCommitted = GuiUtils::DebouncedDragFloat3("Offset", o, 0.1f, 0.0f, 0.0f, "%.1f");
    GuiUtils::StoreEdited(ovr.offset, o);
    if (offsetCommitted && preview.GetPreviewActor()) {
        auto snapshot = BuildMeshSnapshot();
        GameHook::QueueAction([this, snapshot](const RuntimeContextSnapshot&) { ApplyMeshToPreview(snapshot); });
    }

    (void)GuiUtils::SameLineIfFitsButton("Restore Model Adjustments");
    if (ImGui::SmallButton("Restore Model Adjustments")) {
        ovr.scale = {1.0, 1.0, 1.0};
        ovr.rotation = {0.0, 0.0, 0.0};
        ovr.offset = {0.0, 0.0, 0.0};
        if (preview.GetPreviewActor()) {
            auto snapshot = BuildMeshSnapshot();
            GameHook::QueueAction([this, snapshot](const RuntimeContextSnapshot&) { ApplyMeshToPreview(snapshot); });
        }
    }
}

void WeaponEditorSection::RenderMeshCombo(int slotIdx) {
    if (meshPool.empty()) return;

    if (meshComboWidth == 0.0f) {
        float maxW = 0;
        for (const auto& e : meshPool) {
            float w = ImGui::CalcTextSize(e.display.c_str()).x;
            if (w > maxW) maxW = w;
        }
        meshComboWidth = GuiUtils::ComboWidthFromText(maxW);
    }

    auto& ovr = meshOverrides[slotIdx];
    ImGui::Checkbox("##meshEn", &ovr.enabled);
    (void)GuiUtils::SameLineIfFits(meshComboWidth);
    if (!ovr.enabled) ImGui::BeginDisabled();

    const char* comboPreview = (ovr.poolIndex >= 0 && ovr.poolIndex < static_cast<int>(meshPool.size()))
                                   ? meshPool[ovr.poolIndex].name.c_str()
                                   : "None";

    if (GuiUtils::BeginSizedCombo("Model", comboPreview, meshComboWidth)) {
        GuiUtils::SetComboSearchWidth(meshComboWidth);
        ImGui::InputTextWithHint("##mf", "Search models...", meshFilters[slotIdx], 64);

        const char* filter = meshFilters[slotIdx];
        const size_t filterLen = std::strlen(filter);
        const bool hasFilter = filterLen > 0;

        if (hasFilter) {
            if (filteredMeshVersion != meshPoolVersion || filteredMeshFilter != filter) {
                filteredMeshIndices.clear();
                filteredMeshIndices.reserve(meshPool.size());
                for (int i = 0; i < static_cast<int>(meshPool.size()); ++i) {
                    const auto& entry = meshPool[i];
                    if (GuiUtils::MatchesFilter(entry.name.c_str(), entry.name.size(), filter, filterLen))
                        filteredMeshIndices.push_back(i);
                }
                filteredMeshFilter = filter;
                filteredMeshVersion = meshPoolVersion;
            }
            ImGui::TextDisabled(
                "Showing %d of %d", static_cast<int>(filteredMeshIndices.size()), static_cast<int>(meshPool.size())
            );
        }
        ImGui::Separator();

        if (ImGui::Selectable("None", ovr.poolIndex < 0)) {
            ovr.enabled = false;
            ovr.poolIndex = -1;
            ovr.mesh = nullptr;
            ovr.path.clear();
        }

        auto renderMeshOption = [&](int i) {
            ImGui::PushID(i);
            if (ImGui::Selectable(meshPool[i].display.c_str(), ovr.poolIndex == i)) {
                ovr.poolIndex = i;
                ovr.mesh = meshPool[i].mesh;
                ovr.meshType = meshPool[i].type;
                ovr.path = PresetUtils::ObjectToAbsolutePath(ovr.mesh);
                if (preview.GetPreviewActor()) {
                    auto snapshot = BuildMeshSnapshot();
                    GameHook::QueueAction([this, snapshot](const RuntimeContextSnapshot&) {
                        ApplyMeshToPreview(snapshot);
                    });
                }
            }
            if (ovr.poolIndex == i) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        };

        if (hasFilter) {
            GuiUtils::RenderClippedList(static_cast<int>(filteredMeshIndices.size()), -1, [&](int row) {
                renderMeshOption(filteredMeshIndices[static_cast<size_t>(row)]);
            });
        } else {
            GuiUtils::RenderClippedList(static_cast<int>(meshPool.size()), ovr.poolIndex, renderMeshOption);
        }
        ImGui::EndCombo();
    }

    if (ovr.mesh) {
        RenderMeshTransformControls(ovr);
    }

    if (!ovr.enabled) ImGui::EndDisabled();
}

void WeaponEditorSection::PublishMeshEntries(std::vector<MeshPoolEntry> entries, bool fullReplace) {
    if (entries.empty() && !fullReplace) return;
    {
        std::scoped_lock lock(pendingRenderMutex);
        pendingRenderUpdates.meshBatches.push_back({std::move(entries), fullReplace});
    }
    pendingRenderReady.store(true, std::memory_order_release);
}

void WeaponEditorSection::PublishDraftUpdate(PendingDraftUpdate update) {
    {
        std::scoped_lock lock(pendingRenderMutex);
        if (draftRevision.load(std::memory_order_acquire) != update.revision) {
            if (!update.presetApply) return;
            auto& error = pendingRenderUpdates.presetError;
            if (!error || update.revision >= error->revision)
                error = PendingPresetError{"Preset could not be loaded; your current edits were kept", update.revision};
        } else {
            pendingRenderUpdates.draft = std::move(update);
        }
    }
    pendingRenderReady.store(true, std::memory_order_release);
}

std::uint64_t WeaponEditorSection::BeginFeedbackRequest(FeedbackOrigin origin) noexcept {
    return feedbackRequests[static_cast<std::size_t>(origin)].fetch_add(1, std::memory_order_relaxed) + 1;
}

void WeaponEditorSection::PublishFeedback(
    FeedbackOrigin origin, std::string error, std::uint64_t request, std::uint64_t revision
) {
    const auto index = static_cast<std::size_t>(origin);
    {
        std::scoped_lock lock(pendingRenderMutex);
        if ((request != 0 && request != feedbackRequests[index].load(std::memory_order_relaxed)) ||
            (revision != 0 && revision != draftRevision.load(std::memory_order_acquire)))
            return;
        pendingRenderUpdates.feedback[index] = PendingFeedback{
            .origin = origin,
            .sequence = ++feedbackSequence,
            .request = request,
            .revision = revision,
            .error = std::move(error),
        };
    }
    pendingRenderReady.store(true, std::memory_order_release);
}

void WeaponEditorSection::PublishPresetError(std::string message, std::uint64_t revision) {
    {
        std::scoped_lock lock(pendingRenderMutex);
        auto& error = pendingRenderUpdates.presetError;
        if (!error || revision >= error->revision) error = PendingPresetError{std::move(message), revision};
    }
    pendingRenderReady.store(true, std::memory_order_release);
}

void WeaponEditorSection::ApplyDraftUpdate(PendingDraftUpdate update) {
    renderDraftRevision = update.revision;
    weaponPassport = update.data.passport;
    PresetApplication::NormalizeWeaponPassport(weaponPassport);
    weaponPaths = std::move(update.data.classPaths);
    gripMeshPath = std::move(update.data.gripMeshPath);
    coaInt = update.data.coaInt;
    deferredWeaponName = std::move(update.data.deferredWeaponName);
    if (!update.replaceAll) return;

    runtimeProps = update.data.runtimeProps;
    for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
        const auto& preset = update.data.meshPresets[i];
        auto& meshOverride = meshOverrides[i];
        meshOverride.enabled = preset.enabled;
        meshOverride.mesh = update.loadedMeshes[i];
        meshOverride.poolIndex = FindMeshPoolIndexByObject(meshOverride.mesh);
        meshOverride.meshType = preset.meshType;
        meshOverride.path = preset.meshPath;
        meshOverride.scale = preset.scale;
        meshOverride.rotation = preset.rotation;
        meshOverride.offset = preset.offset;
    }
}

void WeaponEditorSection::DrainPendingRenderUpdates() {
    if (!pendingRenderReady.load(std::memory_order_acquire) ||
        !pendingRenderReady.exchange(false, std::memory_order_acq_rel))
        return;

    PendingRenderUpdates updates;
    {
        std::scoped_lock lock(pendingRenderMutex);
        updates = std::move(pendingRenderUpdates);
        pendingRenderUpdates = {};
    }

    bool meshPoolChanged = false;
    for (auto& batch : updates.meshBatches) {
        if (batch.fullReplace) {
            meshPool = std::move(batch.entries);
            meshScanQueued = false;
            meshPoolChanged = true;
            continue;
        }
        for (auto& entry : batch.entries) {
            if (!entry.mesh || FindMeshPoolIndexByObject(entry.mesh) >= 0) continue;
            meshPool.push_back(std::move(entry));
            meshPoolChanged = true;
        }
    }
    if (meshPoolChanged) {
        meshComboWidth = 0.0f;
        filteredMeshIndices.clear();
        filteredMeshFilter.clear();
        ++meshPoolVersion;
        ResolveMeshOverrideIndices();
    }

    const std::uint64_t currentRevision = draftRevision.load(std::memory_order_acquire);
    if (updates.draft) {
        const std::uint64_t updateRevision = updates.draft->revision;
        const bool presetApply = updates.draft->presetApply;
        if (updateRevision == currentRevision) {
            ApplyDraftUpdate(std::move(*updates.draft));
            if (presetApply && updateRevision == pendingPresetApplyRevision) {
                presets.CompletePendingApply(true);
                pendingPresetApplyRevision = 0;
            }
        } else if (presetApply && updateRevision == pendingPresetApplyRevision) {
            presets.CompletePendingApply(false, "Preset could not be loaded; your current edits were kept");
            pendingPresetApplyRevision = 0;
        }
    }
    std::array<PendingFeedback*, FEEDBACK_ORIGIN_COUNT> validFeedback{};
    std::size_t feedbackCount = 0;
    for (auto& pending : updates.feedback) {
        if (!pending) continue;
        const auto index = static_cast<std::size_t>(pending->origin);
        if ((pending->request != 0 &&
             pending->request != feedbackRequests[index].load(std::memory_order_relaxed)) ||
            (pending->revision != 0 && pending->revision != currentRevision))
            continue;
        validFeedback[feedbackCount++] = &*pending;
    }
    std::sort(
        validFeedback.begin(), validFeedback.begin() + static_cast<std::ptrdiff_t>(feedbackCount),
        [](const auto* a, const auto* b) { return a->sequence < b->sequence; }
    );
    for (std::size_t i = 0; i < feedbackCount; ++i) {
        auto& feedback = *validFeedback[i];
        auto& statusToken = feedbackStatusTokens[static_cast<std::size_t>(feedback.origin)];
        if (feedback.error.empty()) {
            presets.status.ClearText(statusToken);
            statusToken = 0;
        } else {
            presets.status.SetError(std::move(feedback.error));
            statusToken = presets.status.revision;
        }
    }
    if (updates.presetError && updates.presetError->revision == pendingPresetApplyRevision) {
        presets.CompletePendingApply(false, std::move(updates.presetError->message));
        pendingPresetApplyRevision = 0;
    }
}

int WeaponEditorSection::FindMeshPoolIndexByObject(SDK::UObject* mesh) const {
    if (!mesh) return -1;
    for (int i = 0; i < static_cast<int>(meshPool.size()); ++i)
        if (meshPool[static_cast<size_t>(i)].mesh == mesh) return i;
    return -1;
}

void WeaponEditorSection::ResolveMeshOverrideIndices() {
    for (auto& meshOverride : meshOverrides) {
        if (!meshOverride.mesh) {
            meshOverride.poolIndex = -1;
            continue;
        }

        meshOverride.poolIndex = FindMeshPoolIndexByObject(meshOverride.mesh);
        if (meshOverride.poolIndex >= 0) {
            const auto& entry = meshPool[static_cast<size_t>(meshOverride.poolIndex)];
            meshOverride.meshType = entry.type;
            meshOverride.path = PresetUtils::ObjectToAbsolutePath(entry.mesh);
        }
    }
}

void WeaponEditorSection::RenderMeshTab() {
    ImGui::PushID("mesh");

    if (meshPool.empty()) QueueMeshScan();

    if (meshPool.empty()) {
        ImGui::TextDisabled("Finding available models...");
        ImGui::PopID();
        return;
    }

    if (ImGui::Button("Refresh Model List")) {
        QueueMeshScan();
    }
    GuiUtils::HelpTooltip("Update the list of available models");
    char meshStatus[64];
    std::snprintf(meshStatus, sizeof(meshStatus), "(%zu models)", meshPool.size());
    (void)GuiUtils::SameLineIfFits(ImGui::CalcTextSize(meshStatus).x);
    ImGui::TextDisabled("%s", meshStatus);
    (void)GuiUtils::SameLineIfFitsButton("Restore Original Models");
    if (ImGui::Button("Restore Original Models")) {
        for (int i = 0; i < MODULE_SLOT_COUNT; ++i)
            meshOverrides[i] = {};
        std::memset(meshFilters, 0, sizeof(meshFilters));
    }
    GuiUtils::HelpTooltip("Use the weapon's original models");

    GuiUtils::SetNextInputWidth(
        ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Add Model").x - ImGui::GetStyle().FramePadding.x * 2 -
        ImGui::GetStyle().ItemSpacing.x
    );
    ImGui::InputTextWithHint(
        "##assetPath", "Model address (e.g. Assets/Animals/Horse_001/SkeletalMeshes/SK_Animal_Horse_002)", assetPathBuf,
        sizeof(assetPathBuf)
    );
    GuiUtils::HelpTooltip("Use a full or partial model address");
    ImGui::SameLine();
    if (ImGui::Button("Add Model") && assetPathBuf[0]) {
        const std::uint64_t request = BeginFeedbackRequest(FeedbackOrigin::AddModel);
        auto pathCopy = std::string(assetPathBuf);
        if (!GameHook::QueueAction([this, pathCopy = std::move(pathCopy), request](const RuntimeContextSnapshot&) {
                PublishFeedback(
                    FeedbackOrigin::AddModel, LoadAssetByPath(pathCopy.c_str()) ? "" : "Model could not be added",
                    request
                );
            }))
            PublishFeedback(FeedbackOrigin::AddModel, "Model could not be added", request);
    }
    GuiUtils::HelpTooltip("Add this model to the list");

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

    ImGui::SeparatorText("Base Values");
    GuiUtils::RenderFreeTierCombo("Tier", weaponPassport.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461);
    GuiUtils::HelpTooltip("Base weapon tier");
    GuiUtils::RenderPriceDrag("Price", weaponPassport.Price_60_83FE5A624EA188485BBE4E9C8606AEE5);
    GuiUtils::HelpTooltip("Base weapon price");

    ImGui::SeparatorText("Custom Stats");
    GuiUtils::HelpTooltip("Enable only the weapon values you want to change.");

    if (ImGui::Button("Clear Custom Stats")) runtimeProps = {};
    GuiUtils::HelpTooltip("Disable every custom weapon value");
    GuiUtils::RenderOverrideCount(CountAllActive());

    ImGui::Spacing();
    if (ImGui::TreeNodeEx("Combat", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderOverrideGroup(combatFields);
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Weight")) {
        RenderOverrideField(physicsFields[0]);
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Dismemberment")) {
        RenderOverrideGroup(dismemberFields);
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Capabilities")) {
        RenderOverrideGroup(toggleFields);
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Stamina")) {
        RenderOverrideGroup(staminaFields);
        ImGui::TreePop();
    }

    ImGui::PopID();
}

WeaponPresetData WeaponEditorSection::BuildPresetData() const {
    WeaponPresetData d;
    d.passport = weaponPassport;
    d.runtimeProps = runtimeProps;
    d.classPaths = weaponPaths;
    d.gripMeshPath = gripMeshPath;
    d.coaInt = coaInt;
    d.deferredWeaponName = deferredWeaponName;

    for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
        d.meshPresets[i].enabled = meshOverrides[i].enabled;
        d.meshPresets[i].meshPath = meshOverrides[i].path;
        d.meshPresets[i].meshType = meshOverrides[i].meshType;
        d.meshPresets[i].scale = meshOverrides[i].scale;
        d.meshPresets[i].rotation = meshOverrides[i].rotation;
        d.meshPresets[i].offset = meshOverrides[i].offset;
    }

    return d;
}

bool WeaponEditorSection::PrepareDraftUpdate(PendingDraftUpdate& update, std::string& error) {
    PresetApplication::NormalizeWeaponPassport(update.data.passport);
    if (!PresetApplication::MaterializeWeaponPreset(update.data, &error)) return false;

    for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
        const auto& preset = update.data.meshPresets[i];
        if (preset.meshPath.empty()) {
            if (!preset.enabled) continue;
            error = "A custom model does not have a model address";
            return false;
        }

        auto* loaded = LoadAssetByPath(preset.meshPath.c_str());
        if (!loaded) {
            if (!preset.enabled) continue;
            error = "A custom model is unavailable";
            return false;
        }

        auto* expectedClass =
            preset.meshType == MeshType::Skeletal ? SDK::USkeletalMesh::StaticClass() : SDK::UStaticMesh::StaticClass();
        if (!loaded->IsA(expectedClass)) {
            if (!preset.enabled) continue;
            error = "A custom model is not compatible";
            return false;
        }
        update.loadedMeshes[i] = loaded;
    }
    error.clear();
    return true;
}

PresetApplyDisposition WeaponEditorSection::ApplyPresetData(const WeaponPresetData& data) {
    const std::uint64_t revision = draftRevision.fetch_add(1, std::memory_order_acq_rel) + 1;
    pendingPresetApplyRevision = revision;
    weaponGenerationPending.store(false, std::memory_order_release);
    {
        std::scoped_lock lock(pendingRenderMutex);
        pendingRenderUpdates.draft.reset();
    }

    auto applyPreset = [this, data = WeaponPresetData(data), revision](const RuntimeContextSnapshot&) mutable {
        if (draftRevision.load(std::memory_order_acquire) != revision) {
            PublishPresetError("Preset could not be loaded; your current edits were kept", revision);
            return;
        }

        PendingDraftUpdate update;
        update.revision = revision;
        update.data = std::move(data);
        update.replaceAll = true;
        update.presetApply = true;
        std::string error;
        if (!PrepareDraftUpdate(update, error)) {
            PublishPresetError("Could not load preset: " + error, revision);
            return;
        }
        if (!PublishAppliedPresetSpawnSnapshot(update)) {
            PublishPresetError("Preset could not be loaded; your current edits were kept", revision);
            return;
        }
        PublishDraftUpdate(std::move(update));
    };
    const bool queued = GameHook::QueueAction(std::move(applyPreset));
    if (!queued) {
        pendingPresetApplyRevision = 0;
        presets.status.SetError("Could not load preset");
        return PresetApplyDisposition::Rejected;
    }
    return PresetApplyDisposition::Pending;
}

void WeaponEditorSection::RenderSpawnFooter() {
    auto [world, player] = RenderPlayerWorld();

    if (!player || !world) ImGui::BeginDisabled();
    if (GuiUtils::Button("Spawn Weapon", GuiUtils::ButtonTone::Primary)) SpawnWeapon();
    GuiUtils::HelpTooltip("Place the weapon shown in the editor in front of you");
    if (!player || !world) ImGui::EndDisabled();
}

WeaponEditorSection::WeaponEditorSection(ModContext& ctx) : Section(ctx, SECTION) {
    ResetWeaponPassport();
    BuildDescriptors();
    PublishSpawnDraftSnapshot();
    InitKeybinds();

    preview.SetCleanupCallback([this]() {
        std::scoped_lock lock(skeletalPreviewMutex);
        for (int i = 0; i < MODULE_SLOT_COUNT; ++i)
            skeletalPreviewComps[i] = nullptr;
    });
}

void WeaponEditorSection::OnOpen() {
    DrainPendingRenderUpdates();
    if (activeTab == 3) QueueMeshScan();
}

void WeaponEditorSection::InitKeybinds() {
    keybinds.Add({
        .name = "Spawn Weapon",
        .tooltip = "Place the weapon shown in the editor in front of you",
        .configSection = "SpawnWeapon",
        .keyPtr = &cfg.spawnKey,
        .callback =
            [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                SpawnDraftSnapshot draft;
                {
                    std::scoped_lock lock(spawnDraftMutex);
                    draft = publishedSpawnDraft;
                }
                SpawnWeapon(runtime, std::move(draft));
            },
        .params =
            {KeybindParam(
                 "snap_to_ground", "Place on Ground", &cfg.spawn.snapToGround, "Place spawned weapon on the ground"
             ),
             KeybindParam(
                 "distance_forward", "Distance", &cfg.spawn.distanceForward, 50.0f, 300.0f,
                 "How far in front of the player the weapon appears"
             ),
             KeybindParam("distance_up", "Height", &cfg.spawn.distanceUp, 0.0f, 200.0f, "How high the weapon appears"),
             KeybindParam("scale", "Size", &cfg.spawn.scale, 0.1f, 5.0f, "Weapon size"),
             KeybindParam(
                 "live_preview", "Preview Changes", &cfg.preview.livePreview, "Show your edits on a preview weapon"
             )},
    });
}

void WeaponEditorSection::Render() {
    auto [world, player] = RenderPlayerWorld();

    preview.InvalidateIfDead(player, world);
    preview.SyncToggleState();
    DrainPendingRenderUpdates();
    const bool presetApplyPending = presets.IsApplyPending();
    if (presetApplyPending) ImGui::BeginDisabled();

    keybinds.Render();
    ImGui::Spacing();

    RenderGenerationControls();

    if (!globalModules.populated.load(std::memory_order_acquire) && !modulePoolQueued) {
        modulePoolQueued = true;
        GameHook::QueueAction([this](const RuntimeContextSnapshot&) { globalModules.Populate(); });
    }

    GuiUtils::RenderPreviewControls(cfg.preview, "preview weapon");

    presets.status.Render();

    GuiUtils::BeginScrollWithFooter("##weapon_scroll");

    static constexpr const char* WE_TAB_LABELS[] = {"Parts",  "Size & Weight", "Appearance",
                                                    "Models", "Stats",         "Presets"};
    const int previousTab = activeTab;
    GuiUtils::RenderUnderlineTabs("##WeaponEditorTabs", activeTab, WE_TAB_LABELS, 6);
    if (activeTab == 3 && previousTab != activeTab) QueueMeshScan();
    switch (activeTab) {
        case 0: RenderModulesTab(); break;
        case 1: RenderGeometryTab(); break;
        case 2: RenderAppearanceTab(); break;
        case 3: RenderMeshTab(); break;
        case 4: RenderStatsTab(); break;
        case 5:
            presets.RenderPresetsTab(
                [this](const char*, bool) {
                    return PresetBuildResult<WeaponPresetData>::Success(BuildPresetData());
                },
                [this](const WeaponPresetData& data) { return ApplyPresetData(data); }
            );
            break;
        default: break;
    }

    ImGui::EndChild();

    RenderSpawnFooter();

    if (presetApplyPending) ImGui::EndDisabled();

    if (!presetApplyPending && cfg.preview.livePreview && player && world) {
        bool needsUpdate = weaponPaths != lastPreviewedPaths ||
                           !WeaponPassportEquals(weaponPassport, lastPreviewedPassport) ||
                           runtimeProps != lastPreviewedProps;
        preview.Update(needsUpdate, [this]() { SpawnPreview(); });
        preview.Rotate();
    }
    PublishSpawnDraftSnapshot();
}
