#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"

class GeneralSection : public CollapsibleSection {
private:
    static inline int sloMoKey = 0x5A; // Z
    static inline float slowMotionSpeed = 0.3f;

    static inline int saveLoadoutKey = 0x54; // T
    static inline int infiniteStaminaKey = 0x49; // I

public:
    GeneralSection() : CollapsibleSection("General") {
        std::initializer_list<Parameter> slowMotionParams = {
            Parameter("speed", "Speed", &slowMotionSpeed, 0.1f, 0.9f)
        };
        
        BindWithParams("Toggle Slow Motion", &sloMoKey, slowMotionParams, [this]() {
            worldSettings->TimeDilation = (worldSettings->TimeDilation == 1.0f) ? slowMotionSpeed : 1.0f;
        }, worldSettings);

        Hook("Infinite Stamina", "OnWalkingOffLedge", &infiniteStaminaKey, [this]() {
            player->Stamina = 100.0f;
        }, player);
        
        Bind("Save Loadout", &saveLoadoutKey, [this]() {
            player->Save_Loadout();
        }, player);
    }
};