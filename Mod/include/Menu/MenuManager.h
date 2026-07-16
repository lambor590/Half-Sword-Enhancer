#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "Section.h"
#include "MenuTab.h"

class ModContext;
struct KeybindEntry;

class MenuManager {
public:
    static MenuManager& Get();

    MenuManager(const MenuManager&) = delete;
    MenuManager& operator=(const MenuManager&) = delete;

    template <typename T> void AddSection(ModContext& ctx) {
        const MenuTab tab = T::SECTION.tab;
        auto& sectionVec = sections[static_cast<size_t>(tab)];
        sectionVec.push_back(std::make_unique<T>(ctx));
        if (!selectedSection) selectedSection = sectionVec.back().get();
    }

    void LoadNavigationState();
    void RenderMenu();

private:
    static constexpr size_t TAB_COUNT = static_cast<size_t>(MenuTab::Count);

    static constexpr std::array<const char*, TAB_COUNT> TAB_LABELS =
        {"Player", "World", "Spawn", "Equipment", "Settings"};

    static constexpr float SIDEBAR_MIN_WIDTH = 150.0f;
    static constexpr float SIDEBAR_MAX_WIDTH = 360.0f;
    static constexpr float CONTENT_MIN_WIDTH = 320.0f;
    static constexpr float SPLITTER_THICKNESS = 4.0f;
    static constexpr float SECTION_INDENT = 14.0f;
    static constexpr float SIDEBAR_HPAD = 10.0f;
    static constexpr float CATEGORY_VGAP = 4.0f;
    static constexpr float ARROW_SIZE = 4.0f;
    static constexpr float ARROW_INDENT = 8.0f;

    MenuManager() = default;

    enum class SearchResultType : uint8_t { Category, Section, Action };

    struct SearchResult {
        SearchResultType type;
        Section* section;
        KeybindEntry* entry = nullptr;
    };

    std::array<std::vector<std::unique_ptr<Section>>, TAB_COUNT> sections;
    Section* selectedSection = nullptr;
    Section* openedSection = nullptr;
    float sidebarWidth = 190.0f;
    bool focusSearch = false;
    size_t activeSearchResult = 0;
    bool scrollToActiveSearchResult = false;

    char searchBuffer[128] = "";
    std::vector<SearchResult> searchResults;

    static constexpr const char* GetTabLabel(MenuTab tab) noexcept { return TAB_LABELS[static_cast<size_t>(tab)]; }
    void ClearSearch() noexcept;
    void UpdateSearchResults();
    void ActivateSearchResult(SearchResult result);
    void SelectSection(Section* section);

    void RenderContent();
    void RenderSectionDescription();
    void RenderSidebar();
    void RenderSearchBar();
    void RenderSearchResults();
    void RenderSplitter(float maximumSidebarWidth);
    void RenderCategoryHeader(const char* label, MenuTab tab, bool& firstVisible);
    void RenderCategorySections(MenuTab tab);
};
