#pragma once

#include <algorithm>
#include <array>
#include <vector>
#include <memory>
#include <utility>
#include <string>

#include "ICollapsibleSection.h"
#include "DefaultStyle.h"

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

    ICollapsibleSection* selectedSection = nullptr;
    MenuTab openCategory = MenuTab::Gameplay;
    float sidebarWidth = 140.0f;
    bool sidebarVisible = true;

    static constexpr float SIDEBAR_MIN_WIDTH = 60.0f;
    static constexpr float SPLITTER_THICKNESS = 4.0f;
    static constexpr float SECTION_INDENT = 16.0f;

    MenuManager() = default;

    void RenderSplitter() {
        ImGui::PushStyleColor(ImGuiCol_Button, DefaultStyle::darkInk);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DefaultStyle::mediumWood);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, DefaultStyle::oldBrass);

        ImGui::Button("##splitter", ImVec2(SPLITTER_THICKNESS, -1));

        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }

        if (ImGui::IsItemActive()) {
            float delta = ImGui::GetIO().MouseDelta.x;

            if (sidebarVisible) {
                sidebarWidth += delta;
                if (sidebarWidth < SIDEBAR_MIN_WIDTH) {
                    sidebarVisible = false;
                    sidebarWidth = 0.0f;
                } else {
                    sidebarWidth = std::clamp(sidebarWidth, SIDEBAR_MIN_WIDTH, 300.0f);
                }
            } else if (delta > 0.0f) {
                sidebarVisible = true;
                sidebarWidth = SIDEBAR_MIN_WIDTH + delta;
            }
        }
    }

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
            sectionVec.reserve(8);
        }
        sectionVec.push_back(std::make_unique<T>());
        if (!selectedSection) selectedSection = sectionVec.back().get();
    }

    void RenderMenu() {
        if (sidebarVisible) {
            ImGui::BeginChild("nav_sidebar", ImVec2(sidebarWidth, 0), true);

            for (auto& [tab, label] : tabOrder) {
                auto& sects = sections[static_cast<size_t>(tab)];
                if (sects.empty()) continue;

                ImGui::SetNextItemOpen(tab == openCategory);
                if (ImGui::CollapsingHeader(label)) {
                    openCategory = tab;
                    ImGui::Indent(SECTION_INDENT);
                    for (auto& section : sects) {
                        if (ImGui::Selectable(section->GetName().c_str(), selectedSection == section.get())) {
                            selectedSection = section.get();
                        }
                    }
                    ImGui::Unindent(SECTION_INDENT);
                }
            }

            ImGui::EndChild();
            ImGui::SameLine();
        }

        RenderSplitter();
        ImGui::SameLine();

        ImGui::BeginChild("content_panel", ImVec2(0, 0), false);
        if (selectedSection) {
            selectedSection->RenderContent();
        }
        ImGui::EndChild();
    }
};
