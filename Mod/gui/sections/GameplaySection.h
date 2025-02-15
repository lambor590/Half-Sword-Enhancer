#pragma once
#include "ISection.h"
#include "Action.h"
#include <Windows.h>

class GameplaySection : public ISection {
public:
    void Setup(MenuSection& section) override {
        SetupCombat(section);
        SetupMovement(section);
    }

private:
    void SetupCombat(MenuSection& section) {
        // Combat header
        section.AddItem({
            "combat",
            "Combat",
            "Combat related features",
            true
        }, {
            "Combat", false, 0, nullptr
        });

        // Auto Block
        section.AddItem({
            "combat.autoblock",
            "Auto Block",
            "Automatically blocks incoming attacks",
            false
        }, {
            "Auto Block",
            false,
            VK_F1,
            []() { /* Implementar auto block */ }
        });

        // Auto Parry
        section.AddItem({
            "combat.parry",
            "Auto Parry",
            "Automatically parries incoming attacks",
            false
        }, {
            "Auto Parry",
            false,
            VK_F2,
            []() { /* Implementar auto parry */ }
        });
    }

    void SetupMovement(MenuSection& section) {
        // Movement header
        section.AddItem({
            "movement",
            "Movement",
            "Movement related features",
            true
        }, {
            "Movement", false, 0, nullptr
        });

        // Speed Control
        section.AddItem({
            "movement.speed",
            "Speed Control",
            "Control character movement speed",
            false
        }, {
            "Speed Control",
            false,
            VK_F3,
            []() { /* Implementar speed control */ }
        });
    }
};