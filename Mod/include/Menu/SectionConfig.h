#pragma once

#include <cstdint>

namespace SectionConfig {

    struct PlayerConfig {
        int infiniteStaminaKey = 0x49; // I
        int infiniteConsciousnessKey = -1;
        int enemyInfiniteConsciousnessKey = -1;
        int getUpKey = -1;
        int possessWillieKey = -1;
        int invulnerabilityKey = -1;
        int noPainKey = -1;
        int noKickCooldownKey = -1;
        int enemyNoPainKey = -1;
        int ragdollKey = -1;
        int enemyRagdollKey = -1;
        int jumpKey = 0x4A; // J
        int playerSpeedKey = 0x50; // P
        int playerStrengthKey = -1;
        int bodyTonusKey = -1;
        int dashKey = -1;
        int biteAttackKey = -1;
        int enemyBiteKey = -1;

        float jumpForce = 5000.0f;
        float playerRunMultiplier = 1.0f;
        float playerWalkMultiplier = 1.0f;
        float playerStrengthMultiplier = 1.0f;
        float playerGrabForceMultiplier = 1.0f;
        float playerHandsRigidityMultiplier = 1.0f;
        float bodyTonusAllBodyMultiplier = 1.0f;
        bool bodyTonusNoBodyWeakening = false;
        float dashForce = 7000.0f;
        float biteRange = 300.0f;
    };

    struct WorldConfig {
        int sloMoKey = 0x5A; // Z
        int customGravityKey = 0x4C; // L
        int killAllEnemiesKey = -1;
        int toggleEnemyAIKey = -1;
        int destroyWilliesKey = -1;
        int clearBloodKey = -1;
        int clearObjectsKey = -1;
        int setGamePausedKey = -1;

        float slowMotionSpeed = 0.4f;
        float customGravityValue = 0.0f;
        float killAllEnemiesRadius = 1000.0f;
        bool snapNeckEnemies = false;
        float toggleEnemyAIRadius = 1000.0f;
        bool destroyDeadOnly = true;
        bool destroyDisintegrate = true;
        float clearObjectsRadius = 1000.0f;
    };

    struct NPCConfig {
        int spawnEnemyKey = 0x4E; // N

        float spawnDistanceForward = 200.0f;
        float spawnDistanceUp = 0.0f;
        float spawnScale = 1.0f;
        bool bodyguard = false;
        bool snapToGround = true;
        int npcTeam = 0;
        int npcTypeIndex = 0;
    };

    struct ItemConfig {
        int spawnItemKey = -1;

        float spawnDistanceForward = 150.0f;
        float spawnDistanceUp = 50.0f;
        float spawnScale = 1.0f;
        bool snapToGround = true;

        uint8_t currentCategoryIndex = 0;
        uint8_t currentWeaponSubcategoryIndex = 0;
        uint16_t currentItemIndex = 0;
    };

    inline PlayerConfig player;
    inline WorldConfig world;
    inline NPCConfig npc;
    inline ItemConfig item;

}
