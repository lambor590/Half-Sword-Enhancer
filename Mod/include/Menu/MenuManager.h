#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <vector>
#include <memory>
#include <utility>
#include <string>
#include <string_view>

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

    char searchBuffer[128] = "";
    bool searchActive = false;

    struct SearchResult {
        MenuTab tab;
        ICollapsibleSection* section;
        std::string_view functionName;
    };
    std::vector<SearchResult> searchResults;

    static constexpr float SIDEBAR_MIN_WIDTH = 60.0f;
    static constexpr float SPLITTER_THICKNESS = 4.0f;
    static constexpr float SECTION_INDENT = 14.0f;
    static constexpr float SIDEBAR_HPAD = 10.0f;
    static constexpr float CATEGORY_VGAP = 4.0f;
    static constexpr float ARROW_SIZE = 4.0f;
    static constexpr float ARROW_INDENT = 8.0f;

    MenuManager() = default;

    static bool matchesSearch(std::string_view text, const char* lowerNeedle, size_t needleLen) noexcept {
        if (needleLen == 0) return true;
        if (needleLen > text.size()) return false;

        for (size_t i = 0; i <= text.size() - needleLen; ++i) {
            bool match = true;
            for (size_t j = 0; j < needleLen; ++j) {
                if (static_cast<char>(std::tolower(static_cast<unsigned char>(text[i + j]))) !=
                    lowerNeedle[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
        return false;
    }

    void UpdateSearchResults() noexcept {
        searchResults.clear();

        if (searchBuffer[0] == '\0') {
            searchActive = false;
            return;
        }

        searchActive = true;

        char lowerNeedle[sizeof(searchBuffer)];
        size_t needleLen = 0;
        for (; searchBuffer[needleLen] != '\0'; ++needleLen) {
            lowerNeedle[needleLen] = static_cast<char>(
                std::tolower(static_cast<unsigned char>(searchBuffer[needleLen])));
        }

        for (auto& [tab, label] : tabOrder) {
            auto& sects = sections[static_cast<size_t>(tab)];
            for (auto& section : sects) {
                if (matchesSearch(section->GetName(), lowerNeedle, needleLen)) {
                    searchResults.push_back({tab, section.get(), {}});
                    continue;
                }

                for (auto& fn : section->GetFunctions()) {
                    if (matchesSearch(fn->GetName(), lowerNeedle, needleLen)) {
                        searchResults.push_back({tab, section.get(), fn->GetName()});
                    }
                }
            }
        }
    }

    void RenderSearchBar() {
        float availWidth = ImGui::GetContentRegionAvail().x;
        bool hasText = searchBuffer[0] != '\0';
        float clearBtnWidth = hasText ? ImGui::CalcTextSize("X").x + 8.0f : 0.0f;
        float inputWidth = availWidth - (hasText ? clearBtnWidth + 4.0f : 0.0f);

        ImGui::SetNextItemWidth(inputWidth);
        if (ImGui::InputTextWithHint("##GlobalSearch", "Search...", searchBuffer, sizeof(searchBuffer))) {
            UpdateSearchResults();
        }

        if (hasText) {
            ImGui::SameLine(0, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, DefaultStyle::transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.71f, 0.57f, 0.25f, 0.12f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.71f, 0.57f, 0.25f, 0.25f));
            if (ImGui::Button("X##ClearSearch")) {
                searchBuffer[0] = '\0';
                searchActive = false;
                searchResults.clear();
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::Dummy(ImVec2(0, CATEGORY_VGAP));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        dl->AddLine(pos, ImVec2(pos.x + w, pos.y),
            ImGui::ColorConvertFloat4ToU32(ImVec4(
                DefaultStyle::mediumWood.x, DefaultStyle::mediumWood.y,
                DefaultStyle::mediumWood.z, 0.35f)), 1.0f);
        ImGui::Dummy(ImVec2(0, CATEGORY_VGAP));
    }

    void RenderSearchResults() {
        if (searchResults.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, DefaultStyle::textDisabled);
            ImGui::TextUnformatted("No results");
            ImGui::PopStyleColor();
            return;
        }

        MenuTab currentTab = static_cast<MenuTab>(255);
        std::string funcDisplay;
        funcDisplay.reserve(64);
        bool shouldClearSearch = false;

        for (auto& result : searchResults) {
            if (result.tab != currentTab) {
                if (currentTab != static_cast<MenuTab>(255)) {
                    ImGui::Dummy(ImVec2(0, CATEGORY_VGAP));
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    float w = ImGui::GetContentRegionAvail().x;
                    dl->AddLine(pos, ImVec2(pos.x + w, pos.y),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(
                            DefaultStyle::mediumWood.x, DefaultStyle::mediumWood.y,
                            DefaultStyle::mediumWood.z, 0.35f)), 1.0f);
                    ImGui::Dummy(ImVec2(0, CATEGORY_VGAP));
                }
                currentTab = result.tab;

                ImGui::PushStyleColor(ImGuiCol_Text, DefaultStyle::textDisabled);
                ImGui::TextUnformatted(tabOrder[static_cast<size_t>(result.tab)].second);
                ImGui::PopStyleColor();
            }

            ImGui::Indent(SECTION_INDENT);

            const char* displayText;
            if (result.functionName.empty()) {
                displayText = result.section->GetName().c_str();
            } else {
                funcDisplay.clear();
                funcDisplay.append(result.section->GetName());
                funcDisplay.append(" > ");
                funcDisplay.append(result.functionName);
                displayText = funcDisplay.c_str();
            }

            bool isSelected = selectedSection == result.section;
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.71f, 0.57f, 0.25f, 0.18f));
            if (ImGui::Selectable(displayText, isSelected)) {
                selectedSection = result.section;
                openCategory = result.tab;
                shouldClearSearch = true;
            }
            ImGui::PopStyleColor();

            if (isSelected) {
                auto mn = ImGui::GetItemRectMin();
                auto mx = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(mn.x - 5, mn.y + 1),
                    ImVec2(mn.x - 2, mx.y - 1),
                    ImGui::ColorConvertFloat4ToU32(DefaultStyle::oldBrass),
                    1.0f);
            }

            ImGui::Unindent(SECTION_INDENT);

            if (shouldClearSearch) break;
        }

        if (shouldClearSearch) {
            searchBuffer[0] = '\0';
            searchActive = false;
            searchResults.clear();
        }
    }

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
        ImVec2 winPos = ImGui::GetWindowPos();
        float bgTop = winPos.y + ImGui::GetFrameHeight();
        float bgBottom = winPos.y + ImGui::GetWindowHeight() - style.WindowBorderSize;
        float bgLeft = winPos.x + style.WindowBorderSize;

        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(bgLeft, bgTop),
            ImVec2(bgLeft + sidebarWidth + style.WindowPadding.x, bgBottom),
            ImGui::ColorConvertFloat4ToU32(DefaultStyle::darkInk),
            style.WindowRounding,
            ImDrawFlags_RoundCornersBottomLeft
        );

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(SIDEBAR_HPAD, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        ImGui::BeginChild("nav_sidebar", ImVec2(sidebarWidth, ImGui::GetContentRegionAvail().y), ImGuiChildFlags_AlwaysUseWindowPadding);

        RenderSearchBar();

        ImGui::PushStyleColor(ImGuiCol_Header, DefaultStyle::transparent);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.71f, 0.57f, 0.25f, 0.12f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.71f, 0.57f, 0.25f, 0.25f));

        if (searchActive) {
            RenderSearchResults();
        } else {

        bool firstVisible = true;
        for (auto& [tab, label] : tabOrder) {
            auto& sects = sections[static_cast<size_t>(tab)];
            if (sects.empty()) continue;

            if (!firstVisible) {
                ImGui::Dummy(ImVec2(0, CATEGORY_VGAP));
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 pos = ImGui::GetCursorScreenPos();
                float w = ImGui::GetContentRegionAvail().x;
                dl->AddLine(pos, ImVec2(pos.x + w, pos.y),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(
                        DefaultStyle::mediumWood.x, DefaultStyle::mediumWood.y,
                        DefaultStyle::mediumWood.z, 0.35f)), 1.0f);
                ImGui::Dummy(ImVec2(0, CATEGORY_VGAP));
            }
            firstVisible = false;

            ImGui::PushStyleColor(ImGuiCol_Text, DefaultStyle::parchmentDark);
            ImGui::Indent(ARROW_INDENT);
            if (ImGui::Selectable(label, tab == openCategory)) {
                openCategory = tab;
            }
            ImGui::Unindent(ARROW_INDENT);
            ImGui::PopStyleColor();

            {
                auto min = ImGui::GetItemRectMin();
                auto max = ImGui::GetItemRectMax();
                float midY = (min.y + max.y) * 0.5f;
                float ax = min.x - ARROW_INDENT * 0.6f;
                ImU32 col = ImGui::ColorConvertFloat4ToU32(DefaultStyle::textDisabled);
                ImDrawList* dl = ImGui::GetWindowDrawList();
                if (tab == openCategory) {
                    dl->AddTriangleFilled(
                        ImVec2(ax - ARROW_SIZE, midY - ARROW_SIZE * 0.5f),
                        ImVec2(ax + ARROW_SIZE, midY - ARROW_SIZE * 0.5f),
                        ImVec2(ax, midY + ARROW_SIZE * 0.5f), col);
                } else {
                    dl->AddTriangleFilled(
                        ImVec2(ax - ARROW_SIZE * 0.5f, midY - ARROW_SIZE),
                        ImVec2(ax + ARROW_SIZE * 0.5f, midY),
                        ImVec2(ax - ARROW_SIZE * 0.5f, midY + ARROW_SIZE), col);
                }
            }

            if (tab == openCategory) {
                ImGui::Indent(SECTION_INDENT);
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.71f, 0.57f, 0.25f, 0.18f));
                for (auto& section : sects) {
                    bool isSelected = selectedSection == section.get();
                    if (ImGui::Selectable(section->GetName().c_str(), isSelected)) {
                        selectedSection = section.get();
                    }
                    if (isSelected) {
                        auto mn = ImGui::GetItemRectMin();
                        auto mx = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRectFilled(
                            ImVec2(mn.x - 5, mn.y + 1),
                            ImVec2(mn.x - 2, mx.y - 1),
                            ImGui::ColorConvertFloat4ToU32(DefaultStyle::oldBrass),
                            1.0f);
                    }
                }
                ImGui::PopStyleColor();
                ImGui::Unindent(SECTION_INDENT);
            }
        }

        }

        ImGui::PopStyleColor(3);

        ImGui::EndChild();
        ImGui::PopStyleVar(3);
    }
};
