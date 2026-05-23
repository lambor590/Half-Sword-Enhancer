#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Section.h"
#include "MenuTab.h"

class ModContext;
struct ImVec2;

class MenuManager {
public:
    static MenuManager& Get();

    MenuManager(const MenuManager&) = delete;
    MenuManager& operator=(const MenuManager&) = delete;

    template <typename T> void AddSection(MenuTab tab, ModContext& ctx) {
        auto& sectionVec = sections[static_cast<size_t>(tab)];
        if (sectionVec.empty()) {
            sectionVec.reserve(8);
        }
        sectionVec.push_back(std::make_unique<T>(ctx));
        if (!selectedSection) selectedSection = sectionVec.back().get();
        RebuildRenderGroups(tab);
    }

    void AddSection(MenuTab tab, std::unique_ptr<Section> section);

    void RenderMenu();

private:
    static constexpr size_t TAB_COUNT = static_cast<size_t>(MenuTab::Count);

    static constexpr std::array<std::pair<MenuTab, const char*>, TAB_COUNT> TAB_ORDER = {
        {{MenuTab::Player, "Player"},
         {MenuTab::World, "World"},
         {MenuTab::Spawner, "Spawner"},
         {MenuTab::Equipment, "Equipment"},
         {MenuTab::Settings, "Settings"}}};

    static constexpr float SIDEBAR_MIN_WIDTH = 60.0f;
    static constexpr float SPLITTER_THICKNESS = 4.0f;
    static constexpr float SECTION_INDENT = 14.0f;
    static constexpr float SIDEBAR_HPAD = 10.0f;
    static constexpr float CATEGORY_VGAP = 4.0f;
    static constexpr float ARROW_SIZE = 4.0f;
    static constexpr float ARROW_INDENT = 8.0f;

    MenuManager() = default;

    struct SearchResult {
        MenuTab tab;
        Section* section;
        std::string_view functionName;
    };

    struct RenderGroup {
        const char* name = nullptr;
        std::vector<Section*> sections;
    };

    std::array<std::vector<std::unique_ptr<Section>>, TAB_COUNT> sections;
    std::array<std::vector<RenderGroup>, TAB_COUNT> renderGroups;
    Section* selectedSection = nullptr;
    Section* openedSection = nullptr;
    MenuTab openCategory = MenuTab::Player;
    float sidebarWidth = 140.0f;
    bool sidebarVisible = true;

    char searchBuffer[128] = "";
    bool searchActive = false;
    std::vector<SearchResult> searchResults;

    // --- Search helpers ---
    static bool MatchesSearch(std::string_view text, const char* lowerNeedle, size_t needleLen) noexcept;
    void UpdateSearchResults() noexcept;
    void RebuildRenderGroups(MenuTab tab);
    void SelectSection(Section* section, MenuTab tab);

    // --- Sidebar rendering ---
    void RenderSidebar();
    void RenderSearchBar();
    void RenderSearchResults();
    void RenderSplitter();
    void RenderCategoryHeader(const char* label, MenuTab tab, bool& firstVisible);
    void RenderCategorySections(MenuTab tab);
};
