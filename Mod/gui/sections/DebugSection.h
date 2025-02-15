#pragma once
#include "ISection.h"
#include "Action.h"
#include <Windows.h>

class DebugSection : public ISection {
public:
    void Setup(MenuSection& section) override {
        // Performance Info
        section.AddItem({
            "debug.performance",
            "Performance",
            "Show performance metrics",
            false
        }, {
            "Performance Info",
            true,
            0,
            []() { /* Always visible */ }
        });

        // Debug Logging
        section.AddItem({
            "debug.logging",
            "Logging",
            "Enable/disable debug logging",
            false
        }, {
            "Debug Logging",
            false,
            VK_F8,
            []() { /* Implementar logging toggle */ }
        });
    }
};