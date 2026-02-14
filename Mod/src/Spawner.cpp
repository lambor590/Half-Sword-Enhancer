#include <array>
#include <string_view>
#include <unordered_map>

#include "Utils/Spawner.h"
#include "Utils/EquipmentGenerator.h"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/ModularWeaponBP_Customizable_classes.hpp"
#include "SDK/BP_Armor_Master_classes.hpp"
#include "Hooks/GameHook.h"

namespace Spawner {

    namespace {
        std::unordered_map<std::string, SDK::UClass*> classCache;

        constexpr std::array<std::pair<std::string_view, ActorType>, 12> TYPE_MAP = {{
            {"Willie_BP", ActorType::Willie},
            {"Customizable", ActorType::Weapon},
            {"ModularWeapon", ActorType::Weapon},
            {"BP_Weapon_Reforged", ActorType::Weapon},
            {"BP_Weapon_Ranged", ActorType::Weapon},
            {"BP_Weapon_Treasure", ActorType::Weapon},
            {"BP_Weapon_Improv", ActorType::Weapon},
            {"Shield", ActorType::Shield},
            {"Dagger", ActorType::Tool},
            {"Tool", ActorType::Tool},
            {"BP_Modular", ActorType::Armor},
            {"Armor", ActorType::Armor}
        }};

        constexpr std::array<float, 6> GROUND_OFFSETS = {{
            50.0f,  // Willie
            5.0f,   // Weapon
            5.0f,   // Shield
            3.0f,   // Tool
            5.0f,   // Armor
            10.0f   // Unknown
        }};
    }

    ActorType GetActorType(const std::string& classPath) {
        std::string_view pathView(classPath);
        for (const auto& [key, type] : TYPE_MAP) {
            if (pathView.find(key) != std::string_view::npos) {
                return type;
            }
        }
        return ActorType::Unknown;
    }

    float GetGroundOffsetForType(ActorType type, const SDK::FVector& scale) {
        float baseOffset = GROUND_OFFSETS[static_cast<size_t>(type)];
        return baseOffset * (static_cast<float>(scale.Z) * 1.2f);
    }

    static SDK::UClass* LoadActorClass(const std::string& classPath) {
        auto it = classCache.find(classPath);
        if (it != classCache.end())
            return it->second;

        std::wstring wClassName(classPath.begin(), classPath.end());
        SDK::FString classPathFStr(wClassName.c_str());
        SDK::FSoftClassPath softClassPath = SDK::UKismetSystemLibrary::MakeSoftClassPath(classPathFStr);
        SDK::TSoftClassPtr<SDK::UClass> softClassRef = SDK::UKismetSystemLibrary::Conv_SoftClassPathToSoftClassRef(softClassPath);
        SDK::UClass* loaded = SDK::UKismetSystemLibrary::LoadClassAsset_Blocking(softClassRef);

        if (loaded)
            classCache[classPath] = loaded;
        return loaded;
    }

    static SDK::AActor* SpawnModularWeapon(const SDK::UWorld* world, SDK::UClass* actorClass, const SDK::FTransform& transform, SDK::Enum_Ranks tier) {
        EquipmentGenerator::Init(world);
        auto passport = EquipmentGenerator::GenerateSpecificWeapon(actorClass, tier);

        auto* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
            world, actorClass, transform,
            SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
            nullptr, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);
        if (!actor) return nullptr;

        auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
        weapon->Weapon_Passport = passport;

        SDK::UGameplayStatics::FinishSpawningActor(actor, transform, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);

        return actor;
    }

    static SDK::AActor* SpawnModularArmor(const SDK::UWorld* world, SDK::UClass* actorClass, const SDK::FTransform& transform) {
        SDK::FStr_Passport_Armor1 passport{};
        passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = actorClass;

        auto* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
            world, actorClass, transform,
            SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
            nullptr, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);
        if (!actor) return nullptr;

        auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(actor);
        armor->Armor_Passport = passport;

        SDK::UGameplayStatics::FinishSpawningActor(actor, transform, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);
        return actor;
    }

    static SDK::AActor* SpawnWeaponWithPassport(const SDK::UWorld* world, const SDK::FStr_Passport_Weapon1& passport, const SDK::FTransform& transform) {
        SDK::UClass* weaponClass = passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC;
        if (!weaponClass) return nullptr;

        auto* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
            world, weaponClass, transform,
            SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
            nullptr, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);
        if (!actor) return nullptr;

        auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
        weapon->Weapon_Passport = passport;

        SDK::UGameplayStatics::FinishSpawningActor(actor, transform, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);

        return actor;
    }

    static SDK::AActor* SpawnActorInternal(const SDK::UWorld* world, SDK::UClass* actorClass, const SDK::FTransform& transform) {
        SDK::AActor* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
            world,
            actorClass,
            transform,
            SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
            nullptr,
            SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime
        );

        SDK::UGameplayStatics::FinishSpawningActor(actor, transform, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);
        return actor;
    }

    SDK::FVector GetGroundPosition(const SDK::UWorld* world, SDK::FVector position, float groundOffset, float traceDistance) {
        SDK::FHitResult hitResult;
        SDK::FVector startPos = position;
        startPos.Z += traceDistance * 0.5f;
        SDK::FVector endPos = position;
        endPos.Z -= traceDistance * 0.5f;

        SDK::TArray<SDK::AActor*> actorsToIgnore;
        SDK::TArray<SDK::EObjectTypeQuery> objectTypes;
        objectTypes.Add(SDK::EObjectTypeQuery::ObjectTypeQuery1);

        bool hit = SDK::UKismetSystemLibrary::LineTraceSingleForObjects(
            world,
            startPos,
            endPos,
            objectTypes,
            true,
            actorsToIgnore,
            SDK::EDrawDebugTrace::None,
            &hitResult,
            true,
            SDK::FLinearColor(1.0f, 0.0f, 0.0f, 1.0f),
            SDK::FLinearColor(0.0f, 1.0f, 0.0f, 1.0f),
            1.0f
        );

        if (hit && hitResult.bBlockingHit) {
            SDK::FVector groundPos = hitResult.Location;
            groundPos.Z += groundOffset;
            return groundPos;
        }

        SDK::FVector groundLevel = position;
        groundLevel.Z += groundOffset;
        return groundLevel;
    }

    void ClearCache() {
        classCache.clear();
        EquipmentGenerator::ClearCache();
    }

    void SpawnActor(const SDK::UWorld* world, const std::string& className, const SDK::FTransform& transform, std::function<void(SDK::AActor*)> callback, bool snapToGround, int tier) {
        GameHook::QueueAction([world, className, transform, callback, snapToGround, tier]() {
            SDK::FTransform finalTransform = transform;
            ActorType actorType = GetActorType(className);

            if (snapToGround) {
                float groundOffset = GetGroundOffsetForType(actorType, transform.Scale3D);
                SDK::FVector groundPosition = GetGroundPosition(world, transform.Translation, groundOffset);
                finalTransform.Translation = groundPosition;
            }

            SDK::UClass* actorClass = LoadActorClass(className);
            SDK::AActor* spawnedActor = nullptr;

            if (actorType == ActorType::Weapon) {
                spawnedActor = SpawnModularWeapon(world, actorClass, finalTransform, static_cast<SDK::Enum_Ranks>(tier));
            } else if (actorType == ActorType::Armor) {
                spawnedActor = SpawnModularArmor(world, actorClass, finalTransform);
            }

            if (!spawnedActor) {
                spawnedActor = SpawnActorInternal(world, actorClass, finalTransform);
            }

            if (callback) {
                callback(spawnedActor);
            }
        });
    }

    void SpawnWeaponFromPassport(const SDK::UWorld* world, const SDK::FStr_Passport_Weapon1& passport, const SDK::FTransform& transform, bool snapToGround) {
        GameHook::QueueAction([world, passport, transform, snapToGround]() {
            SDK::FTransform finalTransform = transform;

            if (snapToGround) {
                float groundOffset = GetGroundOffsetForType(ActorType::Weapon, transform.Scale3D);
                SDK::FVector groundPosition = GetGroundPosition(world, transform.Translation, groundOffset);
                finalTransform.Translation = groundPosition;
            }

            SpawnWeaponWithPassport(world, passport, finalTransform);
        });
    }

    static SDK::AActor* SpawnArmorWithPassport(const SDK::UWorld* world, const SDK::FStr_Passport_Armor1& passport, const SDK::FTransform& transform) {
        SDK::UClass* armorClass = passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
        if (!armorClass) return nullptr;

        auto* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
            world, armorClass, transform,
            SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
            nullptr, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);
        if (!actor) return nullptr;

        auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(actor);
        armor->Armor_Passport = passport;

        SDK::UGameplayStatics::FinishSpawningActor(actor, transform, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);
        return actor;
    }

    void SpawnArmorFromPassport(const SDK::UWorld* world, const SDK::FStr_Passport_Armor1& passport, const SDK::FTransform& transform, bool snapToGround) {
        GameHook::QueueAction([world, passport, transform, snapToGround]() {
            SDK::FTransform finalTransform = transform;

            if (snapToGround) {
                float groundOffset = GetGroundOffsetForType(ActorType::Armor, transform.Scale3D);
                SDK::FVector groundPosition = GetGroundPosition(world, transform.Translation, groundOffset);
                finalTransform.Translation = groundPosition;
            }

            SpawnArmorWithPassport(world, passport, finalTransform);
        });
    }

    SDK::UClass* LoadClass(const std::string& classPath) {
        return LoadActorClass(classPath);
    }

    void SpawnCustomizableFromPassport(const SDK::UWorld* world, const SDK::FStr_Passport_Weapon1& passport, const SDK::FTransform& transform, bool snapToGround, std::function<void(SDK::AActor*)> callback) {
        GameHook::QueueAction([world, passport, transform, snapToGround, callback]() {
            SDK::FTransform finalTransform = transform;

            if (snapToGround) {
                float groundOffset = GetGroundOffsetForType(ActorType::Weapon, transform.Scale3D);
                finalTransform.Translation = GetGroundPosition(world, transform.Translation, groundOffset);
            }

            SDK::UClass* weaponClass = SDK::AModularWeaponBP_Customizable_C::StaticClass();
            auto* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
                world, weaponClass, finalTransform,
                SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
                nullptr, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);
            if (!actor) return;

            static_cast<SDK::AModularWeaponBP_Customizable_C*>(actor)->Weapon_Passport = passport;
            SDK::UGameplayStatics::FinishSpawningActor(actor, finalTransform, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);

            if (callback) callback(actor);
        });
    }

    void SpawnCustomizableWeapon(const SDK::UWorld* world, CustomizableWeapon type, const SDK::FTransform& transform, bool snapToGround, int tier) {
        GameHook::QueueAction([world, type, transform, snapToGround, tier]() {
            SDK::FTransform finalTransform = transform;

            if (snapToGround) {
                float groundOffset = GetGroundOffsetForType(ActorType::Weapon, transform.Scale3D);
                finalTransform.Translation = GetGroundPosition(world, transform.Translation, groundOffset);
            }

            EquipmentGenerator::Init(world);
            auto passport = EquipmentGenerator::GenerateCustomizableWeapon(type, static_cast<SDK::Enum_Ranks>(tier));

            SDK::UClass* weaponClass = SDK::AModularWeaponBP_Customizable_C::StaticClass();
            auto* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
                world, weaponClass, finalTransform,
                SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
                nullptr, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);
            if (!actor) return;

            static_cast<SDK::AModularWeaponBP_Customizable_C*>(actor)->Weapon_Passport = passport;
            SDK::UGameplayStatics::FinishSpawningActor(actor, finalTransform, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime);
        });
    }

}
