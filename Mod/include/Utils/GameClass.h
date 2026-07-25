#pragma once

#include <string_view>

#include "SDK/BP_Armor_Master_classes.hpp"
#include "SDK/BP_Armor_Modular_Core_Master_classes.hpp"
#include "SDK/GI_Settings_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"

namespace GameClass {
    inline bool IsSubclassOf(const SDK::UClass* type, std::string_view baseName) {
        for (auto* current = static_cast<const SDK::UStruct*>(type); current; current = current->SuperStruct)
            if (current->GetName() == baseName) return true;
        return false;
    }

    inline bool IsObjectOf(const SDK::UObject* object, std::string_view baseName) {
        return object && IsSubclassOf(object->Class, baseName);
    }

    inline bool IsWillieClass(SDK::UClass* type) {
        return IsSubclassOf(type, "Willie_BP_C");
    }

    inline bool IsWillie(const SDK::UObject* object) {
        return object && IsWillieClass(object->Class);
    }

    inline bool IsModularWeapon(const SDK::UObject* object) {
        return IsObjectOf(object, "ModularWeaponBP_C");
    }

    inline bool IsArmor(const SDK::UObject* object) {
        return IsObjectOf(object, "BP_Armor_Master_C");
    }

    inline bool IsModularArmor(const SDK::UObject* object) {
        return IsObjectOf(object, "BP_Armor_Modular_Core_Master_C");
    }

    inline bool IsGameSettings(const SDK::UObject* object) {
        return IsObjectOf(object, "GI_Settings_C");
    }
}
