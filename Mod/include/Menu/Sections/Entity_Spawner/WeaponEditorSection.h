#pragma once

#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <random>
#include <atomic>
#include <unordered_set>
#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Hooks/GameHook.h"
#include "Utils/Spawner.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/CustomizableWeapon.h"
#include "Utils/TierValidation.h"
#include "Utils/BlueprintRegistry.h"
#include "SDK/BP_GameWeapon_Customizable_Master_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/ModularWeaponBP_Customizable_classes.hpp"
#include "Utils/WeaponPresetSerializer.h"
#include "Utils/GuiUtils.h"
#include "Utils/GlobalModulePool.h"

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
    bool weaponGenerationPending = false;
    bool modulePoolQueued = false;

    using WeaponRuntimeProps = decltype(WeaponPresetData::runtimeProps);

    WeaponRuntimeProps runtimeProps{};

    SDK::AActor* previewActor = nullptr;
    double lastChangeTime = 0.0;
    double previewYaw = 0.0;
    SDK::FStr_Passport_Weapon1 lastPreviewedPassport{};
    WeaponRuntimeProps lastPreviewedProps{};

    char moduleFilters[6][64] = {};

    char presetNameBuf[128] = {};
    std::vector<PresetListEntry> presetList;
    bool presetListDirty = true;
    std::string statusMessage;
    double statusMessageTime = 0.0;
    bool statusIsError = false;

    enum class WeaponModuleSlot : int { Head = 0, Guard, Grip, Pommel, Count };
    static constexpr int MODULE_SLOT_COUNT = static_cast<int>(WeaponModuleSlot::Count);
    static constexpr const char* MODULE_SLOT_NAMES[] = {"Head", "Guard", "Grip", "Pommel"};

    struct MeshPoolEntry {
        SDK::UObject* mesh;
        std::string name;
        const char* category;
        MeshType type;
    };

    struct MeshOverride {
        bool enabled = false;
        SDK::UObject* mesh = nullptr;
        int poolIndex = -1;
        MeshType meshType = MeshType::Static;
        SDK::FVector scale = {1.0, 1.0, 1.0};
        SDK::FRotator rotation = {0.0, 0.0, 0.0};
        SDK::FVector offset = {0.0, 0.0, 0.0};
    };

    std::vector<MeshPoolEntry> meshPool;
    std::unordered_set<SDK::UObject*> meshSeen;
    bool meshScanQueued = false;
    float meshComboWidth = 0.0f;
    char meshFilters[MODULE_SLOT_COUNT][64] = {};
    char assetPathBuf[256] = {};
    struct MeshSnapshot {
        MeshOverride slots[MODULE_SLOT_COUNT];
    };

    MeshOverride meshOverrides[MODULE_SLOT_COUNT];
    SDK::USkeletalMeshComponent* skeletalPreviewComps[MODULE_SLOT_COUNT] = {};

    GlobalModulePool& globalModules = GlobalModulePool::Get();

    void PopulateGlobalModulePool() {
        globalModules.Populate();
    }

    static const char* ExtractCategory(const std::string& fullName) {
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

    static bool HasExcludedPath(const std::string& fullName) {
        static constexpr const char* PATTERNS[] = {
            "/Engine/", "/Effects/", "/UltraDynamicSky/", "/MetaHumans/",
            "/Tests/", "/Character/Material", "/Collisions/", "/Niagara/",
            "/Debug/", "/Editor", "/NavMesh/",
            "/Plugins/", "/Developer/", "/BasicShapes/", "/EditorMeshes/",
            "/Geometry/", "/MaterialEditor/", "/PCG/", "/FieldSystem/",
            "/GeometryCollection/", "/ChaosFlesh/", "/ChaosVehicles/"
        };
        for (auto* p : PATTERNS)
            if (fullName.find(p) != std::string::npos) return true;
        return false;
    }

    static bool HasExcludedName(const std::string& name) {
        static constexpr const char* PREFIXES[] = {
            "UCX_", "UBX_", "USP_", "UCP_", "SM_Preview", "SM_Template"
        };
        for (auto* prefix : PREFIXES)
            if (name.compare(0, std::strlen(prefix), prefix) == 0) return true;

        static constexpr const char* SUBSTRINGS[] = {
            "_Collision", "Proxy", "Placeholder", "NavMesh"
        };
        for (auto* sub : SUBSTRINGS)
            if (name.find(sub) != std::string::npos) return true;

        return false;
    }

    static bool IsStaticMeshInvalid(SDK::UStaticMesh* sm) {
        if (sm->StaticMaterials.Num() == 0) return true;

        auto& bounds = sm->ExtendedBounds;
        if (bounds.SphereRadius < 0.5 || bounds.SphereRadius > 50000.0) return true;

        double minDim = (std::min)({bounds.BoxExtent.X, bounds.BoxExtent.Y, bounds.BoxExtent.Z});
        if (minDim < 0.01 && bounds.SphereRadius > 100.0) return true;

        return false;
    }

    static bool IsSkeletalMeshInvalid(SDK::USkeletalMesh* sk) {
        if (sk->Materials.Num() == 0) return true;

        auto bounds = sk->GetBounds();
        if (bounds.SphereRadius < 0.5 || bounds.SphereRadius > 50000.0) return true;

        return false;
    }

    void CollectMeshesFromWeapon(SDK::AModularWeaponBP_C* weapon) {
        SDK::UStaticMeshComponent* comps[] = {weapon->Head, weapon->Guard, weapon->Grip, weapon->Pommel};
        for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
            if (!comps[i]) continue;
            auto* mesh = comps[i]->StaticMesh;
            if (!mesh || !meshSeen.insert(mesh).second) continue;
            meshPool.push_back({mesh, mesh->GetName(), ExtractCategory(mesh->GetFullName()), MeshType::Static});
        }
        meshComboWidth = 0.0f;
    }

    static std::string ExtractAssetPath(const std::string& fullName) {
        size_t spacePos = fullName.find(' ');
        if (spacePos == std::string::npos) return "";
        return fullName.substr(spacePos + 1);
    }

    SDK::UObject* LoadAssetByPath(const char* pathStr) {
        std::string path(pathStr);
        if (path.empty()) return nullptr;

        if (path[0] != '/')
            path = "/Game/" + path;

        if (path.find('.') == std::string::npos) {
            size_t lastSlash = path.rfind('/');
            if (lastSlash != std::string::npos)
                path += "." + path.substr(lastSlash + 1);
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
        if (loaded->IsA(skeletalMeshClass)) type = MeshType::Skeletal;
        else if (loaded->IsA(staticMeshClass)) type = MeshType::Static;
        else return nullptr;

        if (meshSeen.insert(loaded).second) {
            meshPool.push_back({loaded, loaded->GetName(), ExtractCategory(loaded->GetFullName()), type});
            meshComboWidth = 0.0f;
        }
        return loaded;
    }

    void ScanAllMeshes() {
        auto* staticClass = SDK::UStaticMesh::StaticClass();
        auto* skeletalClass = SDK::USkeletalMesh::StaticClass();
        int count = SDK::UObject::GObjects->Num();

        meshPool.reserve(2048);
        meshSeen.reserve(4096);

        for (int i = 0; i < count; ++i) {
            auto* obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!obj) continue;

            MeshType type;
            if (obj->Class == staticClass) type = MeshType::Static;
            else if (obj->Class == skeletalClass) type = MeshType::Skeletal;
            else continue;

            if (obj->IsDefaultObject()) continue;
            if (!meshSeen.insert(obj).second) continue;

            std::string name = obj->GetName();
            if (HasExcludedName(name)) continue;

            std::string fullName = obj->GetFullName();
            if (HasExcludedPath(fullName)) continue;

            if (type == MeshType::Static) {
                if (IsStaticMeshInvalid(static_cast<SDK::UStaticMesh*>(obj))) continue;
            } else {
                if (IsSkeletalMeshInvalid(static_cast<SDK::USkeletalMesh*>(obj))) continue;
            }

            meshPool.push_back({obj, std::move(name), ExtractCategory(fullName), type});
        }
        meshComboWidth = 0.0f;
    }

    bool HasAnyMeshOverride() const {
        for (int i = 0; i < MODULE_SLOT_COUNT; ++i)
            if (meshOverrides[i].enabled && meshOverrides[i].mesh) return true;
        return false;
    }

    void ApplyMeshOverrides(SDK::AModularWeaponBP_C* weapon,
        const MeshSnapshot& snap, SDK::USkeletalMeshComponent** outSkeletalComps = nullptr)
    {
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

            auto* added = weapon->AddComponentByClass(
                SDK::USkeletalMeshComponent::StaticClass(),
                false, SDK::FTransform{}, false);
            if (!added) continue;

            auto* skelComp = static_cast<SDK::USkeletalMeshComponent*>(added);
            skelComp->SetSkeletalMeshAsset(static_cast<SDK::USkeletalMesh*>(slot.mesh));
            skelComp->SetAnimationMode(SDK::EAnimationMode::AnimationCustomMode, false);
            skelComp->SetRenderStatic(true);
            skelComp->SetSimulatePhysics(false);
            skelComp->SetEnableGravity(false);
            skelComp->SetComponentTickEnabled(false);

            skelComp->K2_AttachToComponent(comps[i], SDK::FName(),
                SDK::EAttachmentRule::SnapToTarget,
                SDK::EAttachmentRule::SnapToTarget,
                SDK::EAttachmentRule::SnapToTarget, false);

            skelComp->SetRelativeScale3D(slot.scale);
            skelComp->K2_SetRelativeRotation(slot.rotation, false, nullptr, true);
            skelComp->K2_SetRelativeLocation(slot.offset, false, nullptr, true);

            if (outSkeletalComps) outSkeletalComps[i] = skelComp;
        }
    }

    MeshSnapshot BuildMeshSnapshot() const {
        MeshSnapshot snap;
        std::copy(std::begin(meshOverrides), std::end(meshOverrides), snap.slots);
        return snap;
    }

    void ApplyMeshToPreview() {
        if (!previewActor) return;
        for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
            if (skeletalPreviewComps[i]) {
                skeletalPreviewComps[i]->K2_DestroyComponent(skeletalPreviewComps[i]);
                skeletalPreviewComps[i] = nullptr;
            }
        }
        auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(previewActor);
        ApplyMeshOverrides(weapon, BuildMeshSnapshot(), skeletalPreviewComps);
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
    }

    void QueueGeneration(CustomizableWeapon type, SDK::Enum_Ranks tier) {
        weaponGenerationPending = true;
        GameHook::QueueAction([this, type, tier]() {
            EquipmentGenerator::Init(world);
            weaponPassport = EquipmentGenerator::GenerateCustomizableWeapon(type, tier);
            PopulateGlobalModulePool();
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
        return CountActiveOverrides() > 0;
    }

    int CountActiveOverrides() const {
        const bool flags[] = {
            runtimeProps.rigidity.enabled, runtimeProps.edgeSharpness.enabled,
            runtimeProps.rawDamage.enabled, runtimeProps.cuttingRate.enabled,
            runtimeProps.stabRate.enabled, runtimeProps.defRating.enabled,
            runtimeProps.gripRate.enabled, runtimeProps.drawCutRate.enabled,
            runtimeProps.tipSharpness.enabled, runtimeProps.kickPower.enabled,
            runtimeProps.matDensity.enabled, runtimeProps.dismemberSharp.enabled,
            runtimeProps.dismemberBlunt.enabled, runtimeProps.doubleEdged.enabled,
            runtimeProps.piercing.enabled, runtimeProps.noStab.enabled,
            runtimeProps.staminaBurnR.enabled, runtimeProps.staminaBurnL.enabled,
            runtimeProps.staminaBurn2H.enabled, runtimeProps.staminaBurn2HAlt.enabled
        };
        int count = 0;
        for (bool f : flags) count += f;
        return count;
    }

    void DestroyPreview() {
        for (int i = 0; i < MODULE_SLOT_COUNT; ++i)
            skeletalPreviewComps[i] = nullptr;
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
        if (!ComponentValidator::Validate(player) || !ComponentValidator::Validate(world)) return;

        lastPreviewedPassport = weaponPassport;
        lastPreviewedProps = runtimeProps;

        auto props = runtimeProps;
        bool hasOverrides = HasAnyRuntimeOverride();
        bool hasMesh = HasAnyMeshOverride();
        auto meshSnap = hasMesh ? BuildMeshSnapshot() : MeshSnapshot{};

        Spawner::SpawnCustomizableFromPassport(world, weaponPassport, BuildSpawnTransform(), cfg.snapToGround,
            [this, props, hasOverrides, hasMesh, meshSnap](SDK::AActor* actor) {
                auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
                CollectMeshesFromWeapon(weapon);
                if (!cfg.livePreview) {
                    actor->K2_DestroyActor();
                    return;
                }
                weapon->Simulates_Physics = false;
                weapon->Turn_Off_Collision();
                actor->SetActorEnableCollision(false);
                if (hasOverrides) ApplyRuntimeProps(actor, props);
                if (hasMesh) ApplyMeshOverrides(weapon, meshSnap, skeletalPreviewComps);
                previewActor = actor;
                if (cfg.autoRotate)
                    actor->K2_SetActorRotation(SDK::FRotator{0.0, previewYaw, 0.0}, true);
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

    void RotatePreview() {
        if (!previewActor || !cfg.autoRotate) return;

        previewYaw += cfg.rotationSpeed * static_cast<double>(ImGui::GetIO().DeltaTime);
        if (previewYaw >= 360.0) previewYaw -= 360.0;
        if (previewYaw < 0.0) previewYaw += 360.0;

        double yaw = previewYaw;
        SDK::AActor* actor = previewActor;
        GameHook::QueueAction([actor, yaw]() {
            if (actor) actor->K2_SetActorRotation(SDK::FRotator{0.0, yaw, 0.0}, true);
        });
    }

    void SpawnFromPassport() {
        if (cfg.livePreview) {
            cfg.livePreview = false;
            DestroyPreview();
        }

        bool hasOverrides = HasAnyRuntimeOverride();
        bool hasMesh = HasAnyMeshOverride();

        auto props = runtimeProps;
        auto meshSnap = hasMesh ? BuildMeshSnapshot() : MeshSnapshot{};

        auto callback = [this, props, hasOverrides, hasMesh, meshSnap](SDK::AActor* actor) {
            auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
            CollectMeshesFromWeapon(weapon);
            if (hasOverrides) ApplyRuntimeProps(actor, props);
            if (hasMesh) ApplyMeshOverrides(weapon, meshSnap);
        };

        Spawner::SpawnCustomizableFromPassport(world, weaponPassport, BuildSpawnTransform(), cfg.snapToGround, callback);
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
                if (GuiUtils::MatchesFilter(e.name.c_str(), e.name.size(), filterBuf, filterLen)) ++visible;
            ImGui::TextDisabled("Showing %d of %d", visible, static_cast<int>(options.size()));
        }

        ImGui::Separator();

        if (allowNone && ImGui::Selectable("None", current == nullptr))
            current = nullptr;

        char display[128];
        for (const auto& e : options) {
            if (hasFilter && !GuiUtils::MatchesFilter(e.name.c_str(), e.name.size(), filterBuf, filterLen))
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

    static void RenderFreeTierCombo(const char* label, SDK::Enum_Ranks& tier) {
        int val = static_cast<int>(tier);
        GuiUtils::RenderFreeTierCombo(label, val);
        tier = static_cast<SDK::Enum_Ranks>(val);
    }

    static void RenderValidatedTierCombo(const char* label, int& tier, uint16_t validMask) {
        tier = TierValidation::NearestValidTier(validMask, tier);

        ImGui::SetNextItemWidth(GuiUtils::CachedTierComboWidth());
        if (ImGui::BeginCombo(label, GuiUtils::TIER_LABELS[tier])) {
            for (int t = 0; t <= 8; ++t) {
                if (!(validMask & (1 << t))) continue;
                if (ImGui::Selectable(GuiUtils::TIER_LABELS[t], t == tier))
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

    void RenderGenerationControls() {
        BlueprintRegistry::Get().EnsureTiersScanned();
        ImGui::PushID("gen");

        static float weaponTypeComboW = GuiUtils::CalcComboWidth(WEAPON_TYPE_NAMES, WEAPON_TYPE_COUNT);
        ImGui::SetNextItemWidth(weaponTypeComboW);
        int typeIdx = cfg.weaponType - 1;
        if (ImGui::Combo("##Type", &typeIdx, WEAPON_TYPE_NAMES, WEAPON_TYPE_COUNT))
            cfg.weaponType = typeIdx + 1;
        TooltipHelper::ShowTooltip("Base weapon archetype that determines available modules and valid tiers");

        ImGui::SameLine();
        uint16_t weaponMask = TierValidation::VALID_TIER_MASKS[cfg.weaponType];
        RenderValidatedTierCombo("##GenTier", cfg.weaponTier, weaponMask);
        TooltipHelper::ShowTooltip("Quality tier - affects generated module selection and weapon stats");

        bool canGenerate = ComponentValidator::Validate(player) && ComponentValidator::Validate(world);

        ImGui::Spacing();
        if (!canGenerate) ImGui::BeginDisabled();
        if (ImGui::Button("Generate"))
            GenerateWeaponPassport();
        TooltipHelper::ShowTooltip("Generate weapon passport using selected type and tier");
        ImGui::SameLine();
        if (ImGui::Button("Randomize"))
            RandomizeWeaponPassport();
        TooltipHelper::ShowTooltip("Pick random type and tier, then generate");
        if (!canGenerate) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Reset"))
            CreateBlankWeaponPassport();
        TooltipHelper::ShowTooltip("Clear all passport data to blank defaults");

        if (weaponGenerationPending) {
            ImGui::SameLine();
            ImGui::TextDisabled("Generating...");
        } else if (weaponPassport.WeaponClass_54_B478ECF7499977809745A3973AD678EC) {
            auto className = PresetUtils::ClassToString(weaponPassport.WeaponClass_54_B478ECF7499977809745A3973AD678EC);
            if (!className.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", className.c_str());
            }
        }

        ImGui::PopID();
    }

    void RenderModulesTab() {
        ImGui::PushID("modules");

        if (!globalModules.populated.load(std::memory_order_acquire)) {
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
        if (globalModules.subMods1.empty() && globalModules.subMods2.empty())
            ImGui::TextDisabled("No sub-modules available for this weapon type");

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
        TooltipHelper::ShowTooltip("Scale and weight of the head component");
        RenderSizeMassRow("Guard",
            weaponPassport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704,
            weaponPassport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927);
        TooltipHelper::ShowTooltip("Scale and weight of the guard component");
        RenderSizeMassRow("Grip",
            weaponPassport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF,
            weaponPassport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10);
        TooltipHelper::ShowTooltip("Scale and weight of the grip component");
        RenderSizeMassRow("Pommel",
            weaponPassport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E,
            weaponPassport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173);
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

    void RenderAppearanceTab() {
        ImGui::PushID("appearance");

        ImGui::TextDisabled("Metal");
        RenderMaterialCombo("Steel", weaponPassport.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36);
        TooltipHelper::ShowTooltip("Primary metal surface finish");
        RenderMaterialCombo("Colored", weaponPassport.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2);
        TooltipHelper::ShowTooltip("Secondary metallic accent layer");

        ImGui::Spacing();
        ImGui::TextDisabled("Organic");
        RenderMaterialCombo("Wood", weaponPassport.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A);
        TooltipHelper::ShowTooltip("Wood grain pattern for handle and wooden parts");
        RenderColorEditor("Wood Color", weaponPassport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743);
        TooltipHelper::ShowTooltip("Tint color applied to wooden surfaces");

        ImGui::Spacing();
        RenderMaterialCombo("Leather", weaponPassport.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB);
        TooltipHelper::ShowTooltip("Leather wrap style for grip sections");
        RenderColorEditor("Leather Color", weaponPassport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638);
        TooltipHelper::ShowTooltip("Tint color applied to leather wrapping");

        ImGui::Spacing();
        if (ImGui::Button("Reset Appearance")) {
            weaponPassport.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36 = static_cast<SDK::Enum_MaterialLayer>(3);
            weaponPassport.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2 = static_cast<SDK::Enum_MaterialLayer>(0);
            weaponPassport.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A = static_cast<SDK::Enum_MaterialLayer>(14);
            weaponPassport.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB = static_cast<SDK::Enum_MaterialLayer>(10);
            weaponPassport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 = {0.4f, 0.26f, 0.13f, 1.0f};
            weaponPassport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 = {0.3f, 0.18f, 0.08f, 1.0f};
        }
        TooltipHelper::ShowTooltip("Reset all materials and colors to defaults");

        ImGui::PopID();
    }

    void RenderMeshCombo(int slotIdx) {
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

        const char* preview = (ovr.poolIndex >= 0 && ovr.poolIndex < static_cast<int>(meshPool.size()))
            ? meshPool[ovr.poolIndex].name.c_str() : "None";

        ImGui::SetNextItemWidth(meshComboWidth);
        if (ImGui::BeginCombo("Mesh", preview)) {
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
                if (hasFilter && !GuiUtils::MatchesFilter(meshPool[i].name.c_str(), meshPool[i].name.size(),
                    meshFilters[slotIdx], filterLen)) continue;
                ImGui::PushID(i);
                const char* tag = meshPool[i].type == MeshType::Skeletal ? "SK" : "SM";
                std::snprintf(display, sizeof(display), "%-36s [%s][%s]", meshPool[i].name.c_str(), meshPool[i].category, tag);
                if (ImGui::Selectable(display, ovr.poolIndex == i)) {
                    ovr.poolIndex = i;
                    ovr.mesh = meshPool[i].mesh;
                    ovr.meshType = meshPool[i].type;
                    if (previewActor) {
                        GameHook::QueueAction([this]() { ApplyMeshToPreview(); });
                    }
                }
                if (ovr.poolIndex == i) ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }

        if (ovr.mesh) {
            float s[3] = {static_cast<float>(ovr.scale.X), static_cast<float>(ovr.scale.Y), static_cast<float>(ovr.scale.Z)};
            ImGui::SetNextItemWidth(meshComboWidth * 0.6f);
            if (ImGui::DragFloat3("Scale", s, 0.01f, 0.01f, 10.0f, "%.2f")) {
                ovr.scale = {s[0], s[1], s[2]};
                if (previewActor) GameHook::QueueAction([this]() { ApplyMeshToPreview(); });
            }

            float r[3] = {static_cast<float>(ovr.rotation.Pitch), static_cast<float>(ovr.rotation.Yaw), static_cast<float>(ovr.rotation.Roll)};
            ImGui::SetNextItemWidth(meshComboWidth * 0.6f);
            if (ImGui::DragFloat3("Rotation", r, 1.0f, -180.0f, 180.0f, "%.1f")) {
                ovr.rotation = {r[0], r[1], r[2]};
                if (previewActor) GameHook::QueueAction([this]() { ApplyMeshToPreview(); });
            }

            float o[3] = {static_cast<float>(ovr.offset.X), static_cast<float>(ovr.offset.Y), static_cast<float>(ovr.offset.Z)};
            ImGui::SetNextItemWidth(meshComboWidth * 0.6f);
            if (ImGui::DragFloat3("Offset", o, 0.1f, -500.0f, 500.0f, "%.1f")) {
                ovr.offset = {o[0], o[1], o[2]};
                if (previewActor) GameHook::QueueAction([this]() { ApplyMeshToPreview(); });
            }

            ImGui::SameLine();
            if (ImGui::SmallButton("Reset")) {
                ovr.scale = {1.0, 1.0, 1.0};
                ovr.rotation = {0.0, 0.0, 0.0};
                ovr.offset = {0.0, 0.0, 0.0};
                if (previewActor) GameHook::QueueAction([this]() { ApplyMeshToPreview(); });
            }
        }

        if (!ovr.enabled) ImGui::EndDisabled();
    }

    void RenderMeshTab() {
        ImGui::PushID("mesh");

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
            meshPool.clear();
            meshSeen.clear();
            meshComboWidth = 0.0f;
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

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Load").x - ImGui::GetStyle().FramePadding.x * 2 - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputTextWithHint("##assetPath", "Asset path (e.g. Assets/Animals/Horse_001/SkeletalMeshes/SK_Animal_Horse_002)", assetPathBuf, sizeof(assetPathBuf));
        TooltipHelper::ShowTooltip("Full or partial UE asset path. Prefix /Game/ and suffix .AssetName are added automatically if missing");
        ImGui::SameLine();
        if (ImGui::Button("Load") && assetPathBuf[0]) {
            auto pathCopy = std::string(assetPathBuf);
            GameHook::QueueAction([this, pathCopy]() {
                auto* result = LoadAssetByPath(pathCopy.c_str());
                if (result) SetStatus("Loaded: " + std::string(result->GetName()));
                else SetStatus("Failed to load asset", true);
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

    void RenderStatsTab() {
        ImGui::PushID("stats");

        ImGui::TextDisabled("Passport");
        RenderFreeTierCombo("Tier", weaponPassport.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461);
        TooltipHelper::ShowTooltip("Stored tier value in the passport, independent of generation tier");
        GuiUtils::RenderPriceDrag("Price", weaponPassport.Price_60_83FE5A624EA188485BBE4E9C8606AEE5);
        TooltipHelper::ShowTooltip("Weapon price value stored in the passport");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("Runtime Overrides");
        TooltipHelper::ShowTooltip("Override weapon stats after spawning. Enable each to apply its value.");

        if (ImGui::Button("Reset All Overrides"))
            runtimeProps = {};
        TooltipHelper::ShowTooltip("Disable all runtime overrides");
        int activeCount = CountActiveOverrides();
        if (activeCount > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%d active)", activeCount);
        }

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
            GuiUtils::RenderOverrideInt("Sharp Level", runtimeProps.dismemberSharp, 0, 10);
            TooltipHelper::ShowTooltip("Sharp dismemberment threshold (higher = easier to sever)");
            GuiUtils::RenderOverrideInt("Blunt Level", runtimeProps.dismemberBlunt, 0, 10);
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

    WeaponPresetData BuildPresetData() const {
        WeaponPresetData d;
        d.name = presetNameBuf;
        d.passport = weaponPassport;
        d.runtimeProps = runtimeProps;

        for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
            d.meshPresets[i].enabled = meshOverrides[i].enabled;
            if (meshOverrides[i].enabled && meshOverrides[i].mesh) {
                d.meshPresets[i].meshName = meshOverrides[i].mesh->GetName();
                d.meshPresets[i].meshPath = ExtractAssetPath(meshOverrides[i].mesh->GetFullName());
            }
            d.meshPresets[i].meshType = meshOverrides[i].meshType;
            d.meshPresets[i].scale = meshOverrides[i].scale;
            d.meshPresets[i].rotation = meshOverrides[i].rotation;
            d.meshPresets[i].offset = meshOverrides[i].offset;
        }

        return d;
    }

    void ApplyPresetData(const WeaponPresetData& d) {
        weaponPassport = d.passport;
        runtimeProps = d.runtimeProps;

        for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
            meshOverrides[i].enabled = d.meshPresets[i].enabled;
            meshOverrides[i].mesh = nullptr;
            meshOverrides[i].poolIndex = -1;
            meshOverrides[i].meshType = d.meshPresets[i].meshType;
            meshOverrides[i].scale = d.meshPresets[i].scale;
            meshOverrides[i].rotation = d.meshPresets[i].rotation;
            meshOverrides[i].offset = d.meshPresets[i].offset;
            if (d.meshPresets[i].enabled && !d.meshPresets[i].meshName.empty()) {
                for (int j = 0; j < static_cast<int>(meshPool.size()); ++j) {
                    if (meshPool[j].name == d.meshPresets[i].meshName) {
                        meshOverrides[i].poolIndex = j;
                        meshOverrides[i].mesh = meshPool[j].mesh;
                        break;
                    }
                }
                if (!meshOverrides[i].mesh) {
                    SDK::UObject* found = nullptr;
                    if (d.meshPresets[i].meshType == MeshType::Skeletal)
                        found = SDK::UObject::FindObjectFast<SDK::USkeletalMesh>(d.meshPresets[i].meshName);
                    else
                        found = SDK::UObject::FindObjectFast<SDK::UStaticMesh>(d.meshPresets[i].meshName);
                    if (found) {
                        meshSeen.insert(found);
                        meshPool.push_back({found, d.meshPresets[i].meshName,
                            ExtractCategory(found->GetFullName()), d.meshPresets[i].meshType});
                        meshOverrides[i].poolIndex = static_cast<int>(meshPool.size()) - 1;
                        meshOverrides[i].mesh = found;
                        meshComboWidth = 0.0f;
                    }
                }
            }
        }

        struct PendingMeshLoad { int slot; std::string path; };
        std::vector<PendingMeshLoad> pending;
        for (int i = 0; i < MODULE_SLOT_COUNT; ++i) {
            if (meshOverrides[i].enabled && !meshOverrides[i].mesh && !d.meshPresets[i].meshPath.empty())
                pending.push_back({i, d.meshPresets[i].meshPath});
        }
        if (!pending.empty()) {
            GameHook::QueueAction([this, pending]() {
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

    void SetStatus(std::string msg, bool isError = false) {
        statusMessage = std::move(msg);
        statusMessageTime = ImGui::GetTime();
        statusIsError = isError;
    }

    void RefreshPresetList() {
        presetList = WeaponPresetSerializer::ListPresets();
        presetListDirty = false;
    }

    void RenderPresetsTab() {
        ImGui::PushID("presets");

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
                SetStatus("Error saving preset", true);
            }
        }
        TooltipHelper::ShowTooltip("Save current weapon configuration as a named preset");
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
            int visibleRows = (std::min)(static_cast<int>(presetList.size()), 8);
            float listHeight = ImGui::GetTextLineHeightWithSpacing() * visibleRows + ImGui::GetStyle().FramePadding.y * 2;
            ImGui::BeginChild("##presetList", ImVec2(0, listHeight), ImGuiChildFlags_Borders);

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
                if (textW > 0)
                    ImGui::SameLine(textW);
                if (ImGui::Button("Load")) {
                    auto result = WeaponPresetSerializer::LoadFromFile(presetList[i].path);
                    if (result.success) {
                        ApplyPresetData(result);
                        strncpy_s(presetNameBuf, result.name.c_str(), _TRUNCATE);
                        SetStatus("Loaded: " + result.name);
                    } else {
                        SetStatus("Error: " + result.error, true);
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

            ImGui::EndChild();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Open Presets Folder", ImVec2(-1, 0))) {
            PresetUtils::OpenInExplorer(WeaponPresetSerializer::GetPresetsDirectory());
        }

        ImGui::PopID();
    }

    void RenderSpawnFooter() {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool canSpawn = ComponentValidator::Validate(player) && ComponentValidator::Validate(world);
        if (!canSpawn) ImGui::BeginDisabled();
        if (ImGui::Button("Spawn Weapon", ImVec2(-1, 0)))
            SpawnFromPassport();
        TooltipHelper::ShowTooltip("Spawn the weapon with current settings. Disables live preview");
        if (!canSpawn) ImGui::EndDisabled();
    }

public:
    WeaponEditorSection() : CollapsibleSection("Weapon Editor") {
        CreateBlankWeaponPassport();

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

        if (!globalModules.populated.load(std::memory_order_acquire) && !modulePoolQueued) {
            modulePoolQueued = true;
            GameHook::QueueAction([this]() { PopulateGlobalModulePool(); });
        }

        if (GuiUtils::CheckboxWithTooltip("Live Preview", &cfg.livePreview, "Auto-spawn a preview weapon that updates as you edit")) {
            if (!cfg.livePreview)
                DestroyPreview();
        }
        if (cfg.livePreview) {
            ImGui::SameLine();
            GuiUtils::CheckboxWithTooltip("Auto-Rotate", &cfg.autoRotate, "Continuously rotate the preview weapon");
            if (cfg.autoRotate) {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
                ImGui::SliderFloat("Rotation Speed", &cfg.rotationSpeed, -360.0f, 360.0f, "%.0f deg/s");
                TooltipHelper::ShowTooltip("Rotation speed in degrees/second. Negative values reverse direction");
            }
        }

        if (!statusMessage.empty()) {
            if (ImGui::GetTime() - statusMessageTime > 3.0)
                statusMessage.clear();
            else {
                auto color = statusIsError ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
                ImGui::TextColored(color, "%s", statusMessage.c_str());
            }
        }

        ImGui::Spacing();
        if (ImGui::BeginTabBar("##WeaponEditorTabs")) {
            if (ImGui::BeginTabItem("Modules"))    { RenderModulesTab();    ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Geometry"))    { RenderGeometryTab();   ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Appearance"))  { RenderAppearanceTab(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Mesh"))        { RenderMeshTab();       ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Stats"))       { RenderStatsTab();      ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Presets"))     { RenderPresetsTab();    ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }

        RenderSpawnFooter();

        if (cfg.livePreview) {
            UpdatePreview();
            RotatePreview();
        }
    }
};
