#pragma once

#include <array>
#include <vector>
#include <memory>
#include <utility>
#include <string>

#include "ICollapsibleSection.h"

enum class MenuTab : uint8_t {
    Gameplay,
    Entity_Spawner,
    Loadout_Manager,
    Post_Process_Settings,
    Settings,
    Count
};

class MenuManager {
private:
    static constexpr size_t TabCount = static_cast<size_t>(MenuTab::Count);

    std::array<std::vector<std::unique_ptr<ICollapsibleSection>>, TabCount> sections;

    static constexpr std::array<std::pair<MenuTab, const char*>, TabCount> tabOrder = {{
        {MenuTab::Gameplay, "Gameplay"},
        {MenuTab::Entity_Spawner, "Entity Spawner"},
        {MenuTab::Loadout_Manager, "Loadout Manager"},
        {MenuTab::Post_Process_Settings, "Post Process"},
        {MenuTab::Settings, "Settings"}
    }};

    static constexpr ImGuiTabBarFlags tabBarFlags =
        ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_Reorderable;

    static constexpr const char* comingSoonText = "Coming Soon";
    static constexpr const char* mainTabBarId = "MainTabBar";

    MenuManager() = default;

public:
    static MenuManager& Get() {
        static MenuManager instance;
        return instance;
    }

    MenuManager(const MenuManager&) = delete;
    MenuManager& operator=(const MenuManager&) = delete;

    template<typename T>
    void AddSection(MenuTab tab) {
        auto& sectionVec = sections[static_cast<size_t>(tab)];
        if (sectionVec.empty()) {
            sectionVec.reserve(8); // Reserve space for typical section count
        }
        sectionVec.push_back(std::make_unique<T>());
    }

    void RenderSections(MenuTab tab) {
        auto& sects = sections[static_cast<size_t>(tab)];
        for (auto& section : sects) {
            section->Render();
        }
    }

    template<MenuTab tab>
    constexpr bool IsComingSoon() {
        return tab == MenuTab::Post_Process_Settings || tab == MenuTab::Loadout_Manager;
    }

    void RenderMenu() {
        if (ImGui::BeginTabBar(mainTabBarId, tabBarFlags)) {
            for (auto& p : tabOrder) {
                auto tab = p.first;
                auto label = p.second;
                if (ImGui::BeginTabItem(label)) {
                    auto& sects = sections[static_cast<size_t>(tab)];
                    if (!sects.empty()) {
                        RenderSections(tab);
                    } else if (tab == MenuTab::Post_Process_Settings || tab == MenuTab::Loadout_Manager) {
                        ImGui::Text(comingSoonText);
                    }
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
}; 