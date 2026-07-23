#pragma once

#include <atomic>

#include "Core/ModContext.h"
#include "Hooks/GameHook.h"

namespace SDK {
    class AActor;
    class ACameraActor;
    class ADebugCameraController;
    class APlayerController;
    class UObject;
    class UCheatManager;
    class UPlayer;
    class UWorld;
    struct FMinimalViewInfo;
}

struct FreeCameraSettings {
    float speedScale = 1.0f;
    float fov = 0.0f;
    bool ignoreTimeDilation = true;
};

struct ScreenOverlaySettings {
    bool visualEffects = true;
    bool resultMenus = true;
    bool onlyInFreeCamera = true;
};

class FreeCameraManager {
public:
    static FreeCameraManager& Get();

    void Apply(bool enabled, bool lockPlayerInput, FreeCameraSettings settings);
    void UpdateSettings(FreeCameraSettings settings);
    void ConfigureScreenOverlays(ScreenOverlaySettings settings);
    [[nodiscard]] bool IsActive(SDK::UWorld* world) const noexcept;
    [[nodiscard]] bool IsPlayerInputLocked(SDK::UWorld* world) const noexcept;
    void OnRuntimeStart();
    // Game-thread only. Restores camera, input, and hidden widgets before hooks are removed.
    void PrepareForRuntimeShutdown(const RuntimeContextSnapshot& runtime);
    void OnRuntimeShutdown() noexcept;

    FreeCameraManager(const FreeCameraManager&) = delete;
    FreeCameraManager& operator=(const FreeCameraManager&) = delete;

private:
    FreeCameraManager() = default;

    void Enable(const RuntimeContextSnapshot& runtime, bool lockPlayerInput, const FreeCameraSettings& settings);
    void Disable(const RuntimeContextSnapshot& runtime);
    bool EnsureCheatManager();
    bool EnableDebugCamera(const RuntimeContextSnapshot& runtime, const FreeCameraSettings& settings);
    void ApplyDebugCameraSettings(const FreeCameraSettings& settings);
    void ApplyFrozenCameraSettings(const FreeCameraSettings& settings);
    void DisableDebugCamera();
    void FreezeDebugCamera(const RuntimeContextSnapshot& runtime, const FreeCameraSettings& settings);
    bool SpawnFrozenCamera(const RuntimeContextSnapshot& runtime, const SDK::FMinimalViewInfo& viewInfo);
    void DestroyFrozenCamera();
    void RestoreOriginalViewTarget();
    void RestoreOriginalPlayerController();
    void ClearState() noexcept;
    void PublishState() noexcept;
    void ApplyPlayerInputLock(const RuntimeContextSnapshot& runtime, bool locked, const FreeCameraSettings& settings);
    void ApplyScreenOverlayVisibility(const RuntimeContextSnapshot& runtime);
    void ApplyScreenOverlay(SDK::UObject* object);
    void RestoreGameplayInput(const SDK::UObject* worldContext);
    void EnsureScreenOverlayHooks();
    [[nodiscard]] bool ShouldHideVisualEffects() noexcept;

    bool playerInputLocked = false;
    int hiddenResultMenuCount = 0;
    GameHook::SubscriptionGroup screenOverlaySubscriptions;
    ScreenOverlaySettings screenOverlays;
    SDK::UWorld* activeWorld = nullptr;
    SDK::APlayerController* originalController = nullptr;
    SDK::UPlayer* originalPlayer = nullptr;
    SDK::UCheatManager* cheatManager = nullptr;
    SDK::ADebugCameraController* debugController = nullptr;
    SDK::AActor* originalViewTarget = nullptr;
    SDK::ACameraActor* frozenCameraActor = nullptr;
    std::atomic<SDK::UWorld*> publishedWorld = nullptr;
    std::atomic_bool publishedInputLocked = false;
};
