#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

#include "Menu/SectionConfig.h"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/Enum_WeaponType_Specific_structs.hpp"
#include "SDK/Str_Passport_Weapon1_structs.hpp"
#include "SDK/Str_Passport_Armor1_structs.hpp"

namespace SDK {
    class AWillie_BP_C;
}

struct WeaponClassPaths;

namespace Spawner {
    enum class ActorType : std::uint8_t { Willie, Weapon, Shield, Tool, Armor, Unknown };
    inline constexpr SDK::Enum_Ranks DEFAULT_SPAWN_TIER = SDK::Enum_Ranks::NewEnumerator4;

    ActorType GetActorType(std::string_view classPath);
    float GetGroundOffsetForType(ActorType type, const SDK::FVector& scale = {1.0f, 1.0f, 1.0f});
    void ClearCache();

    /// Deferred spawn helper: begins deferred actor spawn -> runs optional pre-finish callback -> finishes spawning.
    /// All spawn functions use this internally. Exposed publicly so callers outside Spawner (e.g. MapLoaderSection)
    /// can use it directly instead of duplicating the pattern.
    SDK::AActor* DeferredSpawn(
        const SDK::UWorld* world, SDK::UClass* actorClass, const SDK::FTransform& transform,
        const std::function<void(SDK::AActor*)>& preFinishCallback = nullptr
    );

    SDK::FVector GetGroundPosition(
        const SDK::UWorld* world, const SDK::FVector& position, float groundOffset = 50.0f,
        float traceDistance = 1000.0f
    );
    void SpawnActor(
        const SDK::UWorld* world, const std::string& className, const SDK::FTransform& transform,
        const std::function<void(SDK::AActor*)>& callback = nullptr, bool snapToGround = false,
        SDK::Enum_Ranks tier = DEFAULT_SPAWN_TIER, const std::function<void(SDK::AActor*)>& postSpawnCallback = nullptr,
        SDK::Enum_WeaponType_Specific weaponSpecificType = SDK::Enum_WeaponType_Specific::NewEnumerator7
    );
    void SpawnArmorFromPassport(
        const SDK::UWorld* world, const SDK::FStr_Passport_Armor1& passport, const SDK::FTransform& transform,
        bool snapToGround = false, const std::function<void(SDK::AActor*)>& callback = nullptr
    );
    void SpawnCustomizableFromPassport(
        const SDK::UWorld* world, const SDK::FStr_Passport_Weapon1& passport, const SDK::FTransform& transform,
        bool snapToGround = false, const std::function<void(SDK::AActor*)>& callback = nullptr
    );

    SDK::UClass* LoadClass(const std::string& classPath);

    SDK::FTransform BuildSpawnTransform(
        SDK::AWillie_BP_C* player, float distanceForward, float distanceUp, float scale
    );
    SDK::FTransform BuildSpawnTransform(SDK::AWillie_BP_C* player, const SpawnConfig& cfg);

    bool SpawnAndEquipArmor(
        const SDK::UWorld* world, SDK::AWillie_BP_C* willie, const SDK::FStr_Passport_Armor1& passport
    );

    void LoadWeaponClasses(SDK::FStr_Passport_Weapon1& passport, const WeaponClassPaths& paths);
}
