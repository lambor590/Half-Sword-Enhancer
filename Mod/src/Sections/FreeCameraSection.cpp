#include "Menu/Sections/World/FreeCameraSection.h"

#include "Utils/FreeCameraManager.h"

namespace {
    constexpr const char* TOGGLE_TOOLTIP = "Move and look around independently from the player";
    constexpr const char* INPUT_LOCK_TOOLTIP = "Movement controls the camera while the player stays still";
    constexpr const char* START_CONTROL_TOOLTIP =
        "The player stays still and movement controls the camera as soon as Free Camera starts";
    constexpr const char* MOVEMENT_SPEED_TOOLTIP = "How quickly the camera moves";
    constexpr const char* FOV_TOOLTIP = "How wide the camera view is. Zero keeps the current view";
    constexpr const char* FULL_SPEED_TOOLTIP = "Camera movement stays at normal speed while the game is slowed";
}

FreeCameraSection::FreeCameraSection(ModContext& ctx) : Section(ctx, SECTION) {
    InitKeybinds();
}

FreeCameraSection::~FreeCameraSection() {
    FreeCameraManager::Get().Apply(false, false, cfg.camera);
}

void FreeCameraSection::InitKeybinds() {
    keybinds.Add({
        .name = "Free Camera",
        .tooltip = TOGGLE_TOOLTIP,
        .configSection = "FreeCamera",
        .keyPtr = &cfg.toggleKey,
        .callback =
            [this](bool active, [[maybe_unused]] const RuntimeContextSnapshot&) { SetFreeCameraEnabled(active); },
        .kind = KeybindKind::State,
        .stateGetter =
            [this]() {
                const auto runtime = RenderSnapshot();
                return FreeCameraManager::Get().IsActive(runtime.world);
            },
        .available =
            [this]() {
                const auto runtime = RenderSnapshot();
                return runtime.world && runtime.controller;
            },
        .applyOnToggle = true,
        .params =
            {KeybindParam(
                 "start_camera_control", "Camera Control by Default", &cfg.startWithCameraControl, START_CONTROL_TOOLTIP
             ),
             KeybindParam("speed_scale", "Movement Speed", &cfg.camera.speedScale, 0.1f, 10.0f, MOVEMENT_SPEED_TOOLTIP),
             KeybindParam("fov", "Field of View", &cfg.camera.fov, 0.0f, 170.0f, FOV_TOOLTIP),
             KeybindParam(
                 "ignore_time_dilation", "Full Speed in Slow Motion", &cfg.camera.ignoreTimeDilation, FULL_SPEED_TOOLTIP
             )},
        .onParamsChanged = [this]() { FreeCameraManager::Get().UpdateSettings(cfg.camera); },
    });

    keybinds.Add({
        .name = "Control Camera",
        .tooltip = INPUT_LOCK_TOOLTIP,
        .configSection = "FreeCameraInput",
        .keyPtr = &cfg.inputLockKey,
        .callback = [this](bool active, [[maybe_unused]] const RuntimeContextSnapshot&) { SetInputLocked(active); },
        .kind = KeybindKind::State,
        .stateGetter =
            [this]() {
                const auto runtime = RenderSnapshot();
                return FreeCameraManager::Get().IsPlayerInputLocked(runtime.world);
            },
        .available =
            [this]() {
                const auto runtime = RenderSnapshot();
                return FreeCameraManager::Get().IsActive(runtime.world);
            },
        .applyOnToggle = true,
    });
}

void FreeCameraSection::SetFreeCameraEnabled(bool enabled) {
    const bool lockInput = enabled && cfg.startWithCameraControl;
    FreeCameraManager::Get().Apply(enabled, lockInput, cfg.camera);
}

void FreeCameraSection::SetInputLocked(bool locked) {
    FreeCameraManager::Get().Apply(true, locked, cfg.camera);
}

void FreeCameraSection::Render() {
    keybinds.Render();
}
