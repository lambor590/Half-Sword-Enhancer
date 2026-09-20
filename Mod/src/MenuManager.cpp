#include "Menu/MenuManager.h"
#include "DefaultStyle.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "ConfigManager.h"
#include "KeybindManager.h"
#include "Menu/Keybind.h"
#include "Menu/SectionStyle.h"
#include "NotificationManager.h"
#include "Utils/DiscordPresence.h"
#include "Utils/GuiUtils.h"
#include "imgui/imgui.h"

namespace {
    constexpr float COMMUNITY_BUTTON_SIZE = 32.0f;
    constexpr float SIDEBAR_HPAD = 12.0f;
    constexpr float SECTION_INDENT = 10.0f;
    constexpr ImVec4 SIDEBAR_BACKGROUND =
        MakeColor(DefaultStyle::BLACK.x, DefaultStyle::BLACK.y, DefaultStyle::BLACK.z);

    bool NavigationButton(const char* label, bool selected, bool category = false, const char* location = nullptr) {
        const float width = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
        const float paddingX = SIDEBAR_HPAD + (!category && !location ? SECTION_INDENT : 0.0f);
        const float paddingY = category ? 6.0f : 4.0f;
        const float textWidth = (std::max)(1.0f, width - paddingX - SIDEBAR_HPAD);
        const float wrapWidth = location ? textWidth : 0.0f;
        const char* labelEnd = GuiUtils::VisibleLabelEnd(label);
        const ImVec2 titleSize = ImGui::CalcTextSize(label, labelEnd, false, wrapWidth);
        const float height = paddingY * 2.0f + titleSize.y +
                             (location ? 4.0f + ImGui::CalcTextSize(location, nullptr, false, textWidth).y : 0.0f);
        const ImVec4 textColor =
            selected ? (category ? DefaultStyle::BRIGHT_BRASS : DefaultStyle::PARCHMENT) : DefaultStyle::PARCHMENT_DARK;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            selected ? (category ? DefaultStyle::DARK_WOOD : DefaultStyle::HEADER) : DefaultStyle::CLEAR
        );
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DefaultStyle::HEADER_HOVERED);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, DefaultStyle::HEADER_ACTIVE);
        // One native hit target, with shared text alignment for navigation and search results.
        ImGui::PushStyleColor(ImGuiCol_Text, DefaultStyle::CLEAR);
        const bool pressed = ImGui::Button(label, ImVec2(width, height));
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();

        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemVisible()) {
            const ImVec2 minimum = ImGui::GetItemRectMin();
            const ImVec2 maximum = ImGui::GetItemRectMax();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            if (selected && !category) {
                drawList->AddRectFilled(
                    ImVec2(minimum.x + 3, minimum.y + paddingY), ImVec2(minimum.x + 5, maximum.y - paddingY),
                    ImGui::GetColorU32(DefaultStyle::BRIGHT_BRASS), 1.0f
                );
            }
            drawList->PushClipRect(
                ImVec2(minimum.x + paddingX, minimum.y), ImVec2(maximum.x - SIDEBAR_HPAD, maximum.y), true
            );
            drawList->AddText(
                ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(minimum.x + paddingX, minimum.y + paddingY),
                ImGui::GetColorU32(textColor), label, labelEnd, wrapWidth
            );
            if (location) {
                drawList->AddText(
                    ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(minimum.x + paddingX, minimum.y + paddingY + titleSize.y + 4.0f),
                    ImGui::GetColorU32(DefaultStyle::TEXT_DISABLED), location, nullptr, textWidth
                );
            }
            drawList->PopClipRect();
        }
        if (!location && titleSize.x > textWidth) GuiUtils::ClippedTextTooltip(label);
        return pressed;
    }

    bool MatchesSearch(const char* text, const char* filter, size_t filterLength) {
        return text && text[0] != '\0' && GuiUtils::MatchesFilter(text, std::strlen(text), filter, filterLength);
    }

    void RenderDiscordButton() {
        ImGui::PushStyleColor(ImGuiCol_Button, DefaultStyle::CLEAR);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DefaultStyle::CLEAR);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, DefaultStyle::CLEAR);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        const bool pressed = ImGui::Button("##JoinDiscord", ImVec2(COMMUNITY_BUTTON_SIZE, COMMUNITY_BUTTON_SIZE));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        const bool hovered = ImGui::IsItemHovered();
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        const ImU32 color = ImGui::GetColorU32(
            hovered || ImGui::IsItemFocused() ? DefaultStyle::BRIGHT_BRASS : DefaultStyle::TEXT_DISABLED
        );
        if (ImGui::IsItemVisible()) {
            const ImVec2 minimum = ImGui::GetItemRectMin();
            const auto point = [minimum](float x, float y) {
                return ImVec2(minimum.x + 4.0f + x * 0.375f, minimum.y + 7.0f + y * 0.375f);
            };
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            // Discord's 64 x 48 symbol, with its outline wound clockwise for ImGui.
            // https://discord.com/branding
            drawList->PathLineTo(point(40.918f, 0.025f));
            drawList->PathBezierCubicCurveTo(point(45.426f, 0.783f), point(49.821f, 2.134f), point(53.975f, 4.041f));
            drawList->PathBezierCubicCurveTo(point(61.136f, 14.510f), point(64.697f, 26.330f), point(63.383f, 39.968f));
            drawList->PathBezierCubicCurveTo(point(58.547f, 43.542f), point(53.129f, 46.257f), point(47.358f, 48.0f));
            drawList->PathBezierCubicCurveTo(point(46.057f, 46.257f), point(44.908f, 44.401f), point(43.923f, 42.469f));
            drawList->PathBezierCubicCurveTo(point(45.805f, 41.762f), point(47.611f, 40.903f), point(49.341f, 39.880f));
            drawList->PathBezierCubicCurveTo(point(48.886f, 39.577f), point(48.444f, 39.236f), point(48.015f, 38.882f));
            drawList->PathBezierCubicCurveTo(point(37.862f, 43.656f), point(26.117f, 43.656f), point(15.977f, 38.882f));
            drawList->PathBezierCubicCurveTo(point(15.548f, 39.211f), point(15.106f, 39.552f), point(14.651f, 39.880f));
            drawList->PathBezierCubicCurveTo(point(16.381f, 40.890f), point(18.187f, 41.762f), point(20.056f, 42.456f));
            drawList->PathBezierCubicCurveTo(point(19.071f, 44.388f), point(17.922f, 46.245f), point(16.621f, 47.987f));
            drawList->PathBezierCubicCurveTo(point(10.850f, 46.245f), point(5.432f, 43.517f), point(0.596f, 39.943f));
            drawList->PathBezierCubicCurveTo(point(-0.516f, 28.186f), point(1.720f, 16.265f), point(9.978f, 4.028f));
            drawList->PathBezierCubicCurveTo(point(14.146f, 2.122f), point(18.540f, 0.770f), point(23.049f, 0.0f));
            drawList->PathBezierCubicCurveTo(point(23.668f, 1.099f), point(24.236f, 2.235f), point(24.728f, 3.397f));
            drawList->PathBezierCubicCurveTo(point(29.540f, 2.677f), point(34.427f, 2.677f), point(39.226f, 3.397f));
            drawList->PathBezierCubicCurveTo(point(39.731f, 2.235f), point(40.286f, 1.099f), point(40.905f, 0.0f));
            drawList->PathFillConcave(color);
            const ImU32 background = ImGui::GetColorU32(SIDEBAR_BACKGROUND);
            drawList->AddEllipseFilled(point(21.464f, 26.374f), ImVec2(2.14f, 2.375f), background);
            drawList->AddEllipseFilled(point(42.509f, 26.374f), ImVec2(2.14f, 2.375f), background);
        }
        GuiUtils::HelpTooltip("Join our Discord");

        if (pressed) {
            const auto openInShell = ImGui::GetPlatformIO().Platform_OpenInShellFn;
            if (!openInShell || !openInShell(ImGui::GetCurrentContext(), DiscordPresence::COMMUNITY_URL)) {
                ImGui::SetClipboardText(DiscordPresence::COMMUNITY_URL);
                NotificationManager::NotifyAction("Could not open Discord. Invite copied to clipboard");
            }
        }
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
        const char* description = selectedSection->GetDescription();
        if (description && description[0] != '\0') ImGui::TextWrapped("%s", description);
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
            searchResults.push_back({SearchResultType::Category, tabSections.front().get(), nullptr, "Group"});
        }

        for (auto& section : tabSections) {
            if (MatchesSearch(section->GetName(), searchBuffer, filterLength) ||
                MatchesSearch(section->GetDescription(), searchBuffer, filterLength)) {
                searchResults.push_back({SearchResultType::Section, section.get(), nullptr, TAB_LABELS[tabIndex]});
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
                    searchResults.push_back(
                        {SearchResultType::Action, section.get(), &entry,
                         std::string(TAB_LABELS[tabIndex]) + " / " + section->GetName()}
                    );
                }
            }
        }
    }
    scrollToActiveSearchResult = !searchResults.empty();
}

void MenuManager::ActivateSearchResult(SearchResult result) {
    SelectSection(result.section);
    if (result.type == SearchResultType::Action && result.entry) {
        if (auto* keybinds = result.section->GetSearchKeybinds()) keybinds->RequestHighlight(result.entry);
    }
    searchBuffer[0] = '\0';
    searchResults.clear();
    activeSearchResult = 0;
    scrollToActiveSearchResult = false;
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

    GuiUtils::SetNextInputWidth(availableWidth);
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
    if (searchInputActive) {
        ImGui::SetItemKeyOwner(ImGuiKey_UpArrow);
        ImGui::SetItemKeyOwner(ImGuiKey_DownArrow);
    } else {
        GuiUtils::HelpTooltip("Find a section or action (Ctrl+K). Use the arrow keys and Enter to open it.");
    }
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
}

void MenuManager::RenderSearchResults() {
    if (searchResults.empty()) {
        ImGui::Indent(SIDEBAR_HPAD);
        ImGui::TextUnformatted("No matches");
        GuiUtils::TextDisabledWrapped("Try another name or keyword.");
        ImGui::Unindent(SIDEBAR_HPAD);
        return;
    }

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
        if (NavigationButton(label, selected, false, result.location.c_str())) {
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

        if (activate) break;
    }

    scrollToActiveSearchResult = false;
    if (activate) ActivateSearchResult(activatedResult);
}

void MenuManager::RenderSplitter(float maximumSidebarWidth) {
    ImGui::PushStyleColor(ImGuiCol_Button, DefaultStyle::CLEAR);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DefaultStyle::HEADER_HOVERED);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, DefaultStyle::HEADER_ACTIVE);
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

    const ImVec2 minimum = ImGui::GetItemRectMin();
    const ImVec2 maximum = ImGui::GetItemRectMax();
    const float middleX = (minimum.x + maximum.x) * 0.5f;
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(middleX, minimum.y), ImVec2(middleX, maximum.y),
        ImGui::GetColorU32(ImGui::IsItemActive() ? ImGuiCol_SeparatorActive : ImGuiCol_Separator)
    );

    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (ImGui::IsItemActive()) {
        sidebarWidth = std::clamp(sidebarWidth + ImGui::GetIO().MouseDelta.x, SIDEBAR_MIN_WIDTH, maximumSidebarWidth);
    }
    if (ImGui::IsItemDeactivated()) ConfigManager::Get().SetFloat("GUI", "navigation_width", sidebarWidth);
}

void MenuManager::RenderCategoryHeader(const char* label, MenuTab tab, bool& firstVisible) {
    if (!firstVisible) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + CATEGORY_VGAP);
    firstVisible = false;

    bool selected = selectedSection && selectedSection->GetTab() == tab;
    if (NavigationButton(label, selected, true)) {
        if (!selected) {
            auto& tabSections = sections[static_cast<size_t>(tab)];
            if (!tabSections.empty()) SelectSection(tabSections.front().get());
            selected = true;
        }
    }

    if (!ImGui::IsItemVisible()) return;
    const ImVec2 minimum = ImGui::GetItemRectMin();
    const ImVec2 maximum = ImGui::GetItemRectMax();
    const float middleY = (minimum.y + maximum.y) * 0.5f;
    const float arrowX = maximum.x - 12.0f;
    const ImU32 color = ImGui::GetColorU32(selected ? DefaultStyle::BRIGHT_BRASS : DefaultStyle::TEXT_DISABLED);
    const std::array points =
        selected
            ? std::array{
                  ImVec2(arrowX - ARROW_SIZE, middleY - ARROW_SIZE * 0.5f),
                  ImVec2(arrowX, middleY + ARROW_SIZE * 0.5f),
                  ImVec2(arrowX + ARROW_SIZE, middleY - ARROW_SIZE * 0.5f)
              }
            : std::array{
                  ImVec2(arrowX - ARROW_SIZE * 0.5f, middleY - ARROW_SIZE), ImVec2(arrowX + ARROW_SIZE * 0.5f, middleY),
                  ImVec2(arrowX - ARROW_SIZE * 0.5f, middleY + ARROW_SIZE)
              };
    ImGui::GetWindowDrawList()
        ->AddPolyline(points.data(), static_cast<int>(points.size()), color, ImDrawFlags_None, 1.5f);
}

void MenuManager::RenderCategorySections(MenuTab tab) {
    for (auto& section : sections[static_cast<size_t>(tab)]) {
        const bool selected = selectedSection == section.get();
        if (NavigationButton(section->GetName(), selected)) SelectSection(section.get());
    }
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
        ImGui::ColorConvertFloat4ToU32(SIDEBAR_BACKGROUND), style.WindowRounding, ImDrawFlags_RoundCornersBottomLeft
    );

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(SIDEBAR_HPAD, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(SIDEBAR_HPAD, SectionStyle::FRAME_PADDING.y));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, SIDEBAR_HPAD - 4.0f);
    ImGui::BeginChild(
        "nav_sidebar", ImVec2(sidebarWidth, ImGui::GetContentRegionAvail().y), ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    RenderSearchBar();

    const float contentWidth = ImGui::GetContentRegionAvail().x;
    // Keep the scroll track in the outer gutter so rows retain the search field's width.
    ImGui::SetNextWindowContentSize(ImVec2(contentWidth, 0));
    ImGui::BeginChild(
        "nav_sections", ImVec2(contentWidth + SIDEBAR_HPAD - 2.0f, -COMMUNITY_BUTTON_SIZE - style.ItemSpacing.y)
    );
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 2));
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
    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SIDEBAR_HPAD - 4.0f);
    RenderDiscordButton();

    ImGui::EndChild();
    ImGui::PopStyleVar(4);
}
