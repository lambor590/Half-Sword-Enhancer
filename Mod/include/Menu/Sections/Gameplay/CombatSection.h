#pragma once
#include <Windows.h>
#include <memory>
#include "../../ICollapsibleSection.h"

class CombatSection : public CollapsibleSection {
private:
    static inline int parryKey = VK_BACK;

public:
    CombatSection() : CollapsibleSection("Combat") {
        // Ejemplo de función hookeada
        AddFunction(std::make_unique<HookedFunction>(
            "Auto Parry",
            "GameLogic::HandleParry",
            std::function<void()>([]() { /* código del hook */ })
        ));

        AddFunction(std::make_unique<KeybindFunction>(
            "Quick Parry",
            &parryKey,
            std::function<void()>([]() { /* código de la función */ })
        ));
    }
}; 