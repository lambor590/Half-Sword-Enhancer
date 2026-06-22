#include "Utils/FreeCameraManager.h"

#include <algorithm>

#include "Hooks/GameHook.h"

#include "SDK/Engine_classes.hpp"
#include "SDK/Engine_structs.hpp"

namespace {
    [[nodiscard]] float CustomFov(const FreeCameraSettings& settings) noexcept {
        return settings.fov <= 0.0f ? 0.0f : std::clamp(settings.fov, 5.0f, 170.0f);
    }

    [[nodiscard]] SDK::AActor* PawnActor(SDK::APlayerController* controller) noexcept {
        return controller && controller->Pawn ? static_cast<SDK::AActor*>(controller->Pawn) : nullptr;
    }

    [[nodiscard]] SDK::ADebugCameraController* AsDebugCameraController(SDK::APlayerController* controller) noexcept {
        return controller && controller->IsA(SDK::ADebugCameraController::StaticClass())
                   ? static_cast<SDK::ADebugCameraController*>(controller)
                   : nullptr;
    }

    [[nodiscard]] SDK::APlayerController* UnwrapOriginalController(SDK::APlayerController* controller) noexcept {
        for (int depth = 0; depth < 4; ++depth) {
            auto* debug = AsDebugCameraController(controller);
            if (!debug || !debug->OriginalControllerRef || debug->OriginalControllerRef == controller) break;
            controller = debug->OriginalControllerRef;
        }
        return controller;
    }
}

FreeCameraManager& FreeCameraManager::Get() {
    static FreeCameraManager instance;
    return instance;
}

void FreeCameraManager::Apply(bool enabled, bool lockPlayerInput, FreeCameraSettings settings) {
    GameHook::QueueAction([enabled, lockPlayerInput, settings](const RuntimeContextSnapshot& runtime) {
        auto& manager = FreeCameraManager::Get();
        if (enabled) {
            manager.Enable(runtime, lockPlayerInput, settings);
        } else {
            manager.Disable(runtime);
        }
    });
}

void FreeCameraManager::Enable(
    const RuntimeContextSnapshot& runtime, bool lockPlayerInput, const FreeCameraSettings& settings
) {
    if (!runtime.world || !runtime.controller) {
        ClearState();
        return;
    }

    if (active) {
        const bool wasPlayerInputLocked = playerInputLocked;
        ApplyPlayerInputLock(runtime, lockPlayerInput, settings);
        if (wasPlayerInputLocked && playerInputLocked) {
            ApplyDebugCameraSettings(settings);
        }
        return;
    }

    active = true;
    activeWorld = runtime.world;
    originalController = UnwrapOriginalController(runtime.controller);
    originalViewTarget = runtime.controller->GetViewTarget();
    playerInputLocked = false;

    ApplyPlayerInputLock(runtime, lockPlayerInput, settings);
}

void FreeCameraManager::Disable(const RuntimeContextSnapshot& runtime) {
    if (!active && AsDebugCameraController(runtime.controller)) {
        active = true;
        activeWorld = runtime.world;
        debugController = AsDebugCameraController(runtime.controller);
        originalController = UnwrapOriginalController(debugController->OriginalControllerRef);
        originalPlayer = debugController->OriginalPlayer;
        cheatManager = originalController ? originalController->CheatManager : nullptr;
        playerInputLocked = true;
    }

    const bool sameWorld = activeWorld ? runtime.world == activeWorld : runtime.world != nullptr;

    if (sameWorld && debugController) {
        DisableDebugCamera();
    }

    if (sameWorld) {
        RestoreOriginalViewTarget();
        DestroyFrozenCamera();
    }

    ClearState();
}

bool FreeCameraManager::EnsureCheatManager() {
    if (!originalController) {
        return false;
    }

    cheatManager = originalController->CheatManager;
    if (!cheatManager) {
        originalController->EnableCheats();
        cheatManager = originalController->CheatManager;
    }

    if (!cheatManager) {
        auto* cheatObject = SDK::UGameplayStatics::SpawnObject(SDK::UCheatManager::StaticClass(), originalController);
        if (cheatObject && cheatObject->IsA(SDK::UCheatManager::StaticClass())) {
            cheatManager = static_cast<SDK::UCheatManager*>(cheatObject);
            originalController->CheatManager = cheatManager;
            originalController->CheatClass = SDK::UCheatManager::StaticClass();
            cheatManager->DebugCameraControllerClass = SDK::ADebugCameraController::StaticClass();
            cheatManager->ReceiveInitCheatManager();
        }
    }

    if (!cheatManager) {
        return false;
    }

    if (!cheatManager->DebugCameraControllerClass.Get()) {
        cheatManager->DebugCameraControllerClass = SDK::ADebugCameraController::StaticClass();
    }
    return true;
}

bool FreeCameraManager::EnableDebugCamera(const RuntimeContextSnapshot& runtime, const FreeCameraSettings& settings) {
    if (!runtime.world || !originalController || !EnsureCheatManager()) {
        return false;
    }

    cheatManager->EnableDebugCamera();
    debugController = cheatManager->DebugCameraControllerRef;

    if (!debugController && runtime.controller && runtime.controller->IsA(SDK::ADebugCameraController::StaticClass())) {
        debugController = static_cast<SDK::ADebugCameraController*>(runtime.controller);
    }
    if (debugController) {
        originalController =
            UnwrapOriginalController(originalController ? originalController : debugController->OriginalControllerRef);
        originalPlayer = originalPlayer ? originalPlayer : debugController->OriginalPlayer;
    }

    if (!debugController) {
        return false;
    }

    if (debugController->PlayerCameraManager) {
        debugController->PlayerCameraManager->SetGameCameraCutThisFrame();
    } else if (originalController && originalController->PlayerCameraManager) {
        originalController->PlayerCameraManager->SetGameCameraCutThisFrame();
    }
    ApplyDebugCameraSettings(settings);
    DestroyFrozenCamera();
    return true;
}

void FreeCameraManager::ApplyDebugCameraSettings(const FreeCameraSettings& settings) {
    if (!debugController) {
        return;
    }

    const float speedScale = settings.speedScale > 0.0f ? settings.speedScale : 1.0f;
    debugController->SetPawnMovementSpeedScale(speedScale);
    if (const float fov = CustomFov(settings); fov > 0.0f) {
        debugController->FOV(fov);
    }

    auto* spectatorPawn = debugController->GetSpectatorPawn();
    auto* movement = spectatorPawn ? spectatorPawn->GetMovementComponent() : nullptr;

    if (movement && movement->IsA(SDK::USpectatorPawnMovement::StaticClass())) {
        static_cast<SDK::USpectatorPawnMovement*>(movement)->bIgnoreTimeDilation = settings.ignoreTimeDilation;
    }
}

void FreeCameraManager::DisableDebugCamera() {
    auto* manager = cheatManager ? cheatManager : (originalController ? originalController->CheatManager : nullptr);
    if (!manager) {
        debugController = nullptr;
        return;
    }

    manager->DisableDebugCamera();
    RestoreOriginalPlayerController();
    debugController = nullptr;
}

void FreeCameraManager::FreezeDebugCamera(const RuntimeContextSnapshot& runtime, const FreeCameraSettings& settings) {
    SDK::FMinimalViewInfo viewInfo{};
    auto* debug = debugController ? debugController : AsDebugCameraController(runtime.controller);
    SDK::APlayerController* cameraSource = debug ? static_cast<SDK::APlayerController*>(debug) : originalController;
    if (!cameraSource) {
        return;
    }

    if (cameraSource->PlayerCameraManager) {
        viewInfo = cameraSource->PlayerCameraManager->CameraCachePrivate.POV;
    } else {
        cameraSource->GetPlayerViewPoint(&viewInfo.Location, &viewInfo.Rotation);
        viewInfo.FOV = 90.0f;
        viewInfo.AspectRatio = 16.0f / 9.0f;
        viewInfo.ProjectionMode = SDK::ECameraProjectionMode::Perspective;
    }
    if (const float fov = CustomFov(settings); fov > 0.0f) {
        viewInfo.FOV = fov;
    }

    DisableDebugCamera();
    SpawnFrozenCamera(runtime, viewInfo);
}

bool FreeCameraManager::SpawnFrozenCamera(const RuntimeContextSnapshot& runtime, const SDK::FMinimalViewInfo& viewInfo) {
    if (!runtime.world || !originalController) {
        return false;
    }

    DestroyFrozenCamera();

    SDK::FTransform transform{};
    transform.Translation = viewInfo.Location;
    transform.Scale3D = {1.0, 1.0, 1.0};
    auto* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
        runtime.world, SDK::ACameraActor::StaticClass(), transform, SDK::ESpawnActorCollisionHandlingMethod::AlwaysSpawn,
        nullptr, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime
    );
    if (!actor) {
        return false;
    }

    if (auto* finished = SDK::UGameplayStatics::FinishSpawningActor(
            actor, transform, SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime
        )) {
        actor = finished;
    }
    if (!actor->IsA(SDK::ACameraActor::StaticClass())) {
        actor->K2_DestroyActor();
        return false;
    }

    frozenCameraActor = static_cast<SDK::ACameraActor*>(actor);
    frozenCameraActor->K2_SetActorLocationAndRotation(viewInfo.Location, viewInfo.Rotation, false, nullptr, true);
    if (auto* cameraComponent = frozenCameraActor->CameraComponent) {
        cameraComponent->SetProjectionMode(viewInfo.ProjectionMode);
        if (viewInfo.FOV > 0.0f) {
            cameraComponent->SetFieldOfView(viewInfo.FOV);
        }
        if (viewInfo.AspectRatio > 0.0f) {
            cameraComponent->SetAspectRatio(viewInfo.AspectRatio);
        }
        cameraComponent->SetConstraintAspectRatio(viewInfo.bConstrainAspectRatio);
        if (originalPlayer && originalPlayer->IsA(SDK::ULocalPlayer::StaticClass())) {
            auto* localPlayer = static_cast<SDK::ULocalPlayer*>(originalPlayer);
            cameraComponent->SetAspectRatioAxisConstraint(localPlayer->AspectRatioAxisConstraint);
        }
    }
    originalController->SetViewTargetWithBlend(
        frozenCameraActor, 0.0f, SDK::EViewTargetBlendFunction::VTBlend_Linear, 0.0f, false
    );
    if (originalController->PlayerCameraManager) {
        originalController->PlayerCameraManager->SetGameCameraCutThisFrame();
    }
    return true;
}

void FreeCameraManager::DestroyFrozenCamera() {
    if (!frozenCameraActor) {
        return;
    }

    frozenCameraActor->K2_DestroyActor();
    frozenCameraActor = nullptr;
}

void FreeCameraManager::RestoreOriginalViewTarget() {
    if (!originalController) {
        return;
    }

    auto* viewTarget = originalViewTarget ? originalViewTarget : PawnActor(originalController);
    if (!viewTarget) {
        return;
    }

    originalController->SetViewTargetWithBlend(
        viewTarget, 0.0f, SDK::EViewTargetBlendFunction::VTBlend_Linear, 0.0f, false
    );
}

void FreeCameraManager::RestoreOriginalPlayerController() {
    if (!originalPlayer || !originalController) {
        return;
    }

    originalPlayer->PlayerController = originalController;
    originalController->Player = originalPlayer;
}

void FreeCameraManager::ApplyPlayerInputLock(
    const RuntimeContextSnapshot& runtime, bool locked, const FreeCameraSettings& settings
) {
    if (locked == playerInputLocked && ((locked && debugController) || (!locked && frozenCameraActor))) {
        return;
    }

    if (locked) {
        if (EnableDebugCamera(runtime, settings)) {
            playerInputLocked = true;
        }
        return;
    }

    if (debugController || AsDebugCameraController(runtime.controller)) {
        FreezeDebugCamera(runtime, settings);
    }
    playerInputLocked = false;
}

void FreeCameraManager::ClearState() noexcept {
    active = false;
    playerInputLocked = false;
    activeWorld = nullptr;
    originalController = nullptr;
    originalPlayer = nullptr;
    cheatManager = nullptr;
    debugController = nullptr;
    originalViewTarget = nullptr;
    frozenCameraActor = nullptr;
}
