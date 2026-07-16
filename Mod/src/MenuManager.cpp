#include "Menu/MenuManager.h"
#include "DefaultStyle.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

#include "ConfigManager.h"
#include "KeybindManager.h"
#include "Menu/Keybind.h"
#include "Menu/SectionStyle.h"
#include "Utils/GuiUtils.h"
#include "imgui/imgui.h"

namespace {
    constexpr ImVec4 BRASS_SUBTLE =
        {DefaultStyle::OLD_BRASS.x, DefaultStyle::OLD_BRASS.y, DefaultStyle::OLD_BRASS.z, 0.12f};
    constexpr ImVec4 BRASS_MEDIUM =
        {DefaultStyle::OLD_BRASS.x, DefaultStyle::OLD_BRASS.y, DefaultStyle::OLD_BRASS.z, 0.18f};
    constexpr ImVec4 BRASS_STRONG =
        {DefaultStyle::OLD_BRASS.x, DefaultStyle::OLD_BRASS.y, DefaultStyle::OLD_BRASS.z, 0.25f};
    constexpr ImVec4 SEPARATOR_COLOR =
        {DefaultStyle::MEDIUM_WOOD.x, DefaultStyle::MEDIUM_WOOD.y, DefaultStyle::MEDIUM_WOOD.z, 0.35f};

    void DrawSeparator(float verticalGap) {
        ImGui::Dummy(ImVec2(0, verticalGap));
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 position = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        drawList->AddLine(
            position, ImVec2(position.x + width, position.y), ImGui::ColorConvertFloat4ToU32(SEPARATOR_COLOR), 1.0f
        );
        ImGui::Dummy(ImVec2(0, verticalGap));
    }

    void DrawSelectionAccent() {
        const ImVec2 minimum = ImGui::GetItemRectMin();
        const ImVec2 maximum = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(minimum.x - 5, minimum.y + 1), ImVec2(minimum.x - 2, maximum.y - 1),
            ImGui::ColorConvertFloat4ToU32(DefaultStyle::OLD_BRASS), 1.0f
        );
    }

    bool ContentSelectable(const char* label, bool selected, bool accent = false) {
        const ImVec2 labelSize = ImGui::CalcTextSize(label, nullptr, true);
        const float width = (std::max)(1.0f, (std::min)(labelSize.x, ImGui::GetContentRegionAvail().x));

        ImGui::PushStyleColor(ImGuiCol_Header, BRASS_MEDIUM);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, selected ? BRASS_STRONG : BRASS_SUBTLE);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, BRASS_STRONG);
        const bool pressed = ImGui::Selectable(label, selected, ImGuiSelectableFlags_None, ImVec2(width, 0.0f));
        ImGui::PopStyleColor(3);

        if (selected && accent) DrawSelectionAccent();
        return pressed;
    }

    bool MatchesSearch(const char* text, const char* filter, size_t filterLength) {
        return text && text[0] != '\0' && GuiUtils::MatchesFilter(text, std::strlen(text), filter, filterLength);
    }

    bool MatchesSearch(std::string_view text, const char* filter, size_t filterLength) {
        return !text.empty() && GuiUtils::MatchesFilter(text.data(), text.size(), filter, filterLength);
    }
} // namespace

MenuManager& MenuManager::Get() {
    static MenuManager instance;
    return instance;
}

void MenuManager::LoadNavigationState() {
    auto& config = ConfigManager::Get();
    sidebarWidth =
        std::clamp(config.GetFloat("GUI", "navigation_width", sidebarWidth), SIDEBAR_MIN_WIDTH, SIDEBAR_MAX_WIDTH);

    const std::string lastSection = config.GetString("GUI", "last_section", "");
    if (lastSection.empty()) return;

    for (auto& tabSections : sections) {
        for (auto& section : tabSections) {
            if (lastSection == section->GetName()) {
                selectedSection = section.get();
                return;
            }
        }
    }
}

void MenuManager::RenderMenu() {
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_K, ImGuiInputFlags_RouteGlobal)) {
        focusSearch = true;
        activeSearchResult = 0;
        scrollToActiveSearchResult = false;
    }

    const float maximumSidebarWidth = std::clamp(
        ImGui::GetContentRegionAvail().x - CONTENT_MIN_WIDTH - SPLITTER_THICKNESS, SIDEBAR_MIN_WIDTH, SIDEBAR_MAX_WIDTH
    );
    sidebarWidth = (std::min)(sidebarWidth, maximumSidebarWidth);
    RenderSidebar();
    ImGui::SameLine(0, 0);
    RenderSplitter(maximumSidebarWidth);
    ImGui::SameLine(0, 0);

    if (selectedSection != openedSection) {
        openedSection = selectedSection;
        if (openedSection) openedSection->OnOpen();
    }

    RenderContent();
}

void MenuManager::RenderContent() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::BeginChild(
        "content_panel", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    if (selectedSection) {
        ImGui::PushID(selectedSection);
        ImGui::BeginChild("section_content", ImVec2(0, 0));
        RenderSectionDescription();
        {
            const SectionStyle::StyleRAII style;
            selectedSection->Render();
        }
        ImGui::EndChild();
        ImGui::PopID();
    } else {
        ImGui::TextDisabled("No section selected");
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void MenuManager::RenderSectionDescription() {
    const char* description = selectedSection->GetDescription();
    if (description && description[0] != '\0') ImGui::TextWrapped("%s", description);
}

void MenuManager::ClearSearch() noexcept {
    searchBuffer[0] = '\0';
    searchResults.clear();
    activeSearchResult = 0;
    scrollToActiveSearchResult = false;
}

void MenuManager::UpdateSearchResults() {
    searchResults.clear();
    activeSearchResult = 0;
    scrollToActiveSearchResult = false;
    if (searchBuffer[0] == '\0') return;

    const size_t filterLength = std::strlen(searchBuffer);
    for (size_t tabIndex = 0; tabIndex < TAB_COUNT; ++tabIndex) {
        auto& tabSections = sections[tabIndex];
        if (tabSections.empty()) continue;

        if (MatchesSearch(TAB_LABELS[tabIndex], searchBuffer, filterLength)) {
            searchResults.push_back({SearchResultType::Category, tabSections.front().get()});
        }

        for (auto& section : tabSections) {
            if (MatchesSearch(section->GetName(), searchBuffer, filterLength) ||
                MatchesSearch(section->GetDescription(), searchBuffer, filterLength)) {
                searchResults.push_back({SearchResultType::Section, section.get()});
            }

            auto* keybinds = section->GetSearchKeybinds();
            if (!keybinds) continue;

            for (auto& entry : keybinds->Entries()) {
                bool matches = MatchesSearch(entry.name.c_str(), searchBuffer, filterLength) ||
                               MatchesSearch(entry.tooltip.c_str(), searchBuffer, filterLength);
                for (const auto& param : entry.params) {
                    if (matches) break;
                    matches = MatchesSearch(param.displayName, searchBuffer, filterLength) ||
                              MatchesSearch(param.tooltip, searchBuffer, filterLength);
                }
                if (matches) {
                    searchResults.push_back({SearchResultType::Action, section.get(), &entry});
                }
            }
        }
    }
}

void MenuManager::ActivateSearchResult(SearchResult result) {
    SelectSection(result.section);
    if (result.type == SearchResultType::Action && result.entry) {
        if (auto* keybinds = result.section->GetSearchKeybinds()) keybinds->RequestHighlight(result.entry);
    }
    ClearSearch();
}

void MenuManager::SelectSection(Section* section) {
    if (!section || section == selectedSection) return;
    KeybindRuntime::FlushPendingParamChanges();
    KeybindManager::CancelRebind();
    selectedSection = section;
    ConfigManager::Get().SetString("GUI", "last_section", selectedSection->GetName());
}

void MenuManager::RenderSearchBar() {
    const float availableWidth = ImGui::GetContentRegionAvail().x;

    ImGui::SetNextItemWidth(availableWidth);
    if (focusSearch) {
        ImGui::SetKeyboardFocusHere();
        focusSearch = false;
    }

    const bool submitted = ImGui::InputTextWithHint(
        "##GlobalSearch", "Search menu...", searchBuffer, sizeof(searchBuffer),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EscapeClearsAll
    );
    const bool searchInputActive = ImGui::IsItemActive();
    const bool searchInputEdited = ImGui::IsItemEdited();
    if (!searchInputActive)
        GuiUtils::HelpTooltip("Find a section or action (Ctrl+K). Use the arrow keys and Enter to open it.");
    if (searchInputEdited) UpdateSearchResults();

    if (!searchResults.empty()) {
        activeSearchResult = (std::min)(activeSearchResult, searchResults.size() - 1);
        if (searchInputActive && ImGui::IsKeyPressed(ImGuiKey_UpArrow) && activeSearchResult > 0) {
            --activeSearchResult;
            scrollToActiveSearchResult = true;
        } else if (
            searchInputActive && ImGui::IsKeyPressed(ImGuiKey_DownArrow) &&
            activeSearchResult < searchResults.size() - 1
        ) {
            ++activeSearchResult;
            scrollToActiveSearchResult = true;
        }
    }

    if (submitted && !searchResults.empty()) {
        ActivateSearchResult(searchResults[activeSearchResult]);
    }

    DrawSeparator(CATEGORY_VGAP);
}

void MenuManager::RenderSearchResults() {
    if (searchResults.empty()) {
        activeSearchResult = 0;
        scrollToActiveSearchResult = false;
        ImGui::TextDisabled("No results");
        return;
    }

    activeSearchResult = (std::min)(activeSearchResult, searchResults.size() - 1);
    bool activate = false;
    SearchResult activatedResult{SearchResultType::Section, nullptr};

    for (size_t index = 0; index < searchResults.size(); ++index) {
        const auto& result = searchResults[index];
        const char* label =
            result.type == SearchResultType::Category
                ? GetTabLabel(result.section->GetTab())
                : (result.type == SearchResultType::Action ? result.entry->name.c_str() : result.section->GetName());

        ImGui::PushID(static_cast<int>(index));
        const bool selected = index == activeSearchResult;
        if (ContentSelectable(label, selected, selected)) {
            activatedResult = result;
            activate = true;
        }
        if (scrollToActiveSearchResult && index == activeSearchResult) ImGui::SetScrollHereY(0.5f);

        if (result.type != SearchResultType::Category) {
            const char* description = result.type == SearchResultType::Action ? result.entry->tooltip.c_str()
                                                                              : result.section->GetDescription();
            if (description && description[0] != '\0') GuiUtils::HelpTooltip(description);
        }

        ImGui::PopID();

        ImGui::Indent(SECTION_INDENT);
        if (result.type == SearchResultType::Category) {
            ImGui::TextDisabled("Group");
        } else if (result.type == SearchResultType::Section) {
            ImGui::TextDisabled("%s", GetTabLabel(result.section->GetTab()));
        } else {
            ImGui::TextDisabled("%s / %s", GetTabLabel(result.section->GetTab()), result.section->GetName());
        }
        ImGui::Unindent(SECTION_INDENT);

        if (activate) break;
        ImGui::Spacing();
    }

    scrollToActiveSearchResult = false;
    if (activate) ActivateSearchResult(activatedResult);
}

void MenuManager::RenderSplitter(float maximumSidebarWidth) {
    ImGui::PushStyleColor(ImGuiCol_Button, DefaultStyle::DARK_LEATHER);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DefaultStyle::MEDIUM_WOOD);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, DefaultStyle::OLD_BRASS);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

    const float paddingY = ImGui::GetStyle().WindowPadding.y;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - paddingY);
    const float splitterHeight = ImGui::GetContentRegionAvail().y + paddingY;

    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    ImGui::Button("##splitter", ImVec2(SPLITTER_THICKNESS, splitterHeight));
    ImGui::PopItemFlag();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (ImGui::IsItemActive()) {
        sidebarWidth = std::clamp(sidebarWidth + ImGui::GetIO().MouseDelta.x, SIDEBAR_MIN_WIDTH, maximumSidebarWidth);
    }
    if (ImGui::IsItemDeactivated()) ConfigManager::Get().SetFloat("GUI", "navigation_width", sidebarWidth);
}

void MenuManager::RenderCategoryHeader(const char* label, MenuTab tab, bool& firstVisible) {
    if (!firstVisible) DrawSeparator(CATEGORY_VGAP);
    firstVisible = false;

    bool selected = selectedSection && selectedSection->GetTab() == tab;
    ImGui::PushStyleColor(ImGuiCol_Text, DefaultStyle::PARCHMENT_DARK);
    ImGui::Indent(ARROW_INDENT);
    if (ContentSelectable(label, selected)) {
        if (!selected) {
            auto& tabSections = sections[static_cast<size_t>(tab)];
            if (!tabSections.empty()) SelectSection(tabSections.front().get());
            selected = true;
        }
    }
    ImGui::Unindent(ARROW_INDENT);
    ImGui::PopStyleColor();

    const ImVec2 minimum = ImGui::GetItemRectMin();
    const ImVec2 maximum = ImGui::GetItemRectMax();
    const float middleY = (minimum.y + maximum.y) * 0.5f;
    const float arrowX = minimum.x - ARROW_INDENT * 0.6f;
    const ImU32 color = ImGui::ColorConvertFloat4ToU32(DefaultStyle::TEXT_DISABLED);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (selected) {
        drawList->AddTriangleFilled(
            ImVec2(arrowX - ARROW_SIZE, middleY - ARROW_SIZE * 0.5f),
            ImVec2(arrowX + ARROW_SIZE, middleY - ARROW_SIZE * 0.5f), ImVec2(arrowX, middleY + ARROW_SIZE * 0.5f), color
        );
    } else {
        drawList->AddTriangleFilled(
            ImVec2(arrowX - ARROW_SIZE * 0.5f, middleY - ARROW_SIZE), ImVec2(arrowX + ARROW_SIZE * 0.5f, middleY),
            ImVec2(arrowX - ARROW_SIZE * 0.5f, middleY + ARROW_SIZE), color
        );
    }
}

void MenuManager::RenderCategorySections(MenuTab tab) {
    ImGui::Indent(SECTION_INDENT);

    for (auto& section : sections[static_cast<size_t>(tab)]) {
        const bool selected = selectedSection == section.get();
        if (ContentSelectable(section->GetName(), selected, selected)) SelectSection(section.get());
    }

    ImGui::Unindent(SECTION_INDENT);
}

void MenuManager::RenderSidebar() {
    const auto& style = ImGui::GetStyle();
    const ImVec2 windowPosition = ImGui::GetWindowPos();
    const float backgroundTop = windowPosition.y + ImGui::GetFrameHeight();
    const float backgroundBottom = windowPosition.y + ImGui::GetWindowHeight() - style.WindowBorderSize;
    const float backgroundLeft = windowPosition.x + style.WindowBorderSize;

    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(backgroundLeft, backgroundTop),
        ImVec2(backgroundLeft + sidebarWidth + style.WindowPadding.x, backgroundBottom),
        ImGui::ColorConvertFloat4ToU32(DefaultStyle::DARK_INK), style.WindowRounding, ImDrawFlags_RoundCornersBottomLeft
    );

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(SIDEBAR_HPAD, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::BeginChild(
        "nav_sidebar", ImVec2(sidebarWidth, ImGui::GetContentRegionAvail().y), ImGuiChildFlags_AlwaysUseWindowPadding
    );

    RenderSearchBar();

    if (searchBuffer[0] != '\0') {
        RenderSearchResults();
    } else {
        bool firstVisible = true;
        for (size_t tabIndex = 0; tabIndex < TAB_COUNT; ++tabIndex) {
            const auto tab = static_cast<MenuTab>(tabIndex);
            auto& tabSections = sections[tabIndex];
            if (tabSections.empty()) continue;

            RenderCategoryHeader(TAB_LABELS[tabIndex], tab, firstVisible);
            if (selectedSection && selectedSection->GetTab() == tab) RenderCategorySections(tab);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
}
