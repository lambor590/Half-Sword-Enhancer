#pragma once

#include <atomic>

#include "Menu/Section.h"
#include "Menu/Keybind.h"

class WorldActionsSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::World, "Game Actions", "Change game speed and gravity, affect NPCs, or clean up the current map."
    };

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
        float toggleEnemyAIRadius = 1000.0f;
        float clearObjectsRadius = 1000.0f;

        bool snapNeckEnemies = false;
        bool destroyDeadOnly = true;
        bool destroyDisintegrate = true;
    };

private:
    Config cfg;
    std::atomic<SDK::UWorld*> stateWorld = nullptr;
    std::atomic_bool slowMotionActive = false;
    std::atomic_bool customGravityActive = false;
    std::atomic_bool paused = false;
    KeybindList keybinds;

    void InitKeybinds();
    void SyncStateWorld(SDK::UWorld* world) noexcept;
    bool CurrentWorldState(const std::atomic_bool& state) const noexcept;

public:
    explicit WorldActionsSection(ModContext& ctx);
    void Render() override;
    KeybindList* GetSearchKeybinds() noexcept override { return &keybinds; }
};
