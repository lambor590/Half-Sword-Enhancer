#pragma once

#include "Menu/Section.h"
#include "Menu/Keybind.h"

class PlayerAbilitiesSection : public Section {
public:
    struct Config {
        int infiniteStaminaKey = 0x49; // I
        int enemyInfiniteStaminaKey = -1;
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
        int jumpKey = 0x4A;        // J
        int playerSpeedKey = 0x50; // P
        int playerStrengthKey = -1;
        int bodyTonusKey = -1;
        int dashKey = -1;
        int biteAttackKey = -1;
        int enemyBiteKey = -1;
        int enemyBiteAllKey = -1;
        int enemyDrunkKey = -1;

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
        float biteAllRange = 500.0f;
        float enemyDrunkLevel = 1.0f;
        float consciousnessMultiplier = 1.0f;
        float enemyConsciousnessMultiplier = 1.0f;
        int consciousnessMultiplierKey = -1;
        int enemyConsciousnessMultiplierKey = -1;
    };

private:
    Config cfg;
    KeybindEntries keybinds;

    void InitKeybinds();

public:
    explicit PlayerAbilitiesSection(ModContext& ctx);
    void Render() override;
};
