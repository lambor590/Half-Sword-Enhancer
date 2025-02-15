#pragma once
#include "ISection.h"
#include "Action.h"
#include <Windows.h>

class SettingsSection : public ISection {
public:
    void Setup(MenuSection& section) override {
        // Interface Settings
        section.AddItem({
            "settings.interface",
            "Interface",
            "UI customization settings",
            true
        }, {
            "Interface", false, 0, nullptr
        });

        // Menu Key
        section.AddItem({
            "settings.interface.menukey",
            "Menu Key",
            "Change the key to show/hide the menu",
            false
        }, {
            "Menu Key",
            true,
            VK_INSERT,
            []() { /* Menu key está gestionado en WndProc */ }
        });

        // Save Settings
        section.AddItem({
            "settings.save",
            "Save Settings",
            "Save current configuration",
            false
        }, {
            "Save Settings",
            true,
            0,
            []() { /* Implementar guardado de configuración */ }
        });
    }
};