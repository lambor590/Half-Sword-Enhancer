#pragma once

#include <vector>
#include <array>
#include <string_view>
#include <span>
#include <cctype>
#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Utils/Spawner.h"
#include "DefaultStyle.h"

#define WEAPON_PATH(s) "/Game/Assets/Weapons/Blueprints/Built_Weapons" s
#define ARMOR_PATH(s) "/Game/Assets/Armor/Blueprints/Built_Armor" s
#define PROP_PATH(s) "/Game/Assets/Props" s

struct ItemInfo {
    constexpr ItemInfo(const char* name, const char* path) : displayName(name), classPath(path) {}
    const char* displayName;
    const char* classPath;
};

enum class ItemCategory : uint8_t {
    Weapons, Helmets, BodyArmor, Arms, Legs, Hands, Feet, Neck, Shoulders, Waist, Props, COUNT
};

enum class WeaponSubcategory : uint8_t {
    Swords, Maces, Axes, Polearms, Daggers, Tools, Shields, Improvised, COUNT
};

template<size_t N>
using ItemArray = std::array<ItemInfo, N>;

class ItemSection : public CollapsibleSection {
private:
    SectionConfig::ItemConfig& cfg = SectionConfig::item;

    static constexpr uint8_t WEAPONS_INDEX = 0;
    static constexpr uint8_t PROPS_INDEX = 10;

    static constexpr std::array categories{
        "Weapons", "Helmets", "Body Armor", "Arms", "Legs",
        "Hands", "Feet", "Neck", "Shoulders", "Waist", "Props"
    };

    static constexpr std::array weaponSubcategories{
        "Swords", "Maces", "Axes", "Polearms", "Daggers", "Tools", "Shields", "Improvised"
    };

    static constexpr ItemArray<21> swordItems{{
        {"Long Sword T1", WEAPON_PATH("/ModularWeaponBP_LongSword_T1.ModularWeaponBP_LongSword_T1_C")},
        {"Long Sword T2", WEAPON_PATH("/ModularWeaponBP_LongSword_T2.ModularWeaponBP_LongSword_T2_C")},
        {"Long Sword T3", WEAPON_PATH("/ModularWeaponBP_LongSword_T3.ModularWeaponBP_LongSword_T3_C")},
        {"Long Sword T4", WEAPON_PATH("/ModularWeaponBP_LongSword_T4.ModularWeaponBP_LongSword_T4_C")},
        {"Arming Sword", WEAPON_PATH("/ModularWeaponBP_ArmingSword.ModularWeaponBP_ArmingSword_C")},
        {"Arming Sword T1", WEAPON_PATH("/ModularWeaponBP_ArmingSword_T1.ModularWeaponBP_ArmingSword_T1_C")},
        {"Arming Sword T2", WEAPON_PATH("/ModularWeaponBP_ArmingSword_T2.ModularWeaponBP_ArmingSword_T2_C")},
        {"Arming Sword T3", WEAPON_PATH("/ModularWeaponBP_ArmingSword_T3.ModularWeaponBP_ArmingSword_T3_C")},
        {"Bastard Sword T1", WEAPON_PATH("/ModularWeaponBP_BastardSword_T1.ModularWeaponBP_BastardSword_T1_C")},
        {"Bastard Sword T2", WEAPON_PATH("/ModularWeaponBP_BastardSword_T2.ModularWeaponBP_BastardSword_T2_C")},
        {"Bastard Sword T3", WEAPON_PATH("/ModularWeaponBP_BastardSword_T3.ModularWeaponBP_BastardSword_T3_C")},
        {"Great Sword", WEAPON_PATH("/ModularWeaponBP_GreatSword.ModularWeaponBP_GreatSword_C")},
        {"Short Falchion", WEAPON_PATH("/ModularWeaponBP_Falchion_Short.ModularWeaponBP_Falchion_Short_C")},
        {"Short Falchion T1", WEAPON_PATH("/ModularWeaponBP_Falchion_Short_T1.ModularWeaponBP_Falchion_Short_T1_C")},
        {"Short Falchion T2", WEAPON_PATH("/ModularWeaponBP_Falchion_Short_T2.ModularWeaponBP_Falchion_Short_T2_C")},
        {"Short Falchion T3", WEAPON_PATH("/ModularWeaponBP_Falchion_Short_T3.ModularWeaponBP_Falchion_Short_T3_C")},
        {"Long Falchion", WEAPON_PATH("/ModularWeaponBP_Falchion_Long.ModularWeaponBP_Falchion_Long_C")},
        {"Long Falchion T1", WEAPON_PATH("/ModularWeaponBP_Falchion_Long_T1.ModularWeaponBP_Falchion_Long_T1_C")},
        {"Long Falchion T2", WEAPON_PATH("/ModularWeaponBP_Falchion_Long_T2.ModularWeaponBP_Falchion_Long_T2_C")},
        {"Long Falchion T3", WEAPON_PATH("/ModularWeaponBP_Falchion_Long_T3.ModularWeaponBP_Falchion_Long_T3_C")},
        {"Rapier", WEAPON_PATH("/ModularWeaponBP_Rapier.ModularWeaponBP_Rapier_C")}
    }};

    static constexpr ItemArray<9> maceItems{{
        {"Short Low Tier Mace", WEAPON_PATH("/Tiers/ModularWeaponBP_Mace_Low_Tier_Short.ModularWeaponBP_Mace_Low_Tier_Short_C")},
        {"Long Low Tier Mace", WEAPON_PATH("/Tiers/ModularWeaponBP_Mace_Low_Tier_Long.ModularWeaponBP_Mace_Low_Tier_Long_C")},
        {"Short Mid Tier Mace", WEAPON_PATH("/Tiers/ModularWeaponBP_Mace_Mid_Tier_Short.ModularWeaponBP_Mace_Mid_Tier_Short_C")},
        {"Average Mid Tier Mace", WEAPON_PATH("/Tiers/ModularWeaponBP_Mace_Mid_Tier_Avg.ModularWeaponBP_Mace_Mid_Tier_Avg_C")},
        {"Long Mid Tier Mace", WEAPON_PATH("/Tiers/ModularWeaponBP_Mace_Mid_Tier_Long.ModularWeaponBP_Mace_Mid_Tier_Long_C")},
        {"Short High Tier Mace", WEAPON_PATH("/Tiers/ModularWeaponBP_Mace_High_Tier_Short.ModularWeaponBP_Mace_High_Tier_Short_C")},
        {"Average High Tier Mace", WEAPON_PATH("/Tiers/ModularWeaponBP_Mace_High_Tier_Avg.ModularWeaponBP_Mace_High_Tier_Avg_C")},
        {"Long High Tier Mace", WEAPON_PATH("/Tiers/ModularWeaponBP_Mace_High_Tier_Long.ModularWeaponBP_Mace_High_Tier_Long_C")},
        {"Giant High Tier Mace", WEAPON_PATH("/Tiers/ModularWeaponBP_Mace_High_Tier_Giant.ModularWeaponBP_Mace_High_Tier_Giant_C")}
    }};

    static constexpr ItemArray<2> axeItems{{
        {"Axe", WEAPON_PATH("/Reforged/ModularWeaponBP_Axe.ModularWeaponBP_Axe_C")},
        {"Two-Handed Axe", WEAPON_PATH("/Reforged/ModularWeaponBP_Axe2H.ModularWeaponBP_Axe2H_C")}
    }};

    static constexpr ItemArray<23> polearmItems{{
        {"Spear A", WEAPON_PATH("/Reforged/ModularWeaponBP_Spear_A.ModularWeaponBP_Spear_A_C")},
        {"Spear B", WEAPON_PATH("/Reforged/ModularWeaponBP_Spear_B.ModularWeaponBP_Spear_B_C")},
        {"Halberd A", WEAPON_PATH("/Reforged/ModularWeaponBP_Halberd_A.ModularWeaponBP_Halberd_A_C")},
        {"Halberd B", WEAPON_PATH("/Reforged/ModularWeaponBP_Halberd_B.ModularWeaponBP_Halberd_B_C")},
        {"Halberd C", WEAPON_PATH("/Reforged/ModularWeaponBP_Halberd_C.ModularWeaponBP_Halberd_C_C")},
        {"Halberd D", WEAPON_PATH("/Reforged/ModularWeaponBP_Halberd_D.ModularWeaponBP_Halberd_D_C")},
        {"Billhook A", WEAPON_PATH("/Reforged/ModularWeaponBP_Billhook_A.ModularWeaponBP_Billhook_A_C")},
        {"Billhook B", WEAPON_PATH("/Reforged/ModularWeaponBP_Billhook_B.ModularWeaponBP_Billhook_B_C")},
        {"War Staff A", WEAPON_PATH("/Reforged/ModularWeaponBP_WarStaff_A.ModularWeaponBP_WarStaff_A_C")},
        {"War Staff B", WEAPON_PATH("/Reforged/ModularWeaponBP_WarStaff_B.ModularWeaponBP_WarStaff_B_C")},
        {"High Tier Polearm Big", WEAPON_PATH("/Tiers/ModularWeaponBP_Polearm_High_Tier_Big.ModularWeaponBP_Polearm_High_Tier_Big_C")},
        {"High Tier Polearm Gold", WEAPON_PATH("/Tiers/ModularWeaponBP_Polearm_High_Tier_Gold.ModularWeaponBP_Polearm_High_Tier_Gold_C")},
        {"High Tier Polearm", WEAPON_PATH("/Tiers/ModularWeaponBP_Polearm_High_Tier.ModularWeaponBP_Polearm_High_Tier_C")},
        {"Mid Tier Polearm", WEAPON_PATH("/Tiers/ModularWeaponBP_Polearm_Mid_Tier.ModularWeaponBP_Polearm_Mid_Tier_C")},
        {"Low Tier Polearm", WEAPON_PATH("/Tiers/ModularWeaponBP_Polearm_Low_Tier.ModularWeaponBP_Polearm_Low_Tier_C")},
        {"Short Hafted Low Tier", WEAPON_PATH("/Tiers/ModularWeaponBP_Hafted_Low_Tier_Short.ModularWeaponBP_Hafted_Low_Tier_Short_C")},
        {"Long Hafted Low Tier", WEAPON_PATH("/Tiers/ModularWeaponBP_Hafted_Low_Tier_Long.ModularWeaponBP_Hafted_Low_Tier_Long_C")},
        {"Short Hafted Mid Tier", WEAPON_PATH("/Tiers/ModularWeaponBP_Hafted_Mid_Tier_Short.ModularWeaponBP_Hafted_Mid_Tier_Short_C")},
        {"Average Hafted Mid Tier", WEAPON_PATH("/Tiers/ModularWeaponBP_Hafted_Mid_Tier_Avg.ModularWeaponBP_Hafted_Mid_Tier_Avg_C")},
        {"Long Hafted Mid Tier", WEAPON_PATH("/Tiers/ModularWeaponBP_Hafted_Mid_Tier_Long.ModularWeaponBP_Hafted_Mid_Tier_Long_C")},
        {"Short Hafted High Tier", WEAPON_PATH("/Tiers/ModularWeaponBP_Hafted_High_Tier_Short.ModularWeaponBP_Hafted_High_Tier_Short_C")},
        {"Average Hafted High Tier", WEAPON_PATH("/Tiers/ModularWeaponBP_Hafted_High_Tier_Avg.ModularWeaponBP_Hafted_High_Tier_Avg_C")},
        {"Long Hafted High Tier", WEAPON_PATH("/Tiers/ModularWeaponBP_Hafted_High_Tier_Long.ModularWeaponBP_Hafted_High_Tier_Long_C")}
    }};

    static constexpr ItemArray<6> daggerItems{{
        {"Dagger", WEAPON_PATH("/ModularWeaponBP_Dagger.ModularWeaponBP_Dagger_C")},
        {"Dagger T1", WEAPON_PATH("/ModularWeaponBP_Dagger_T1.ModularWeaponBP_Dagger_T1_C")},
        {"Dagger T2", WEAPON_PATH("/ModularWeaponBP_Dagger_T2.ModularWeaponBP_Dagger_T2_C")},
        {"Dagger T3", WEAPON_PATH("/ModularWeaponBP_Dagger_T3.ModularWeaponBP_Dagger_T3_C")},
        {"Rondel", WEAPON_PATH("/Reforged/ModularWeaponBP_Rondel.ModularWeaponBP_Rondel_C")},
        {"Rondel Gold", WEAPON_PATH("/Reforged/ModularWeaponBP_Rondel_Gold.ModularWeaponBP_Rondel_Gold_C")}
    }};

    static constexpr ItemArray<23> toolItems{{
        {"Hammer A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Hammer_A.BP_Weapon_Tool_Hammer_A_C")},
        {"Hammer B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Hammer_B.BP_Weapon_Tool_Hammer_B_C")},
        {"Hammer C", WEAPON_PATH("/Tools/BP_Weapon_Tool_Hammer_C.BP_Weapon_Tool_Hammer_C_C")},
        {"Axe A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Axe_A.BP_Weapon_Tool_Axe_A_C")},
        {"Axe B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Axe_B.BP_Weapon_Tool_Axe_B_C")},
        {"Axe C", WEAPON_PATH("/Tools/BP_Weapon_Tool_Axe_C.BP_Weapon_Tool_Axe_C_C")},
        {"Axe D", WEAPON_PATH("/Tools/BP_Weapon_Tool_Axe_D.BP_Weapon_Tool_Axe_D_C")},
        {"Hoe A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Hoe_A.BP_Weapon_Tool_Hoe_A_C")},
        {"Hoe B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Hoe_B.BP_Weapon_Tool_Hoe_B_C")},
        {"Knife A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Knife_A.BP_Weapon_Tool_Knife_A_C")},
        {"Knife B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Knife_B.BP_Weapon_Tool_Knife_B_C")},
        {"Knife C", WEAPON_PATH("/Tools/BP_Weapon_Tool_Knife_C.BP_Weapon_Tool_Knife_C_C")},
        {"Pitchfork A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Pitchfork_A.BP_Weapon_Tool_Pitchfork_A_C")},
        {"Scissors", WEAPON_PATH("/Tools/BP_Weapon_Tool_Scissors.BP_Weapon_Tool_Scissors_C")},
        {"Scythe A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Scythe_A.BP_Weapon_Tool_Scythe_A_C")},
        {"Shovel A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Shovel_A.BP_Weapon_Tool_Shovel_A_C")},
        {"Shovel B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Shovel_B.BP_Weapon_Tool_Shovel_B_C")},
        {"Sickle A", WEAPON_PATH("/Tools/BP_Weapon_Tool_Sickle_A.BP_Weapon_Tool_Sickle_A_C")},
        {"Sickle B", WEAPON_PATH("/Tools/BP_Weapon_Tool_Sickle_B.BP_Weapon_Tool_Sickle_B_C")},
        {"Sickle C", WEAPON_PATH("/Tools/BP_Weapon_Tool_Sickle_C.BP_Weapon_Tool_Sickle_C_C")},
        {"Sickle D", WEAPON_PATH("/Tools/BP_Weapon_Tool_Sickle_D.BP_Weapon_Tool_Sickle_D_C")},
        {"Sickle E", WEAPON_PATH("/Tools/BP_Weapon_Tool_Sickle_E.BP_Weapon_Tool_Sickle_E_C")},
        {"Tongs", WEAPON_PATH("/Tools/BP_Weapon_Tool_Tongs.BP_Weapon_Tool_Tongs_C")}
    }};

    static constexpr ItemArray<8> shieldItems{{
        {"Buckler Shield", WEAPON_PATH("/Shield_Buckler.Shield_Buckler_C")},
        {"Buckler Shield Gold", WEAPON_PATH("/Shield_Buckler_Gold.Shield_Buckler_Gold_C")},
        {"Boss Grip Shield", WEAPON_PATH("/Shield_BossGrip.Shield_BossGrip_C")},
        {"Light Pavise", WEAPON_PATH("/Shield_Pavise_Light.Shield_Pavise_Light_C")},
        {"Heavy Pavise", WEAPON_PATH("/Shield_Pavise_Heavy.Shield_Pavise_Heavy_C")},
        {"Tower Pavise", WEAPON_PATH("/Shield_Pavise_Tower.Shield_Pavise_Tower_C")},
        {"Tagre Shield", WEAPON_PATH("/Shield_Tagre.Shield_Tagre_C")},
        {"Tagre Shield Gold", WEAPON_PATH("/Shield_Tagre_Gold.Shield_Tagre_Gold_C")}
    }};

    static constexpr ItemArray<4> improvisedItems{{
        {"Small Candlestick", WEAPON_PATH("/Improvized/BP_Weapon_Improv_CandleStick_Small.BP_Weapon_Improv_CandleStick_Small_C")},
        {"Big Candlestick", WEAPON_PATH("/Improvized/BP_Weapon_Improv_CandleStick_Big.BP_Weapon_Improv_CandleStick_Big_C")},
        {"Lantern", WEAPON_PATH("/Improvized/BP_Weapon_Improv_Lantern.BP_Weapon_Improv_Lantern_C")},
        {"Stool", WEAPON_PATH("/Improvized/BP_Weapon_Improv_Stool.BP_Weapon_Improv_Stool_C")}
    }};

    static constexpr ItemArray<43> helmetItems{{
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
        {"Bycocket B 002 Brown", ARMOR_PATH("/Cloth/Hats/BP_Armor_Head_Hat_Bycocket_B_002_Brown.BP_Armor_Head_Hat_Bycocket_B_002_Brown_C")},
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

    static constexpr ItemArray<25> bodyArmorItems{{
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
        {"Gambeson A Red", ARMOR_PATH("/BP_Armor_Body_Gambeson_A_Red.BP_Armor_Body_Gambeson_A_Red_C")},
        {"Gambeson A Gray", ARMOR_PATH("/BP_Armor_Body_Gambeson_A_Gray.BP_Armor_Body_Gambeson_A_Gray_C")},
        {"Gambeson B T1", ARMOR_PATH("/BP_Armor_Body_Gambeson_B_T1.BP_Armor_Body_Gambeson_B_T1_C")},
        {"Gambeson B T2", ARMOR_PATH("/BP_Armor_Body_Gambeson_B_T2.BP_Armor_Body_Gambeson_B_T2_C")},
        {"Doublet", ARMOR_PATH("/Cloth/BP_Armor_Body_Doublet.BP_Armor_Body_Doublet_C")},
        {"Arming Doublet", ARMOR_PATH("/Cloth/BP_Armor_Body_Doublet_Arming.BP_Armor_Body_Doublet_Arming_C")},
        {"Arming Doublet Black", ARMOR_PATH("/Cloth/BP_Armor_Body_Doublet_Arming_Black.BP_Armor_Body_Doublet_Arming_Black_C")},
        {"Arming Doublet Purple", ARMOR_PATH("/Cloth/BP_Armor_Body_Doublet_Arming_Purple.BP_Armor_Body_Doublet_Arming_Purple_C")},
        {"Shirt A", ARMOR_PATH("/Cloth/BP_Armor_Body_Shirt_A.BP_Armor_Body_Shirt_A_C")},
        {"Shirt B", ARMOR_PATH("/Cloth/BP_Armor_Body_Shirt_B.BP_Armor_Body_Shirt_B_C")}
    }};

    static constexpr ItemArray<10> armItems{{
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

    static constexpr ItemArray<19> legItems{{
        {"Cuisse A T2", ARMOR_PATH("/Metal/Legs/BP_Armor_Legs_Cuisse_A_T2.BP_Armor_Legs_Cuisse_A_T2_C")},
        {"Cuisse A T3", ARMOR_PATH("/Metal/Legs/BP_Armor_Legs_Cuisse_A_T3.BP_Armor_Legs_Cuisse_A_T3_C")},
        {"Cuisse B", ARMOR_PATH("/Metal/Legs/BP_Armor_Legs_Cuisse_B.BP_Armor_Legs_Cuisse_B_C")},
        {"Cuisse G", ARMOR_PATH("/Metal/Legs/BP_Armor_Legs_Cuisse_G.BP_Armor_Legs_Cuisse_G_C")},
        {"Greaves T2", ARMOR_PATH("/Metal/Legs/BP_Armor_Legs_Greaves_T2.BP_Armor_Legs_Greaves_T2_C")},
        {"Greaves T3", ARMOR_PATH("/Metal/Legs/BP_Armor_Legs_Greaves_T3.BP_Armor_Legs_Greaves_T3_C")},
        {"Hosen A", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_A.BP_Armor_Legs_Hosen_A_C")},
        {"Hosen A Brown", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_A_Brown.BP_Armor_Legs_Hosen_A_Brown_C")},
        {"Hosen B", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_B.BP_Armor_Legs_Hosen_B_C")},
        {"Hosen C", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_C.BP_Armor_Legs_Hosen_C_C")},
        {"Hosen C Poor", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_C_Poor.BP_Armor_Legs_Hosen_C_Poor_C")},
        {"Hosen C Black", ARMOR_PATH("/Unique/BP_Armor_Hosen_C_Black.BP_Armor_Hosen_C_Black_C")},
        {"Arming Hosen A", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_Arming_A.BP_Armor_Legs_Hosen_Arming_A_C")},
        {"Arming Hosen A Purple", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_Arming_A_Purple.BP_Armor_Legs_Hosen_Arming_A_Purple_C")},
        {"Arming Hosen B", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_Arming_B.BP_Armor_Legs_Hosen_Arming_B_C")},
        {"Arming Hosen B Black", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_Arming_B_Black.BP_Armor_Legs_Hosen_Arming_B_Black_C")},
        {"Arming Hosen C", ARMOR_PATH("/Cloth/BP_Armor_Legs_Hosen_Arming_C.BP_Armor_Legs_Hosen_Arming_C_C")},
        {"Arming Hosen C Blue", ARMOR_PATH("/Unique/BP_Armor_Hosen_Arming_C_Blue.BP_Armor_Hosen_Arming_C_Blue_C")},
        {"Panties", ARMOR_PATH("/Cloth/BP_Armor_Legs_Panties.BP_Armor_Legs_Panties_C")}
    }};

    static constexpr ItemArray<7> handItems{{
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

    static constexpr ItemArray<5> neckItems{{
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

    static constexpr ItemArray<9> propItems{{
        {"Basket", PROP_PATH("/Blueprints/BP_Container_Basket_001.BP_Container_Basket_001_C")},
        {"Candle", PROP_PATH("/Candle/Blueprints/BP_Candle.BP_Candle_C")},
        {"Candle Light", PROP_PATH("/Lights/Blueprints/BP_CandleLight.BP_CandleLight_C")},
        {"Chest Coffin", PROP_PATH("/Blueprints/BP_Container_Chest_Coffin_001.BP_Container_Chest_Coffin_001_C")},
        {"Chest Coffin 2", PROP_PATH("/Blueprints/BP_Container_Chest_Coffin_002.BP_Container_Chest_Coffin_002_C")},
        {"Chest", PROP_PATH("/Blueprints/BP_Container_Chest_001.BP_Container_Chest_001_C")},
        {"Clutter", PROP_PATH("/Blueprints/BP_Container_Clutter_001.BP_Container_Clutter_001_C")},
        {"Skeleton", PROP_PATH("/Blueprints/BP_Container_Skeleton_001.BP_Container_Skeleton_001_C")},
        {"Table", PROP_PATH("/Blueprints/BP_Container_Table_001.BP_Container_Table_001_C")}
    }};

    static constexpr std::array<const ItemInfo*, 8> weaponArrays{{
        swordItems.data(),
        maceItems.data(),
        axeItems.data(),
        polearmItems.data(),
        daggerItems.data(),
        toolItems.data(),
        shieldItems.data(),
        improvisedItems.data()
    }};

    static constexpr std::array<size_t, 8> weaponSizes{{
        swordItems.size(),
        maceItems.size(),
        axeItems.size(),
        polearmItems.size(),
        daggerItems.size(),
        toolItems.size(),
        shieldItems.size(),
        improvisedItems.size()
    }};

    static constexpr std::array<const ItemInfo*, 11> armorArrays{{
        nullptr,
        helmetItems.data(),
        bodyArmorItems.data(),
        armItems.data(),
        legItems.data(),
        handItems.data(),
        feetItems.data(),
        neckItems.data(),
        shoulderItems.data(),
        waistItems.data(),
        nullptr
    }};

    static constexpr std::array<size_t, 11> armorSizes{{
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
        0
    }};

    static inline std::vector<const char*> cachedItemNames;
    static inline uint8_t lastCategoryIndex = 255;
    static inline uint8_t lastWeaponSubcategoryIndex = 255;

    static inline char searchBuffer[128] = "";
    static inline std::vector<uint16_t> filteredIndices;
    static inline bool searchActive = false;

    [[nodiscard]] std::pair<const ItemInfo*, size_t> getCurrentItemArray() const noexcept {
        if (cfg.currentCategoryIndex == WEAPONS_INDEX) [[likely]] {
            return {weaponArrays[cfg.currentWeaponSubcategoryIndex], weaponSizes[cfg.currentWeaponSubcategoryIndex]};
        } else if (cfg.currentCategoryIndex == PROPS_INDEX) [[likely]] {
            return {propItems.data(), propItems.size()};
        } else [[unlikely]] {
            return {armorArrays[cfg.currentCategoryIndex], armorSizes[cfg.currentCategoryIndex]};
        }
    }

    [[nodiscard]] const char* getSelectedClassName() const noexcept {
        const auto [items, size] = getCurrentItemArray();
        return (cfg.currentItemIndex < size && items) ? items[cfg.currentItemIndex].classPath : nullptr;
    }

    [[nodiscard]] static const ItemInfo* getItemAt(uint8_t catIdx, uint8_t subIdx, uint16_t itmIdx) noexcept {
        if (catIdx == WEAPONS_INDEX) {
            return (subIdx < weaponSizes.size() && itmIdx < weaponSizes[subIdx])
                ? &weaponArrays[subIdx][itmIdx] : nullptr;
        }
        if (catIdx == PROPS_INDEX) {
            return (itmIdx < propItems.size()) ? &propItems[itmIdx] : nullptr;
        }
        return (catIdx < armorSizes.size() && itmIdx < armorSizes[catIdx])
            ? &armorArrays[catIdx][itmIdx] : nullptr;
    }

    void updateItemNamesCache() noexcept {
        if (lastCategoryIndex != cfg.currentCategoryIndex || lastWeaponSubcategoryIndex != cfg.currentWeaponSubcategoryIndex) [[unlikely]] {
            const auto [items, size] = getCurrentItemArray();
            cachedItemNames.resize(size);
            for (size_t i = 0; i < size; ++i) {
                cachedItemNames[i] = items[i].displayName;
            }
            lastCategoryIndex = cfg.currentCategoryIndex;
            lastWeaponSubcategoryIndex = cfg.currentWeaponSubcategoryIndex;
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
                            filteredIndices.push_back((catIdx << 12) | (subIdx << 8) | i);
                        }
                    }
                }
            } else if (catIdx == PROPS_INDEX) {
                for (uint16_t i = 0; i < propItems.size(); ++i) {
                    if (stristr(propItems[i].displayName, searchBuffer)) {
                        filteredIndices.push_back((catIdx << 12) | i);
                    }
                }
            } else {
                const auto* items = armorArrays[catIdx];
                const size_t size = armorSizes[catIdx];
                if (items) {
                    for (uint16_t i = 0; i < size; ++i) {
                        if (stristr(items[i].displayName, searchBuffer)) {
                            filteredIndices.push_back((catIdx << 12) | i);
                        }
                    }
                }
            }
        }
    }

    void SpawnSelectedItem() const noexcept {
        const char* className = getSelectedClassName();
        if (!className) [[unlikely]] return;

        auto spawnTransform = player->GetTransform();
        const auto forward = player->GetActorForwardVector();
        spawnTransform.Translation.X += forward.X * cfg.spawnDistanceForward;
        spawnTransform.Translation.Y += forward.Y * cfg.spawnDistanceForward;
        spawnTransform.Translation.Z += cfg.spawnDistanceUp;
        spawnTransform.Scale3D = {cfg.spawnScale, cfg.spawnScale, cfg.spawnScale};
        Spawner::SpawnActor(world, className, spawnTransform, nullptr, cfg.snapToGround);
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

    void Render() override {
        if (!ImGui::CollapsingHeader(name.c_str())) [[unlikely]] return;

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

            if (ImGui::BeginCombo("##FilteredItems", "Select item...")) {
                for (uint16_t packedIdx : filteredIndices) {
                    uint8_t catIdx = (packedIdx >> 12) & 0xF;
                    uint8_t subIdx = (packedIdx >> 8) & 0xF;
                    uint16_t itmIdx = packedIdx & 0xFF;

                    const ItemInfo* item = getItemAt(catIdx, subIdx, itmIdx);

                    if (item) {
                        bool isSelected = (cfg.currentCategoryIndex == catIdx && cfg.currentItemIndex == itmIdx && (catIdx != WEAPONS_INDEX || cfg.currentWeaponSubcategoryIndex == subIdx));
                        if (ImGui::Selectable(item->displayName, isSelected)) {
                            cfg.currentCategoryIndex = catIdx;
                            cfg.currentWeaponSubcategoryIndex = subIdx;
                            cfg.currentItemIndex = itmIdx;
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
                if (ImGui::Combo("##SubcategorySelector", &subIndex, weaponSubcategories.data(), static_cast<int>(weaponSubcategories.size()))) [[unlikely]] {
                    cfg.currentWeaponSubcategoryIndex = static_cast<uint8_t>(subIndex);
                    cfg.currentItemIndex = 0;
                }
            }

            updateItemNamesCache();

            ImGui::Text("Item");
            int itemIndex = static_cast<int>(cfg.currentItemIndex);
            if (ImGui::Combo("##ItemSelector", &itemIndex, cachedItemNames.data(), static_cast<int>(cachedItemNames.size()))) [[unlikely]] {
                cfg.currentItemIndex = static_cast<uint16_t>(itemIndex);
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("Spawn Item")) [[unlikely]] {
            auto validatedSpawn = ValidateAndRun([this]() noexcept { SpawnSelectedItem(); }, player, world);
            validatedSpawn();
        }
    }
};
