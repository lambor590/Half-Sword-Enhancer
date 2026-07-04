#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Menu/Section.h"
#include "Utils/GuiUtils.h"
#include "Utils/PropertyBrowser.h"

class SaveEditorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{MenuTab::Player, "Save Editor"};

private:
    struct SaveSlot {
        std::string slotName;
        uintmax_t sizeBytes = 0;
    };

    struct SaveObjectEntry {
        SDK::USaveGame* object = nullptr;
        std::string label;
        bool playerProgression = false;
    };

    struct VisibleCategory {
        std::string name;
        std::vector<const PropertyBrowser::PropertyInfo*> props;
    };

    std::vector<SaveSlot> saveSlots;
    std::vector<SaveObjectEntry> saveObjects;
    int selectedSlotIndex = -1;
    SDK::USaveGame* selectedObject = nullptr;
    std::string selectedObjectLabel;
    std::string selectedSourceLabel;
    std::vector<PropertyBrowser::PropertyInfo> properties;
    PropertyBrowser::CategoryMap categories;
    std::vector<VisibleCategory> visibleCategories;
    std::string visiblePropertyFilter;
    GuiUtils::StatusMessage status;
    char slotNameBuf[128] = "";
    char propSearchBuf[128] = "";
    bool visiblePropertiesReady = false;
    bool slotsNeedRefresh = true;
    bool objectsNeedScan = true;
    bool showAllSaveGames = false;
    bool pendingLoad = false;
    bool pendingSave = false;
    int expandState = 0;

    void SetSlotName(const std::string& slotName);
    void RefreshSlots();
    void ScanLiveSaves();
    void ClearSelectedObject();
    void SelectSaveObject(SDK::USaveGame* object, const std::string& sourceLabel, const std::string& slotName);
    void ValidateSelection();
    void LoadSelectedSlot();
    void SaveSelectedSlot();
    void RebuildVisibleProperties(size_t filterLen);
    void RenderSlotControls();
    void RenderObjectControls();
    void RenderPropertyToolbar();
    void RenderCategory(
        const std::string& categoryName, const std::vector<const PropertyBrowser::PropertyInfo*>& props,
        size_t filterLen
    );
    void RenderProperties();

public:
    explicit SaveEditorSection(ModContext& ctx);
    void OnOpen() override;
    void Render() override;
};
