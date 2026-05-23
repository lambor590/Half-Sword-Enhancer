#include "Menu/MenuManager.h"
#include "DefaultStyle.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <utility>

#include "imgui/imgui.h"

namespace {
    constexpr ImVec4 BRASS_SUBTLE = {
        DefaultStyle::OLD_BRASS.x, DefaultStyle::OLD_BRASS.y, DefaultStyle::OLD_BRASS.z, 0.12f};
    constexpr ImVec4 BRASS_MEDIUM = {
        DefaultStyle::OLD_BRASS.x, DefaultStyle::OLD_BRASS.y, DefaultStyle::OLD_BRASS.z, 0.18f};
    constexpr ImVec4 BRASS_STRONG = {
        DefaultStyle::OLD_BRASS.x, DefaultStyle::OLD_BRASS.y, DefaultStyle::OLD_BRASS.z, 0.25f};

    constexpr ImVec4 SEPARATOR_COLOR = {
        DefaultStyle::MEDIUM_WOOD.x, DefaultStyle::MEDIUM_WOOD.y, DefaultStyle::MEDIUM_WOOD.z, 0.35f};

    void DrawSeparator(float vGap) {
        ImGui::Dummy(ImVec2(0, vGap));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        dl->AddLine(pos, ImVec2(pos.x + w, pos.y), ImGui::ColorConvertFloat4ToU32(SEPARATOR_COLOR), 1.0f);
        ImGui::Dummy(ImVec2(0, vGap));
    }

    void DrawSelectionAccent() {
        auto mn = ImGui::GetItemRectMin();
        auto mx = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(mn.x - 5, mn.y + 1), ImVec2(mn.x - 2, mx.y - 1),
            ImGui::ColorConvertFloat4ToU32(DefaultStyle::OLD_BRASS), 1.0f
        );
    }

} // namespace

MenuManager& MenuManager::Get() {
    static MenuManager instance;
    return instance;
}

void MenuManager::AddSection(MenuTab tab, std::unique_ptr<Section> section) {
    auto& sectionVec = sections[static_cast<size_t>(tab)];
    if (sectionVec.empty()) {
        sectionVec.reserve(8);
    }
    sectionVec.push_back(std::move(section));
    if (!selectedSection) selectedSection = sectionVec.back().get();
    RebuildRenderGroups(tab);
}

void MenuManager::RenderMenu() {
    if (sidebarVisible) {
        RenderSidebar();
        ImGui::SameLine(0, 0);
    }

    RenderSplitter();
    ImGui::SameLine(0, 0);

    if (selectedSection != openedSection) {
        openedSection = selectedSection;
        if (openedSection) openedSection->OnOpen();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::BeginChild("content_panel", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (selectedSection) {
        selectedSection->Render();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

bool MenuManager::MatchesSearch(std::string_view text, const char* lowerNeedle, size_t needleLen) noexcept {
    if (needleLen == 0) return true;
    if (needleLen > text.size()) return false;

    for (size_t i = 0; i <= text.size() - needleLen; ++i) {
        bool match = true;
        for (size_t j = 0; j < needleLen; ++j) {
            if (static_cast<char>(std::tolower(static_cast<unsigned char>(text[i + j]))) != lowerNeedle[j]) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

void MenuManager::UpdateSearchResults() noexcept {
    searchResults.clear();

    if (searchBuffer[0] == '\0') {
        searchActive = false;
        return;
    }

    searchActive = true;

    char lowerNeedle[sizeof(searchBuffer)];
    size_t needleLen = 0;
    for (; searchBuffer[needleLen] != '\0'; ++needleLen) {
        lowerNeedle[needleLen] = static_cast<char>(std::tolower(static_cast<unsigned char>(searchBuffer[needleLen])));
    }

    for (size_t i = 0; i < TAB_COUNT; ++i) {
        const auto& [tab, label] = TAB_ORDER[i];
        auto& sects = sections[i];
        for (auto& section : sects) {
            if (MatchesSearch(section->GetName(), lowerNeedle, needleLen)) {
                searchResults.push_back({tab, section.get(), {}});
            }
        }
    }
}

void MenuManager::RebuildRenderGroups(MenuTab tab) {
    const size_t tabIndex = static_cast<size_t>(tab);
    auto& groups = renderGroups[tabIndex];
    groups.clear();

    for (auto& section : sections[tabIndex]) {
        const char* groupName = section->GetGroup();
        if (!groupName) {
            groups.push_back({nullptr, {section.get()}});
            continue;
        }

        RenderGroup* group = nullptr;
        for (auto& candidate : groups) {
            if (candidate.name && std::strcmp(candidate.name, groupName) == 0) {
                group = &candidate;
                break;
            }
        }
        if (!group) {
            groups.push_back({groupName, {}});
            group = &groups.back();
        }
        group->sections.push_back(section.get());
    }
}

void MenuManager::SelectSection(Section* section, MenuTab tab) {
    selectedSection = section;
    openCategory = tab;
}

void MenuManager::RenderSearchBar() {
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
        ImGui::PushStyleColor(ImGuiCol_Button, DefaultStyle::CLEAR);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BRASS_SUBTLE);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, BRASS_STRONG);
        if (ImGui::Button("X##ClearSearch")) {
            searchBuffer[0] = '\0';
            searchActive = false;
            searchResults.clear();
        }
        ImGui::PopStyleColor(3);
    }

    DrawSeparator(CATEGORY_VGAP);
}

void MenuManager::RenderSearchResults() {
    if (searchResults.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, DefaultStyle::TEXT_DISABLED);
        ImGui::TextUnformatted("No results");
        ImGui::PopStyleColor();
        return;
    }

    auto currentTab = static_cast<MenuTab>(255);
    std::string funcDisplay;
    funcDisplay.reserve(64);
    bool shouldClearSearch = false;

    for (auto& result : searchResults) {
        if (result.tab != currentTab) {
            if (currentTab != static_cast<MenuTab>(255)) {
                DrawSeparator(CATEGORY_VGAP);
            }
            currentTab = result.tab;

            ImGui::PushStyleColor(ImGuiCol_Text, DefaultStyle::TEXT_DISABLED);
            ImGui::TextUnformatted(TAB_ORDER[static_cast<size_t>(result.tab)].second);
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
        ImGui::PushStyleColor(ImGuiCol_Header, BRASS_MEDIUM);
        if (ImGui::Selectable(displayText, isSelected)) {
            SelectSection(result.section, result.tab);
            shouldClearSearch = true;
        }
        ImGui::PopStyleColor();

        if (isSelected) {
            DrawSelectionAccent();
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

void MenuManager::RenderSplitter() {
    ImGui::PushStyleColor(ImGuiCol_Button, DefaultStyle::DARK_LEATHER);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DefaultStyle::MEDIUM_WOOD);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, DefaultStyle::OLD_BRASS);
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

void MenuManager::RenderCategoryHeader(const char* label, MenuTab tab, bool& firstVisible) {
    if (!firstVisible) {
        DrawSeparator(CATEGORY_VGAP);
    }
    firstVisible = false;

    ImGui::PushStyleColor(ImGuiCol_Text, DefaultStyle::PARCHMENT_DARK);
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
        ImU32 col = ImGui::ColorConvertFloat4ToU32(DefaultStyle::TEXT_DISABLED);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (tab == openCategory) {
            dl->AddTriangleFilled(
                ImVec2(ax - ARROW_SIZE, midY - ARROW_SIZE * 0.5f), ImVec2(ax + ARROW_SIZE, midY - ARROW_SIZE * 0.5f),
                ImVec2(ax, midY + ARROW_SIZE * 0.5f), col
            );
        } else {
            dl->AddTriangleFilled(
                ImVec2(ax - ARROW_SIZE * 0.5f, midY - ARROW_SIZE), ImVec2(ax + ARROW_SIZE * 0.5f, midY),
                ImVec2(ax - ARROW_SIZE * 0.5f, midY + ARROW_SIZE), col
            );
        }
    }
}

void MenuManager::RenderCategorySections(MenuTab tab) {
    ImGui::Indent(SECTION_INDENT);
    ImGui::PushStyleColor(ImGuiCol_Header, BRASS_MEDIUM);

    auto renderSection = [this](Section* section) {
        bool isSelected = selectedSection == section;
        if (ImGui::Selectable(section->GetName().c_str(), isSelected)) {
            SelectSection(section, openCategory);
        }
        if (isSelected) {
            DrawSelectionAccent();
        }
    };

    for (auto& group : renderGroups[static_cast<size_t>(tab)]) {
        if (!group.name) {
            renderSection(group.sections.front());
            continue;
        }

        if (ImGui::TreeNodeEx(group.name, ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto* section : group.sections)
                renderSection(section);
            ImGui::TreePop();
        }
    }

    ImGui::PopStyleColor();
    ImGui::Unindent(SECTION_INDENT);
}

void MenuManager::RenderSidebar() {
    const auto& style = ImGui::GetStyle();
    ImVec2 winPos = ImGui::GetWindowPos();
    float bgTop = winPos.y + ImGui::GetFrameHeight();
    float bgBottom = winPos.y + ImGui::GetWindowHeight() - style.WindowBorderSize;
    float bgLeft = winPos.x + style.WindowBorderSize;

    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(bgLeft, bgTop), ImVec2(bgLeft + sidebarWidth + style.WindowPadding.x, bgBottom),
        ImGui::ColorConvertFloat4ToU32(DefaultStyle::DARK_INK), style.WindowRounding, ImDrawFlags_RoundCornersBottomLeft
    );

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(SIDEBAR_HPAD, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::BeginChild(
        "nav_sidebar", ImVec2(sidebarWidth, ImGui::GetContentRegionAvail().y), ImGuiChildFlags_AlwaysUseWindowPadding
    );

    RenderSearchBar();

    ImGui::PushStyleColor(ImGuiCol_Header, DefaultStyle::CLEAR);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BRASS_SUBTLE);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, BRASS_STRONG);

    if (searchActive) {
        RenderSearchResults();
    } else {
        bool firstVisible = true;
        for (size_t i = 0; i < TAB_COUNT; ++i) {
            const auto& [tab, label] = TAB_ORDER[i];
            auto& sects = sections[i];
            if (sects.empty()) continue;

            RenderCategoryHeader(label, tab, firstVisible);

            if (tab == openCategory) {
                RenderCategorySections(tab);
            }
        }
    }

    ImGui::PopStyleColor(3);

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
}
