#include <array>
#include <string_view>
#include <unordered_map>

#include "Utils/Spawner.h"
#include "Utils/AssetOverrideManager.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/WeaponPresetSerializer.h"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/ModularWeaponBP_Customizable_classes.hpp"
#include "SDK/BP_Armor_Master_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"
#include "Hooks/GameHook.h"
#include "Logger.h"

static Logger g_logger("Spawner");

namespace Spawner {

    namespace {
        const SDK::UWorld* cachedWorld = nullptr;

        std::unordered_map<std::string, SDK::UClass*>& ClassCache() {
            static std::unordered_map<std::string, SDK::UClass*> classCache;
            return classCache;
        }

        constexpr std::array<std::pair<std::string_view, ActorType>, 14> TYPE_MAP = {
            {{"Willie_BP", ActorType::Willie},
             {"Customizable", ActorType::Weapon},
             {"ModularWeapon", ActorType::Weapon},
             {"BP_Weapon_Reforged", ActorType::Weapon},
             {"Projectle", ActorType::Unknown},
             {"BP_Weapon_Ranged", ActorType::Weapon},
             {"BP_Weapon_Treasure", ActorType::Weapon},
             {"BP_Weapon_Improv", ActorType::Weapon},
             {"BP_GameWeapon", ActorType::Weapon},
             {"Shield", ActorType::Shield},
             {"Dagger", ActorType::Tool},
             {"Tool", ActorType::Tool},
             {"BP_Modular", ActorType::Armor},
             {"Armor", ActorType::Armor}}
        };

        constexpr std::array<float, 6> GROUND_OFFSETS = {{
            50.0f, // Willie
            5.0f,  // Weapon
            5.0f,  // Shield
            3.0f,  // Tool
            5.0f,  // Armor
            10.0f  // Unknown
        }};

        SDK::UClass* LoadActorClass(const std::string& classPath) {
            auto& classCache = ClassCache();
            auto [it, inserted] = classCache.try_emplace(classPath, nullptr);
            if (!inserted) return it->second;

            std::wstring wClassName(classPath.begin(), classPath.end());
            SDK::FString classPathFStr(wClassName.c_str());
            SDK::FSoftClassPath softClassPath = SDK::UKismetSystemLibrary::MakeSoftClassPath(classPathFStr);
            SDK::TSoftClassPtr<SDK::UClass> softClassRef =
                SDK::UKismetSystemLibrary::Conv_SoftClassPathToSoftClassRef(softClassPath);
            it->second = SDK::UKismetSystemLibrary::LoadClassAsset_Blocking(softClassRef);
            return it->second;
        }

        void ApplySnapToGround(const SDK::UWorld* world, SDK::FTransform& transform, ActorType type) {
            float groundOffset = GetGroundOffsetForType(type, transform.Scale3D);
            transform.Translation = GetGroundPosition(world, transform.Translation, groundOffset);
        }
    }

    ActorType GetActorType(std::string_view classPath) {
        for (const auto& [key, type] : TYPE_MAP) {
            if (classPath.find(key) != std::string_view::npos) {
                return type;
            }
        }
        return ActorType::Unknown;
    }

    float GetGroundOffsetForType(ActorType type, const SDK::FVector& scale) {
        float baseOffset = GROUND_OFFSETS[static_cast<size_t>(type)];
        return baseOffset * (static_cast<float>(scale.Z) * 1.2f);
    }

    SDK::AActor* DeferredSpawn(
        const SDK::UWorld* world, SDK::UClass* actorClass, const SDK::FTransform& transform,
        const std::function<void(SDK::AActor*)>& preFinishCallback
    ) {
        auto* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
            world, actorClass, transform, SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
            nullptr, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime
        );
        if (!actor) return nullptr;

        if (preFinishCallback) preFinishCallback(actor);

        auto* finishedActor = SDK::UGameplayStatics::FinishSpawningActor(
            actor, transform, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime
        );
        if (finishedActor) actor = finishedActor;
        AssetOverrideManager::Get().RequestActorApply(actor);
        return actor;
    }

    SDK::FVector GetGroundPosition(
        const SDK::UWorld* world, const SDK::FVector& position, float groundOffset, float traceDistance
    ) {
        SDK::FHitResult hitResult;
        SDK::FVector startPos = position;
        startPos.Z += traceDistance * 0.5f;
        SDK::FVector endPos = position;
        endPos.Z -= traceDistance * 0.5f;

        SDK::TArray<SDK::AActor*> actorsToIgnore;
        SDK::TArray<SDK::EObjectTypeQuery> objectTypes;
        objectTypes.Add(SDK::EObjectTypeQuery::ObjectTypeQuery1);

        bool hit = SDK::UKismetSystemLibrary::LineTraceSingleForObjects(
            world, startPos, endPos, objectTypes, true, actorsToIgnore, SDK::EDrawDebugTrace::None, &hitResult, true,
            SDK::FLinearColor(1.0f, 0.0f, 0.0f, 1.0f), SDK::FLinearColor(0.0f, 1.0f, 0.0f, 1.0f), 1.0f
        );

        if (hit) {
            SDK::FVector groundPos = hitResult.Location;
            groundPos.Z += groundOffset;
            return groundPos;
        }

        SDK::FVector groundLevel = position;
        groundLevel.Z += groundOffset;
        return groundLevel;
    }

    void ClearCache() {
        ClassCache().clear();
        cachedWorld = nullptr;
        EquipmentGenerator::ClearCache();
    }

    void SpawnActor(
        const SDK::UWorld* world, const std::string& className, const SDK::FTransform& transform,
        const std::function<void(SDK::AActor*)>& callback, bool snapToGround, SDK::Enum_Ranks tier,
        const std::function<void(SDK::AActor*)>& postSpawnCallback
    ) {
        ActorType actorType = GetActorType(className);
        if (world && cachedWorld != world) {
            ClassCache().clear();
            EquipmentGenerator::ClearCache();
            cachedWorld = world;
        }

        SDK::UClass* actorClass = LoadActorClass(className);
        if (!actorClass) {
            g_logger.Log("Failed to load class: %s", className.c_str());
            return;
        }

        if (actorType == ActorType::Weapon && !actorClass->IsSubclassOf(SDK::AModularWeaponBP_C::StaticClass()))
            actorType = ActorType::Unknown;
        if (actorType == ActorType::Armor && !actorClass->IsSubclassOf(SDK::ABP_Armor_Master_C::StaticClass()))
            actorType = ActorType::Unknown;

        if (actorType == ActorType::Weapon) {
            if (className.find("Built_Weapons") != std::string::npos) {
                const std::string_view classPath = className;
                constexpr std::string_view ROOT_TEMPLATE_PREFIX = "/Blueprints/Built_Weapons/ModularWeaponBP_";
                const size_t rootTemplatePos = classPath.find(ROOT_TEMPLATE_PREFIX);
                if (rootTemplatePos != std::string_view::npos &&
                    classPath.find('/', rootTemplatePos + ROOT_TEMPLATE_PREFIX.size()) == std::string_view::npos) {
                    g_logger.Log("Skipping non-spawnable built weapon template: %s", className.c_str());
                    return;
                }
            } else {
                auto passport = EquipmentGenerator::GenerateSpecificWeapon(world, actorClass, tier);
                SpawnCustomizableFromPassport(world, passport, transform, snapToGround, callback);
                return;
            }
        }

        SDK::FTransform finalTransform = transform;
        if (snapToGround) ApplySnapToGround(world, finalTransform, actorType);

        SDK::AActor* spawnedActor = nullptr;

        if (actorType == ActorType::Armor) {
            SDK::FStr_Passport_Armor1 passport{};
            passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = actorClass;
            spawnedActor =
                DeferredSpawn(world, actorClass, finalTransform, [&passport, &finalTransform](SDK::AActor* a) {
                    auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(a);
                    armor->Armor_Passport = passport;
                    armor->World_Transform = finalTransform;
                });
        } else if (actorType == ActorType::Willie) {
            SDK::AActor* actor = DeferredSpawn(world, actorClass, finalTransform, callback);
            if (actor && postSpawnCallback) postSpawnCallback(actor);
            return;
        } else {
            spawnedActor = DeferredSpawn(world, actorClass, finalTransform);
        }

        if (callback && spawnedActor) callback(spawnedActor);
    }

    void SpawnArmorFromPassport(
        const SDK::UWorld* world, const SDK::FStr_Passport_Armor1& passport, const SDK::FTransform& transform,
        bool snapToGround, const std::function<void(SDK::AActor*)>& callback
    ) {
        SDK::UClass* armorClass = passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
        if (!armorClass) return;

        SDK::FTransform finalTransform = transform;
        if (snapToGround) ApplySnapToGround(world, finalTransform, ActorType::Armor);

        auto* actor = DeferredSpawn(world, armorClass, finalTransform, [&passport, &finalTransform](SDK::AActor* a) {
            auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(a);
            armor->Armor_Passport = passport;
            armor->World_Transform = finalTransform;
        });
        if (callback && actor) callback(actor);
    }

    SDK::UClass* LoadClass(const std::string& classPath) {
        return LoadActorClass(classPath);
    }

    void SpawnCustomizableFromPassport(
        const SDK::UWorld* world, const SDK::FStr_Passport_Weapon1& passport, const SDK::FTransform& transform,
        bool snapToGround, const std::function<void(SDK::AActor*)>& callback
    ) {
        if (!EquipmentGenerator::IsPassportValid(passport)) return;

        SDK::FTransform finalTransform = transform;
        if (snapToGround) ApplySnapToGround(world, finalTransform, ActorType::Weapon);

        SDK::UClass* weaponClass = SDK::AModularWeaponBP_Customizable_C::StaticClass();
        auto* actor = DeferredSpawn(world, weaponClass, finalTransform, [&passport](SDK::AActor* a) {
            static_cast<SDK::AModularWeaponBP_Customizable_C*>(a)->Weapon_Passport = passport;
        });

        if (callback) callback(actor);
    }

    bool SpawnAndEquipArmor(
        const SDK::UWorld* world, SDK::AWillie_BP_C* willie, const SDK::FStr_Passport_Armor1& passport
    ) {
        auto* armorClass = passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
        if (!armorClass) return false;

        SDK::FTransform transform{};
        transform.Scale3D = {1.0, 1.0, 1.0};

        auto* actor = DeferredSpawn(world, armorClass, transform, [&passport, &transform](SDK::AActor* a) {
            auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(a);
            armor->Armor_Passport = passport;
            armor->World_Transform = transform;
        });
        if (!actor) return false;

        auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(actor);
        bool pickedUp = false;
        willie->Pick_Up_Armor(armor->DefaultSceneRoot, armor, &pickedUp);
        if (pickedUp) AssetOverrideManager::Get().RequestActorApply(willie);
        return pickedUp;
    }

    SDK::FTransform BuildSpawnTransform(
        SDK::AWillie_BP_C* player, float distanceForward, float distanceUp, float scale
    ) {
        auto transform = player->GetTransform();
        const auto forward = player->GetActorForwardVector();
        transform.Translation.X += forward.X * distanceForward;
        transform.Translation.Y += forward.Y * distanceForward;
        transform.Translation.Z += distanceUp;
        transform.Scale3D = {scale, scale, scale};
        return transform;
    }

    SDK::FTransform BuildSpawnTransform(SDK::AWillie_BP_C* player, const SpawnConfig& cfg) {
        return BuildSpawnTransform(player, cfg.distanceForward, cfg.distanceUp, cfg.scale);
    }

    void LoadWeaponClasses(SDK::FStr_Passport_Weapon1& passport, const WeaponClassPaths& paths) {
        auto load = [](SDK::UClass*& target, const std::string& path) {
            target = path.empty() ? nullptr : LoadClass(path);
        };
        load(passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC, paths.weaponClass);
        load(passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139, paths.headModule);
        load(passport.GuardModule_13_6DD2B06245505E53B529D090333012F0, paths.guardModule);
        load(passport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4, paths.gripModule);
        load(passport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6, paths.pommelModule);
        load(passport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D, paths.subModule1);
        load(passport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9, paths.subModule2);
    }

}
