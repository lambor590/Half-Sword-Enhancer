#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <map>
#include <array>

#include "Menu/ICollapsibleSection.h"
#include "Menu/Utils/Spawner.h"

struct ItemInfo {
    const char* displayName;
    const char* className;
};

enum class ItemCategory : uint8_t {
    Weapons,
    Helmets,
    BodyArmor,
    Arms,
    Legs,
    Hands,
    Feet,
    Neck,
    Shoulders,
    Waist,
    Props,

    COUNT
};

enum class WeaponSubcategory : uint8_t {
    Swords,
    Maces,
    Axes,
    Polearms,
    Daggers,
    Tools,
    Shields,
    Improvised,

    COUNT
};

class ItemSection : public CollapsibleSection {
private:
    static inline int spawnItemKey = -1;
    static inline float spawnDistanceForward = 150.0f;
    static inline float spawnDistanceUp = 50.0f;
    static inline float spawnScale = 1.0f;

    static constexpr size_t MAX_ITEMS = 256;
    static constexpr uint8_t WEAPONS_INDEX = static_cast<uint8_t>(ItemCategory::Weapons);
    static constexpr uint8_t PROPS_INDEX = static_cast<uint8_t>(ItemCategory::Props);

    static inline const char* categories[] = {
        "Weapons",
        "Helmets",
        "Body Armor",
        "Arms",
        "Legs",
        "Hands",
        "Feet",
        "Neck",
        "Shoulders",
        "Waist",
        "Props"
    };
    static inline const int categoriesCount = sizeof(categories) / sizeof(categories[0]);

    static inline const char* weaponSubcategories[] = {
        "Swords",
        "Maces",
        "Axes",
        "Polearms",
        "Daggers",
        "Tools",
        "Shields",
        "Improvised"
    };
    static inline const int weaponSubcategoriesCount = sizeof(weaponSubcategories) / sizeof(weaponSubcategories[0]);

    static inline std::map<ItemCategory, std::map<WeaponSubcategory, std::vector<ItemInfo>>> initWeaponItems() {
        std::map<ItemCategory, std::map<WeaponSubcategory, std::vector<ItemInfo>>> items;

        items[ItemCategory::Weapons][WeaponSubcategory::Swords] = {
            {"Long Sword T1", "ModularWeaponBP_LongSword_T1_C"},
            {"Long Sword T2", "ModularWeaponBP_LongSword_T2_C"},
            {"Long Sword T3", "ModularWeaponBP_LongSword_T3_C"},
            {"Long Sword T4", "ModularWeaponBP_LongSword_T4_C"},
            {"Arming Sword", "ModularWeaponBP_ArmingSword_C"},
            {"Arming Sword T1", "ModularWeaponBP_ArmingSword_T1_C"},
            {"Arming Sword T2", "ModularWeaponBP_ArmingSword_T2_C"},
            {"Arming Sword T3", "ModularWeaponBP_ArmingSword_T3_C"},
            {"Bastard Sword T1", "ModularWeaponBP_BastardSword_T1_C"},
            {"Bastard Sword T2", "ModularWeaponBP_BastardSword_T2_C"},
            {"Bastard Sword T3", "ModularWeaponBP_BastardSword_T3_C"},
            {"Great Sword", "ModularWeaponBP_GreatSword_C"},
            {"Short Falchion", "ModularWeaponBP_Falchion_Short_C"},
            {"Short Falchion T1", "ModularWeaponBP_Falchion_Short_T1_C"},
            {"Short Falchion T2", "ModularWeaponBP_Falchion_Short_T2_C"},
            {"Short Falchion T3", "ModularWeaponBP_Falchion_Short_T3_C"},
            {"Long Falchion", "ModularWeaponBP_Falchion_Long_C"},
            {"Long Falchion T1", "ModularWeaponBP_Falchion_Long_T1_C"},
            {"Long Falchion T2", "ModularWeaponBP_Falchion_Long_T2_C"}
        };

        items[ItemCategory::Weapons][WeaponSubcategory::Maces] = {
            {"Mace", "BP_Weapon_Mace_C"},
            {"Short Mace", "ModularWeaponBP_Mace_Low_Tier_Short_C"},
            {"Average Mace", "ModularWeaponBP_Mace_Mid_Tier_Avg_C"},
            {"Long Mace", "ModularWeaponBP_Mace_Mid_Tier_Long_C"},
            {"Short High Tier Mace", "ModularWeaponBP_Mace_High_Tier_Short_C"},
            {"Average High Tier Mace", "ModularWeaponBP_Mace_High_Tier_Avg_C"},
            {"Giant High Tier Mace", "ModularWeaponBP_Mace_High_Tier_Giant_C"}
        };

        items[ItemCategory::Weapons][WeaponSubcategory::Axes] = {
            {"Axe", "BP_Weapon_Axe_C"},
            {"Modular Axe", "ModularWeaponBP_Axe_C"},
            {"Two-Handed Axe", "ModularWeaponBP_Axe2H_C"}
        };

        items[ItemCategory::Weapons][WeaponSubcategory::Polearms] = {
            {"Spear A", "ModularWeaponBP_Spear_A_C"},
            {"Spear B", "ModularWeaponBP_Spear_B_C"},
            {"Halberd A", "ModularWeaponBP_Halberd_A_C"},
            {"Halberd B", "ModularWeaponBP_Halberd_B_C"},
            {"Halberd D", "ModularWeaponBP_Halberd_D_C"},
            {"Billhook A", "ModularWeaponBP_Billhook_A_C"},
            {"Billhook B", "ModularWeaponBP_Billhook_B_C"},
            {"War Staff A", "ModularWeaponBP_WarStaff_A_C"},
            {"War Staff B", "ModularWeaponBP_WarStaff_B_C"},
            {"High Tier Polearm", "ModularWeaponBP_Polearm_High_Tier_C"},
            {"High Tier Polearm Big", "ModularWeaponBP_Polearm_High_Tier_Big_C"},
            {"High Tier Polearm Gold", "ModularWeaponBP_Polearm_High_Tier_Gold_C"},
            {"Mid Tier Polearm", "ModularWeaponBP_Polearm_Mid_Tier_C"},
            {"Low Tier Polearm", "ModularWeaponBP_Polearm_Low_Tier_C"},
            {"Short Hafted High Tier", "ModularWeaponBP_Hafted_High_Tier_Short_C"},
            {"Short Hafted Mid Tier", "ModularWeaponBP_Hafted_Mid_Tier_Short_C"},
            {"Average Hafted Mid Tier", "ModularWeaponBP_Hafted_Mid_Tier_Avg_C"},
            {"Long Hafted Mid Tier", "ModularWeaponBP_Hafted_Mid_Tier_Long_C"},
            {"Short Hafted Low Tier", "ModularWeaponBP_Hafted_Low_Tier_Short_C"}
        };

        items[ItemCategory::Weapons][WeaponSubcategory::Daggers] = {
            {"Dagger", "BP_Weapon_Dagger_C"},
            {"Modular Dagger", "ModularWeaponBP_Dagger_C"},
            {"Modular Dagger T1", "ModularWeaponBP_Dagger_T1_C"},
            {"Modular Dagger T2", "ModularWeaponBP_Dagger_T2_C"},
            {"Modular Dagger T3", "ModularWeaponBP_Dagger_T3_C"},
            {"Rondel", "ModularWeaponBP_Rondel_C"},
            {"Rondel Gold", "ModularWeaponBP_Rondel_Gold_C"}
        };

        items[ItemCategory::Weapons][WeaponSubcategory::Tools] = {
            {"Hammer A", "BP_Weapon_Tool_Hammer_A_C"},
            {"Hammer B", "BP_Weapon_Tool_Hammer_B_C"},
            {"Hammer C", "BP_Weapon_Tool_Hammer_C_C"},
            {"Axe A", "BP_Weapon_Tool_Axe_A_C"},
            {"Axe B", "BP_Weapon_Tool_Axe_B_C"},
            {"Axe C", "BP_Weapon_Tool_Axe_C_C"},
            {"Knife A", "BP_Weapon_Tool_Knife_A_C"},
            {"Knife B", "BP_Weapon_Tool_Knife_B_C"},
            {"Knife C", "BP_Weapon_Tool_Knife_C_C"},
            {"Scissors", "BP_Weapon_Tool_Scissors_C"},
            {"Sickle A", "BP_Weapon_Tool_Sickle_A_C"},
            {"Sickle D", "BP_Weapon_Tool_Sickle_D_C"},
            {"Sickle E", "BP_Weapon_Tool_Sickle_E_C"},
            {"Scythe A", "BP_Weapon_Tool_Scythe_A_C"},
            {"Shovel A", "BP_Weapon_Tool_Shovel_A_C"},
            {"Shovel B", "BP_Weapon_Tool_Shovel_B_C"},
            {"Pitchfork A", "BP_Weapon_Tool_Pitchfork_A_C"},
            {"Tongs", "BP_Weapon_Tool_Tongs_C"},
            {"Hoe A", "BP_Weapon_Tool_Hoe_A_C"}
        };

        items[ItemCategory::Weapons][WeaponSubcategory::Shields] = {
            {"Buckler Shield", "Shield_Buckler_C"},
            {"Buckler Shield Gold", "Shield_Buckler_Gold_C"},
            {"Boss Grip Shield", "Shield_BossGrip_C"},
            {"Light Pavise", "Shield_Pavise_Light_C"},
            {"Heavy Pavise", "Shield_Pavise_Heavy_C"},
            {"Tower Pavise", "Shield_Pavise_Tower_C"},
            {"Tagre Shield", "Shield_Tagre_C"},
            {"Tagre Shield Gold", "Shield_Tagre_Gold_C"}
        };

        items[ItemCategory::Weapons][WeaponSubcategory::Improvised] = {
            {"Small Candlestick", "BP_Weapon_Improv_CandleStick_Small_C"},
            {"Big Candlestick", "BP_Weapon_Improv_CandleStick_Big_C"},
            {"Stool", "BP_Weapon_Improv_Stool_C"}
        };

        return items;
    }

    static inline std::map<ItemCategory, std::vector<ItemInfo>> initArmorItems() {
        std::map<ItemCategory, std::vector<ItemInfo>> items;

        items[ItemCategory::Helmets] = {
            {"Armet", "BP_Armor_Head_Armet_001_C"},
            {"Armet Gold", "BP_Armor_Head_Armet_001_G_C"},
            {"Armet R", "BP_Armor_Head_Armet_001_R_C"},
            {"Armet B", "BP_Armor_Head_Armet_001_B_C"},
            {"Barbute A", "BP_Armor_Head_Barbute_A_C"},
            {"Barbute B", "BP_Armor_Head_Barbute_B_C"},
            {"Open Sallet A", "BP_Armor_Head_Sallet_Open_A_001_C"},
            {"Open Sallet B", "BP_Armor_Head_Sallet_Open_B_001_C"},
            {"Open Sallet CA", "BP_Armor_Head_Sallet_Open_CA_001_C"},
            {"Open Sallet CB", "BP_Armor_Head_Sallet_Open_CB_001_C"},
            {"Solid Sallet A 001", "BP_Armor_Head_Sallet_Solid_A_001_C"},
            {"Solid Sallet A 002", "BP_Armor_Head_Sallet_Solid_A_002_C"},
            {"Solid Sallet B 001", "BP_Armor_Head_Sallet_Solid_B_001_C"},
            {"Solid Sallet B 002", "BP_Armor_Head_Sallet_Solid_B_002_C"},
            {"Solid Sallet C 001", "BP_Armor_Head_Sallet_Solid_C_001_C"},
            {"Solid Sallet C 002", "BP_Armor_Head_Sallet_Solid_C_002_C"},
            {"Solid Sallet D 001", "BP_Armor_Head_Sallet_Solid_D_001_C"},
            {"Solid Sallet D 002", "BP_Armor_Head_Sallet_Solid_D_002_C"},
            {"Solid Sallet E 001", "BP_Armor_Head_Sallet_Solid_E_001_C"},
            {"Solid Sallet E 002", "BP_Armor_Head_Sallet_Solid_E_002_C"},
            {"Visor Sallet A", "BP_Armor_Head_Sallet_Visor_A_001_C"},
            {"Visor Sallet B", "BP_Armor_Head_Sallet_Visor_B_001_C"},
            {"Kettle Helm A", "BP_Armor_Head_Kettle_Helm_A_C"},
            {"Kettle Helm B", "BP_Armor_Head_Kettle_Helm_B_C"},
            {"Kettle Helm B 2", "BP_Armor_Head_Kettle_Helm_B_2_C"},
            {"Kettle Helm C", "BP_Armor_Head_Kettle_Helm_C_C"},
            {"Kettle Helm D", "BP_Armor_Head_Kettle_Helm_D_C"},
            {"Kettle Helm D 2", "BP_Armor_Head_Kettle_Helm_D_2_C"},
            {"Bycocket A 001", "BP_Armor_Head_Hat_Bycocket_A_001_C"},
            {"Bycocket A 002", "BP_Armor_Head_Hat_Bycocket_A_002_C"},
            {"Bycocket B 001", "BP_Armor_Head_Hat_Bycocket_B_001_C"},
            {"Bycocket B 002", "BP_Armor_Head_Hat_Bycocket_B_002_C"},
            {"Bycocket B 002 Brown", "BP_Armor_Head_Hat_Bycocket_B_002_Brown_C"},
            {"Bycocket C", "BP_Armor_Head_Hat_Bycocket_C_001_C"},
            {"Bycocket D", "BP_Armor_Head_Hat_Bycocket_D_001_C"},
            {"Bycocket E", "BP_Armor_Head_Hat_Bycocket_E_001_C"},
            {"Bycocket F", "BP_Armor_Head_Hat_Bycocket_F_001_C"},
            {"Bycocket G", "BP_Armor_Head_Hat_Bycocket_G_001_C"},
            {"Bycocket H", "BP_Armor_Head_Hat_Bycocket_H_001_C"},
            {"Bycocket I", "BP_Armor_Head_Hat_Bycocket_I_001_C"},
            {"Bycocket J", "BP_Armor_Head_Hat_Bycocket_J_001_C"},
            {"Gnome Hat A", "BP_Armor_Head_Hat_Gnome_A_C"},
            {"Gnome Hat B", "BP_Armor_Head_Hat_Gnome_B_C"}
        };

        items[ItemCategory::BodyArmor] = {
            {"Cuirass A T1", "BP_Armor_Body_Cuirass_A_T1_C"},
            {"Cuirass A T2", "BP_Armor_Body_Cuirass_A_T2_C"},
            {"Cuirass A T3", "BP_Armor_Body_Cuirass_A_T3_C"},
            {"Cuirass B T1", "BP_Armor_Body_Cuirass_B_T1_C"},
            {"Cuirass B T2", "BP_Armor_Body_Cuirass_B_T2_C"},
            {"Cuirass C T1", "BP_Armor_Body_Cuirass_C_T1_C"},
            {"Cuirass C T2", "BP_Armor_Body_Cuirass_C_T2_C"},
            {"Cuirass C T3", "BP_Armor_Body_Cuirass_C_T3_C"},
            {"Cuirass C T3 B", "BP_Armor_Body_Cuirass_C_T3_B_C"},
            {"Cuirass C T3 Gold", "BP_Armor_Body_Cuirass_C_T3_G_C"},
            {"Breastplate A T1", "BP_Armor_Body_Brestplate_A_T1_C"},
            {"Breastplate A T2", "BP_Armor_Body_Brestplate_A_T2_C"},
            {"Breastplate B T1", "BP_Armor_Body_Brestplate_B_T1_C"},
            {"Breastplate B T2", "BP_Armor_Body_Brestplate_B_T2_C"},
            {"Plackard T2", "BP_Armor_Body_Plackard_T2_C"},
            {"Gambeson A T1", "BP_Armor_Body_Gambeson_A_T1_C"},
            {"Gambeson A T2", "BP_Armor_Body_Gambeson_A_T2_C"},
            {"Gambeson A Red", "BP_Armor_Body_Gambeson_A_Red_C"},
            {"Gambeson A Gray", "BP_Armor_Body_Gambeson_A_Gray_C"},
            {"Gambeson B T1", "BP_Armor_Body_Gambeson_B_T1_C"},
            {"Gambeson B T2", "BP_Armor_Body_Gambeson_B_T2_C"},
            {"Doublet", "BP_Armor_Body_Doublet_C"},
            {"Arming Doublet", "BP_Armor_Body_Doublet_Arming_C"},
            {"Arming Doublet Black", "BP_Armor_Body_Doublet_Arming_Black_C"},
            {"Arming Doublet Purple", "BP_Armor_Body_Doublet_Arming_Purple_C"},
            {"Shirt A", "BP_Armor_Body_Shirt_A_C"},
            {"Shirt B", "BP_Armor_Body_Shirt_B_C"}
        };

        items[ItemCategory::Arms] = {
            {"Vambrace A T2", "BP_Armor_Arms_Vambrace_A_T2_C"},
            {"Vambrace A T3", "BP_Armor_Arms_Vambrace_A_T3_C"},
            {"Vambrace B T2", "BP_Armor_Arms_Vambrace_B_T2_C"},
            {"Vambrace B T3", "BP_Armor_Arms_Vambrace_B_T3_C"},
            {"Vambrace C T2", "BP_Armor_Arms_Vambrace_C_T2_C"},
            {"Vambrace C T3", "BP_Armor_Arms_Vambrace_C_T3_C"},
            {"Vambrace C T3 B", "BP_Armor_Arms_Vambrace_C_T3_B_C"},
            {"Vambrace C T3 Gold", "BP_Armor_Arms_Vambrace_C_T3_G_C"},
            {"Chains T1", "BP_Armor_Arms_Chains_T1_C"},
            {"Chains T2", "BP_Armor_Arms_Chains_T2_C"}
        };

        items[ItemCategory::Legs] = {
            {"Cuisse A T2", "BP_Armor_Legs_Cuisse_A_T2_C"},
            {"Cuisse A T3", "BP_Armor_Legs_Cuisse_A_T3_C"},
            {"Cuisse B", "BP_Armor_Legs_Cuisse_B_C"},
            {"Cuisse G", "BP_Armor_Legs_Cuisse_G_C"},
            {"Greaves T2", "BP_Armor_Legs_Greaves_T2_C"},
            {"Greaves T3", "BP_Armor_Legs_Greaves_T3_C"},
            {"Hosen A", "BP_Armor_Legs_Hosen_A_C"},
            {"Hosen A Brown", "BP_Armor_Legs_Hosen_A_Brown_C"},
            {"Hosen B", "BP_Armor_Legs_Hosen_B_C"},
            {"Hosen C", "BP_Armor_Legs_Hosen_C_C"},
            {"Hosen C Poor", "BP_Armor_Legs_Hosen_C_Poor_C"},
            {"Hosen C Black", "BP_Armor_Hosen_C_Black_C"},
            {"Arming Hosen A", "BP_Armor_Legs_Hosen_Arming_A_C"},
            {"Arming Hosen A Purple", "BP_Armor_Legs_Hosen_Arming_A_Purple_C"},
            {"Arming Hosen B", "BP_Armor_Legs_Hosen_Arming_B_C"},
            {"Arming Hosen B Black", "BP_Armor_Legs_Hosen_Arming_B_Black_C"},
            {"Arming Hosen C", "BP_Armor_Legs_Hosen_Arming_C_C"},
            {"Arming Hosen C Blue", "BP_Armor_Hosen_Arming_C_Blue_C"},
            {"Panties", "BP_Armor_Legs_Panties_C"}
        };

        items[ItemCategory::Hands] = {
            {"Gauntlets T1", "BP_Armor_Hands_Gauntlets_T1_C"},
            {"Gauntlets T2", "BP_Armor_Hands_Gauntlets_T2_C"},
            {"Gauntlets T3", "BP_Armor_Hands_Gauntlets_T3_C"},
            {"Gauntlets T3 Gold", "BP_Armor_Hands_Gauntlets_T3_G_C"},
            {"Gauntlets T3B", "BP_Armor_Hands_Gauntlets_T3B_C"},
            {"Half Gauntlets T1", "BP_Armor_Hands_HalfGauntlets_T1_C"},
            {"Half Gauntlets T2", "BP_Armor_Hands_HalfGauntlets_T2_C"}
        };

        items[ItemCategory::Feet] = {
            {"Shoes A", "BP_Armor_Feet_Shoes_A_C"},
            {"Shoes B", "BP_Armor_Feet_Shoes_B_C"},
            {"Shoes C", "BP_Armor_Feet_Shoes_C_C"},
            {"Sabatons A", "BP_Armor_Feet_Sabbatons_A_C"},
            {"Sabatons A Gold", "BP_Armor_Feet_Sabbatons_A_G_C"},
            {"Sabatons AB", "BP_Armor_Feet_Sabbatons_AB_C"}
        };

        items[ItemCategory::Neck] = {
            {"Standard T1", "BP_Armor_Neck_Standard_T1_C"},
            {"Standard T2", "BP_Armor_Neck_Standard_T2_C"},
            {"Standard T3", "BP_Armor_Neck_Standard_T3_C"},
            {"Bevor T2", "BP_Armor_Neck_Bevor_T2_C"},
            {"Bevor T3", "BP_Armor_Neck_Bevor_T3_C"}
        };

        items[ItemCategory::Shoulders] = {
            {"Spaulder A T2", "BP_Armor_Shoulders_Spaulder_A_T2_C"},
            {"Spaulder A T3", "BP_Armor_Shoulders_Spaulder_A_T3_C"},
            {"Spaulder B T2", "BP_Armor_Shoulders_Spaulder_B_T2_C"},
            {"Spaulder B T3", "BP_Armor_Shoulders_Spaulder_B_T3_C"},
            {"Pauldron C", "BP_Armor_Shoulders_Pauldron_C_C"},
            {"Pauldron C Gold", "BP_Armor_Shoulders_Pauldron_C_G_C"},
            {"Pauldron C B", "BP_Armor_Shoulders_Pauldron_C_B_C"}
        };

        items[ItemCategory::Waist] = {
            {"Foulds T1", "BP_Armor_Waist_Foulds_T1_C"},
            {"Foulds T2", "BP_Armor_Waist_Foulds_T2_C"},
            {"Foulds T3", "BP_Armor_Waist_Foulds_T3_C"}
        };

        return items;
    }

    static inline std::vector<ItemInfo> initPropItems() {
        return {
            {"Training Dummy", "BP_Prop_Training_Dummy_001_C"},
            {"Candle Light", "BP_CandleLight_C"}
        };
    }

    static inline std::map<ItemCategory, std::map<WeaponSubcategory, std::vector<ItemInfo>>> weaponItems = initWeaponItems();
    static inline std::map<ItemCategory, std::vector<ItemInfo>> armorItems = initArmorItems();
    static inline std::vector<ItemInfo> propItems = initPropItems();

    static inline int currentCategoryIndex = 0;
    static inline int currentWeaponSubcategoryIndex = 0;
    static inline int currentItemIndex = 0;

    static inline const char* itemNames[MAX_ITEMS];

    void setItemNamesArray(const std::vector<ItemInfo>& items) {
        const int count = min(static_cast<int>(items.size()), static_cast<int>(MAX_ITEMS));
        for (int i = 0; i < count; i++) {
            itemNames[i] = items[i].displayName;
        }
    }

    const char* getSelectedClassName() const {
        if (currentCategoryIndex == WEAPONS_INDEX) {
            auto subcategory = static_cast<WeaponSubcategory>(currentWeaponSubcategoryIndex);
            const auto& items = weaponItems[ItemCategory::Weapons][subcategory];
            return items[currentItemIndex].className;
        }
        else if (currentCategoryIndex == PROPS_INDEX) {
            return propItems[currentItemIndex].className;
        }
        else {
            ItemCategory category = static_cast<ItemCategory>(currentCategoryIndex);
            const auto& items = armorItems[category];
            return items[currentItemIndex].className;
        }
    }

    const char* getSelectedDisplayName() const {
        if (currentCategoryIndex == WEAPONS_INDEX) {
            auto subcategory = static_cast<WeaponSubcategory>(currentWeaponSubcategoryIndex);
            const auto& items = weaponItems[ItemCategory::Weapons][subcategory];
            return items[currentItemIndex].displayName;
        }
        else if (currentCategoryIndex == PROPS_INDEX) {
            return propItems[currentItemIndex].displayName;
        }
        else {
            ItemCategory category = static_cast<ItemCategory>(currentCategoryIndex);
            const auto& items = armorItems[category];
            return items[currentItemIndex].displayName;
        }
    }

    void SpawnSelectedItem() {
        const char* className = getSelectedClassName();

        if (!className) return;

        SDK::FTransform spawnTransform = player->GetTransform();
        spawnTransform.Translation += player->GetActorForwardVector() * spawnDistanceForward;
        spawnTransform.Translation.Z += spawnDistanceUp;
        spawnTransform.Scale3D = SDK::FVector(spawnScale, spawnScale, spawnScale);
        Spawner::SpawnActor(world, className, spawnTransform);
    }

    void RenderItemSelector(const std::vector<ItemInfo>& items) {
        if (items.empty()) return;

        setItemNamesArray(items);
        int itemCount = min(static_cast<int>(items.size()), static_cast<int>(MAX_ITEMS));

        ImGui::Text("Item");
        if (ImGui::Combo("##ItemSelector", &currentItemIndex, itemNames, itemCount)) {
            // Selection changed
        }
    }

public:
    ItemSection() : CollapsibleSection("Item") {
        std::initializer_list<Parameter> spawnItemParams = {
            Parameter("distance_forward", "Forward Distance", &spawnDistanceForward, 50.0f, 300.0f),
            Parameter("distance_up", "Up Distance", &spawnDistanceUp, 0.0f, 200.0f),
            Parameter("scale", "Scale", &spawnScale, 0.1f, 5.0f)
        };

        BindWithParams("Spawn Item", &spawnItemKey, spawnItemParams, [this]() {
            SpawnSelectedItem();
        }, player, world);
    }

    void Render() override {
        if (ImGui::CollapsingHeader(name.c_str())) {
            for (auto& function : functions) {
                function->Render();
            }

            ImGui::Text("Category");
            if (ImGui::Combo("##CategorySelector", &currentCategoryIndex, categories, categoriesCount)) {
                currentItemIndex = 0;
                currentWeaponSubcategoryIndex = 0;
            }

            if (currentCategoryIndex == WEAPONS_INDEX) {
                ImGui::Text("Subcategory");
                if (ImGui::Combo("##SubcategorySelector", &currentWeaponSubcategoryIndex, weaponSubcategories, weaponSubcategoriesCount)) {
                    currentItemIndex = 0;
                }

                auto subcategory = static_cast<WeaponSubcategory>(currentWeaponSubcategoryIndex);
                const auto& items = weaponItems[ItemCategory::Weapons][subcategory];
                RenderItemSelector(items);
            }
            else if (currentCategoryIndex == PROPS_INDEX) {
                RenderItemSelector(propItems);
            }
            else {
                ItemCategory category = static_cast<ItemCategory>(currentCategoryIndex);
                const auto& items = armorItems[category];
                RenderItemSelector(items);
            }

            if (ImGui::Button("Spawn Selected Item")) {
                auto validatedSpawn = ValidateAndRun([this]() {
                    SpawnSelectedItem();
                }, player, world);

                validatedSpawn();
            }
        }
    }
};