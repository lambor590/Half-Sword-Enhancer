#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"

class WorldSection : public CollapsibleSection {
private:
    static inline int sloMoKey = 0x5A; // Z
    static inline float slowMotionSpeed = 0.4f;

    static inline int customGravityKey = 0x4C; // L
    static inline float customGravityValue = 0.0f;

public:
    WorldSection() : CollapsibleSection("World") {
        std::initializer_list<Parameter> slowMotionParams = {
            Parameter("speed", "Speed", &slowMotionSpeed, 0.01f, 0.99f)
        };

        BindWithParams("Toggle Slow Motion", &sloMoKey, slowMotionParams, [this]() {
            worldSettings->TimeDilation = (worldSettings->TimeDilation == 1.0f) ? slowMotionSpeed : 1.0f;
        }, worldSettings);

        std::initializer_list<Parameter> customGravityParams = {
            Parameter("gravity", "Gravity", &customGravityValue, -3000.0f, 3000.0f)
        };

        BindWithParams("Toggle Custom Gravity", &customGravityKey, customGravityParams, [this]() {
            worldSettings->bWorldGravitySet = true;
            worldSettings->WorldGravityZ = (worldSettings->WorldGravityZ == -980.0f) ? customGravityValue : -980.0f;
        }, worldSettings);
    }
};