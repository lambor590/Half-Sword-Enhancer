#include <array>
#include <string_view>
#include <unordered_map>

#include "Utils/Spawner.h"
#include "Utils/AssetOverrideManager.h"
#include "Utils/BoneControl.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/GameClass.h"
#include "Utils/GameConstants.h"
#include "Utils/PresetUtils.h"
#include "Utils/WeaponPresetSerializer.h"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/ModularWeaponBP_Customizable_classes.hpp"
#include "SDK/BP_Armor_Master_classes.hpp"
#include "SDK/BP_Armor_Modular_Core_Master_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"
#include "Hooks/GameHook.h"
#include "Logger.h"

static Logger g_logger("Spawner");

namespace Spawner {

    namespace {
        void InitializeArmorFromPassport(
            SDK::AActor* actor, const SDK::FStr_Passport_Armor1& passport, const SDK::FTransform& transform
        ) {
            auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(actor);
            const auto actorOwnedBlockedSlots = armor->Armor_Passport.SlotsBlocked_45_0807340240E57ACE5A59D39F5E998F51;
            armor->Armor_Passport = passport;
            armor->Armor_Passport.SlotsBlocked_45_0807340240E57ACE5A59D39F5E998F51 = actorOwnedBlockedSlots;
            armor->World_Transform = transform;
            if (!GameClass::IsModularArmor(actor)) return;

            auto* modular = static_cast<SDK::ABP_Armor_Modular_Core_Master_C*>(actor);
            modular->Tier = passport.Tier_50_E497AE434B01B84C559DEE8A863BB42E;
            modular->Installed_Module_1_Int = passport.Module1_5_46B7198E4341C93CBF6AE989EF9898E4;
            modular->Installed_Module_2_Int = passport.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0;
            modular->Installed_Module_3_Int = passport.Module3_9_E282C465414F6D4EF2A8039FBA847AD2;
            modular->Core_Removed = passport.CoreRemoved_12_5CFF8F6D4A05C15812594CAF6771C66B;
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
            static std::unordered_map<std::string, SDK::UClass*> classCache;
            if (const auto cached = classCache.find(classPath); cached != classCache.end()) return cached->second;

            std::wstring wideClassPath;
            if (!PresetUtils::TryUtf8ToWide(classPath, wideClassPath)) return nullptr;
            const auto softPath = SDK::UKismetSystemLibrary::MakeSoftClassPath(SDK::FString(wideClassPath.c_str()));
            const auto softClass = SDK::UKismetSystemLibrary::Conv_SoftClassPathToSoftClassRef(softPath);
            auto* loaded = SDK::UKismetSystemLibrary::LoadClassAsset_Blocking(softClass);
            if (loaded) classCache.emplace(classPath, loaded);
            return loaded;
        }

        void ApplySnapToGround(const SDK::UWorld* world, SDK::FTransform& transform, ActorType type) {
            const float groundOffset = GROUND_OFFSETS[static_cast<size_t>(type)] *
                                       (static_cast<float>(transform.Scale3D.Z) * 1.2f);
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

    SDK::AActor* DeferredSpawn(
        const SDK::UWorld* world, SDK::UClass* actorClass, const SDK::FTransform& transform,
        const std::function<void(SDK::AActor*)>& preFinishCallback, SDK::ESpawnActorScaleMethod scaleMethod
    ) {
        auto* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
            world, actorClass, transform, SDK::ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
            nullptr, scaleMethod
        );
        if (!actor) return nullptr;

        if (preFinishCallback) preFinishCallback(actor);

        auto* finishedActor = SDK::UGameplayStatics::FinishSpawningActor(actor, transform, scaleMethod);
        if (!finishedActor) {
            if (!actor->IsActorBeingDestroyed()) actor->K2_DestroyActor();
            return nullptr;
        }
        actor = finishedActor;
        if (GameClass::IsWillie(actor)) {
            BoneControl::MarkSpawnedWillie(static_cast<SDK::AWillie_BP_C*>(actor));
        }
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

    SDK::AActor* SpawnActor(
        const SDK::UWorld* world, const std::string& className, const SDK::FTransform& transform,
        const std::function<void(SDK::AActor*)>& callback, bool snapToGround, SDK::Enum_Ranks tier,
        const std::function<void(SDK::AActor*)>& postSpawnCallback, SDK::Enum_WeaponType_Specific weaponSpecificType
    ) {
        ActorType actorType = GetActorType(className);
        SDK::UClass* actorClass = LoadActorClass(className);
        if (!actorClass) {
            g_logger.Log("Failed to load class: %s", className.c_str());
            return nullptr;
        }

        if (actorType == ActorType::Weapon && !GameClass::IsSubclassOf(actorClass, "ModularWeaponBP_C"))
            actorType = ActorType::Unknown;
        if (actorType == ActorType::Armor && !GameClass::IsSubclassOf(actorClass, "BP_Armor_Master_C"))
            actorType = ActorType::Unknown;
        if (actorType == ActorType::Willie && !GameClass::IsWillieClass(actorClass)) actorType = ActorType::Unknown;

        if (actorType == ActorType::Weapon) {
            if (className.find("Built_Weapons") == std::string::npos) {
                auto passport = EquipmentGenerator::GenerateSpecificWeapon(world, actorClass, tier, weaponSpecificType);
                return SpawnCustomizableFromPassport(world, passport, transform, snapToGround, callback);
            }

            constexpr std::string_view ROOT_TEMPLATE_PREFIX = "/Blueprints/Built_Weapons/ModularWeaponBP_";
            const std::string_view classPath = className;
            const size_t rootTemplatePos = classPath.find(ROOT_TEMPLATE_PREFIX);
            if (rootTemplatePos != std::string_view::npos &&
                classPath.find('/', rootTemplatePos + ROOT_TEMPLATE_PREFIX.size()) == std::string_view::npos) {
                return nullptr;
            }
        }

        SDK::FTransform finalTransform = transform;
        if (snapToGround) ApplySnapToGround(world, finalTransform, actorType);

        SDK::AActor* spawnedActor = nullptr;

        if (actorType == ActorType::Armor) {
            const auto* defaults = static_cast<SDK::ABP_Armor_Master_C*>(actorClass->ClassDefaultObject);
            if (!defaults) return nullptr;
            auto passport = defaults->Armor_Passport;
            passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = actorClass;
            spawnedActor =
                DeferredSpawn(world, actorClass, finalTransform, [&passport, &finalTransform](SDK::AActor* a) {
                    InitializeArmorFromPassport(a, passport, finalTransform);
                });
        } else if (actorType == ActorType::Willie) {
            SDK::AActor* actor = DeferredSpawn(
                world, actorClass, finalTransform, callback, SDK::ESpawnActorScaleMethod::OverrideRootScale
            );
            if (actor && postSpawnCallback) postSpawnCallback(actor);
            return actor;
        } else {
            spawnedActor = DeferredSpawn(world, actorClass, finalTransform);
        }

        if (callback && spawnedActor) callback(spawnedActor);
        return spawnedActor;
    }

    SDK::AActor* SpawnArmorFromPassport(
        const SDK::UWorld* world, const SDK::FStr_Passport_Armor1& passport, const SDK::FTransform& transform,
        bool snapToGround, const std::function<void(SDK::AActor*)>& callback
    ) {
        SDK::UClass* armorClass = passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
        if (!world || !armorClass || !GameClass::IsArmor(armorClass->ClassDefaultObject)) return nullptr;

        SDK::FTransform finalTransform = transform;
        if (snapToGround) ApplySnapToGround(world, finalTransform, ActorType::Armor);

        auto* actor = DeferredSpawn(world, armorClass, finalTransform, [&passport, &finalTransform](SDK::AActor* a) {
            InitializeArmorFromPassport(a, passport, finalTransform);
        });
        if (callback && actor) callback(actor);
        return actor;
    }

    SDK::UClass* LoadClass(const std::string& classPath) {
        return LoadActorClass(classPath);
    }

    SDK::AActor* SpawnCustomizableFromPassport(
        const SDK::UWorld* world, const SDK::FStr_Passport_Weapon1& passport, const SDK::FTransform& transform,
        bool snapToGround, const std::function<void(SDK::AActor*)>& callback
    ) {
        if (!EquipmentGenerator::IsPassportValid(passport)) return nullptr;

        SDK::FTransform finalTransform = transform;
        if (snapToGround) ApplySnapToGround(world, finalTransform, ActorType::Weapon);

        auto* weaponClass = LoadActorClass(GameConstants::CUSTOMIZABLE_WEAPON_BP_PATH);
        if (!GameClass::IsSubclassOf(weaponClass, "ModularWeaponBP_Customizable_C")) return nullptr;
        auto* actor = DeferredSpawn(world, weaponClass, finalTransform, [&passport](SDK::AActor* a) {
            static_cast<SDK::AModularWeaponBP_C*>(a)->Weapon_Passport = passport;
        });

        if (callback && actor) callback(actor);
        return actor;
    }

    bool SpawnAndEquipArmor(
        const SDK::UWorld* world, SDK::AWillie_BP_C* willie, const SDK::FStr_Passport_Armor1& passport,
        const std::function<void(SDK::AActor*)>& onSpawned
    ) {
        auto* armorClass = passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
        if (!world || !willie || !armorClass || !GameClass::IsArmor(armorClass->ClassDefaultObject)) return false;

        SDK::FTransform transform{};
        transform.Scale3D = {1.0, 1.0, 1.0};

        auto* actor = DeferredSpawn(world, armorClass, transform, [&passport, &transform](SDK::AActor* a) {
            InitializeArmorFromPassport(a, passport, transform);
        });
        if (!actor) return false;

        if (onSpawned) onSpawned(actor);

        auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(actor);
        bool pickedUp = false;
        willie->Pick_Up_Armor(armor->DefaultSceneRoot, armor, &pickedUp);
        if (!pickedUp) {
            actor->K2_DestroyActor();
            return false;
        }

        AssetOverrideManager::Get().RequestActorApply(willie);
        return true;
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

}
