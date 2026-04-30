#pragma once

#include <atomic>

namespace SDK {
    class UWorld;
    class AWillie_BP_C;
    class APlayerController;
    class AWorldSettings;
}

class GameHook;
class ConfigManager;

struct RuntimeContextSnapshot {
    SDK::UWorld* world = nullptr;
    SDK::AWillie_BP_C* player = nullptr;
    SDK::APlayerController* controller = nullptr;
    SDK::AWorldSettings* worldSettings = nullptr;
};

class ModContext {
public:
    static ModContext& Get();

    RuntimeContextSnapshot RefreshGameThreadCache();
    RuntimeContextSnapshot GetRenderSnapshot() const noexcept;

    GameHook& gameHook;
    ConfigManager& configManager;

private:
    ModContext();

    RuntimeContextSnapshot gameThreadSnapshot{};

    std::atomic<SDK::UWorld*> renderWorld{nullptr};
    std::atomic<SDK::AWillie_BP_C*> renderPlayer{nullptr};
    std::atomic<SDK::APlayerController*> renderController{nullptr};
    std::atomic<SDK::AWorldSettings*> renderWorldSettings{nullptr};
};
