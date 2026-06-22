#pragma once

#include "Menu/Keybind.h"
#include "Menu/Section.h"
#include "Utils/FreeCameraManager.h"

class FreeCameraSection : public Section {
public:
    static constexpr SectionDefinition SECTION{MenuTab::World, "Free Camera", "Environment"};

    explicit FreeCameraSection(ModContext& ctx);
    ~FreeCameraSection() override;

    void Render() override;

private:
    struct Config {
        int toggleKey = 0x75;    // F6
        int inputLockKey = 0x76; // F7
        bool startWithCameraControl = true;
        FreeCameraSettings camera;
    };

    Config cfg;
    KeybindList keybinds;
    bool freeCameraEnabled = false;
    bool inputLocked = false;

    void InitKeybinds();
    void SetFreeCameraEnabled(bool enabled);
    void SetInputLocked(bool locked);
};
