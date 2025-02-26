#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

#include "ICollapsibleSection.h"

enum class MenuTab {
    Gameplay,
    Item_Spawner,
    Loadout_Manager,
    Post_Process_Settings,
    Settings
};

class MenuManager {
private:
    inline static MenuManager* instance = nullptr;
    std::unordered_map<MenuTab, std::vector<std::unique_ptr<ICollapsibleSection>>> sections;

    MenuManager() = default;

public:
    static MenuManager& Get() {
        if (!instance) {
            instance = new MenuManager();
        }
        return *instance;
    }

    template<typename T>
    void AddSection(MenuTab tab) {
        sections[tab].push_back(std::make_unique<T>());
    }

    void RenderSections(MenuTab tab) {
        for (auto& section : sections[tab]) {
            section->Render();
        }
    }
}; 