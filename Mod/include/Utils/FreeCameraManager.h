#pragma once

#include "Core/ModContext.h"

namespace SDK {
    class AActor;
    class ACameraActor;
    class ADebugCameraController;
    class APlayerController;
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

class FreeCameraManager {
public:
    static FreeCameraManager& Get();

    void Apply(bool enabled, bool lockPlayerInput, FreeCameraSettings settings);

    FreeCameraManager(const FreeCameraManager&) = delete;
    FreeCameraManager& operator=(const FreeCameraManager&) = delete;

private:
    FreeCameraManager() = default;

    void Enable(const RuntimeContextSnapshot& runtime, bool lockPlayerInput, const FreeCameraSettings& settings);
    void Disable(const RuntimeContextSnapshot& runtime);
    bool EnsureCheatManager();
    bool EnableDebugCamera(const RuntimeContextSnapshot& runtime, const FreeCameraSettings& settings);
    void ApplyDebugCameraSettings(const FreeCameraSettings& settings);
    void DisableDebugCamera();
    void FreezeDebugCamera(const RuntimeContextSnapshot& runtime, const FreeCameraSettings& settings);
    bool SpawnFrozenCamera(const RuntimeContextSnapshot& runtime, const SDK::FMinimalViewInfo& viewInfo);
    void DestroyFrozenCamera();
    void RestoreOriginalViewTarget();
    void RestoreOriginalPlayerController();
    void ClearState() noexcept;
    void ApplyPlayerInputLock(const RuntimeContextSnapshot& runtime, bool locked, const FreeCameraSettings& settings);

    bool active = false;
    bool playerInputLocked = false;
    SDK::UWorld* activeWorld = nullptr;
    SDK::APlayerController* originalController = nullptr;
    SDK::UPlayer* originalPlayer = nullptr;
    SDK::UCheatManager* cheatManager = nullptr;
    SDK::ADebugCameraController* debugController = nullptr;
    SDK::AActor* originalViewTarget = nullptr;
    SDK::ACameraActor* frozenCameraActor = nullptr;
};
