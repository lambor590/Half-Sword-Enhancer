#pragma once
#include "ISection.h"
#include "Action.h"
#include <Windows.h>

class VisualSection : public ISection {
public:
    void Setup(MenuSection& section) override {
        // ESP Features
        section.AddItem({
            "visual.esp",
            "ESP",
            "Extra Sensory Perception features",
            true
        }, {
            "ESP", false, 0, nullptr
        });

        // Player ESP
        section.AddItem({
            "visual.esp.players",
            "Player ESP",
            "Show information about players",
            false
        }, {
            "Player ESP",
            false,
            VK_F5,
            []() { /* Implementar player ESP */ }
        });
    }
};