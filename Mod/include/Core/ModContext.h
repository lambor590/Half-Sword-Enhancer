#pragma once

#include <atomic>
#include <cstdint>

namespace SDK {
    class UWorld;
    class AWillie_BP_C;
    class APlayerController;
    class AWorldSettings;
}

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

private:
    ModContext() = default;

    std::atomic<SDK::UWorld*> renderWorld{nullptr};
    std::atomic<std::int32_t> renderWorldIndex{-1};
    std::atomic<SDK::AWillie_BP_C*> renderPlayer{nullptr};
    std::atomic<std::int32_t> renderPlayerIndex{-1};
    std::atomic<SDK::APlayerController*> renderController{nullptr};
    std::atomic<std::int32_t> renderControllerIndex{-1};
    std::atomic<SDK::AWorldSettings*> renderWorldSettings{nullptr};
    std::atomic<std::int32_t> renderWorldSettingsIndex{-1};
};
