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
    static constexpr float SECTION_INDENT = 8.0f;

    MenuManager() = default;

    void RenderSplitter() {
        ImGui::PushStyleColor(ImGuiCol_Button, DefaultStyle::darkLeather);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DefaultStyle::mediumWood);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, DefaultStyle::oldBrass);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

        float paddingY = ImGui::GetStyle().WindowPadding.y;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - paddingY);
        float splitterHeight = ImGui::GetContentRegionAvail().y + paddingY;

        ImGui::Button("##splitter", ImVec2(SPLITTER_THICKNESS, splitterHeight));

        ImGui::PopStyleVar(2);
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
            RenderSidebar();
            ImGui::SameLine(0, 0);
        }

        RenderSplitter();
        ImGui::SameLine(0, 0);

        ImGui::BeginChild("content_panel", ImVec2(0, 0), false);
        if (selectedSection) {
            selectedSection->RenderContent();
        }
        ImGui::EndChild();
    }

private:
    void RenderSidebar() {
        const auto& style = ImGui::GetStyle();
        ImVec2 contentStart = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(contentStart.x - style.WindowPadding.x, contentStart.y - style.WindowPadding.y),
            ImVec2(contentStart.x + sidebarWidth, ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - style.WindowBorderSize),
            ImGui::ColorConvertFloat4ToU32(DefaultStyle::darkInk),
            style.WindowRounding,
            ImDrawFlags_RoundCornersBottomLeft
        );

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(2, 4));
        ImGui::BeginChild("nav_sidebar", ImVec2(sidebarWidth, 0), ImGuiChildFlags_AlwaysUseWindowPadding);

        ImGui::PushStyleColor(ImGuiCol_Header, DefaultStyle::transparent);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.71f, 0.57f, 0.25f, 0.12f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.71f, 0.57f, 0.25f, 0.25f));

        for (auto& [tab, label] : tabOrder) {
            auto& sects = sections[static_cast<size_t>(tab)];
            if (sects.empty()) continue;

            ImGui::SetNextItemOpen(tab == openCategory);
            ImGui::PushStyleColor(ImGuiCol_Text, DefaultStyle::textDisabled);
            if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                ImGui::PopStyleColor();
                openCategory = tab;
                ImGui::Indent(SECTION_INDENT);
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.71f, 0.57f, 0.25f, 0.18f));
                for (auto& section : sects) {
                    bool isSelected = selectedSection == section.get();
                    if (ImGui::Selectable(section->GetName().c_str(), isSelected)) {
                        selectedSection = section.get();
                    }
                    if (isSelected) {
                        auto min = ImGui::GetItemRectMin();
                        auto max = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRectFilled(
                            ImVec2(min.x - SECTION_INDENT + 2, min.y + 1),
                            ImVec2(min.x - SECTION_INDENT + 4, max.y - 1),
                            ImGui::ColorConvertFloat4ToU32(DefaultStyle::oldBrass),
                            1.0f
                        );
                    }
                }
                ImGui::PopStyleColor();
                ImGui::Unindent(SECTION_INDENT);
            } else {
                ImGui::PopStyleColor();
            }
        }

        ImGui::PopStyleColor(3);

        ImGui::EndChild();
        ImGui::PopStyleVar(3);
    }
};
