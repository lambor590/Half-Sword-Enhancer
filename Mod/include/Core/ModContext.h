#pragma once

namespace SDK {
    class UWorld;
    class AWillie_BP_C;
    class APlayerController;
    class AWorldSettings;
}

class GameHook;
class ConfigManager;

/// Caches the game pointers most sections read every frame.
class ModContext {
public:
    static ModContext& Get();

    /// Refreshes world, player, controller, and worldSettings from the current engine state.
    void RefreshCache();

    SDK::UWorld* world = nullptr;
    SDK::AWillie_BP_C* player = nullptr;
    SDK::APlayerController* controller = nullptr;
    SDK::AWorldSettings* worldSettings = nullptr;

    GameHook& gameHook;
    ConfigManager& configManager;

private:
    ModContext();
};
