#pragma once

#include <array>
#include <vector>
#include <string>
#include <atomic>
#include <unordered_set>

#include "Utils/EquipmentGenerator.h"
#include "Utils/CustomizableWeapon.h"
#include "Utils/BlueprintRegistry.h"
#include "Utils/GameConstants.h"
#include "Utils/PresetUtils.h"
#include "SDK/BP_GameWeapon_Customizable_Master_classes.hpp"

struct GlobalModuleEntry {
    SDK::UClass* cls;
    std::string name;
    std::string path;
    const char* sourceType;
};

struct GlobalModuleSet {
    std::vector<GlobalModuleEntry> heads, guards, grips, pommels, subMods1, subMods2;
    float cachedWidths[6] = {};
};

struct GlobalModulePool {
    GlobalModuleSet all;
    std::array<GlobalModuleSet, GameConstants::WEAPON_TYPE_COUNT + 1> byType;
    std::atomic<bool> populated{false};

    static GlobalModulePool& Get() {
        static GlobalModulePool instance;
        return instance;
    }

    GlobalModuleSet& ForType(int type) {
        if (type < 1 || type > GameConstants::WEAPON_TYPE_COUNT) return byType[0];
        return byType[static_cast<size_t>(type)];
    }

    void Populate() {
        if (populated.load(std::memory_order_acquire)) return;

        static constexpr auto& WEAPON_TYPE_NAMES = GameConstants::WEAPON_TYPE_NAMES;
        static constexpr int WEAPON_TYPE_COUNT = GameConstants::WEAPON_TYPE_COUNT;

        std::unordered_set<SDK::UClass*> seen[6];
        for (int i = 1; i <= WEAPON_TYPE_COUNT; ++i) {
            auto type = static_cast<CustomizableWeapon>(i);
            SDK::UClass* masterClass = EquipmentGenerator::GetCustomizableModulesClass(type);
            if (!masterClass || !masterClass->ClassDefaultObject) continue;

            auto* cdo = reinterpret_cast<SDK::UBP_GameWeapon_Customizable_Master_C*>(masterClass->ClassDefaultObject);
            const char* typeName = WEAPON_TYPE_NAMES[i - 1];
            auto& typeSet = byType[static_cast<size_t>(i)];
            std::unordered_set<SDK::UClass*> typeSeen[6];

            CollectEntries(all.heads, seen[0], cdo->Module_Heads_Array, typeName);
            CollectEntries(typeSet.heads, typeSeen[0], cdo->Module_Heads_Array, typeName);
            CollectEntries(all.guards, seen[1], cdo->Module_Guards_Array, typeName);
            CollectEntries(typeSet.guards, typeSeen[1], cdo->Module_Guards_Array, typeName);
            CollectEntries(all.grips, seen[2], cdo->Module_Grips_Array, typeName);
            CollectEntries(typeSet.grips, typeSeen[2], cdo->Module_Grips_Array, typeName);
            CollectEntries(all.pommels, seen[3], cdo->Module_Pommels_Array, typeName);
            CollectEntries(typeSet.pommels, typeSeen[3], cdo->Module_Pommels_Array, typeName);
            CollectEntries(all.subMods1, seen[4], cdo->Head_Sub_Module_1_Array, typeName);
            CollectEntries(typeSet.subMods1, typeSeen[4], cdo->Head_Sub_Module_1_Array, typeName);
            CollectEntries(all.subMods2, seen[5], cdo->Head_Sub_Module_2_Array, typeName);
            CollectEntries(typeSet.subMods2, typeSeen[5], cdo->Head_Sub_Module_2_Array, typeName);
        }
        populated.store(true, std::memory_order_release);
    }

private:
    GlobalModulePool() = default;

    static void CollectEntries(
        std::vector<GlobalModuleEntry>& out, std::unordered_set<SDK::UClass*>& seen,
        const SDK::TArray<SDK::UClass*>& arr, const char* sourceType
    ) {
        for (int i = 0; i < arr.Num(); ++i) {
            if (arr[i] && seen.insert(arr[i]).second)
                out.push_back(
                    {arr[i], BlueprintRegistry::CleanDisplayName(arr[i]->GetName()),
                     PresetUtils::ObjectToAbsolutePath(arr[i]), sourceType}
                );
        }
    }
};
