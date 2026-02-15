#pragma once

#include <vector>
#include <array>
#include <cctype>
#include <cstdio>
#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Hooks/GameHook.h"
#include "Utils/Spawner.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/TierValidation.h"
#include "Utils/GuiUtils.h"

#define WEAPON_PATH(s) "/Game/Assets/Weapons/Blueprints/Built_Weapons" s
#define ARMOR_PATH(s) "/Game/Assets/Armor/Blueprints/Built_Armor" s
#define PROP_PATH(s) "/Game/Assets/Props" s
#define TRAP_PATH(s) "/Game/Assets/Traps/Blueprints" s
#define MODULAR_PATH(s) "/Game/Assets/Armor/Blueprints/Modular_Armor" s

struct ItemInfo {
    const char* displayName;
    const char* classPath;
    CustomizableWeapon customizable;

    constexpr ItemInfo(const char* name, const char* path)
        : displayName(name), classPath(path), customizable(CustomizableWeapon::None) {}
    constexpr ItemInfo(const char* name, CustomizableWeapon type)
        : displayName(name), classPath(nullptr), customizable(type) {}
};

template<size_t N>
using ItemArray = std::array<ItemInfo, N>;

class ItemSection : public CollapsibleSection {
private:
    SectionConfig::ItemConfig& cfg = SectionConfig::item;

    static constexpr uint8_t WEAPONS_INDEX = 0;
    static constexpr uint8_t MODULAR_ARMOR_INDEX = 10;
    static constexpr uint8_t ARMOR_MODULES_INDEX = 11;
    static constexpr uint8_t RANDOM_ARMOR_INDEX = 12;
    static constexpr uint8_t PROPS_INDEX = 13;

    static constexpr std::array categories{
        "Weapons", "Helmets", "Body Armor", "Arms", "Legs",
        "Hands", "Feet", "Neck", "Shoulders", "Waist",
        "Modular Armor", "Armor Modules",
        "Random Armor", "Props"
    };

    struct ArmorSlotInfo {
        const char* displayName;
        int slotEnum;
    };

    static constexpr std::array<ArmorSlotInfo, 15> randomArmorSlots{{
        {"Head", 0}, {"Hands", 1}, {"Neck (Bevor)", 4}, {"Neck (Standard)", 5},
        {"Arms", 6}, {"Shoulders", 7}, {"Tabard", 8}, {"Chest (Plate)", 9},
        {"Hauberk", 10}, {"Cuisses", 11}, {"Body (Clothing)", 12},
        {"Waist", 13}, {"Legs (Greaves)", 14}, {"Feet", 15}, {"Hosen", 16}
    }};

    static constexpr std::array weaponSubcategories{
        "Swords", "Bastard Swords", "Falchions",
        "Maces", "Hafted", "Polearms", "Pollaxes", "Casted", "Messer",
        "Axes", "Daggers", "Baurnwehr", "Flails",
        "Billhooks", "Halberds", "Spears", "Staves",
        "Tools", "Shields", "Improvised", "Ranged", "Treasure", "Unique"
    };

    static constexpr ItemArray<11> swordItems{{
        {"Arming Sword", CustomizableWeapon::SwordArming},
        {"Short Sword", CustomizableWeapon::SwordShort},
        {"Long Sword", CustomizableWeapon::SwordLong},
        {"Great Sword", WEAPON_PATH("/ModularWeaponBP_GreatSword.ModularWeaponBP_GreatSword_C")},
        {"Arming Sword (Pre-built)", WEAPON_PATH("/ModularWeaponBP_ArmingSword.ModularWeaponBP_ArmingSword_C")},
        {"Arming Sword T1", WEAPON_PATH("/ModularWeaponBP_ArmingSword_T1.ModularWeaponBP_ArmingSword_T1_C")},
        {"Arming Sword T2", WEAPON_PATH("/ModularWeaponBP_ArmingSword_T2.ModularWeaponBP_ArmingSword_T2_C")},
        {"Arming Sword T3", WEAPON_PATH("/ModularWeaponBP_ArmingSword_T3.ModularWeaponBP_ArmingSword_T3_C")},
        {"Long Sword T1", WEAPON_PATH("/ModularWeaponBP_LongSword_T1.ModularWeaponBP_LongSword_T1_C")},
        {"Long Sword T2", WEAPON_PATH("/ModularWeaponBP_LongSword_T2.ModularWeaponBP_LongSword_T2_C")},
        {"Long Sword T3", WEAPON_PATH("/ModularWeaponBP_LongSword_T3.ModularWeaponBP_LongSword_T3_C")},
    }};

    static constexpr ItemArray<3> bastardSwordItems{{
        {"Bastard Sword T1", WEAPON_PATH("/ModularWeaponBP_BastardSword_T1.ModularWeaponBP_BastardSword_T1_C")},
        {"Bastard Sword T2", WEAPON_PATH("/ModularWeaponBP_BastardSword_T2.ModularWeaponBP_BastardSword_T2_C")},
        {"Bastard Sword T3", WEAPON_PATH("/ModularWeaponBP_BastardSword_T3.ModularWeaponBP_BastardSword_T3_C")},
    }};

    static constexpr ItemArray<8> falchionItems{{
        {"Falchion Long", WEAPON_PATH("/ModularWeaponBP_Falchion_Long.ModularWeaponBP_Falchion_Long_C")},
        {"Falchion Long T1", WEAPON_PATH("/ModularWeaponBP_Falchion_Long_T1.ModularWeaponBP_Falchion_Long_T1_C")},
        {"Falchion Long T2", WEAPON_PATH("/ModularWeaponBP_Falchion_Long_T2.ModularWeaponBP_Falchion_Long_T2_C")},
        {"Falchion Long T3", WEAPON_PATH("/ModularWeaponBP_Falchion_Long_T3.ModularWeaponBP_Falchion_Long_T3_C")},
        {"Falchion Short", WEAPON_PATH("/ModularWeaponBP_Falchion_Short.ModularWeaponBP_Falchion_Short_C")},
        {"Falchion Short T1", WEAPON_PATH("/ModularWeaponBP_Falchion_Short_T1.ModularWeaponBP_Falchion_Short_T1_C")},
        {"Falchion Short T2", WEAPON_PATH("/ModularWeaponBP_Falchion_Short_T2.ModularWeaponBP_Falchion_Short_T2_C")},
        {"Falchion Short T3", WEAPON_PATH("/ModularWeaponBP_Falchion_Short_T3.ModularWeaponBP_Falchion_Short_T3_C")},
    }};

    static constexpr ItemArray<3> maceItems{{
        {"Short Mace", CustomizableWeapon::MaceShort},
        {"Mace", CustomizableWeapon::Mace},
        {"Long Mace", CustomizableWeapon::MaceLong},
    }};

    static constexpr ItemArray<3> haftedItems{{
        {"Short Hafted", CustomizableWeapon::HaftedShort},
        {"Hafted", CustomizableWeapon::Hafted},
        {"Long Hafted", CustomizableWeapon::HaftedLong},
    }};

    static constexpr ItemArray<3> polearmItems{{
        {"Short Polearm", CustomizableWeapon::PolearmShort},
        {"Polearm", CustomizableWeapon::Polearm},
        {"Long Polearm", CustomizableWeapon::PolearmLong},
    }};

    static constexpr ItemArray<3> pollaxeItems{{
        {"Short Pollaxe", CustomizableWeapon::PollaxeShort},
        {"Pollaxe", CustomizableWeapon::Pollaxe},
        {"Long Pollaxe", CustomizableWeapon::PollaxeLong},
    }};

    static constexpr ItemArray<3> castedItems{{
        {"Short Casted", CustomizableWeapon::CastedShort},
        {"Casted", CustomizableWeapon::Casted},
        {"Long Casted", CustomizableWeapon::CastedLong},
    }};

    static constexpr ItemArray<1> messerItems{{
        {"Messer", CustomizableWeapon::Messer},
    }};

    static constexpr ItemArray<2> axeItems{{
        {"Axe", WEAPON_PATH("/Reforged/ModularWeaponBP_Axe.ModularWeaponBP_Axe_C")},
        {"Two-Handed Axe", WEAPON_PATH("/Reforged/ModularWeaponBP_Axe2H.ModularWeaponBP_Axe2H_C")}
    }};

    static constexpr ItemArray<7> daggerItems{{
        {"Rondel", WEAPON_PATH("/Reforged/ModularWeaponBP_Rondel.ModularWeaponBP_Rondel_C")},
        {"Rondel Gold", WEAPON_PATH("/Reforged/ModularWeaponBP_Rondel_Gold.ModularWeaponBP_Rondel_Gold_C")},
        {"Dagger Rondel", WEAPON_PATH("/Reforged/ModularWeaponBP_DaggerRondel.ModularWeaponBP_DaggerRondel_C")},
        {"Dagger", WEAPON_PATH("/ModularWeaponBP_Dagger.ModularWeaponBP_Dagger_C")},
        {"Dagger T1", WEAPON_PATH("/ModularWeaponBP_Dagger_T1.ModularWeaponBP_Dagger_T1_C")},
        {"Dagger T2", WEAPON_PATH("/ModularWeaponBP_Dagger_T2.ModularWeaponBP_Dagger_T2_C")},
        {"Dagger T3", WEAPON_PATH("/ModularWeaponBP_Dagger_T3.ModularWeaponBP_Dagger_T3_C")}
    }};

    static constexpr ItemArray<7> baurnwehrItems{{
        {"Baurnwehr A", WEAPON_PATH("/Reforged/BP_Weapon_Reforged_Baurnwehr_A.BP_Weapon_Reforged_Baurnwehr_A_C")},
        {"Baurnwehr A 002", WEAPON_PATH("/Reforged/BP_Weapon_Reforged_Baurnwehr_A_002.BP_Weapon_Reforged_Baurnwehr_A_002_C")},
        {"Baurnwehr B", WEAPON_PATH("/Reforged/BP_Weapon_Reforged_Baurnwehr_B.BP_Weapon_Reforged_Baurnwehr_B_C")},
        {"Baurnwehr C", WEAPON_PATH("/Reforged/BP_Weapon_Reforged_Baurnwehr_C.BP_Weapon_Reforged_Baurnwehr_C_C")},
        {"Baurnwehr D", WEAPON_PATH("/Reforged/BP_Weapon_Reforged_Baurnwehr_D.BP_Weapon_Reforged_Baurnwehr_D_C")},
        {"Baurnwehr E", WEAPON_PATH("/Reforged/BP_Weapon_Reforged_Baurnwehr_E.BP_Weapon_Reforged_Baurnwehr_E_C")},
        {"Baurnwehr F", WEAPON_PATH("/Reforged/BP_Weapon_Reforged_Baurnwehr_F.BP_Weapon_Reforged_Baurnwehr_F_C")}
    }};

    static constexpr ItemArray<4> flailItems{{
        {"Flail A", WEAPON_PATH("/Reforged/ModularWeaponBP_Flail_A.ModularWeaponBP_Flail_A_C")},
        {"Flail B", WEAPON_PATH("/Reforged/ModularWeaponBP_Flail_B.ModularWeaponBP_Flail_B_C")},
        {"Flail C", WEAPON_PATH("/Reforged/ModularWeaponBP_Flail_C.ModularWeaponBP_Flail_C_C")},
        {"Flail D", WEAPON_PATH("/Reforged/ModularWeaponBP_Flail_D.ModularWeaponBP_Flail_D_C")}
    }};

    static constexpr ItemArray<4> billhookItems{{
        {"Billhook A", WEAPON_PATH("/Reforged/ModularWeaponBP_Billhook_A.ModularWeaponBP_Billhook_A_C")},
        {"Billhook B", WEAPON_PATH("/Reforged/ModularWeaponBP_Billhook_B.ModularWeaponBP_Billhook_B_C")},
        {"Billhook C", WEAPON_PATH("/Reforged/ModularWeaponBP_Billhook_C.ModularWeaponBP_Billhook_C_C")},
        {"Billhook D", WEAPON_PATH("/Reforged/ModularWeaponBP_Billhook_D.ModularWeaponBP_Billhook_D_C")}
    }};

    static constexpr ItemArray<4> halberdItems{{
        {"Halberd A", WEAPON_PATH("/Reforged/ModularWeaponBP_Halberd_A.ModularWeaponBP_Halberd_A_C")},
        {"Halberd B", WEAPON_PATH("/Reforged/ModularWeaponBP_Halberd_B.ModularWeaponBP_Halberd_B_C")},
        {"Halberd C", WEAPON_PATH("/Reforged/ModularWeaponBP_Halberd_C.ModularWeaponBP_Halberd_C_C")},
        {"Halberd D", WEAPON_PATH("/Reforged/ModularWeaponBP_Halberd_D.ModularWeaponBP_Halberd_D_C")}
    }};

    static constexpr ItemArray<4> spearItems{{
        {"Spear A", WEAPON_PATH("/Reforged/ModularWeaponBP_Spear_A.ModularWeaponBP_Spear_A_C")},
        {"Spear B", WEAPON_PATH("/Reforged/ModularWeaponBP_Spear_B.ModularWeaponBP_Spear_B_C")},
        {"Spear C", WEAPON_PATH("/Reforged/ModularWeaponBP_Spear_C.ModularWeaponBP_Spear_C_C")},
        {"Spear D", WEAPON_PATH("/Reforged/ModularWeaponBP_Spear_D.ModularWeaponBP_Spear_D_C")}
    }};

    static constexpr ItemArray<3> staffItems{{
        {"Staff", WEAPON_PATH("/Reforged/ModularWeaponBP_Staff.ModularWeaponBP_Staff_C")},
        {"War Staff A", WEAPON_PATH("/Reforged/ModularWeaponBP_WarStaff_A.ModularWeaponBP_WarStaff_A_C")},
        {"War Staff B", WEAPON_PATH("/Reforged/ModularWeaponBP_WarStaff_B.ModularWeaponBP_WarStaff_B_C")}
    }};

    static constexpr ItemArray<35> toolItems{{
        {"Hammer A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Hammer_A.BP_Weapon_Tool_Hammer_A_C")},
        {"Hammer B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Hammer_B.BP_Weapon_Tool_Hammer_B_C")},
        {"Hammer C", WEAPON_PATH("/Tools/BP_Weapon_Tool_Hammer_C.BP_Weapon_Tool_Hammer_C_C")},
        {"Axe A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Axe_A.BP_Weapon_Tool_Axe_A_C")},
        {"Axe B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Axe_B.BP_Weapon_Tool_Axe_B_C")},
        {"Axe C", WEAPON_PATH("/Tools/BP_Weapon_Tool_Axe_C.BP_Weapon_Tool_Axe_C_C")},
        {"Axe D", WEAPON_PATH("/Tools/BP_Weapon_Tool_Axe_D.BP_Weapon_Tool_Axe_D_C")},
        {"Chisel B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Chisel_B.BP_Weapon_Tool_Chisel_B_C")},
        {"Chisel C", WEAPON_PATH("/Tools/BP_Weapon_Tool_Chisel_C.BP_Weapon_Tool_Chisel_C_C")},
        {"Hoe A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Hoe_A.BP_Weapon_Tool_Hoe_A_C")},
        {"Hoe B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Hoe_B.BP_Weapon_Tool_Hoe_B_C")},
        {"Knife A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Knife_A.BP_Weapon_Tool_Knife_A_C")},
        {"Knife B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Knife_B.BP_Weapon_Tool_Knife_B_C")},
        {"Knife C", WEAPON_PATH("/Tools/BP_Weapon_Tool_Knife_C.BP_Weapon_Tool_Knife_C_C")},
        {"Lid A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Lid_A.BP_Weapon_Tool_Lid_A_C")},
        {"Mallet A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Mallet_A.BP_Weapon_Tool_Mallet_A_C")},
        {"Mallet B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Mallet_B.BP_Weapon_Tool_Mallet_B_C")},
        {"Mallet C", WEAPON_PATH("/Tools/BP_Weapon_Tool_Mallet_C.BP_Weapon_Tool_Mallet_C_C")},
        {"Maul", WEAPON_PATH("/Tools/BP_Weapon_Tool_Maul.BP_Weapon_Tool_Maul_C")},
        {"Pickaxe A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Pickaxe_A.BP_Weapon_Tool_Pickaxe_A_C")},
        {"Pickaxe B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Pickaxe_B.BP_Weapon_Tool_Pickaxe_B_C")},
        {"Pitchfork A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Pitchfork_A.BP_Weapon_Tool_Pitchfork_A_C")},
        {"Pitchfork B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Pitchfork_B.BP_Weapon_Tool_Pitchfork_B_C")},
        {"Rake", WEAPON_PATH("/Tools/BP_Weapon_Tool_Rake.BP_Weapon_Tool_Rake_C")},
        {"Scissors", WEAPON_PATH("/Tools/BP_Weapon_Tool_Scissors.BP_Weapon_Tool_Scissors_C")},
        {"Scythe A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Scythe_A.BP_Weapon_Tool_Scythe_A_C")},
        {"Shovel A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Shovel_A.BP_Weapon_Tool_Shovel_A_C")},
        {"Shovel B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Shovel_B.BP_Weapon_Tool_Shovel_B_C")},
        {"Sickle A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Sickle_A.BP_Weapon_Tool_Sickle_A_C")},
        {"Sickle B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Sickle_B.BP_Weapon_Tool_Sickle_B_C")},
        {"Sickle C", WEAPON_PATH("/Tools/BP_Weapon_Tool_Sickle_C.BP_Weapon_Tool_Sickle_C_C")},
        {"Sickle D", WEAPON_PATH("/Tools/BP_Weapon_Tool_Sickle_D.BP_Weapon_Tool_Sickle_D_C")},
        {"Sickle E", WEAPON_PATH("/Tools/BP_Weapon_Tool_Sickle_E.BP_Weapon_Tool_Sickle_E_C")},
        {"Tongs", WEAPON_PATH("/Tools/BP_Weapon_Tool_Tongs.BP_Weapon_Tool_Tongs_C")},
        {"Tool Flail", WEAPON_PATH("/Tools/ModularWeaponBP_Tool_Flail.ModularWeaponBP_Tool_Flail_C")}
    }};

    static constexpr ItemArray<10> shieldItems{{
        {"Buckler Shield", WEAPON_PATH("/Shield_Buckler.Shield_Buckler_C")},
        {"Buckler Shield 2", WEAPON_PATH("/Shield_Buckler_2.Shield_Buckler_2_C")},
        {"Buckler Shield 3", WEAPON_PATH("/Shield_Buckler_3.Shield_Buckler_3_C")},
        {"Buckler Shield Gold", WEAPON_PATH("/Shield_Buckler_Gold.Shield_Buckler_Gold_C")},
        {"Boss Grip Shield", WEAPON_PATH("/Shield_BossGrip.Shield_BossGrip_C")},
        {"Light Pavise", WEAPON_PATH("/Shield_Pavise_Light.Shield_Pavise_Light_C")},
        {"Heavy Pavise", WEAPON_PATH("/Shield_Pavise_Heavy.Shield_Pavise_Heavy_C")},
        {"Tower Pavise", WEAPON_PATH("/Shield_Pavise_Tower.Shield_Pavise_Tower_C")},
        {"Tagre Shield", WEAPON_PATH("/Shield_Tagre.Shield_Tagre_C")},
        {"Tagre Shield Gold", WEAPON_PATH("/Shield_Tagre_Gold.Shield_Tagre_Gold_C")}
    }};

    static constexpr ItemArray<5> improvisedItems{{
        {"Small Candlestick", WEAPON_PATH("/Improvized/BP_Weapon_Improv_CandleStick_Small.BP_Weapon_Improv_CandleStick_Small_C")},
        {"Big Candlestick", WEAPON_PATH("/Improvized/BP_Weapon_Improv_CandleStick_Big.BP_Weapon_Improv_CandleStick_Big_C")},
        {"Lantern", WEAPON_PATH("/Improvized/BP_Weapon_Improv_Lantern.BP_Weapon_Improv_Lantern_C")},
        {"Stool", WEAPON_PATH("/Improvized/BP_Weapon_Improv_Stool.BP_Weapon_Improv_Stool_C")},
        {"Barrel Lid Small", WEAPON_PATH("/Improvized/BP_Weapon_Improv_Barrel_Lid_Small.BP_Weapon_Improv_Barrel_Lid_Small_C")}
    }};

    static constexpr ItemArray<3> rangedItems{{
        {"Crossbow Light", WEAPON_PATH("/Ranged/Weapon/BP_Weapon_Ranged_Weapon_Crossbow_Light.BP_Weapon_Ranged_Weapon_Crossbow_Light_C")},
        {"Bolt", WEAPON_PATH("/Ranged/Projectile/BP_Weapon_Ranged_Projectle_Bolt_1.BP_Weapon_Ranged_Projectle_Bolt_1_C")},
        {"Quiver Bolt", WEAPON_PATH("/Ranged/Quiver/BP_Quiver_Bolt_1.BP_Quiver_Bolt_1_C")}
    }};

    static constexpr ItemArray<4> treasureItems{{
        {"Chalice", WEAPON_PATH("/Treasure/BP_Weapon_Treasure_Chalice_001.BP_Weapon_Treasure_Chalice_001_C")},
        {"Goblet 001", WEAPON_PATH("/Treasure/BP_Weapon_Treasure_Goblet_001.BP_Weapon_Treasure_Goblet_001_C")},
        {"Goblet 002", WEAPON_PATH("/Treasure/BP_Weapon_Treasure_Goblet_002.BP_Weapon_Treasure_Goblet_002_C")},
        {"Paten", WEAPON_PATH("/Treasure/BP_Weapon_Treasure_Paten_001.BP_Weapon_Treasure_Paten_001_C")}
    }};

    static constexpr ItemArray<3> uniqueWeaponItems{{
        {"Baron Beak", WEAPON_PATH("/Unique/ModularWeaponBP_BaronBeak.ModularWeaponBP_BaronBeak_C")},
        {"Weapon Trap", WEAPON_PATH("/BP_Weapon_Trap.BP_Weapon_Trap_C")},
        {"Weapon Trap Kettle", WEAPON_PATH("/BP_Weapon_Trap_Kettle.BP_Weapon_Trap_Kettle_C")}
    }};

    static constexpr ItemArray<42> helmetItems{{
        {"Armet", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Armet_001.BP_Armor_Head_Armet_001_C")},
        {"Armet Gold", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Armet_001_G.BP_Armor_Head_Armet_001_G_C")},
        {"Armet R", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Armet_001_R.BP_Armor_Head_Armet_001_R_C")},
        {"Armet B", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Armet_001_B.BP_Armor_Head_Armet_001_B_C")},
        {"Barbute A", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Barbute_A.BP_Armor_Head_Barbute_A_C")},
        {"Barbute B", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Barbute_B.BP_Armor_Head_Barbute_B_C")},
        {"Open Sallet A", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Open_A_001.BP_Armor_Head_Sallet_Open_A_001_C")},
        {"Open Sallet B", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Open_B_001.BP_Armor_Head_Sallet_Open_B_001_C")},
        {"Open Sallet CA", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Open_CA_001.BP_Armor_Head_Sallet_Open_CA_001_C")},
        {"Open Sallet CB", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Open_CB_001.BP_Armor_Head_Sallet_Open_CB_001_C")},
        {"Solid Sallet A 001", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Solid_A_001.BP_Armor_Head_Sallet_Solid_A_001_C")},
        {"Solid Sallet A 002", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Solid_A_002.BP_Armor_Head_Sallet_Solid_A_002_C")},
        {"Solid Sallet B 001", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Solid_B_001.BP_Armor_Head_Sallet_Solid_B_001_C")},
        {"Solid Sallet B 002", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Solid_B_002.BP_Armor_Head_Sallet_Solid_B_002_C")},
        {"Solid Sallet C 001", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Solid_C_001.BP_Armor_Head_Sallet_Solid_C_001_C")},
        {"Solid Sallet C 002", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Solid_C_002.BP_Armor_Head_Sallet_Solid_C_002_C")},
        {"Solid Sallet D 001", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Solid_D_001.BP_Armor_Head_Sallet_Solid_D_001_C")},
        {"Solid Sallet D 002", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Solid_D_002.BP_Armor_Head_Sallet_Solid_D_002_C")},
        {"Solid Sallet E 001", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Solid_E_001.BP_Armor_Head_Sallet_Solid_E_001_C")},
        {"Solid Sallet E 002", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Solid_E_002.BP_Armor_Head_Sallet_Solid_E_002_C")},
        {"Visor Sallet A", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Visor_A_001.BP_Armor_Head_Sallet_Visor_A_001_C")},
        {"Visor Sallet B", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Sallet_Visor_B_001.BP_Armor_Head_Sallet_Visor_B_001_C")},
        {"Kettle Helm A", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Kettle_Helm_A.BP_Armor_Head_Kettle_Helm_A_C")},
        {"Kettle Helm B", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Kettle_Helm_B.BP_Armor_Head_Kettle_Helm_B_C")},
        {"Kettle Helm B 2", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Kettle_Helm_B_2.BP_Armor_Head_Kettle_Helm_B_2_C")},
        {"Kettle Helm C", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Kettle_Helm_C.BP_Armor_Head_Kettle_Helm_C_C")},
        {"Kettle Helm D", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Kettle_Helm_D.BP_Armor_Head_Kettle_Helm_D_C")},
        {"Kettle Helm D 2", ARMOR_PATH("/Metal/Head/BP_Armor_Head_Kettle_Helm_D_2.BP_Armor_Head_Kettle_Helm_D_2_C")},
        {"Bycocket A 001", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Bycocket_A_001.BP_Armor_Head_Hat_Bycocket_A_001_C")},
        {"Bycocket A 002", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Bycocket_A_002.BP_Armor_Head_Hat_Bycocket_A_002_C")},
        {"Bycocket B 001", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Bycocket_B_001.BP_Armor_Head_Hat_Bycocket_B_001_C")},
        {"Bycocket B 002", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Bycocket_B_002.BP_Armor_Head_Hat_Bycocket_B_002_C")},
        {"Bycocket C", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Bycocket_C_001.BP_Armor_Head_Hat_Bycocket_C_001_C")},
        {"Bycocket D", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Bycocket_D_001.BP_Armor_Head_Hat_Bycocket_D_001_C")},
        {"Bycocket E", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Bycocket_E_001.BP_Armor_Head_Hat_Bycocket_E_001_C")},
        {"Bycocket F", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Bycocket_F_001.BP_Armor_Head_Hat_Bycocket_F_001_C")},
        {"Bycocket G", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Bycocket_G_001.BP_Armor_Head_Hat_Bycocket_G_001_C")},
        {"Bycocket H", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Bycocket_H_001.BP_Armor_Head_Hat_Bycocket_H_001_C")},
        {"Bycocket I", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Bycocket_I_001.BP_Armor_Head_Hat_Bycocket_I_001_C")},
        {"Bycocket J", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Bycocket_J_001.BP_Armor_Head_Hat_Bycocket_J_001_C")},
        {"Gnome Hat A", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Gnome_A.BP_Armor_Head_Hat_Gnome_A_C")},
        {"Gnome Hat B", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Gnome_B.BP_Armor_Head_Hat_Gnome_B_C")}
    }};

    static constexpr ItemArray<29> bodyArmorItems{{
        {"Cuirass A", ARMOR_PATH("/Metal/BP_Armor_Body_Cuirass_A.BP_Armor_Body_Cuirass_A_C")},
        {"Cuirass B", ARMOR_PATH("/Metal/BP_Armor_Body_Cuirass_B.BP_Armor_Body_Cuirass_B_C")},
        {"Cuirass Faulds", ARMOR_PATH("/Metal/BP_Armor_Body_Cuirass_Faulds.BP_Armor_Body_Cuirass_Faulds_C")},
        {"Cuirass Plackard", ARMOR_PATH("/Metal/BP_Armor_Body_Cuirass_Plackard.BP_Armor_Body_Cuirass_Plackard_C")},
        {"Cuirass Tassets", ARMOR_PATH("/Metal/BP_Armor_Body_Cuirass_Tassets.BP_Armor_Body_Cuirass_Tassets_C")},
        {"Cuirass A T2", ARMOR_PATH("/Metal/Chest/BP_Armor_Body_Cuirass_A_T2.BP_Armor_Body_Cuirass_A_T2_C")},
        {"Cuirass B T1", ARMOR_PATH("/Metal/Chest/BP_Armor_Body_Cuirass_B_T1.BP_Armor_Body_Cuirass_B_T1_C")},
        {"Cuirass B T2", ARMOR_PATH("/Metal/Chest/BP_Armor_Body_Cuirass_B_T2.BP_Armor_Body_Cuirass_B_T2_C")},
        {"Cuirass C T1", ARMOR_PATH("/Metal/Chest/BP_Armor_Body_Cuirass_C_T1.BP_Armor_Body_Cuirass_C_T1_C")},
        {"Cuirass C T2", ARMOR_PATH("/Metal/Chest/BP_Armor_Body_Cuirass_C_T2.BP_Armor_Body_Cuirass_C_T2_C")},
        {"Cuirass C T3", ARMOR_PATH("/Metal/Chest/BP_Armor_Body_Cuirass_C_T3.BP_Armor_Body_Cuirass_C_T3_C")},
        {"Cuirass C T3 B", ARMOR_PATH("/Metal/Chest/BP_Armor_Body_Cuirass_C_T3_B.BP_Armor_Body_Cuirass_C_T3_B_C")},
        {"Cuirass C T3 Gold", ARMOR_PATH("/Metal/Chest/BP_Armor_Body_Cuirass_C_T3_G.BP_Armor_Body_Cuirass_C_T3_G_C")},
        {"Breastplate A T1", ARMOR_PATH("/Metal/Chest/BP_Armor_Body_Brestplate_A_T1.BP_Armor_Body_Brestplate_A_T1_C")},
        {"Breastplate A T2", ARMOR_PATH("/Metal/Chest/BP_Armor_Body_Brestplate_A_T2.BP_Armor_Body_Brestplate_A_T2_C")},
        {"Breastplate B T1", ARMOR_PATH("/Metal/Chest/BP_Armor_Body_Brestplate_B_T1.BP_Armor_Body_Brestplate_B_T1_C")},
        {"Breastplate B T2", ARMOR_PATH("/Metal/Chest/BP_Armor_Body_Brestplate_B_T2.BP_Armor_Body_Brestplate_B_T2_C")},
        {"Plackard T2", ARMOR_PATH("/Metal/Chest/BP_Armor_Body_Plackard_T2.BP_Armor_Body_Plackard_T2_C")},
        {"Gambeson A T1", ARMOR_PATH("/BP_Armor_Body_Gambeson_A_T1.BP_Armor_Body_Gambeson_A_T1_C")},
        {"Gambeson A T2", ARMOR_PATH("/BP_Armor_Body_Gambeson_A_T2.BP_Armor_Body_Gambeson_A_T2_C")},
        {"Gambeson A Blue", ARMOR_PATH("/Unique/BP_Armor_Gambeson_A_Blue.BP_Armor_Gambeson_A_Blue_C")},
        {"Gambeson B T1", ARMOR_PATH("/BP_Armor_Body_Gambeson_B_T1.BP_Armor_Body_Gambeson_B_T1_C")},
        {"Gambeson B T2", ARMOR_PATH("/BP_Armor_Body_Gambeson_B_T2.BP_Armor_Body_Gambeson_B_T2_C")},
        {"Doublet", ARMOR_PATH("/Cloth/BP_Armor_Body_Doublet.BP_Armor_Body_Doublet_C")},
        {"Arming Doublet", ARMOR_PATH("/Cloth/BP_Armor_Body_Doublet_Arming.BP_Armor_Body_Doublet_Arming_C")},
        {"Arming Doublet Black", ARMOR_PATH("/Unique/BP_Armor_Doublet_Black.BP_Armor_Doublet_Black_C")},
        {"Arming Doublet Purple", ARMOR_PATH("/Cloth/BP_Armor_Body_Doublet_Arming_Purple.BP_Armor_Body_Doublet_Arming_Purple_C")},
        {"Shirt A", ARMOR_PATH("/Cloth/BP_Armor_Body_Shirt_A.BP_Armor_Body_Shirt_A_C")},
        {"Shirt B", ARMOR_PATH("/Cloth/BP_Armor_Body_Shirt_B.BP_Armor_Body_Shirt_B_C")}
    }};

    static constexpr ItemArray<13> armItems{{
        {"Couter L", ARMOR_PATH("/Metal/BP_Armor_Arms_Couter_L.BP_Armor_Arms_Couter_L_C")},
        {"Couter R", ARMOR_PATH("/Metal/BP_Armor_Arms_Couter_R.BP_Armor_Arms_Couter_R_C")},
        {"Vambrace A", ARMOR_PATH("/Metal/BP_Armor_Arms_Vambrace_A.BP_Armor_Arms_Vambrace_A_C")},
        {"Vambrace A T2", ARMOR_PATH("/Metal/Arms/BP_Armor_Arms_Vambrace_A_T2.BP_Armor_Arms_Vambrace_A_T2_C")},
        {"Vambrace A T3", ARMOR_PATH("/Metal/Arms/BP_Armor_Arms_Vambrace_A_T3.BP_Armor_Arms_Vambrace_A_T3_C")},
        {"Vambrace B T2", ARMOR_PATH("/Metal/Arms/BP_Armor_Arms_Vambrace_B_T2.BP_Armor_Arms_Vambrace_B_T2_C")},
        {"Vambrace B T3", ARMOR_PATH("/Metal/Arms/BP_Armor_Arms_Vambrace_B_T3.BP_Armor_Arms_Vambrace_B_T3_C")},
        {"Vambrace C T2", ARMOR_PATH("/Metal/Arms/BP_Armor_Arms_Vambrace_C_T2.BP_Armor_Arms_Vambrace_C_T2_C")},
        {"Vambrace C T3", ARMOR_PATH("/Metal/Arms/BP_Armor_Arms_Vambrace_C_T3.BP_Armor_Arms_Vambrace_C_T3_C")},
        {"Vambrace C T3 B", ARMOR_PATH("/Metal/Arms/BP_Armor_Arms_Vambrace_C_T3_B.BP_Armor_Arms_Vambrace_C_T3_B_C")},
        {"Vambrace C T3 Gold", ARMOR_PATH("/Metal/Arms/BP_Armor_Arms_Vambrace_C_T3_G.BP_Armor_Arms_Vambrace_C_T3_G_C")},
        {"Chains T1", ARMOR_PATH("/Metal/Arms/BP_Armor_Arms_Chains_T1.BP_Armor_Arms_Chains_T1_C")},
        {"Chains T2", ARMOR_PATH("/Metal/Arms/BP_Armor_Arms_Chains_T2.BP_Armor_Arms_Chains_T2_C")}
    }};

    static constexpr ItemArray<18> legItems{{
        {"Cuisse A", ARMOR_PATH("/Metal/BP_Armor_Legs_Cuisse_A.BP_Armor_Legs_Cuisse_A_C")},
        {"Cuisse A T2", ARMOR_PATH("/Metal/Legs/BP_Armor_Legs_Cuisse_A_T2.BP_Armor_Legs_Cuisse_A_T2_C")},
        {"Cuisse A T3", ARMOR_PATH("/Metal/Legs/BP_Armor_Legs_Cuisse_A_T3.BP_Armor_Legs_Cuisse_A_T3_C")},
        {"Cuisse B", ARMOR_PATH("/Metal/Legs/BP_Armor_Legs_Cuisse_B.BP_Armor_Legs_Cuisse_B_C")},
        {"Cuisse G", ARMOR_PATH("/Metal/Legs/BP_Armor_Legs_Cuisse_G.BP_Armor_Legs_Cuisse_G_C")},
        {"Greaves T2", ARMOR_PATH("/Metal/Legs/BP_Armor_Legs_Greaves_T2.BP_Armor_Legs_Greaves_T2_C")},
        {"Greaves T3", ARMOR_PATH("/Metal/Legs/BP_Armor_Legs_Greaves_T3.BP_Armor_Legs_Greaves_T3_C")},
        {"Hosen A", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_A.BP_Armor_Legs_Hosen_A_C")},
        {"Hosen B", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_B.BP_Armor_Legs_Hosen_B_C")},
        {"Hosen C", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_C.BP_Armor_Legs_Hosen_C_C")},
        {"Hosen C Poor", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_C_Poor.BP_Armor_Legs_Hosen_C_Poor_C")},
        {"Hosen C Black", ARMOR_PATH("/Unique/BP_Armor_Hosen_C_Black.BP_Armor_Hosen_C_Black_C")},
        {"Arming Hosen A", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_Arming_A.BP_Armor_Legs_Hosen_Arming_A_C")},
        {"Arming Hosen A Purple", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_Arming_A_Purple.BP_Armor_Legs_Hosen_Arming_A_Purple_C")},
        {"Arming Hosen B", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_Arming_B.BP_Armor_Legs_Hosen_Arming_B_C")},
        {"Arming Hosen C", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_Arming_C.BP_Armor_Legs_Hosen_Arming_C_C")},
        {"Arming Hosen C Blue", ARMOR_PATH("/Unique/BP_Armor_Hosen_Arming_C_Blue.BP_Armor_Hosen_Arming_C_Blue_C")},
        {"Panties", ARMOR_PATH("/Cloth/BP_Armor_Legs_Panties.BP_Armor_Legs_Panties_C")}
    }};

    static constexpr ItemArray<8> handItems{{
        {"Gauntlets", ARMOR_PATH("/Metal/BP_Armor_Hands_Gauntlets.BP_Armor_Hands_Gauntlets_C")},
        {"Gauntlets T1", ARMOR_PATH("/Metal/Hands/BP_Armor_Hands_Gauntlets_T1.BP_Armor_Hands_Gauntlets_T1_C")},
        {"Gauntlets T2", ARMOR_PATH("/Metal/Hands/BP_Armor_Hands_Gauntlets_T2.BP_Armor_Hands_Gauntlets_T2_C")},
        {"Gauntlets T3", ARMOR_PATH("/Metal/Hands/BP_Armor_Hands_Gauntlets_T3.BP_Armor_Hands_Gauntlets_T3_C")},
        {"Gauntlets T3 Gold", ARMOR_PATH("/Metal/Hands/BP_Armor_Hands_Gauntlets_T3_G.BP_Armor_Hands_Gauntlets_T3_G_C")},
        {"Gauntlets T3B", ARMOR_PATH("/Metal/Hands/BP_Armor_Hands_Gauntlets_T3B.BP_Armor_Hands_Gauntlets_T3B_C")},
        {"Half Gauntlets T1", ARMOR_PATH("/Metal/Hands/BP_Armor_Hands_HalfGauntlets_T1.BP_Armor_Hands_HalfGauntlets_T1_C")},
        {"Half Gauntlets T2", ARMOR_PATH("/Metal/Hands/BP_Armor_Hands_HalfGauntlets_T2.BP_Armor_Hands_HalfGauntlets_T2_C")}
    }};

    static constexpr ItemArray<6> feetItems{{
        {"Shoes A", ARMOR_PATH("/BP_Armor_Feet_Shoes_A.BP_Armor_Feet_Shoes_A_C")},
        {"Shoes B", ARMOR_PATH("/BP_Armor_Feet_Shoes_B.BP_Armor_Feet_Shoes_B_C")},
        {"Shoes C", ARMOR_PATH("/BP_Armor_Feet_Shoes_C.BP_Armor_Feet_Shoes_C_C")},
        {"Sabatons A", ARMOR_PATH("/Metal/Feet/BP_Armor_Feet_Sabbatons_A.BP_Armor_Feet_Sabbatons_A_C")},
        {"Sabatons A Gold", ARMOR_PATH("/Metal/Feet/BP_Armor_Feet_Sabbatons_A_G.BP_Armor_Feet_Sabbatons_A_G_C")},
        {"Sabatons AB", ARMOR_PATH("/Metal/Feet/BP_Armor_Feet_Sabbatons_AB.BP_Armor_Feet_Sabbatons_AB_C")}
    }};

    static constexpr ItemArray<6> neckItems{{
        {"Bevor", ARMOR_PATH("/Metal/BP_Armor_Neck_Bevor.BP_Armor_Neck_Bevor_C")},
        {"Standard T1", ARMOR_PATH("/Mail/BP_Armor_Neck_Standard_T1.BP_Armor_Neck_Standard_T1_C")},
        {"Standard T2", ARMOR_PATH("/Mail/BP_Armor_Neck_Standard_T2.BP_Armor_Neck_Standard_T2_C")},
        {"Standard T3", ARMOR_PATH("/Mail/BP_Armor_Neck_Standard_T3.BP_Armor_Neck_Standard_T3_C")},
        {"Bevor T2", ARMOR_PATH("/Metal/Neck/BP_Armor_Neck_Bevor_T2.BP_Armor_Neck_Bevor_T2_C")},
        {"Bevor T3", ARMOR_PATH("/Metal/Neck/BP_Armor_Neck_Bevor_T3.BP_Armor_Neck_Bevor_T3_C")}
    }};

    static constexpr ItemArray<7> shoulderItems{{
        {"Spaulder A T2", ARMOR_PATH("/Metal/Shoulders/BP_Armor_Shoulders_Spaulder_A_T2.BP_Armor_Shoulders_Spaulder_A_T2_C")},
        {"Spaulder A T3", ARMOR_PATH("/Metal/Shoulders/BP_Armor_Shoulders_Spaulder_A_T3.BP_Armor_Shoulders_Spaulder_A_T3_C")},
        {"Spaulder B T2", ARMOR_PATH("/Metal/Shoulders/BP_Armor_Shoulders_Spaulder_B_T2.BP_Armor_Shoulders_Spaulder_B_T2_C")},
        {"Spaulder B T3", ARMOR_PATH("/Metal/Shoulders/BP_Armor_Shoulders_Spaulder_B_T3.BP_Armor_Shoulders_Spaulder_B_T3_C")},
        {"Pauldron C", ARMOR_PATH("/Metal/Shoulders/BP_Armor_Shoulders_Pauldron_C.BP_Armor_Shoulders_Pauldron_C_C")},
        {"Pauldron C Gold", ARMOR_PATH("/Metal/Shoulders/BP_Armor_Shoulders_Pauldron_C_G.BP_Armor_Shoulders_Pauldron_C_G_C")},
        {"Pauldron C B", ARMOR_PATH("/Metal/Shoulders/BP_Armor_Shoulders_Pauldron_C_B.BP_Armor_Shoulders_Pauldron_C_B_C")}
    }};

    static constexpr ItemArray<3> waistItems{{
        {"Foulds T1", ARMOR_PATH("/Mail/BP_Armor_Waist_Foulds_T1.BP_Armor_Waist_Foulds_T1_C")},
        {"Foulds T2", ARMOR_PATH("/Mail/BP_Armor_Waist_Foulds_T2.BP_Armor_Waist_Foulds_T2_C")},
        {"Foulds T3", ARMOR_PATH("/Mail/BP_Armor_Waist_Foulds_T3.BP_Armor_Waist_Foulds_T3_C")}
    }};

    static constexpr ItemArray<121> modularArmorCoreItems{{
        {"Head Armet", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Armet.BP_Armor_Modular_Core_Head_Armet_C")},
        {"Head Armet Baron", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Armet_Baron.BP_Armor_Modular_Core_Head_Armet_Baron_C")},
        {"Head Barbute 1", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Barbute_1.BP_Armor_Modular_Core_Head_Barbute_1_C")},
        {"Head Barbute B 001", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Barbute_B_001.BP_Armor_Modular_Core_Head_Barbute_B_001_C")},
        {"Head Cap 1", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Cap_1.BP_Armor_Modular_Core_Head_Cap_1_C")},
        {"Head Cap 2", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Cap_2.BP_Armor_Modular_Core_Head_Cap_2_C")},
        {"Head Cap 3", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Cap_3.BP_Armor_Modular_Core_Head_Cap_3_C")},
        {"Head Cap 4", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Cap_4.BP_Armor_Modular_Core_Head_Cap_4_C")},
        {"Head Eisenhut AA 001", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Eisenhut_AA_001.BP_Armor_Modular_Core_Head_Eisenhut_AA_001_C")},
        {"Head Eisenhut AB 001", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Eisenhut_AB_001.BP_Armor_Modular_Core_Head_Eisenhut_AB_001_C")},
        {"Head Eisenhut B 002", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Eisenhut_B_002.BP_Armor_Modular_Core_Head_Eisenhut_B_002_C")},
        {"Head Eisenhut G 002", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Eisenhut_G_002.BP_Armor_Modular_Core_Head_Eisenhut_G_002_C")},
        {"Head Hat 1", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Hat_1.BP_Armor_Modular_Core_Head_Hat_1_C")},
        {"Head Hat 2", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Hat_2.BP_Armor_Modular_Core_Head_Hat_2_C")},
        {"Head Hat 3", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Hat_3.BP_Armor_Modular_Core_Head_Hat_3_C")},
        {"Head Hat 4", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Hat_4.BP_Armor_Modular_Core_Head_Hat_4_C")},
        {"Head KettleHelm 1", MODULAR_PATH("/BP_Armor_Modular_Core_Head_KettleHelm_1.BP_Armor_Modular_Core_Head_KettleHelm_1_C")},
        {"Head KettleHelm 2", MODULAR_PATH("/BP_Armor_Modular_Core_Head_KettleHelm_2.BP_Armor_Modular_Core_Head_KettleHelm_2_C")},
        {"Head KettleHelm 3", MODULAR_PATH("/BP_Armor_Modular_Core_Head_KettleHelm_3.BP_Armor_Modular_Core_Head_KettleHelm_3_C")},
        {"Head KettleHelm 4", MODULAR_PATH("/BP_Armor_Modular_Core_Head_KettleHelm_4.BP_Armor_Modular_Core_Head_KettleHelm_4_C")},
        {"Head KettleHelm 5", MODULAR_PATH("/BP_Armor_Modular_Core_Head_KettleHelm_5.BP_Armor_Modular_Core_Head_KettleHelm_5_C")},
        {"Head KettleHelm F 004", MODULAR_PATH("/BP_Armor_Modular_Core_Head_KettleHelm_F_004.BP_Armor_Modular_Core_Head_KettleHelm_F_004_C")},
        {"Head KettleHelm G 004", MODULAR_PATH("/BP_Armor_Modular_Core_Head_KettleHelm_G_004.BP_Armor_Modular_Core_Head_KettleHelm_G_004_C")},
        {"Head Sallet 1", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_1.BP_Armor_Modular_Core_Head_Sallet_1_C")},
        {"Head Sallet 2", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_2.BP_Armor_Modular_Core_Head_Sallet_2_C")},
        {"Head Sallet 3", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_3.BP_Armor_Modular_Core_Head_Sallet_3_C")},
        {"Head Sallet 4", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_4.BP_Armor_Modular_Core_Head_Sallet_4_C")},
        {"Head Sallet Open A 001", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_Open_A_001.BP_Armor_Modular_Core_Head_Sallet_Open_A_001_C")},
        {"Head Sallet Open A 002", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_Open_A_002.BP_Armor_Modular_Core_Head_Sallet_Open_A_002_C")},
        {"Head Sallet Open A 003", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_Open_A_003.BP_Armor_Modular_Core_Head_Sallet_Open_A_003_C")},
        {"Head Sallet Solid A 001", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_Solid_A_001.BP_Armor_Modular_Core_Head_Sallet_Solid_A_001_C")},
        {"Head Sallet Solid A 003", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_Solid_A_003.BP_Armor_Modular_Core_Head_Sallet_Solid_A_003_C")},
        {"Head Sallet Solid A 004", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_Solid_A_004.BP_Armor_Modular_Core_Head_Sallet_Solid_A_004_C")},
        {"Head Sallet Solid B 003", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_Solid_B_003.BP_Armor_Modular_Core_Head_Sallet_Solid_B_003_C")},
        {"Head Sallet Solid B 004", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_Solid_B_004.BP_Armor_Modular_Core_Head_Sallet_Solid_B_004_C")},
        {"Head Sallet Solid C 001", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_Solid_C_001.BP_Armor_Modular_Core_Head_Sallet_Solid_C_001_C")},
        {"Head Sallet Visor A 001", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_Visor_A_001.BP_Armor_Modular_Core_Head_Sallet_Visor_A_001_C")},
        {"Head Sallet Visor A 002", MODULAR_PATH("/BP_Armor_Modular_Core_Head_Sallet_Visor_A_002.BP_Armor_Modular_Core_Head_Sallet_Visor_A_002_C")},
        {"Body Doublet 1", MODULAR_PATH("/BP_Armor_Modular_Core_Body_Doublet_1.BP_Armor_Modular_Core_Body_Doublet_1_C")},
        {"Body Doublet 2", MODULAR_PATH("/BP_Armor_Modular_Core_Body_Doublet_2.BP_Armor_Modular_Core_Body_Doublet_2_C")},
        {"Body Doublet 3", MODULAR_PATH("/BP_Armor_Modular_Core_Body_Doublet_3.BP_Armor_Modular_Core_Body_Doublet_3_C")},
        {"Body Doublet Arming", MODULAR_PATH("/BP_Armor_Modular_Core_Body_Doublet_Arming.BP_Armor_Modular_Core_Body_Doublet_Arming_C")},
        {"Body Doublet Arming 2", MODULAR_PATH("/BP_Armor_Modular_Core_Body_Doublet_Arming_2.BP_Armor_Modular_Core_Body_Doublet_Arming_2_C")},
        {"Body Hauberk 1", MODULAR_PATH("/BP_Armor_Modular_Core_Body_Hauberk_1.BP_Armor_Modular_Core_Body_Hauberk_1_C")},
        {"Body Shirt 1", MODULAR_PATH("/BP_Armor_Modular_Core_Body_Shirt_1.BP_Armor_Modular_Core_Body_Shirt_1_C")},
        {"Body Tabard 1", MODULAR_PATH("/BP_Armor_Modular_Core_Body_Tabard_1.BP_Armor_Modular_Core_Body_Tabard_1_C")},
        {"Body Tunic 1", MODULAR_PATH("/BP_Armor_Modular_Core_Body_Tunic_1.BP_Armor_Modular_Core_Body_Tunic_1_C")},
        {"Chest Breastplate 1", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Breastplate_1.BP_Armor_Modular_Core_Chest_Breastplate_1_C")},
        {"Chest Breastplate 2", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Breastplate_2.BP_Armor_Modular_Core_Chest_Breastplate_2_C")},
        {"Chest Breastplate 3", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Breastplate_3.BP_Armor_Modular_Core_Chest_Breastplate_3_C")},
        {"Chest Breastplate 4", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Breastplate_4.BP_Armor_Modular_Core_Chest_Breastplate_4_C")},
        {"Chest Breastplate 5", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Breastplate_5.BP_Armor_Modular_Core_Chest_Breastplate_5_C")},
        {"Chest Breastplate 6", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Breastplate_6.BP_Armor_Modular_Core_Chest_Breastplate_6_C")},
        {"Chest Breastplate 10", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Breastplate_10.BP_Armor_Modular_Core_Chest_Breastplate_10_C")},
        {"Chest Breastplate 18", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Breastplate_18.BP_Armor_Modular_Core_Chest_Breastplate_18_C")},
        {"Chest Cuirass 1", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Cuirass_1.BP_Armor_Modular_Core_Chest_Cuirass_1_C")},
        {"Chest Cuirass 2", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Cuirass_2.BP_Armor_Modular_Core_Chest_Cuirass_2_C")},
        {"Chest Cuirass Baron", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Cuirass_Baron.BP_Armor_Modular_Core_Chest_Cuirass_Baron_C")},
        {"Chest Cuirass Brust 1", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Cuirass_Brust_1.BP_Armor_Modular_Core_Chest_Cuirass_Brust_1_C")},
        {"Chest Cuirass Brust 2", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Cuirass_Brust_2.BP_Armor_Modular_Core_Chest_Cuirass_Brust_2_C")},
        {"Chest Cuirass Gothic 5", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Cuirass_Gothic_5.BP_Armor_Modular_Core_Chest_Cuirass_Gothic_5_C")},
        {"Chest Cuirass Gothic 6", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Cuirass_Gothic_6.BP_Armor_Modular_Core_Chest_Cuirass_Gothic_6_C")},
        {"Chest Gambeson 1", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Gambeson_1.BP_Armor_Modular_Core_Chest_Gambeson_1_C")},
        {"Chest Jack 1", MODULAR_PATH("/BP_Armor_Modular_Core_Chest_Jack_1.BP_Armor_Modular_Core_Chest_Jack_1_C")},
        {"Arms Harness 001", MODULAR_PATH("/BP_Armor_Modular_Core_Arms_Harness_001.BP_Armor_Modular_Core_Arms_Harness_001_C")},
        {"Arms Harness 002", MODULAR_PATH("/BP_Armor_Modular_Core_Arms_Harness_002.BP_Armor_Modular_Core_Arms_Harness_002_C")},
        {"Arms Harness 003", MODULAR_PATH("/BP_Armor_Modular_Core_Arms_Harness_003.BP_Armor_Modular_Core_Arms_Harness_003_C")},
        {"Arms Vambrace 1", MODULAR_PATH("/BP_Armor_Modular_Core_Arms_Vambrace_1.BP_Armor_Modular_Core_Arms_Vambrace_1_C")},
        {"Arms Vambrace 2", MODULAR_PATH("/BP_Armor_Modular_Core_Arms_Vambrace_2.BP_Armor_Modular_Core_Arms_Vambrace_2_C")},
        {"Arms Vambrace 3", MODULAR_PATH("/BP_Armor_Modular_Core_Arms_Vambrace_3.BP_Armor_Modular_Core_Arms_Vambrace_3_C")},
        {"Arms Vambrace 4", MODULAR_PATH("/BP_Armor_Modular_Core_Arms_Vambrace_4.BP_Armor_Modular_Core_Arms_Vambrace_4_C")},
        {"Arms Vambrace 5", MODULAR_PATH("/BP_Armor_Modular_Core_Arms_Vambrace_5.BP_Armor_Modular_Core_Arms_Vambrace_5_C")},
        {"Arms Vambrace 6", MODULAR_PATH("/BP_Armor_Modular_Core_Arms_Vambrace_6.BP_Armor_Modular_Core_Arms_Vambrace_6_C")},
        {"Arms Vambrace 7", MODULAR_PATH("/BP_Armor_Modular_Core_Arms_Vambrace_7.BP_Armor_Modular_Core_Arms_Vambrace_7_C")},
        {"Arms Vambrace Baron", MODULAR_PATH("/BP_Armor_Modular_Core_Arms_Vambrace_Baron.BP_Armor_Modular_Core_Arms_Vambrace_Baron_C")},
        {"Hands Gauntlets 1", MODULAR_PATH("/BP_Armor_Modular_Core_Hands_Gauntlets_1.BP_Armor_Modular_Core_Hands_Gauntlets_1_C")},
        {"Hands Gauntlets 4", MODULAR_PATH("/BP_Armor_Modular_Core_Hands_Gauntlets_4.BP_Armor_Modular_Core_Hands_Gauntlets_4_C")},
        {"Hands Gauntlets 10", MODULAR_PATH("/BP_Armor_Modular_Core_Hands_Gauntlets_10.BP_Armor_Modular_Core_Hands_Gauntlets_10_C")},
        {"Hands Gauntlets Baron", MODULAR_PATH("/BP_Armor_Modular_Core_Hands_Gauntlets_Baron.BP_Armor_Modular_Core_Hands_Gauntlets_Baron_C")},
        {"Hands HalfGauntlets 1", MODULAR_PATH("/BP_Armor_Modular_Core_Hands_HalfGauntlets_1.BP_Armor_Modular_Core_Hands_HalfGauntlets_1_C")},
        {"Hands HalfGauntlets 2", MODULAR_PATH("/BP_Armor_Modular_Core_Hands_HalfGauntlets_2.BP_Armor_Modular_Core_Hands_HalfGauntlets_2_C")},
        {"Feet Shoes 1", MODULAR_PATH("/BP_Armor_Modular_Core_Feet_Shoes_1.BP_Armor_Modular_Core_Feet_Shoes_1_C")},
        {"Feet Shoes 2", MODULAR_PATH("/BP_Armor_Modular_Core_Feet_Shoes_2.BP_Armor_Modular_Core_Feet_Shoes_2_C")},
        {"Feet Shoes 3", MODULAR_PATH("/BP_Armor_Modular_Core_Feet_Shoes_3.BP_Armor_Modular_Core_Feet_Shoes_3_C")},
        {"Legs Cuisse", MODULAR_PATH("/BP_Armor_Modular_Core_Legs_Cuisse.BP_Armor_Modular_Core_Legs_Cuisse_C")},
        {"Legs Cuisse 2", MODULAR_PATH("/BP_Armor_Modular_Core_Legs_Cuisse_2.BP_Armor_Modular_Core_Legs_Cuisse_2_C")},
        {"Legs Cuisse 3", MODULAR_PATH("/BP_Armor_Modular_Core_Legs_Cuisse_3.BP_Armor_Modular_Core_Legs_Cuisse_3_C")},
        {"Legs Cuisse 4", MODULAR_PATH("/BP_Armor_Modular_Core_Legs_Cuisse_4.BP_Armor_Modular_Core_Legs_Cuisse_4_C")},
        {"Legs Cuisse Baron", MODULAR_PATH("/BP_Armor_Modular_Core_Legs_Cuisse_Baron.BP_Armor_Modular_Core_Legs_Cuisse_Baron_C")},
        {"Legs Greaves", MODULAR_PATH("/BP_Armor_Modular_Core_Legs_Greaves.BP_Armor_Modular_Core_Legs_Greaves_C")},
        {"Legs Greaves Baron", MODULAR_PATH("/BP_Armor_Modular_Core_Legs_Greaves_Baron.BP_Armor_Modular_Core_Legs_Greaves_Baron_C")},
        {"Legs Poleyn 1", MODULAR_PATH("/BP_Armor_Modular_Core_Legs_Poleyn_1.BP_Armor_Modular_Core_Legs_Poleyn_1_C")},
        {"Legs Hosen 1", MODULAR_PATH("/BP_Armor_Mocular_Core_Legs_Hosen_1.BP_Armor_Mocular_Core_Legs_Hosen_1_C")},
        {"Legs Hosen 2", MODULAR_PATH("/BP_Armor_Mocular_Core_Legs_Hosen_2.BP_Armor_Mocular_Core_Legs_Hosen_2_C")},
        {"Legs Hosen 3", MODULAR_PATH("/BP_Armor_Mocular_Core_Legs_Hosen_3.BP_Armor_Mocular_Core_Legs_Hosen_3_C")},
        {"Legs Trousers 1", MODULAR_PATH("/BP_Armor_Mocular_Core_Legs_Trousers_1.BP_Armor_Mocular_Core_Legs_Trousers_1_C")},
        {"Legs Trousers 2", MODULAR_PATH("/BP_Armor_Mocular_Core_Legs_Trousers_2.BP_Armor_Mocular_Core_Legs_Trousers_2_C")},
        {"Neck Bevor 1", MODULAR_PATH("/BP_Armor_Modular_Core_Neck_Bevor_1.BP_Armor_Modular_Core_Neck_Bevor_1_C")},
        {"Neck Bevor 2", MODULAR_PATH("/BP_Armor_Modular_Core_Neck_Bevor_2.BP_Armor_Modular_Core_Neck_Bevor_2_C")},
        {"Neck Bevor 15", MODULAR_PATH("/BP_Armor_Modular_Core_Neck_Bevor_15.BP_Armor_Modular_Core_Neck_Bevor_15_C")},
        {"Neck Bevor 16", MODULAR_PATH("/BP_Armor_Modular_Core_Neck_Bevor_16.BP_Armor_Modular_Core_Neck_Bevor_16_C")},
        {"Neck Bevor 17", MODULAR_PATH("/BP_Armor_Modular_Core_Neck_Bevor_17.BP_Armor_Modular_Core_Neck_Bevor_17_C")},
        {"Neck Bevor 18", MODULAR_PATH("/BP_Armor_Modular_Core_Neck_Bevor_18.BP_Armor_Modular_Core_Neck_Bevor_18_C")},
        {"Neck Bevor 19", MODULAR_PATH("/BP_Armor_Modular_Core_Neck_Bevor_19.BP_Armor_Modular_Core_Neck_Bevor_19_C")},
        {"Neck Bevor 20", MODULAR_PATH("/BP_Armor_Modular_Core_Neck_Bevor_20.BP_Armor_Modular_Core_Neck_Bevor_20_C")},
        {"Neck Bevor 21", MODULAR_PATH("/BP_Armor_Modular_Core_Neck_Bevor_21.BP_Armor_Modular_Core_Neck_Bevor_21_C")},
        {"Neck Bevor 22", MODULAR_PATH("/BP_Armor_Modular_Core_Neck_Bevor_22.BP_Armor_Modular_Core_Neck_Bevor_22_C")},
        {"Neck Standart 1", MODULAR_PATH("/BP_Armor_Modular_Core_Neck_Standart_1.BP_Armor_Modular_Core_Neck_Standart_1_C")},
        {"Neck Standart Baron", MODULAR_PATH("/BP_Armor_Modular_Core_Neck_Standart_Baron.BP_Armor_Modular_Core_Neck_Standart_Baron_C")},
        {"Shoulders Pauldron 1", MODULAR_PATH("/BP_Armor_Modular_Core_Shoulders_Pauldron_1.BP_Armor_Modular_Core_Shoulders_Pauldron_1_C")},
        {"Shoulders Spaulder 2", MODULAR_PATH("/BP_Armor_Modular_Core_Shoulders_Spaulder_2.BP_Armor_Modular_Core_Shoulders_Spaulder_2_C")},
        {"Shoulders Spaulder 3", MODULAR_PATH("/BP_Armor_Modular_Core_Shoulders_Spaulder_3.BP_Armor_Modular_Core_Shoulders_Spaulder_3_C")},
        {"Shoulders Spaulder 4", MODULAR_PATH("/BP_Armor_Modular_Core_Shoulders_Spaulder_4.BP_Armor_Modular_Core_Shoulders_Spaulder_4_C")},
        {"Shoulders Spaulder 5", MODULAR_PATH("/BP_Armor_Modular_Core_Shoulders_Spaulder_5.BP_Armor_Modular_Core_Shoulders_Spaulder_5_C")},
        {"Shoulders Spaulder 6", MODULAR_PATH("/BP_Armor_Modular_Core_Shoulders_Spaulder_6.BP_Armor_Modular_Core_Shoulders_Spaulder_6_C")},
        {"Shoulders Spaulder 7", MODULAR_PATH("/BP_Armor_Modular_Core_Shoulders_Spaulder_7.BP_Armor_Modular_Core_Shoulders_Spaulder_7_C")},
        {"Shoulders Spaulder 8", MODULAR_PATH("/BP_Armor_Modular_Core_Shoulders_Spaulder_8.BP_Armor_Modular_Core_Shoulders_Spaulder_8_C")},
        {"Shoulders Spaulder A", MODULAR_PATH("/BP_Armor_Modular_Core_Shoulders_Spaulder_A.BP_Armor_Modular_Core_Shoulders_Spaulder_A_C")},
        {"Shoulders Spaulder Baron", MODULAR_PATH("/BP_Armor_Modular_Core_Shoulders_Spaulder_Baron.BP_Armor_Modular_Core_Shoulders_Spaulder_Baron_C")},
        {"Waist MailFoulds 1", MODULAR_PATH("/BP_Armor_Modular_Core_Waist_MailFoulds_1.BP_Armor_Modular_Core_Waist_MailFoulds_1_C")},
        {"Waist MailFoulds Baron", MODULAR_PATH("/BP_Armor_Modular_Core_Waist_MailFoulds_Baron.BP_Armor_Modular_Core_Waist_MailFoulds_Baron_C")},
    }};

    static constexpr ItemArray<58> armorModuleItems{{
        {"Armet Baron Visor", MODULAR_PATH("/BP_Armor_Modular_Module_Armet_Baron_Visor.BP_Armor_Modular_Module_Armet_Baron_Visor_C")},
        {"Armet Baron Wrapper", MODULAR_PATH("/BP_Armor_Modular_Module_Armet_Baron_Wrapper.BP_Armor_Modular_Module_Armet_Baron_Wrapper_C")},
        {"Armet Feathers", MODULAR_PATH("/BP_Armor_Modular_Module_Armet_Feathers.BP_Armor_Modular_Module_Armet_Feathers_C")},
        {"Armet Plume", MODULAR_PATH("/BP_Armor_Modular_Module_Armet_Plume.BP_Armor_Modular_Module_Armet_Plume_C")},
        {"Armet Visor", MODULAR_PATH("/BP_Armor_Modular_Module_Armet_Visor.BP_Armor_Modular_Module_Armet_Visor_C")},
        {"Bevor 15 Foulds", MODULAR_PATH("/BP_Armor_Modular_Module_Bevor_15_Foulds.BP_Armor_Modular_Module_Bevor_15_Foulds_C")},
        {"Bevor 16 Foulds", MODULAR_PATH("/BP_Armor_Modular_Module_Bevor_16_Foulds.BP_Armor_Modular_Module_Bevor_16_Foulds_C")},
        {"Bevor 19 Foulds", MODULAR_PATH("/BP_Armor_Modular_Module_Bevor_19_Foulds.BP_Armor_Modular_Module_Bevor_19_Foulds_C")},
        {"Bevor 22 Foulds", MODULAR_PATH("/BP_Armor_Modular_Module_Bevor_22_Foulds.BP_Armor_Modular_Module_Bevor_22_Foulds_C")},
        {"Breastplate 1 Foulds", MODULAR_PATH("/BP_Armor_Modular_Module_Breastplate_1_Foulds.BP_Armor_Modular_Module_Breastplate_1_Foulds_C")},
        {"Breastplate 2 Foulds", MODULAR_PATH("/BP_Armor_Modular_Module_Breastplate_2_Foulds.BP_Armor_Modular_Module_Breastplate_2_Foulds_C")},
        {"Breastplate 3 Foulds", MODULAR_PATH("/BP_Armor_Modular_Module_Breastplate_3_Foulds.BP_Armor_Modular_Module_Breastplate_3_Foulds_C")},
        {"Breastplate 4 Foulds", MODULAR_PATH("/BP_Armor_Modular_Module_Breastplate_4_Foulds.BP_Armor_Modular_Module_Breastplate_4_Foulds_C")},
        {"Breastplate 5 Foulds", MODULAR_PATH("/BP_Armor_Modular_Module_Breastplate_5_Foulds.BP_Armor_Modular_Module_Breastplate_5_Foulds_C")},
        {"Breastplate 5 Tassets", MODULAR_PATH("/BP_Armor_Modular_Module_Breastplate_5_Tassets.BP_Armor_Modular_Module_Breastplate_5_Tassets_C")},
        {"Breastplate 6 Foulds", MODULAR_PATH("/BP_Armor_Modular_Module_Breastplate_6_Foulds.BP_Armor_Modular_Module_Breastplate_6_Foulds_C")},
        {"Cuirass 1 Foulds", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_1_Foulds.BP_Armor_Modular_Module_Cuirass_1_Foulds_C")},
        {"Cuirass 1 Plackard", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_1_Plackard.BP_Armor_Modular_Module_Cuirass_1_Plackard_C")},
        {"Cuirass 1 Tassets", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_1_Tassets.BP_Armor_Modular_Module_Cuirass_1_Tassets_C")},
        {"Cuirass 2 Foulds", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_2_Foulds.BP_Armor_Modular_Module_Cuirass_2_Foulds_C")},
        {"Cuirass 2 Plackard", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_2_Plackard.BP_Armor_Modular_Module_Cuirass_2_Plackard_C")},
        {"Cuirass 2 Tassets", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_2_Tassets.BP_Armor_Modular_Module_Cuirass_2_Tassets_C")},
        {"Cuirass Baron Foulds", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Baron_Foulds.BP_Armor_Modular_Module_Cuirass_Baron_Foulds_C")},
        {"Cuirass Baron Plackard", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Baron_Plackard.BP_Armor_Modular_Module_Cuirass_Baron_Plackard_C")},
        {"Cuirass Baron Tassets", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Baron_Tassets.BP_Armor_Modular_Module_Cuirass_Baron_Tassets_C")},
        {"Cuirass Brust 1 Foulds Back", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Brust_1_Foulds_Back.BP_Armor_Modular_Module_Cuirass_Brust_1_Foulds_Back_C")},
        {"Cuirass Brust 1 Foulds Front", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Brust_1_Foulds_Front.BP_Armor_Modular_Module_Cuirass_Brust_1_Foulds_Front_C")},
        {"Cuirass Brust 2 Foulds Back", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Brust_2_Foulds_Back.BP_Armor_Modular_Module_Cuirass_Brust_2_Foulds_Back_C")},
        {"Cuirass Brust 2 Foulds Front", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Brust_2_Foulds_Front.BP_Armor_Modular_Module_Cuirass_Brust_2_Foulds_Front_C")},
        {"Cuirass Gothic 5 Back", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Gothic_5_Back.BP_Armor_Modular_Module_Cuirass_Gothic_5_Back_C")},
        {"Cuirass Gothic 5 Foulds Back", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Gothic_5_Foulds_Back.BP_Armor_Modular_Module_Cuirass_Gothic_5_Foulds_Back_C")},
        {"Cuirass Gothic 5 Foulds Front", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Gothic_5_Foulds_Front.BP_Armor_Modular_Module_Cuirass_Gothic_5_Foulds_Front_C")},
        {"Cuirass Gothic 5 Straps", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Gothic_5_Straps.BP_Armor_Modular_Module_Cuirass_Gothic_5_Straps_C")},
        {"Cuirass Gothic 6 Back", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Gothic_6_Back.BP_Armor_Modular_Module_Cuirass_Gothic_6_Back_C")},
        {"Cuirass Gothic 6 Foulds Back", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Gothic_6_Foulds_Back.BP_Armor_Modular_Module_Cuirass_Gothic_6_Foulds_Back_C")},
        {"Cuirass Gothic 6 Foulds Front", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Gothic_6_Foulds_Front.BP_Armor_Modular_Module_Cuirass_Gothic_6_Foulds_Front_C")},
        {"Cuirass Gothic 6 Straps", MODULAR_PATH("/BP_Armor_Modular_Module_Cuirass_Gothic_6_Straps.BP_Armor_Modular_Module_Cuirass_Gothic_6_Straps_C")},
        {"Feet Sabbatons 1", MODULAR_PATH("/BP_Armor_Modular_Module_Feet_Sabbatons_1.BP_Armor_Modular_Module_Feet_Sabbatons_1_C")},
        {"Hands Gauntlets Baron Gloves", MODULAR_PATH("/BP_Armor_Modular_Module_Hands_Gauntlets_Baron_Gloves1.BP_Armor_Modular_Module_Hands_Gauntlets_Baron_Gloves1_C")},
        {"Hands HalfGauntlets 1 Gloves", MODULAR_PATH("/BP_Armor_Modular_Module_Hands_HalfGauntlets_1_Gloves.BP_Armor_Modular_Module_Hands_HalfGauntlets_1_Gloves_C")},
        {"Hauberk 1 Foulds 0", MODULAR_PATH("/BP_Armor_Modular_Module_Hauberk_1_Foulds_0.BP_Armor_Modular_Module_Hauberk_1_Foulds_0_C")},
        {"Hauberk 1 Foulds 1", MODULAR_PATH("/BP_Armor_Modular_Module_Hauberk_1_Foulds_1.BP_Armor_Modular_Module_Hauberk_1_Foulds_1_C")},
        {"Hauberk 1 Sleeves 0", MODULAR_PATH("/BP_Armor_Modular_Module_Hauberk_1_Sleeves_0.BP_Armor_Modular_Module_Hauberk_1_Sleeves_0_C")},
        {"Hauberk 1 Sleeves 1", MODULAR_PATH("/BP_Armor_Modular_Module_Hauberk_1_Sleeves_1.BP_Armor_Modular_Module_Hauberk_1_Sleeves_1_C")},
        {"HelmetStrap 1", MODULAR_PATH("/BP_Armor_Modular_Module_HelmetStrap_1.BP_Armor_Modular_Module_HelmetStrap_1_C")},
        {"HelmetStrap 2", MODULAR_PATH("/BP_Armor_Modular_Module_HelmetStrap_2.BP_Armor_Modular_Module_HelmetStrap_2_C")},
        {"Jack 1 Chains", MODULAR_PATH("/BP_Armor_Modular_Module_Jack_1_Chains.BP_Armor_Modular_Module_Jack_1_Chains_C")},
        {"Pauldron 1 Plate L", MODULAR_PATH("/BP_Armor_Modular_Module_Pauldron_1_Plate_L.BP_Armor_Modular_Module_Pauldron_1_Plate_L_C")},
        {"Pauldron 1 Plate R", MODULAR_PATH("/BP_Armor_Modular_Module_Pauldron_1_Plate_R.BP_Armor_Modular_Module_Pauldron_1_Plate_R_C")},
        {"Sallet 1 Visor", MODULAR_PATH("/BP_Armor_Modular_Module_Sallet_1_Visor.BP_Armor_Modular_Module_Sallet_1_Visor_C")},
        {"Sallet 4 Visor", MODULAR_PATH("/BP_Armor_Modular_Module_Sallet_4_Visor.BP_Armor_Modular_Module_Sallet_4_Visor_C")},
        {"Sallet Visor A 001", MODULAR_PATH("/BP_Armor_Modular_Module_Sallet_Visor_A_001_Visor.BP_Armor_Modular_Module_Sallet_Visor_A_001_Visor_C")},
        {"Sallet Visor A 002", MODULAR_PATH("/BP_Armor_Modular_Module_Sallet_Visor_A_002_Visor.BP_Armor_Modular_Module_Sallet_Visor_A_002_Visor_C")},
        {"Spaulder Rondels 1", MODULAR_PATH("/BP_Armor_Modular_Module_Spaulder_Rondels_1.BP_Armor_Modular_Module_Spaulder_Rondels_1_C")},
        {"Vambrace 1 Couter L", MODULAR_PATH("/BP_Armor_Modular_Module_Vambrace_1_Couter_L.BP_Armor_Modular_Module_Vambrace_1_Couter_L_C")},
        {"Vambrace 1 Couter R", MODULAR_PATH("/BP_Armor_Modular_Module_Vambrace_1_Couter_R.BP_Armor_Modular_Module_Vambrace_1_Couter_R_C")},
        {"Vambrace 7 Couter L", MODULAR_PATH("/BP_Armor_Modular_Module_Vambrace_7_Couter_L.BP_Armor_Modular_Module_Vambrace_7_Couter_L_C")},
        {"Vambrace 7 Couter R", MODULAR_PATH("/BP_Armor_Modular_Module_Vambrace_7_Couter_R.BP_Armor_Modular_Module_Vambrace_7_Couter_R_C")},
    }};

    static constexpr ItemArray<37> propItems{{
        {"Basket", PROP_PATH("/Blueprints/BP_Container_Basket_001.BP_Container_Basket_001_C")},
        {"Candle", PROP_PATH("/Candle/Blueprints/BP_Candle.BP_Candle_C")},
        {"Candle Light", PROP_PATH("/Lights/Blueprints/BP_CandleLight.BP_CandleLight_C")},
        {"Candle Moveable", PROP_PATH("/Candle/Blueprints/BP_CandleMoveable.BP_CandleMoveable_C")},
        {"Candle Holder Large A 001", PROP_PATH("/Lights/Blueprints/BP_Prop_Light_Candle_Holder_Large_A_001.BP_Prop_Light_Candle_Holder_Large_A_001_C")},
        {"Candle Holder Medium C 2", PROP_PATH("/Lights/Blueprints/BP_Prop_Light_Candle_Holder_Medium_C_2.BP_Prop_Light_Candle_Holder_Medium_C_2_C")},
        {"Candle Lantern A 001", PROP_PATH("/Lights/Blueprints/BP_Prop_Light_Candle_Lantern_A_001.BP_Prop_Light_Candle_Lantern_A_001_C")},
        {"Candle Stand 001", PROP_PATH("/Lights/Blueprints/BP_Prop_Light_Candle_Stand_001.BP_Prop_Light_Candle_Stand_001_C")},
        {"Chandelier A 001", PROP_PATH("/Lights/Blueprints/BP_Prop_Light_Chandelier_A_001.BP_Prop_Light_Chandelier_A_001_C")},
        {"Sconce 001", PROP_PATH("/Lights/Blueprints/BP_Prop_Light_Sconce_001.BP_Prop_Light_Sconce_001_C")},
        {"Chest Coffin", PROP_PATH("/Blueprints/BP_Container_Chest_Coffin_001.BP_Container_Chest_Coffin_001_C")},
        {"Chest Coffin 2", PROP_PATH("/Blueprints/BP_Container_Chest_Coffin_002.BP_Container_Chest_Coffin_002_C")},
        {"Chest 001", PROP_PATH("/Blueprints/BP_Container_Chest_001.BP_Container_Chest_001_C")},
        {"Chest 002", PROP_PATH("/Blueprints/BP_Container_Chest_002.BP_Container_Chest_002_C")},
        {"Chest 003", PROP_PATH("/Blueprints/BP_Container_Chest_003.BP_Container_Chest_003_C")},
        {"Chest 004", PROP_PATH("/Blueprints/BP_Container_Chest_004.BP_Container_Chest_004_C")},
        {"Chest 005", PROP_PATH("/Blueprints/BP_Container_Chest_005.BP_Container_Chest_005_C")},
        {"Clutter", PROP_PATH("/Blueprints/BP_Container_Clutter_001.BP_Container_Clutter_001_C")},
        {"Skeleton", PROP_PATH("/Blueprints/BP_Container_Skeleton_001.BP_Container_Skeleton_001_C")},
        {"Table", PROP_PATH("/Blueprints/BP_Container_Table_001.BP_Container_Table_001_C")},
        {"Cabinet A 006", PROP_PATH("/Furniture/Blueprints/BP_Prop_Furniture_Cabinet_A_006.BP_Prop_Furniture_Cabinet_A_006_C")},
        {"Cabinet C 006", PROP_PATH("/Furniture/Blueprints/BP_Prop_Furniture_Cabinet_C_006.BP_Prop_Furniture_Cabinet_C_006_C")},
        {"Fence Bags", PROP_PATH("/Fence/Blueprints/BP_Fence_Bags.BP_Fence_Bags_C")},
        {"Fence Flimsy Big", PROP_PATH("/Fence/Blueprints/BP_Fence_Flimsy_Big.BP_Fence_Flimsy_Big_C")},
        {"Fence Flimsy Curved", PROP_PATH("/Fence/Blueprints/BP_Fence_Flimsy_Curved.BP_Fence_Flimsy_Curved_C")},
        {"Fence Flimsy Small", PROP_PATH("/Fence/Blueprints/BP_Fence_Flimsy_Small.BP_Fence_Flimsy_Small_C")},
        {"Barrel A 001", PROP_PATH("/Smithing/Blueprints/BP_Prop_Smithing_Barrel_A_001.BP_Prop_Smithing_Barrel_A_001_C")},
        {"Barrel A 002", PROP_PATH("/Smithing/Blueprints/BP_Prop_Smithing_Barrel_A_002.BP_Prop_Smithing_Barrel_A_002_C")},
        {"Barrel B 002", PROP_PATH("/Smithing/Blueprints/BP_Prop_Smithing_Barrel_B_002.BP_Prop_Smithing_Barrel_B_002_C")},
        {"Furnace A 001", PROP_PATH("/Smithing/Blueprints/BP_Prop_Smithing_Furnace_A_001.BP_Prop_Smithing_Furnace_A_001_C")},
        {"Furnace A 002", PROP_PATH("/Smithing/Blueprints/BP_Prop_Smithing_Furnace_A_002.BP_Prop_Smithing_Furnace_A_002_C")},
        {"GrindStone A 003", PROP_PATH("/Smithing/Blueprints/BP_Prop_Smithing_GrindStone_A_003.BP_Prop_Smithing_GrindStone_A_003_C")},
        {"Tool Rack A 001", PROP_PATH("/Smithing/Blueprints/BP_Prop_Smithing_ToolRack_A_001.BP_Prop_Smithing_ToolRack_A_001_C")},
        {"Water Wheel A 001", PROP_PATH("/Construction/Blueprints/BP_Prop_Construction_WaterWheel_A_001.BP_Prop_Construction_WaterWheel_A_001_C")},
        {"Chain Trap", TRAP_PATH("/Chain_BP.Chain_BP_C")},
        {"Trap", TRAP_PATH("/Trap_BP.Trap_BP_C")},
        {"Trap Kettle", TRAP_PATH("/Trap_Kettle_BP.Trap_Kettle_BP_C")}
    }};

    static constexpr std::array<const ItemInfo*, 23> weaponArrays{{
        swordItems.data(), bastardSwordItems.data(), falchionItems.data(),
        maceItems.data(), haftedItems.data(), polearmItems.data(), pollaxeItems.data(), castedItems.data(), messerItems.data(),
        axeItems.data(), daggerItems.data(), baurnwehrItems.data(), flailItems.data(),
        billhookItems.data(), halberdItems.data(), spearItems.data(), staffItems.data(),
        toolItems.data(), shieldItems.data(), improvisedItems.data(),
        rangedItems.data(), treasureItems.data(), uniqueWeaponItems.data()
    }};

    static constexpr std::array<size_t, 23> weaponSizes{{
        swordItems.size(), bastardSwordItems.size(), falchionItems.size(),
        maceItems.size(), haftedItems.size(), polearmItems.size(), pollaxeItems.size(), castedItems.size(), messerItems.size(),
        axeItems.size(), daggerItems.size(), baurnwehrItems.size(), flailItems.size(),
        billhookItems.size(), halberdItems.size(), spearItems.size(), staffItems.size(),
        toolItems.size(), shieldItems.size(), improvisedItems.size(),
        rangedItems.size(), treasureItems.size(), uniqueWeaponItems.size()
    }};

    static constexpr std::array<const ItemInfo*, 14> armorArrays{{
        nullptr,                        // Weapons (index 0)
        helmetItems.data(),
        bodyArmorItems.data(),
        armItems.data(),
        legItems.data(),
        handItems.data(),
        feetItems.data(),
        neckItems.data(),
        shoulderItems.data(),
        waistItems.data(),
        modularArmorCoreItems.data(),   // Modular Armor (index 10)
        armorModuleItems.data(),        // Armor Modules (index 11)
        nullptr,                        // Random Armor (index 12)
        nullptr                         // Props (index 13)
    }};

    static constexpr std::array<size_t, 14> armorSizes{{
        0,
        helmetItems.size(),
        bodyArmorItems.size(),
        armItems.size(),
        legItems.size(),
        handItems.size(),
        feetItems.size(),
        neckItems.size(),
        shoulderItems.size(),
        waistItems.size(),
        modularArmorCoreItems.size(),
        armorModuleItems.size(),
        0,
        0
    }};

    static inline std::vector<const char*> cachedItemNames;
    static inline uint8_t lastCategoryIndex = 255;
    static inline uint8_t lastSubcategoryIndex = 255;

    static inline char searchBuffer[128] = "";
    static inline std::vector<uint32_t> filteredIndices;
    static inline bool searchActive = false;

    [[nodiscard]] std::pair<const ItemInfo*, size_t> getCurrentItemArray() const noexcept {
        if (cfg.currentCategoryIndex == WEAPONS_INDEX)
            return {weaponArrays[cfg.currentWeaponSubcategoryIndex], weaponSizes[cfg.currentWeaponSubcategoryIndex]};
        if (cfg.currentCategoryIndex == RANDOM_ARMOR_INDEX)
            return {nullptr, 0};
        if (cfg.currentCategoryIndex == PROPS_INDEX)
            return {propItems.data(), propItems.size()};
        if (cfg.currentCategoryIndex < armorArrays.size())
            return {armorArrays[cfg.currentCategoryIndex], armorSizes[cfg.currentCategoryIndex]};
        return {nullptr, 0};
    }

    [[nodiscard]] static const ItemInfo* getItemAt(uint8_t catIdx, uint8_t subIdx, uint16_t itmIdx) noexcept {
        if (catIdx == WEAPONS_INDEX) {
            return (subIdx < weaponSizes.size() && itmIdx < weaponSizes[subIdx])
                ? &weaponArrays[subIdx][itmIdx] : nullptr;
        }
        if (catIdx == RANDOM_ARMOR_INDEX) return nullptr;
        if (catIdx == PROPS_INDEX) {
            return (itmIdx < propItems.size()) ? &propItems[itmIdx] : nullptr;
        }
        return (catIdx < armorSizes.size() && itmIdx < armorSizes[catIdx])
            ? &armorArrays[catIdx][itmIdx] : nullptr;
    }

    void updateItemNamesCache() noexcept {
        uint8_t currentSub = (cfg.currentCategoryIndex == WEAPONS_INDEX) ? cfg.currentWeaponSubcategoryIndex : 0;
        if (lastCategoryIndex != cfg.currentCategoryIndex || lastSubcategoryIndex != currentSub) [[unlikely]] {
            const auto [items, size] = getCurrentItemArray();
            cachedItemNames.resize(size);
            for (size_t i = 0; i < size; ++i) {
                cachedItemNames[i] = items[i].displayName;
            }
            lastCategoryIndex = cfg.currentCategoryIndex;
            lastSubcategoryIndex = currentSub;
        }
    }

    static const char* stristr(const char* haystack, const char* needle) {
        if (!*needle) return haystack;
        for (; *haystack; ++haystack) {
            if (std::tolower(static_cast<unsigned char>(*haystack)) == std::tolower(static_cast<unsigned char>(*needle))) {
                const char* h = haystack;
                const char* n = needle;
                while (*h && *n && std::tolower(static_cast<unsigned char>(*h)) == std::tolower(static_cast<unsigned char>(*n))) {
                    ++h; ++n;
                }
                if (!*n) return haystack;
            }
        }
        return nullptr;
    }

    void updateFilteredItems() noexcept {
        filteredIndices.clear();

        if (searchBuffer[0] == '\0') {
            searchActive = false;
            return;
        }

        searchActive = true;

        for (uint8_t catIdx = 0; catIdx < static_cast<uint8_t>(categories.size()); ++catIdx) {
            if (catIdx == WEAPONS_INDEX) {
                for (uint8_t subIdx = 0; subIdx < static_cast<uint8_t>(weaponSubcategories.size()); ++subIdx) {
                    const auto* items = weaponArrays[subIdx];
                    const size_t size = weaponSizes[subIdx];
                    for (uint16_t i = 0; i < size; ++i) {
                        if (stristr(items[i].displayName, searchBuffer)) {
                            filteredIndices.push_back((catIdx << 16) | (subIdx << 8) | i);
                        }
                    }
                }
            } else if (catIdx == RANDOM_ARMOR_INDEX) {
                continue;
            } else if (catIdx == PROPS_INDEX) {
                for (uint16_t i = 0; i < propItems.size(); ++i) {
                    if (stristr(propItems[i].displayName, searchBuffer)) {
                        filteredIndices.push_back((catIdx << 16) | i);
                    }
                }
            } else {
                const auto* items = armorArrays[catIdx];
                const size_t size = armorSizes[catIdx];
                if (items) {
                    for (uint16_t i = 0; i < size; ++i) {
                        if (stristr(items[i].displayName, searchBuffer)) {
                            filteredIndices.push_back((catIdx << 16) | i);
                        }
                    }
                }
            }
        }
    }

    void SpawnSelectedItem() const noexcept {
        auto spawnTransform = player->GetTransform();
        const auto forward = player->GetActorForwardVector();
        spawnTransform.Translation.X += forward.X * cfg.spawnDistanceForward;
        spawnTransform.Translation.Y += forward.Y * cfg.spawnDistanceForward;
        spawnTransform.Translation.Z += cfg.spawnDistanceUp;
        spawnTransform.Scale3D = {cfg.spawnScale, cfg.spawnScale, cfg.spawnScale};

        if (cfg.currentCategoryIndex == RANDOM_ARMOR_INDEX) {
            if (cfg.currentItemIndex >= randomArmorSlots.size()) return;
            auto slot = static_cast<SDK::EArmorSlots_Enum>(randomArmorSlots[cfg.currentItemIndex].slotEnum);
            auto tier = static_cast<SDK::Enum_Ranks>(cfg.spawnTier);
            bool snap = cfg.snapToGround;
            auto transform = spawnTransform;
            GameHook::QueueAction([this, slot, tier, transform, snap]() {
                EquipmentGenerator::Init(world);
                auto passport = EquipmentGenerator::GenerateArmor(tier, slot, 0.5);
                if (passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43)
                    Spawner::SpawnArmorFromPassport(world, passport, transform, snap);
            });
            return;
        }

        const auto [items, size] = getCurrentItemArray();
        if (cfg.currentItemIndex >= size || !items) return;
        const auto& item = items[cfg.currentItemIndex];

        if (item.customizable != CustomizableWeapon::None) {
            Spawner::SpawnCustomizableWeapon(world, item.customizable, spawnTransform, cfg.snapToGround, cfg.spawnTier);
        } else if (item.classPath) {
            Spawner::SpawnActor(world, item.classPath, spawnTransform, nullptr, cfg.snapToGround, cfg.spawnTier);
        }
    }

public:
    ItemSection() : CollapsibleSection("Item") {
        Function("Spawn Item")
            .WithKey(&cfg.spawnItemKey)
            .WithParams({
                Parameter("snap_to_ground", "Snap to Ground", &cfg.snapToGround, "Automatically adjust height to touch the ground"),
                Parameter("distance_forward", "Forward Distance", &cfg.spawnDistanceForward, 50.0f, 300.0f, "How far in front the item appears"),
                Parameter("distance_up", "Up Distance", &cfg.spawnDistanceUp, 0.0f, 200.0f, "Height offset for spawn position"),
                Parameter("scale", "Scale", &cfg.spawnScale, 0.1f, 5.0f, "Size multiplier for the spawned item")
            })
            .WithTooltip("Spawns the selected item with configurable position and size")
            .Action([this]() { SpawnSelectedItem(); }, player, world);
    }

    void RenderContent() override {
        SectionStyle::StyleRAII style;

        for (auto& function : functions) {
            function->Render();
            ImGui::Spacing();
        }

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Search");
        ImGui::SameLine();
        bool searchChanged = ImGui::InputText("##ItemSearch", searchBuffer, sizeof(searchBuffer), ImGuiInputTextFlags_AutoSelectAll);

        if (searchChanged) {
            updateFilteredItems();
        }

        if (searchActive && !filteredIndices.empty()) {
            ImGui::Text("Found: %zu items", filteredIndices.size());
            ImGui::Spacing();

            float maxFilteredW = 0;
            for (uint32_t pi : filteredIndices) {
                const ItemInfo* fi = getItemAt((pi >> 16) & 0xFF, (pi >> 8) & 0xFF, pi & 0xFF);
                if (fi) {
                    float w = ImGui::CalcTextSize(fi->displayName).x;
                    if (w > maxFilteredW) maxFilteredW = w;
                }
            }
            ImGui::SetNextItemWidth(maxFilteredW + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2);
            if (ImGui::BeginCombo("##FilteredItems", "Select item...")) {
                for (uint32_t packedIdx : filteredIndices) {
                    uint8_t catIdx = (packedIdx >> 16) & 0xFF;
                    uint8_t subIdx = (packedIdx >> 8) & 0xFF;
                    uint16_t itmIdx = packedIdx & 0xFF;

                    const ItemInfo* item = getItemAt(catIdx, subIdx, itmIdx);

                    if (item) {
                        bool isSelected = (cfg.currentCategoryIndex == catIdx && cfg.currentItemIndex == itmIdx &&
                            (catIdx != WEAPONS_INDEX || cfg.currentWeaponSubcategoryIndex == subIdx));
                        if (ImGui::Selectable(item->displayName, isSelected)) {
                            cfg.currentCategoryIndex = catIdx;
                            if (catIdx == WEAPONS_INDEX) cfg.currentWeaponSubcategoryIndex = subIdx;
                            cfg.currentItemIndex = itmIdx;
                            searchBuffer[0] = '\0';
                            searchActive = false;
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                }
                ImGui::EndCombo();
            }

            if (ImGui::Button("Clear Search")) {
                searchBuffer[0] = '\0';
                searchActive = false;
            }

        } else if (searchActive && filteredIndices.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No items found");
            if (ImGui::Button("Clear Search")) {
                searchBuffer[0] = '\0';
                searchActive = false;
            }

        } else {
            ImGui::Text("Category");
            int catIndex = static_cast<int>(cfg.currentCategoryIndex);
            ImGui::SetNextItemWidth(GuiUtils::CalcComboWidth(categories.data(), static_cast<int>(categories.size())));
            if (ImGui::Combo("##CategorySelector", &catIndex, categories.data(), static_cast<int>(categories.size()))) [[unlikely]] {
                cfg.currentCategoryIndex = static_cast<uint8_t>(catIndex);
                cfg.currentItemIndex = 0;
                if (cfg.currentCategoryIndex != WEAPONS_INDEX) [[likely]] {
                    cfg.currentWeaponSubcategoryIndex = 0;
                }
            }

            if (cfg.currentCategoryIndex == WEAPONS_INDEX) [[likely]] {
                ImGui::Text("Subcategory");
                int subIndex = static_cast<int>(cfg.currentWeaponSubcategoryIndex);
                ImGui::SetNextItemWidth(GuiUtils::CalcComboWidth(weaponSubcategories.data(), static_cast<int>(weaponSubcategories.size())));
                if (ImGui::Combo("##SubcategorySelector", &subIndex, weaponSubcategories.data(), static_cast<int>(weaponSubcategories.size()))) [[unlikely]] {
                    cfg.currentWeaponSubcategoryIndex = static_cast<uint8_t>(subIndex);
                    cfg.currentItemIndex = 0;
                }
            }

            if (cfg.currentCategoryIndex == RANDOM_ARMOR_INDEX) {
                ImGui::Text("Armor Slot");
                int slotIndex = static_cast<int>(cfg.currentItemIndex);
                auto armorSlotGetter = [](void* data, int idx) -> const char* {
                    return static_cast<const ArmorSlotInfo*>(data)[idx].displayName;
                };
                ImGui::SetNextItemWidth(GuiUtils::CalcComboWidth(armorSlotGetter, (void*)randomArmorSlots.data(), static_cast<int>(randomArmorSlots.size())));
                if (ImGui::Combo("##ArmorSlotSelector", &slotIndex,
                    armorSlotGetter, (void*)randomArmorSlots.data(), static_cast<int>(randomArmorSlots.size()))) {
                    cfg.currentItemIndex = static_cast<uint16_t>(slotIndex);
                }
            } else {
                updateItemNamesCache();

                ImGui::Text("Item");
                int itemIndex = static_cast<int>(cfg.currentItemIndex);
                ImGui::SetNextItemWidth(GuiUtils::CalcComboWidth(cachedItemNames.data(), static_cast<int>(cachedItemNames.size())));
                if (ImGui::Combo("##ItemSelector", &itemIndex, cachedItemNames.data(), static_cast<int>(cachedItemNames.size()))) [[unlikely]] {
                    cfg.currentItemIndex = static_cast<uint16_t>(itemIndex);
                }
            }

            if (cfg.currentCategoryIndex == WEAPONS_INDEX) {
                uint8_t sub = cfg.currentWeaponSubcategoryIndex;
                if (cfg.currentItemIndex < weaponSizes[sub]) {
                    const auto& currentItem = weaponArrays[sub][cfg.currentItemIndex];
                    if (currentItem.customizable != CustomizableWeapon::None) {
                        uint16_t mask = TierValidation::VALID_TIER_MASKS[static_cast<uint8_t>(currentItem.customizable)];
                        cfg.spawnTier = TierValidation::NearestValidTier(mask, cfg.spawnTier);

                        char preview[16];
                        std::snprintf(preview, sizeof(preview), "Tier %d", cfg.spawnTier);
                        ImGui::Text("Tier");
                        ImGui::SetNextItemWidth(ImGui::CalcTextSize("Tier 8").x + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2);
                        if (ImGui::BeginCombo("##TierCombo", preview)) {
                            for (int t = 0; t <= 8; ++t) {
                                if (!(mask & (1 << t))) continue;
                                char label[16];
                                std::snprintf(label, sizeof(label), "Tier %d", t);
                                if (ImGui::Selectable(label, t == cfg.spawnTier))
                                    cfg.spawnTier = t;
                                if (t == cfg.spawnTier)
                                    ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    }
                }
            } else if (cfg.currentCategoryIndex == RANDOM_ARMOR_INDEX) {
                if (cfg.currentItemIndex < TierValidation::VALID_ARMOR_TIER_MASKS.size()) {
                    uint16_t mask = TierValidation::VALID_ARMOR_TIER_MASKS[cfg.currentItemIndex];
                    cfg.spawnTier = TierValidation::NearestValidTier(mask, cfg.spawnTier);

                    char preview[16];
                    std::snprintf(preview, sizeof(preview), "Tier %d", cfg.spawnTier);
                    ImGui::Text("Tier");
                    ImGui::SetNextItemWidth(ImGui::CalcTextSize("Tier 8").x + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2);
                    if (ImGui::BeginCombo("##ArmorTierCombo", preview)) {
                        for (int t = 0; t <= 8; ++t) {
                            if (!(mask & (1 << t))) continue;
                            char label[16];
                            std::snprintf(label, sizeof(label), "Tier %d", t);
                            if (ImGui::Selectable(label, t == cfg.spawnTier))
                                cfg.spawnTier = t;
                            if (t == cfg.spawnTier)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Spawn Item")) [[unlikely]] {
            if (ComponentValidator::Validate(player) && ComponentValidator::Validate(world)) {
                SpawnSelectedItem();
            }
        }

    }
};
