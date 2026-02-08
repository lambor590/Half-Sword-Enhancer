#include <array>
#include <string_view>
#include <unordered_map>

#include "Utils/Spawner.h"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/Modular_Weapon_Grip_classes.hpp"
#include "SDK/Weapon_Sword_Grip_Master_classes.hpp"
#include "Hooks/GameHook.h"

namespace Spawner {

    namespace {
        std::unordered_map<std::string, SDK::UClass*> classCache;

        constexpr std::array<std::pair<std::string_view, ActorType>, 11> TYPE_MAP = {{
            {"Willie_BP", ActorType::Willie},
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

    static SDK::FStr_Passport_Weapon1 BuildPassportFromCDO(SDK::UClass* actorClass,
        SDK::AModularWeaponBP_C* weaponCDO, SDK::AModular_Weapon_Grip_C* gripCDO) {

        SDK::FStr_Passport_Weapon1 passport{};
        passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC = actorClass;
        passport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4 = weaponCDO->Grip_Module;

        if (gripCDO) {
            if (gripCDO->Array_Head.Num() > 0)
                passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 = gripCDO->Array_Head[0];
            else if (gripCDO->Module_Head)
                passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 = gripCDO->Module_Head;

            if (gripCDO->Array_Guard.Num() > 0)
                passport.GuardModule_13_6DD2B06245505E53B529D090333012F0 = gripCDO->Array_Guard[0];
            else if (gripCDO->Module_Guard)
                passport.GuardModule_13_6DD2B06245505E53B529D090333012F0 = gripCDO->Module_Guard;

            if (gripCDO->Array_Pommel.Num() > 0)
                passport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6 = gripCDO->Array_Pommel[0];
            else if (gripCDO->Module_Pommel)
                passport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6 = gripCDO->Module_Pommel;

            if (!passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139
                && gripCDO->IsA(SDK::AWeapon_Sword_Grip_Master_C::StaticClass())) {
                auto* swordGrip = static_cast<SDK::AWeapon_Sword_Grip_Master_C*>(gripCDO);
                if (swordGrip->Blades_1.Num() > 0)
                    passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 = swordGrip->Blades_1[0];
                else if (swordGrip->Blades_2.Num() > 0)
                    passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 = swordGrip->Blades_2[0];
                else if (swordGrip->Blades_3.Num() > 0)
                    passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 = swordGrip->Blades_3[0];
            }
        }

        passport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D = weaponCDO->Sub_Module_1_Class;
        passport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9 = weaponCDO->Sub_Module_2_Class;

        passport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57 = weaponCDO->Head_Size;
        passport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 = weaponCDO->Guard_Size;
        passport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF = weaponCDO->Grip_Size;
        passport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E = weaponCDO->Pommel_Size;

        passport.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7 = weaponCDO->Custom_Mass_Scale_Head;
        passport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927 = weaponCDO->Custom_Mass_Scale_Guard;
        passport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10 = weaponCDO->Custom_Mass_Scale_Grip;
        passport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173 = weaponCDO->Custom_Mass_Scale_Pommel;

        return passport;
    }

    static SDK::AActor* SpawnModularWeapon(const SDK::UWorld* world, SDK::UClass* actorClass, const SDK::FTransform& transform) {
        auto* weaponCDO = static_cast<SDK::AModularWeaponBP_C*>(actorClass->ClassDefaultObject);
        if (!weaponCDO || !weaponCDO->Grip_Module) return nullptr;

        auto* gripCDO = static_cast<SDK::AModular_Weapon_Grip_C*>(
            weaponCDO->Grip_Module->ClassDefaultObject);

        auto passport = BuildPassportFromCDO(actorClass, weaponCDO, gripCDO);

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
    }

    void SpawnActor(const SDK::UWorld* world, const std::string& className, const SDK::FTransform& transform, std::function<void(SDK::AActor*)> callback, bool snapToGround) {
        GameHook::QueueAction([world, className, transform, callback, snapToGround]() {
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
                spawnedActor = SpawnModularWeapon(world, actorClass, finalTransform);
            }

            if (!spawnedActor) {
                spawnedActor = SpawnActorInternal(world, actorClass, finalTransform);
            }

            if (callback) {
                callback(spawnedActor);
            }
        });
    }

}
