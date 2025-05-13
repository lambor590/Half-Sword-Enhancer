#pragma once

#include <array>
#include <vector>
#include <memory>
#include <utility>
#include <string>

#include "ICollapsibleSection.h"

enum class MenuTab {
    Gameplay,
    Entity_Spawner,
    Loadout_Manager,
    Post_Process_Settings,
    Settings,
    Count
};

class MenuManager {
private:
    inline static MenuManager* instance = nullptr;
    inline static constexpr size_t TabCount = static_cast<size_t>(MenuTab::Count);
    std::array<std::vector<std::unique_ptr<ICollapsibleSection>>, TabCount> sections;
    inline static constexpr std::array<std::pair<MenuTab, const char*>, TabCount> tabOrder = {{
        {MenuTab::Gameplay, "Gameplay"},
        {MenuTab::Entity_Spawner, "Entity Spawner"},
        {MenuTab::Loadout_Manager, "Loadout Manager"},
        {MenuTab::Post_Process_Settings, "Post Process"},
        {MenuTab::Settings, "Settings"}
    }};

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
        sections[static_cast<size_t>(tab)].push_back(std::make_unique<T>());
    }

    void RenderSections(MenuTab tab) {
        auto& sects = sections[static_cast<size_t>(tab)];
        for (auto& section : sects) {
            section->Render();
        }
    }

    void RenderMenu() {
        if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_Reorderable)) {
            for (auto& p : tabOrder) {
                auto tab = p.first;
                auto label = p.second;
                if (ImGui::BeginTabItem(label)) {
                    auto& sects = sections[static_cast<size_t>(tab)];
                    if (!sects.empty()) {
                        RenderSections(tab);
                    } else if (tab == MenuTab::Loadout_Manager || tab == MenuTab::Post_Process_Settings) {
                        ImGui::Text("Coming Soon");
                    }
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
}; 