#pragma once

#include <Windows.h>
#include <memory>
#include <functional>

#include "Menu/ICollapsibleSection.h"

class GeneralSection : public CollapsibleSection {
private:
    static inline int sloMoKey = 0x5A; // Z
    static inline int saveLoadoutKey = 0x54; // T
    static inline int infiniteStaminaKey = 0x49; // I

public:
    GeneralSection() : CollapsibleSection("General") {
        Bind("Toggle Slow Motion", &sloMoKey, [this]() {
            worldSettings->TimeDilation = (worldSettings->TimeDilation == 1.0f) ? 0.3f : 1.0f;
        }, worldSettings);

        Hook("Infinite Stamina", "OnWalkingOffLedge", &infiniteStaminaKey, [this]() {
            player->Stamina = 100.0f;
        }, player);
        
        Bind("Save Loadout", &saveLoadoutKey, [this]() {
            player->Save_Loadout();
        }, player);
    }
};