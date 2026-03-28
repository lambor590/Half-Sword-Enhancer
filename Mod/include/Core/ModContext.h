#pragma once

namespace SDK {
    class UWorld;
    class AWillie_BP_C;
    class APlayerController;
    class AWorldSettings;
}

class GameHook;
class ConfigManager;

/// Single context object replacing global pointer soup.
/// Provides per-frame cached component pointers validated once per render frame.
/// Sections and GUI access game components through this instead of ComponentValidator chains.
class ModContext {
public:
    static ModContext& Get();

    /// Validates and caches world/player/controller/worldSettings.
    /// Called once per render frame before sections access these pointers.
    void RefreshCache();

    // Per-frame cached component pointers
    SDK::UWorld* world = nullptr;
    SDK::AWillie_BP_C* player = nullptr;
    SDK::APlayerController* controller = nullptr;
    SDK::AWorldSettings* worldSettings = nullptr;

    // Subsystem references
    GameHook& gameHook;
    ConfigManager& configManager;

private:
    ModContext();
};
