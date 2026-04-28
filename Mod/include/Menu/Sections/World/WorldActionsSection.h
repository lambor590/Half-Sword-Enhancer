#pragma once

#include "Menu/Section.h"
#include "Menu/Keybind.h"

class WorldActionsSection : public Section {
public:
    struct Config {
        int sloMoKey = 0x5A;         // Z
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

private:
    Config cfg;
    KeybindEntries keybinds;

    void InitKeybinds();

public:
    explicit WorldActionsSection(ModContext& ctx);
    void Render() override;
};
