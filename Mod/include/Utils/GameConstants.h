#pragma once

#include <iterator>
#include <random>

namespace GameConstants {

    struct ArmorSlotInfo {
        const char* name;
        int slotEnum;
    };

    inline constexpr ArmorSlotInfo ARMOR_SLOTS[] =
        {{"Head", 0},
         {"Hands", 1},
         {"Neck (Bevor)", 4},
         {"Neck (Standard)", 5},
         {"Arms", 6},
         {"Shoulders", 7},
         {"Tabard", 8},
         {"Chest (Plate)", 9},
         {"Hauberk", 10},
         {"Cuisses", 11},
         {"Body (Clothing)", 12},
         {"Waist", 13},
         {"Legs (Greaves)", 14},
         {"Feet", 15},
         {"Hosen", 16}};
    inline constexpr int ARMOR_SLOT_COUNT = static_cast<int>(std::size(ARMOR_SLOTS));
    static_assert(std::size(ARMOR_SLOTS) == ARMOR_SLOT_COUNT);

    inline constexpr const char* WEAPON_TYPE_NAMES[] = {"Arming Sword",  "Short Sword", "Long Sword",
                                                        "Short Mace",    "Mace",        "Long Mace",
                                                        "Short Hafted",  "Hafted",      "Long Hafted",
                                                        "Short Polearm", "Polearm",     "Long Polearm",
                                                        "Short Pollaxe", "Pollaxe",     "Long Pollaxe",
                                                        "Short Casted",  "Casted",      "Long Casted",
                                                        "Messer"};
    inline constexpr int WEAPON_TYPE_COUNT = static_cast<int>(std::size(WEAPON_TYPE_NAMES));

    inline constexpr const char* MATERIAL_LAYER_NAMES[] = {
        "Brushed Steel 1",
        "Brushed Steel 2",
        "Brushed Steel 3",
        "Steel",
        "Iron",
        "Gilded",
        "Copper",
        "Brass",
        "Bronze",
        "Gold",
        "Leather",
        "Turned Leather 1",
        "Turned Leather 2",
        "Turned Leather 3",
        "Wood",
        "Old Wood",
    };
    inline constexpr int MATERIAL_LAYER_COUNT = static_cast<int>(std::size(MATERIAL_LAYER_NAMES));
    static_assert(std::size(MATERIAL_LAYER_NAMES) == MATERIAL_LAYER_COUNT);

    inline int RandomInt(int min, int max) noexcept {
        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }

    constexpr const char* WILLIE_BP_PATH = "/Game/Character/Blueprints/Willie_BP.Willie_BP_C";

    constexpr float DEFAULT_HEALTH = 100.0f;
    constexpr float DEFAULT_PAIN = 0.0f;
    constexpr float DEFAULT_PLAYER_SPEED = 1.5f;
    constexpr float DEFAULT_MUSCLE_POWER = 35.0f;
    constexpr float DEFAULT_GRAB_FORCE = 10000.0f;
    constexpr float DEFAULT_HANDS_RIGIDITY = 0.666f;
    constexpr float DEFAULT_ALL_BODY_TONUS = 100.0f;
    constexpr float DEFAULT_TIME_DILATION = 1.0f;
    constexpr float DEFAULT_GRAVITY = -980.0f;
    constexpr float MIN_HEALTH = 0.0f;
    constexpr float MAX_DISTANCE = FLT_MAX;
    constexpr float FULL_TONUS = 1.0f;
    constexpr float GET_UP_RATE = 1.0f;
}