#include "Utils/FreeCameraManager.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "Hooks/GameHook.h"

#include "SDK/Engine_classes.hpp"
#include "SDK/Engine_parameters.hpp"
#include "SDK/Engine_structs.hpp"
#include "SDK/UI_DED_classes.hpp"
#include "SDK/UI_DeathDoor_classes.hpp"
#include "SDK/UI_GiveUp_classes.hpp"
#include "SDK/UI_HUD_classes.hpp"
#include "SDK/UI_Lose_classes.hpp"
#include "SDK/UI_WIN_classes.hpp"
#include "SDK/UI_WinScreen_classes.hpp"
#include "SDK/UMG_classes.hpp"
#include "SDK/UMG_parameters.hpp"

namespace {
    struct SavedWidgetState {
        SDK::UWidget* widget = nullptr;
        SDK::ESlateVisibility visibility = SDK::ESlateVisibility::Visible;
        bool enabled = true;
    };

    std::vector<SavedWidgetState>& SavedOverlayStates() {
        static std::vector<SavedWidgetState> states;
        return states;
    }

    [[nodiscard]] float CustomFov(const FreeCameraSettings& settings) noexcept {
        return settings.fov <= 0.0f ? 0.0f : std::clamp(settings.fov, 5.0f, 170.0f);
    }

    [[nodiscard]] SDK::AActor* PawnActor(SDK::APlayerController* controller) noexcept {
        return controller && controller->Pawn ? static_cast<SDK::AActor*>(controller->Pawn) : nullptr;
    }

    [[nodiscard]] SDK::ADebugCameraController* AsDebugCameraController(SDK::APlayerController* controller) {
        return controller && controller->IsA(SDK::ADebugCameraController::StaticClass())
                   ? static_cast<SDK::ADebugCameraController*>(controller)
                   : nullptr;
    }

    [[nodiscard]] SDK::APlayerController* UnwrapOriginalController(SDK::APlayerController* controller) {
        for (int depth = 0; depth < 4; ++depth) {
            auto* debug = AsDebugCameraController(controller);
            if (!debug || !debug->OriginalControllerRef || debug->OriginalControllerRef == controller) break;
            controller = debug->OriginalControllerRef;
        }
        return controller;
    }

    bool SetScreenOverlayHidden(SDK::UWidget* widget, bool hidden) {
        if (!widget) return false;

        auto& saved = SavedOverlayStates();
        const auto existing = std::ranges::find(saved, widget, &SavedWidgetState::widget);
        if (hidden) {
            const bool changed = existing == saved.end();
            if (changed) {
                saved.push_back({widget, widget->GetVisibility(), widget->GetIsEnabled()});
            }
            widget->SetIsEnabled(false);
            widget->SetVisibility(SDK::ESlateVisibility::Collapsed);
            return changed;
        }

        if (existing == saved.end()) return false;
        widget->SetIsEnabled(existing->enabled);
        widget->SetVisibility(existing->visibility);
        saved.erase(existing);
        return true;
    }

    void SetHudEffectWidgetsHidden(SDK::UUI_HUD_C* hud, bool hidden) {
        if (!hud) return;

        SetScreenOverlayHidden(hud->ArmLDmg, hidden);
        SetScreenOverlayHidden(hud->ArmRDmg, hidden);
        SetScreenOverlayHidden(hud->Black, hidden);
        SetScreenOverlayHidden(hud->HeadDmg, hidden);
        SetScreenOverlayHidden(hud->HPDmg1, hidden);
        SetScreenOverlayHidden(hud->HPDmg2, hidden);
        SetScreenOverlayHidden(hud->HPDmg3, hidden);
        SetScreenOverlayHidden(hud->LegLDmg, hidden);
        SetScreenOverlayHidden(hud->LegRDmg, hidden);
        SetScreenOverlayHidden(hud->TextHurt, hidden);
        SetScreenOverlayHidden(hud->TextWin, hidden);
        SetScreenOverlayHidden(hud->Vignette, hidden);
        SetScreenOverlayHidden(hud->Vignette_Pain, hidden);
        SetScreenOverlayHidden(hud->Vignette_WakeUp, hidden);
    }

    [[nodiscard]] bool IsResultMenu(SDK::UObject* object) {
        return object->IsA(SDK::UUI_Lose_C::StaticClass()) || object->IsA(SDK::UUI_DeathDoor_C::StaticClass()) ||
               object->IsA(SDK::UUI_DED_C::StaticClass()) || object->IsA(SDK::UUI_GiveUp_C::StaticClass()) ||
               object->IsA(SDK::UUI_WIN_C::StaticClass()) || object->IsA(SDK::UUI_WinScreen_C::StaticClass());
    }

    bool FindWidgets(SDK::UObject* worldContext, SDK::UClass* widgetClass, SDK::TArray<SDK::UUserWidget*>& out) {
        auto* libraryClass = SDK::UWidgetBlueprintLibrary::StaticClass();
        auto* findWidgets =
            libraryClass ? libraryClass->GetFunction("WidgetBlueprintLibrary", "GetAllWidgetsOfClass") : nullptr;
        auto* library = findWidgets ? SDK::UWidgetBlueprintLibrary::GetDefaultObj() : nullptr;
        if (!library) return false;

        SDK::Params::WidgetBlueprintLibrary_GetAllWidgetsOfClass params{};
        params.WorldContextObject = worldContext;
        params.WidgetClass = widgetClass;
        params.TopLevelOnly = false;

        const auto flags = findWidgets->FunctionFlags;
        findWidgets->FunctionFlags |= 0x400;
        library->ProcessEvent(findWidgets, &params);
        findWidgets->FunctionFlags = flags;
        out = params.FoundWidgets;
        return true;
    }

    template <typename Widget> int ApplyScreenOverlayClass(SDK::UObject* worldContext, bool hidden) {
        SDK::TArray<SDK::UUserWidget*> widgets;
        if (!FindWidgets(worldContext, Widget::StaticClass(), widgets)) return 0;
        int delta = 0;
        for (int i = 0; i < widgets.Num(); ++i) {
            if (SetScreenOverlayHidden(widgets[i], hidden)) {
                delta += hidden ? 1 : -1;
            }
        }
        return delta;
    }

    void ApplyHudEffectWidgets(SDK::UObject* worldContext, bool hidden) {
        SDK::TArray<SDK::UUserWidget*> widgets;
        if (!FindWidgets(worldContext, SDK::UUI_HUD_C::StaticClass(), widgets)) return;
        for (int i = 0; i < widgets.Num(); ++i) {
            SetHudEffectWidgetsHidden(static_cast<SDK::UUI_HUD_C*>(widgets[i]), hidden);
        }
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

void FreeCameraManager::ConfigureScreenOverlays(ScreenOverlaySettings settings) {
    GameHook::QueueAction([settings](const RuntimeContextSnapshot& runtime) {
        auto& manager = FreeCameraManager::Get();
        manager.screenOverlays = settings;
        manager.EnsureScreenOverlayHooks();
        manager.ApplyScreenOverlayVisibility(runtime);
    });
}

void FreeCameraManager::OnRuntimeStart() {
    GameHook::QueueAction([](const RuntimeContextSnapshot&) {
        FreeCameraManager::Get().EnsureScreenOverlayHooks();
    });
}

void FreeCameraManager::UpdateSettings(FreeCameraSettings settings) {
    GameHook::QueueAction([settings](const RuntimeContextSnapshot& runtime) {
        auto& manager = FreeCameraManager::Get();
        if (!manager.activeWorld) return;
        if (!runtime.world || manager.activeWorld != runtime.world) {
            manager.ClearState();
            return;
        }
        if (manager.playerInputLocked) {
            manager.ApplyDebugCameraSettings(settings);
        } else {
            manager.ApplyFrozenCameraSettings(settings);
        }
    });
}

bool FreeCameraManager::IsActive(SDK::UWorld* world) const noexcept {
    return world && publishedWorld.load(std::memory_order_acquire) == world;
}

bool FreeCameraManager::IsPlayerInputLocked(SDK::UWorld* world) const noexcept {
    return IsActive(world) && publishedInputLocked.load(std::memory_order_acquire);
}

void FreeCameraManager::PrepareForRuntimeShutdown(const RuntimeContextSnapshot& runtime) {
    auto cleanupRuntime = runtime;
    if (!cleanupRuntime.world && activeWorld && SDK::UKismetSystemLibrary::IsValid(activeWorld)) {
        cleanupRuntime.world = activeWorld;
    }
    if (!cleanupRuntime.controller) {
        if (debugController && SDK::UKismetSystemLibrary::IsValid(debugController)) {
            cleanupRuntime.controller = debugController;
        } else if (originalController && SDK::UKismetSystemLibrary::IsValid(originalController)) {
            cleanupRuntime.controller = originalController;
        }
    }

    const auto configuredOverlays = screenOverlays;
    screenOverlays.visualEffects = false;
    screenOverlays.resultMenus = false;
    Disable(cleanupRuntime);
    screenOverlays = configuredOverlays;
    hiddenResultMenuCount = 0;
}

void FreeCameraManager::OnRuntimeShutdown() noexcept {
    screenOverlaySubscriptions.Reset();
    SavedOverlayStates().clear();
    ClearState();
}

void FreeCameraManager::EnsureScreenOverlayHooks() {
    if (screenOverlaySubscriptions.IsSubscribed()) return;

    screenOverlaySubscriptions.Reset();
    const auto constructHook = screenOverlaySubscriptions.Subscribe(
        "Construct", GameHook::HookPhase::After, [](GameHook::ProcessEventContext& context) {
            FreeCameraManager::Get().ApplyConstructedScreenOverlay(context.object);
        }
    );
    const auto pauseHook = screenOverlaySubscriptions.Subscribe(
        "SetGamePaused", GameHook::HookPhase::After, [](GameHook::ProcessEventContext& context) {
            auto& manager = FreeCameraManager::Get();
            const auto* params = context.Params<SDK::Params::GameplayStatics_SetGamePaused>();
            if (!params || !params->bPaused || !manager.screenOverlays.resultMenus ||
                !manager.ShouldHideScreenOverlays() || manager.hiddenResultMenuCount <= 0) {
                return;
            }
            manager.RestoreGameplayInput(params->WorldContextObject);
        }
    );
    if (constructHook == GameHook::INVALID_HOOK_HANDLE || pauseHook == GameHook::INVALID_HOOK_HANDLE) {
        screenOverlaySubscriptions.Reset();
    }
}

void FreeCameraManager::Enable(
    const RuntimeContextSnapshot& runtime, bool lockPlayerInput, const FreeCameraSettings& settings
) {
    if (!runtime.world || !runtime.controller) {
        ClearState();
        return;
    }

    if (activeWorld && activeWorld != runtime.world) ClearState();

    if (activeWorld) {
        const bool wasPlayerInputLocked = playerInputLocked;
        ApplyPlayerInputLock(runtime, lockPlayerInput, settings);
        if (wasPlayerInputLocked == playerInputLocked) {
            if (playerInputLocked) {
                ApplyDebugCameraSettings(settings);
            } else {
                ApplyFrozenCameraSettings(settings);
            }
        }
        ApplyScreenOverlayVisibility(runtime);
        PublishState();
        return;
    }

    activeWorld = runtime.world;
    originalController = UnwrapOriginalController(runtime.controller);
    originalViewTarget = runtime.controller->GetViewTarget();
    playerInputLocked = false;

    ApplyPlayerInputLock(runtime, lockPlayerInput, settings);
    ApplyScreenOverlayVisibility(runtime);
    PublishState();
}

void FreeCameraManager::Disable(const RuntimeContextSnapshot& runtime) {
    if (!activeWorld && AsDebugCameraController(runtime.controller)) {
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
    ApplyScreenOverlayVisibility(runtime);
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

void FreeCameraManager::ApplyFrozenCameraSettings(const FreeCameraSettings& settings) {
    if (!frozenCameraActor || !frozenCameraActor->CameraComponent) return;
    if (const float fov = CustomFov(settings); fov > 0.0f) {
        frozenCameraActor->CameraComponent->SetFieldOfView(fov);
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

bool FreeCameraManager::SpawnFrozenCamera(
    const RuntimeContextSnapshot& runtime, const SDK::FMinimalViewInfo& viewInfo
) {
    if (!runtime.world || !originalController) {
        return false;
    }

    DestroyFrozenCamera();

    SDK::FTransform transform{};
    transform.Translation = viewInfo.Location;
    transform.Scale3D = {1.0, 1.0, 1.0};
    auto* actor = SDK::UGameplayStatics::BeginDeferredActorSpawnFromClass(
        runtime.world, SDK::ACameraActor::StaticClass(), transform,
        SDK::ESpawnActorCollisionHandlingMethod::AlwaysSpawn, nullptr,
        SDK::ESpawnActorScaleMethod::SelectDefaultAtRuntime
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
    originalController
        ->SetViewTargetWithBlend(frozenCameraActor, 0.0f, SDK::EViewTargetBlendFunction::VTBlend_Linear, 0.0f, false);
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

    originalController
        ->SetViewTargetWithBlend(viewTarget, 0.0f, SDK::EViewTargetBlendFunction::VTBlend_Linear, 0.0f, false);
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
    playerInputLocked = false;
    activeWorld = nullptr;
    originalController = nullptr;
    originalPlayer = nullptr;
    cheatManager = nullptr;
    debugController = nullptr;
    originalViewTarget = nullptr;
    frozenCameraActor = nullptr;
    PublishState();
}

void FreeCameraManager::PublishState() noexcept {
    publishedInputLocked.store(playerInputLocked, std::memory_order_release);
    publishedWorld.store(activeWorld, std::memory_order_release);
}

bool FreeCameraManager::ShouldHideScreenOverlays() noexcept {
    if (activeWorld && activeWorld != SDK::UWorld::GetWorld()) ClearState();
    return (screenOverlays.visualEffects || screenOverlays.resultMenus) &&
           (!screenOverlays.onlyInFreeCamera || activeWorld);
}

void FreeCameraManager::ApplyConstructedScreenOverlay(SDK::UObject* object) {
    if (!ShouldHideScreenOverlays() || !object) return;
    if (screenOverlays.visualEffects && object->IsA(SDK::UUI_HUD_C::StaticClass())) {
        SetHudEffectWidgetsHidden(static_cast<SDK::UUI_HUD_C*>(object), true);
        return;
    }
    if (!screenOverlays.resultMenus || !IsResultMenu(object)) return;
    if (SetScreenOverlayHidden(static_cast<SDK::UWidget*>(object), true)) {
        ++hiddenResultMenuCount;
    }
    RestoreGameplayInput(object);
}

void FreeCameraManager::ApplyScreenOverlayVisibility(const RuntimeContextSnapshot& runtime) {
    if (!runtime.world) return;

    const bool hidden = ShouldHideScreenOverlays();
    const bool hideResultMenus = hidden && screenOverlays.resultMenus;
    ApplyHudEffectWidgets(runtime.world, hidden && screenOverlays.visualEffects);
    const int resultMenuDelta = ApplyScreenOverlayClass<SDK::UUI_Lose_C>(runtime.world, hideResultMenus) +
                                ApplyScreenOverlayClass<SDK::UUI_DeathDoor_C>(runtime.world, hideResultMenus) +
                                ApplyScreenOverlayClass<SDK::UUI_DED_C>(runtime.world, hideResultMenus) +
                                ApplyScreenOverlayClass<SDK::UUI_GiveUp_C>(runtime.world, hideResultMenus) +
                                ApplyScreenOverlayClass<SDK::UUI_WIN_C>(runtime.world, hideResultMenus) +
                                ApplyScreenOverlayClass<SDK::UUI_WinScreen_C>(runtime.world, hideResultMenus);
    hiddenResultMenuCount += resultMenuDelta;
    if (hiddenResultMenuCount < 0) hiddenResultMenuCount = 0;
    if (hideResultMenus && resultMenuDelta > 0) {
        RestoreGameplayInput(runtime.world);
    }
}

void FreeCameraManager::RestoreGameplayInput(const SDK::UObject* worldContext) {
    worldContext = worldContext ? worldContext : SDK::UWorld::GetWorld();
    if (!worldContext) return;

    if (SDK::UGameplayStatics::IsGamePaused(worldContext)) {
        SDK::UGameplayStatics::SetGamePaused(worldContext, false);
    }

    auto* controller = SDK::UGameplayStatics::GetPlayerController(worldContext, 0);
    if (!controller && debugController) {
        controller = static_cast<SDK::APlayerController*>(debugController);
    }
    if (!controller) {
        controller = originalController;
    }
    if (!controller) return;

    SDK::UWidgetBlueprintLibrary::SetInputMode_GameOnly(controller, false);
    SDK::UWidgetBlueprintLibrary::SetFocusToGameViewport();
    controller->bShowMouseCursor = false;
    controller->SetIgnoreMoveInput(false);
    controller->SetIgnoreLookInput(false);
}
