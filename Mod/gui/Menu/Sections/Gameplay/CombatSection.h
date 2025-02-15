#pragma once

#include <Windows.h>
#include <memory>

#include "Menu/ICollapsibleSection.h"

class CombatSection : public CollapsibleSection {
private:
    static inline int parryKey = VK_BACK;

public:
    CombatSection() : CollapsibleSection("Combat") {
        // Ejemplo de función hookeada
        AddFunction(std::make_unique<HookedFunction>(
            "Auto Parry",
            "GameLogic::HandleParry",
            std::function<void()>([this]() { 
                instances.Update(); // Actualizamos solo cuando la función se ejecuta
                if (world && player) {
                    /* código del hook usando world y player directamente */ 
                }
            })
        ));

        AddFunction(std::make_unique<KeybindFunction>(
            "Quick Parry",
            &parryKey,
            std::function<void()>([this]() { 
                instances.Update(); // Actualizamos solo cuando se presiona la tecla
                if (world && player) {
                    /* código de la función usando world y player directamente */ 
                }
            })
        ));
    }
};