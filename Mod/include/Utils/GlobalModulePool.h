#pragma once

#include <vector>
#include <string>
#include <atomic>
#include <unordered_set>

#include "Utils/EquipmentGenerator.h"
#include "Utils/CustomizableWeapon.h"
#include "SDK/BP_GameWeapon_Customizable_Master_classes.hpp"

struct GlobalModuleEntry {
    SDK::UClass* cls;
    std::string name;
    const char* sourceType;
};

struct GlobalModulePool {
    std::vector<GlobalModuleEntry> heads, guards, grips, pommels, subMods1, subMods2;
    float cachedWidths[6] = {};
    std::atomic<bool> populated{false};

    static GlobalModulePool& Get() {
        static GlobalModulePool instance;
        return instance;
    }

    void Populate() {
        if (populated.load(std::memory_order_acquire)) return;

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

        std::unordered_set<SDK::UClass*> seen[6];
        for (int i = 1; i <= WEAPON_TYPE_COUNT; ++i) {
            auto type = static_cast<CustomizableWeapon>(i);
            SDK::UClass* masterClass = EquipmentGenerator::GetCustomizableModulesClass(type);
            if (!masterClass || !masterClass->ClassDefaultObject) continue;

            auto* cdo = reinterpret_cast<SDK::UBP_GameWeapon_Customizable_Master_C*>(
                masterClass->ClassDefaultObject);
            const char* typeName = WEAPON_TYPE_NAMES[i - 1];

            CollectEntries(heads,    seen[0], cdo->Module_Heads_Array, typeName);
            CollectEntries(guards,   seen[1], cdo->Module_Guards_Array, typeName);
            CollectEntries(grips,    seen[2], cdo->Module_Grips_Array, typeName);
            CollectEntries(pommels,  seen[3], cdo->Module_Pommels_Array, typeName);
            CollectEntries(subMods1, seen[4], cdo->Head_Sub_Module_1_Array, typeName);
            CollectEntries(subMods2, seen[5], cdo->Head_Sub_Module_2_Array, typeName);
        }
        populated.store(true, std::memory_order_release);
    }

private:
    GlobalModulePool() = default;

    static void CollectEntries(std::vector<GlobalModuleEntry>& out,
        std::unordered_set<SDK::UClass*>& seen,
        const SDK::TArray<SDK::UClass*>& arr, const char* sourceType)
    {
        for (int i = 0; i < arr.Num(); ++i) {
            if (arr[i] && seen.insert(arr[i]).second)
                out.push_back({arr[i], arr[i]->GetName(), sourceType});
        }
    }
};
