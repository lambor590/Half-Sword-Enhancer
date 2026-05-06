#include "Menu/Sections/Equipment/WeaponEditorSection.h"
#include "Menu/SectionRegistry.h"
#include "Menu/SectionStyle.h"

REGISTER_SECTION(WeaponEditorSection, MenuTab::Equipment);

#include <cstdio>
#include <cstring>
#include <tuple>
#include <algorithm>
#include <span>

#include "Hooks/GameHook.h"
#include "Utils/BlueprintRegistry.h"
#include "Utils/CustomizableWeapon.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/GuiUtils.h"
#include "Utils/PresetUtils.h"
#include "Utils/Spawner.h"
#include "Utils/TierValidation.h"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/ModularWeaponBP_Customizable_classes.hpp"

namespace {

    bool ColorEquals(const SDK::FLinearColor& a, const SDK::FLinearColor& b) {
        return a.R == b.R && a.G == b.G && a.B == b.B && a.A == b.A;
    }

    auto PassportFields(const SDK::FStr_Passport_Weapon1& p) {
        return std::tie(
            p.WeaponClass_54_B478ECF7499977809745A3973AD678EC, p.ID_70_C02CF656483647A1933EEA96314B78A6,
            p.Name_57_3729B51148E846FE8DD336B9419BCEE1, p.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D,
            p.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9, p.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139,
            p.GuardModule_13_6DD2B06245505E53B529D090333012F0, p.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6,
            p.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4, p.HeadSize_21_2D425E61473B8F64FBAB51B223459D57,
            p.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704, p.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF,
            p.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E, p.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7,
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
    GameHook::QueueAction([this, type, tier](const RuntimeContextSnapshot& runtime) {
        auto* world = runtime.world;
        if (!world) {
            weaponGenerationPending = false;
            return;
        }
        auto generated = EquipmentGenerator::GenerateCustomizableWeapon(world, type, tier);
        ClearWeaponPassportPadding(generated);
        if (!EquipmentGenerator::IsPassportValid(generated)) {
            SetStatus("Generation failed for this type/tier", true);
        } else {
            weaponPassport = generated;
        }
        globalModules.Populate();
        weaponGenerationPending = false;
    });
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
        SetStatus("Weapon tiers are still scanning", true);
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
        OverrideField(
            "Rigidity", rp.rigidity, 0.0, 0.0, 0.0, 0.1f,
            "Structural stiffness - affects impact resistance and damage transfer"
        ),
        OverrideField(
            "Edge Sharpness", rp.edgeSharpness, 0.0, 0.0, 0.0, 0.1f,
            "Cutting edge quality - determines slashing effectiveness"
        ),
        OverrideField("Raw Damage", rp.rawDamage, 0.0, 0.0, 0.0, 0.1f, "Base damage multiplier before other modifiers"),
        OverrideField(
            "Cutting Rate", rp.cuttingRate, 0.0, 0.0, 0.0, 0.01f, "Slashing damage multiplier for cutting attacks"
        ),
        OverrideField("Stab Rate", rp.stabRate, 0.0, 0.0, 0.0, 0.01f, "Thrusting damage multiplier for stab attacks"),
        OverrideField(
            "Def Rating", rp.defRating, 0.0, 0.0, 0.0, 0.01f, "Defensive effectiveness when blocking or parrying"
        ),
        OverrideField("Grip Rate", rp.gripRate, 0.0, 0.0, 0.0, 0.01f, "Weapon handling and control precision"),
        OverrideField(
            "Draw Cut Rate", rp.drawCutRate, 0.0, 0.0, 0.0, 0.01f, "Damage bonus for drawing/slicing motions"
        ),
        OverrideField(
            "Tip Sharpness", rp.tipSharpness, 0.0, 0.0, 0.0, 0.1f,
            "Point sharpness - affects piercing on thrust attacks"
        ),
        OverrideField("Kick Power", rp.kickPower, 0.0, 0.0, 0.0, 0.1f, "Knockback force applied on impact"),
    };
    physicsFields = {
        OverrideField(
            "Mat Density", rp.matDensity, 0.0, 0.0, 0.0, 0.1f, "Material density - affects momentum and swing weight"
        ),
    };
    dismemberFields = {
        OverrideField(
            "Sharp Level", rp.dismemberSharp, 0, 0, 0, 0.1f, "Sharp dismemberment threshold (higher = easier to sever)"
        ),
        OverrideField(
            "Blunt Level", rp.dismemberBlunt, 0, 0, 0, 0.1f, "Blunt dismemberment threshold (higher = easier to crush)"
        ),
    };
    toggleFields = {
        OverrideField("Double Edged", rp.doubleEdged, false, "Both edges can cut (swords vs single-edge weapons)"),
        OverrideField("Piercing", rp.piercing, false, "Weapon can pierce through armor"),
        OverrideField("No Stab", rp.noStab, false, "Disables thrust attacks (for blunt weapons)"),
    };
    staminaFields = {
        OverrideField(
            "R Hand Burn", rp.staminaBurnR, 0.0, 0.0, 0.0, 0.01f, "Stamina drain rate when wielding in right hand"
        ),
        OverrideField(
            "L Hand Burn", rp.staminaBurnL, 0.0, 0.0, 0.0, 0.01f, "Stamina drain rate when wielding in left hand"
        ),
        OverrideField(
            "2H Burn", rp.staminaBurn2H, 0.0, 0.0, 0.0, 0.01f, "Stamina drain rate for two-handed default grip"
        ),
        OverrideField(
            "2H Alt Burn", rp.staminaBurn2HAlt, 0.0, 0.0, 0.0, 0.01f,
            "Stamina drain for alternate two-handed grip (half-sword, mordschlag)"
        ),
    };
}

int WeaponEditorSection::CountAllActive() const {
    return CountActive(combatFields) + CountActive(physicsFields) + CountActive(dismemberFields) +
           CountActive(toggleFields) + CountActive(staminaFields);
}

namespace {

    using W = SDK::AModularWeaponBP_C;

    static constexpr OverrideSetter COMBAT_SETTERS[] = {
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Rigidity = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Edge_Sharpness = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Raw_Damage = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Cutting_Rate = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Stab_Rate = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Def_Rating = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Grip_Rate = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Draw_Cut_Rate = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Tip_Sharpness = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Kick_Power = GetDouble(f); },
    };

    static constexpr OverrideSetter PHYSICS_SETTERS[] = {
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Mat_Density = GetDouble(f); },
    };

    static constexpr OverrideSetter DISMEMBER_SETTERS[] = {
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Dismemberment_Level_Sharp = GetInt(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Dismemberment_Level_Blunt = GetInt(f); },
    };

    static constexpr OverrideSetter TOGGLE_SETTERS[] = {
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Double_Edged = GetBool(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->Piercing = GetBool(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->NoStab = GetBool(f); },
    };

    static constexpr OverrideSetter STAMINA_SETTERS[] = {
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->R_Hand_Stamina_Burn_Rate = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->L_Hand_Stamina_Burn_Rate = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->TwoH_Default_Stamina_Burn_Rate = GetDouble(f); },
        [](void* a, const OverrideDescriptor& f) { static_cast<W*>(a)->TwoH_Alt_Stamina_Burn_Rate = GetDouble(f); },
    };

} // namespace

void WeaponEditorSection::ApplyOverridesToActor(SDK::AActor* actor) const {
    if (!actor) return;
    auto* w = static_cast<void*>(static_cast<SDK::AModularWeaponBP_C*>(actor));
    ApplyWithSetters(combatFields, w, COMBAT_SETTERS);
    ApplyWithSetters(physicsFields, w, PHYSICS_SETTERS);
    ApplyWithSetters(dismemberFields, w, DISMEMBER_SETTERS);
    ApplyWithSetters(toggleFields, w, TOGGLE_SETTERS);
    ApplyWithSetters(staminaFields, w, STAMINA_SETTERS);
}

void WeaponEditorSection::SpawnPreview() {
    preview.Destroy();
    auto [world, player] = RenderPlayerWorld();
    if (!player || !world) return;

    lastPreviewedPassport = weaponPassport;
    ClearWeaponPassportPadding(lastPreviewedPassport);
    lastPreviewedProps = runtimeProps;

    bool hasOverrides = CountAllActive() > 0;
    bool hasMesh = HasAnyMeshOverride();
    auto meshSnap = hasMesh ? BuildMeshSnapshot() : MeshSnapshot{};

    auto transform = Spawner::BuildSpawnTransform(player, cfg.spawn);
    bool snap = cfg.spawn.snapToGround;
    auto passport = weaponPassport;

    GameHook::QueueAction([this, passport, transform, snap, hasOverrides, hasMesh,
                           meshSnap](const RuntimeContextSnapshot& runtime) {
        if (!runtime.world) return;
        Spawner::SpawnCustomizableFromPassport(
            runtime.world, passport, transform, snap,
            [this, hasOverrides, hasMesh, meshSnap](SDK::AActor* actor) {
                auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
                CollectMeshesFromWeapon(weapon);
                if (!cfg.preview.livePreview) {
                    actor->K2_DestroyActor();
                    return;
                }
                weapon->Simulates_Physics = false;
                weapon->Turn_Off_Collision();
                actor->SetActorEnableCollision(false);
                if (hasOverrides) ApplyOverridesToActor(actor);
                if (hasMesh) ApplyMeshOverrides(weapon, meshSnap, skeletalPreviewComps);
                preview.SetPreviewActor(actor, runtime.world);
                if (cfg.preview.autoRotate) actor->K2_SetActorRotation(SDK::FRotator{0.0, preview.GetYaw(), 0.0}, true);
            }
        );
    });
}

void WeaponEditorSection::SpawnFromPassport() {
    auto [world, player] = RenderPlayerWorld();
    if (!player || !world) return;

    if (cfg.preview.livePreview) {
        cfg.preview.livePreview = false;
        preview.Destroy();
    }

    bool hasOverrides = CountAllActive() > 0;
    bool hasMesh = HasAnyMeshOverride();
    auto meshSnap = hasMesh ? BuildMeshSnapshot() : MeshSnapshot{};

    auto callback = [this, hasOverrides, hasMesh, meshSnap](SDK::AActor* actor) {
        auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
        CollectMeshesFromWeapon(weapon);
        if (hasOverrides) ApplyOverridesToActor(actor);
        if (hasMesh) ApplyMeshOverrides(weapon, meshSnap, nullptr, true);
    };

    auto transform = Spawner::BuildSpawnTransform(player, cfg.spawn);
    bool snap = cfg.spawn.snapToGround;
    auto passport = weaponPassport;

    GameHook::QueueAction([passport, transform, snap,
                           callback = std::move(callback)](const RuntimeContextSnapshot& runtime) {
        if (runtime.world) Spawner::SpawnCustomizableFromPassport(runtime.world, passport, transform, snap, callback);
    });
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
    auto [world, player] = RenderPlayerWorld();

    BlueprintRegistry::Get().EnsureTiersScanned();
    ImGui::PushID("gen");

    RenderWeaponTypeCombo();
    TooltipHelper::ShowTooltip("Base weapon archetype that determines available modules and valid tiers");

    ImGui::SameLine();
    uint16_t weaponMask = TierValidation::VALID_TIER_MASKS[cfg.weaponType];
    RenderValidatedTierCombo("##GenTier", cfg.weaponTier, weaponMask);
    TooltipHelper::ShowTooltip("Quality tier - affects generated module selection and weapon stats");

    ImGui::Spacing();
    if (!player || !world || weaponMask == 0) ImGui::BeginDisabled();
    if (ImGui::Button("Generate")) GenerateWeaponPassport();
    TooltipHelper::ShowTooltip("Generate weapon passport using selected type and tier");
    if (!player || !world || weaponMask == 0) ImGui::EndDisabled();

    ImGui::SameLine();
    if (!player || !world) ImGui::BeginDisabled();
    if (ImGui::Button("Randomize")) RandomizeWeaponPassport();
    TooltipHelper::ShowTooltip("Pick random type and tier, then generate");
    if (!player || !world) ImGui::EndDisabled();

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
    bool scaleCommitted = GuiUtils::DebouncedDragFloat3("Scale", s, 0.01f, 0.0f, 0.0f, "%.2f");
    GuiUtils::StoreEdited(ovr.scale, s);
    if (scaleCommitted && preview.GetPreviewActor())
        GameHook::QueueAction([this](const RuntimeContextSnapshot&) { ApplyMeshToPreview(); });

    float r[3] = {
        static_cast<float>(ovr.rotation.Pitch), static_cast<float>(ovr.rotation.Yaw),
        static_cast<float>(ovr.rotation.Roll)};
    ImGui::SetNextItemWidth(meshComboWidth * 0.6f);
    bool rotationCommitted = GuiUtils::DebouncedDragFloat3("Rotation", r, 1.0f, -180.0f, 180.0f, "%.1f");
    GuiUtils::StoreEdited(ovr.rotation, r);
    if (rotationCommitted && preview.GetPreviewActor())
        GameHook::QueueAction([this](const RuntimeContextSnapshot&) { ApplyMeshToPreview(); });

    float o[3] = {static_cast<float>(ovr.offset.X), static_cast<float>(ovr.offset.Y), static_cast<float>(ovr.offset.Z)};
    ImGui::SetNextItemWidth(meshComboWidth * 0.6f);
    bool offsetCommitted = GuiUtils::DebouncedDragFloat3("Offset", o, 0.1f, 0.0f, 0.0f, "%.1f");
    GuiUtils::StoreEdited(ovr.offset, o);
    if (offsetCommitted && preview.GetPreviewActor())
        GameHook::QueueAction([this](const RuntimeContextSnapshot&) { ApplyMeshToPreview(); });

    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) {
        ovr.scale = {1.0, 1.0, 1.0};
        ovr.rotation = {0.0, 0.0, 0.0};
        ovr.offset = {0.0, 0.0, 0.0};
        if (preview.GetPreviewActor())
            GameHook::QueueAction([this](const RuntimeContextSnapshot&) { ApplyMeshToPreview(); });
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

    if (GuiUtils::BeginSizedCombo("Mesh", comboPreview, meshComboWidth)) {
        GuiUtils::SetComboSearchWidth(meshComboWidth);
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
                    GameHook::QueueAction([this](const RuntimeContextSnapshot&) { ApplyMeshToPreview(); });
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
        GameHook::QueueAction([this](const RuntimeContextSnapshot&) { ScanAllMeshes(); });
    }

    if (meshPool.empty()) {
        ImGui::TextDisabled("Scanning meshes...");
        ImGui::PopID();
        return;
    }

    if (ImGui::Button("Refresh")) {
        meshScanQueued = true;
        GameHook::QueueAction([this](const RuntimeContextSnapshot&) { ScanAllMeshes(); });
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
        GameHook::QueueAction([this, pathCopy](const RuntimeContextSnapshot&) {
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
    GuiUtils::RenderOverrideCount(CountAllActive());

    ImGui::Spacing();
    if (ImGui::TreeNodeEx("Combat", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderOverrideGroup(combatFields);
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Physics")) {
        RenderOverrideField(physicsFields[0]);
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Dismemberment")) {
        RenderOverrideGroup(dismemberFields);
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Toggles")) {
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

    const auto& p = weaponPassport;
    auto& paths = d.classPaths;
    paths.weaponClass = PresetUtils::ObjectToAbsolutePath(p.WeaponClass_54_B478ECF7499977809745A3973AD678EC);
    paths.headModule = PresetUtils::ObjectToAbsolutePath(p.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139);
    paths.guardModule = PresetUtils::ObjectToAbsolutePath(p.GuardModule_13_6DD2B06245505E53B529D090333012F0);
    paths.gripModule = PresetUtils::ObjectToAbsolutePath(p.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4);
    paths.pommelModule = PresetUtils::ObjectToAbsolutePath(p.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6);
    paths.subModule1 = PresetUtils::ObjectToAbsolutePath(p.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D);
    paths.subModule2 = PresetUtils::ObjectToAbsolutePath(p.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9);

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

    GameHook::QueueAction([this, paths = std::move(d.classPaths)](const RuntimeContextSnapshot&) {
        Spawner::LoadWeaponClasses(weaponPassport, paths);
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
        GameHook::QueueAction([this, pending = std::move(pending)](const RuntimeContextSnapshot&) {
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

void WeaponEditorSection::SetStatus(const std::string& msg, bool isError) {
    presets.status.Set(msg, isError);
}

void WeaponEditorSection::RenderSpawnFooter() {
    auto [world, player] = RenderPlayerWorld();

    if (!player || !world) ImGui::BeginDisabled();
    if (ImGui::Button("Spawn Weapon", ImVec2(-1, 0))) SpawnFromPassport();
    TooltipHelper::ShowTooltip("Spawn the weapon with current settings. Disables live preview");
    if (!player || !world) ImGui::EndDisabled();
}

WeaponEditorSection::WeaponEditorSection(ModContext& ctx) : Section(ctx, "Weapon Editor") {
    CreateBlankWeaponPassport();
    BuildDescriptors();
    InitKeybinds();

    preview.SetCleanupCallback([this]() {
        for (int i = 0; i < MODULE_SLOT_COUNT; ++i)
            skeletalPreviewComps[i] = nullptr;
    });
}

void WeaponEditorSection::InitKeybinds() {
    AddKeybind(
        keybinds,
        {
            .name = "Spawn Weapon",
            .tooltip = "Spawns the currently edited weapon with runtime overrides applied",
            .configSection = "SpawnWeapon",
            .keyPtr = &cfg.spawnKey,
            .callback = [this]([[maybe_unused]] bool, const RuntimeContextSnapshot&) { SpawnFromPassport(); },
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
        }
    );
}

void WeaponEditorSection::Render() {
    SectionStyle::StyleRAII style;
    auto [world, player] = RenderPlayerWorld();

    preview.InvalidateIfDead(player, world);
    preview.SyncToggleState();

    KeybindUI::RenderKeybindList(keybinds);
    ImGui::Spacing();

    RenderGenerationControls();

    if (!globalModules.populated.load(std::memory_order_acquire) && !modulePoolQueued) {
        modulePoolQueued = true;
        GameHook::QueueAction([this](const RuntimeContextSnapshot&) { globalModules.Populate(); });
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
        default: break;
    }

    ImGui::EndChild();

    RenderSpawnFooter();

    if (cfg.preview.livePreview) {
        bool needsUpdate =
            !WeaponPassportEquals(weaponPassport, lastPreviewedPassport) || runtimeProps != lastPreviewedProps;
        preview.Update(needsUpdate, [this]() { SpawnPreview(); });
        preview.Rotate();
    }
}
