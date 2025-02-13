#pragma once
#include <Windows.h>
#include <memory>
#include "../../ICollapsibleSection.h"

class MovementSection : public CollapsibleSection {
private:
    static inline int speedKey = VK_LSHIFT;

public:
    MovementSection() : CollapsibleSection("Movement") {
        // Ejemplo de función hookeada
        AddFunction(std::make_unique<HookedFunction>(
            "Infinite Jump",
            "GameLogic::CheckJump",
            std::function<void()>([]() { /* código del hook */ })
        ));

        AddFunction(std::make_unique<KeybindFunction>(
            "Speed Boost",
            &speedKey,
            std::function<void()>([]() { /* código de la función */ })
        ));
    }
};