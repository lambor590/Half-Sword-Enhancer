#pragma once

#include <array>
#include <functional>
#include <string>

#include "Core/ModContext.h"
#include "Menu/SectionConfig.h"
#include "Utils/CustomizableWeapon.h"
#include "Utils/LivePreviewManager.h"
#include "Utils/WeaponClassPaths.h"
#include "SDK/Engine_classes.hpp"
#include "SDK/Str_Passport_Armor1_structs.hpp"
#include "SDK/Str_Passport_Weapon1_structs.hpp"

namespace SpawnWorkflow {
    using ActorCallback = std::function<void(SDK::AActor*)>;

    bool QueueWeaponSpawn(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::FStr_Passport_Weapon1 passport,
        WeaponClassPaths classPaths, ActorCallback onSpawned = nullptr
    );
    bool QueueWeaponPreview(
        const RuntimeContextSnapshot& snapshot, LivePreviewManager& preview, const SpawnConfig& spawn,
        SDK::FStr_Passport_Weapon1 passport, WeaponClassPaths classPaths, ActorCallback onSpawned,
        ActorCallback onPreviewReady
    );

    bool QueueArmorSpawn(
        const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, SDK::FStr_Passport_Armor1 passport,
        ActorCallback onSpawned = nullptr
    );
    bool QueueArmorPreview(
        const RuntimeContextSnapshot& snapshot, LivePreviewManager& preview, const SpawnConfig& spawn,
        SDK::FStr_Passport_Armor1 passport, ActorCallback onPreviewReady = nullptr
    );

    struct ItemSpawnRequest {
        enum class Kind { ClassPath, GeneratedCustomizableWeapon, RandomArmor, ModularArmor, ArmorPreset };

        Kind kind = Kind::ClassPath;
        std::string classPath;
        std::string armorCorePath;
        std::array<int, 3> modules{};
        SDK::FStr_Passport_Armor1 armorPassport{};
        SDK::EArmorSlots_Enum armorSlot{};
        CustomizableWeapon customizable = CustomizableWeapon::None;
        SDK::Enum_Ranks tier{};

        static ItemSpawnRequest ClassPath(std::string classPath, SDK::Enum_Ranks tier);
        static ItemSpawnRequest GeneratedCustomizableWeapon(CustomizableWeapon type, SDK::Enum_Ranks tier);
        static ItemSpawnRequest RandomArmor(SDK::EArmorSlots_Enum slot, SDK::Enum_Ranks tier);
        static ItemSpawnRequest ModularArmor(std::string classPath, std::array<int, 3> modules);
        static ItemSpawnRequest ArmorPreset(SDK::FStr_Passport_Armor1 passport, std::string armorCorePath);
    };

    bool SpawnItem(const RuntimeContextSnapshot& runtime, const SpawnConfig& spawn, const ItemSpawnRequest& request);
    bool QueueItemSpawn(const RuntimeContextSnapshot& snapshot, const SpawnConfig& spawn, ItemSpawnRequest request);
}
