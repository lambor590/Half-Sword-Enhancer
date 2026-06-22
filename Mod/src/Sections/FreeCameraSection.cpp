#include "Menu/Sections/World/FreeCameraSection.h"

#include "Menu/SectionStyle.h"
#include "Utils/FreeCameraManager.h"

namespace {
    constexpr const char* TOGGLE_TOOLTIP = "Toggle the game's free camera";
    constexpr const char* INPUT_LOCK_TOOLTIP = "Switch control between the free camera and the player";
    constexpr const char* START_CONTROL_TOOLTIP = "Start in camera control mode when free camera is enabled";
    constexpr const char* SPEED_SCALE_TOOLTIP = "Multiplier applied to the free camera movement speed";
    constexpr const char* FOV_TOOLTIP = "Free camera field of view. 0 preserves the current game FOV";
    constexpr const char* IGNORE_TIME_DILATION_TOOLTIP = "Keep free camera speed stable during slow motion";
}

FreeCameraSection::FreeCameraSection(ModContext& ctx) : Section(ctx, SECTION) {
    InitKeybinds();
}

FreeCameraSection::~FreeCameraSection() {
    FreeCameraManager::Get().Apply(false, false, cfg.camera);
}

void FreeCameraSection::InitKeybinds() {
    keybinds.Add(
        {
            .name = "Toggle Free Camera",
            .tooltip = TOGGLE_TOOLTIP,
            .configSection = "FreeCamera",
            .keyPtr = &cfg.toggleKey,
            .callback =
                [this]([[maybe_unused]] bool, [[maybe_unused]] const RuntimeContextSnapshot&) {
                    SetFreeCameraEnabled(!freeCameraEnabled);
                },
            .params =
                {KeybindParam(
                     "start_camera_control", "Start Camera Control", &cfg.startWithCameraControl,
                     START_CONTROL_TOOLTIP
                 ),
                 KeybindParam("speed_scale", "Speed Scale", &cfg.camera.speedScale, 0.1f, 10.0f, SPEED_SCALE_TOOLTIP),
                 KeybindParam("fov", "FOV", &cfg.camera.fov, 0.0f, 170.0f, FOV_TOOLTIP),
                 KeybindParam(
                     "ignore_time_dilation", "Ignore Slow Motion", &cfg.camera.ignoreTimeDilation,
                     IGNORE_TIME_DILATION_TOOLTIP
                 )},
        }
    );

    keybinds.Add(
        {
            .name = "Toggle Camera Control",
            .tooltip = INPUT_LOCK_TOOLTIP,
            .configSection = "FreeCameraInput",
            .keyPtr = &cfg.inputLockKey,
            .callback =
                [this]([[maybe_unused]] bool, [[maybe_unused]] const RuntimeContextSnapshot&) {
                    SetInputLocked(!inputLocked);
                },
        }
    );
}

void FreeCameraSection::SetFreeCameraEnabled(bool enabled) {
    freeCameraEnabled = enabled;
    inputLocked = enabled && cfg.startWithCameraControl;
    FreeCameraManager::Get().Apply(freeCameraEnabled, inputLocked, cfg.camera);
}

void FreeCameraSection::SetInputLocked(bool locked) {
    if (!freeCameraEnabled) {
        inputLocked = false;
        FreeCameraManager::Get().Apply(false, false, cfg.camera);
        return;
    }

    inputLocked = locked;
    FreeCameraManager::Get().Apply(freeCameraEnabled, inputLocked, cfg.camera);
}

void FreeCameraSection::Render() {
    const SectionStyle::StyleRAII style;
    keybinds.Render();
}
