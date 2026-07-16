#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "Menu/Section.h"
#include "Utils/GuiUtils.h"
#include "Utils/PropertyBrowser.h"

class SaveEditorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::Player, "Save Editor", "Change the contents of any Half Sword save."
    };

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

    enum class Operation : std::uint8_t { Load, Save };

    struct OperationResult {
        Operation operation = Operation::Load;
        SDK::USaveGame* object = nullptr;
        std::string slotName;
        std::string message;
        bool error = false;
    };

    std::vector<SaveSlot> saveSlots;
    std::vector<SaveObjectEntry> saveObjects;
    int selectedSlotIndex = -1;
    SDK::USaveGame* selectedObject = nullptr;
    std::string selectedObjectLabel;
    std::string selectedSourceLabel;
    PropertyBrowser::PanelState propertyPanel;
    GuiUtils::StatusMessage status;
    char slotNameBuf[128] = "";
    bool slotsNeedRefresh = true;
    bool objectsNeedScan = true;
    bool showAllSaveGames = false;
    bool pendingLoad = false;
    bool pendingSave = false;
    std::mutex operationResultMutex;
    std::vector<OperationResult> operationResults;

    void SetSlotName(const std::string& slotName);
    void RefreshSlots();
    void ScanLiveSaves();
    void ClearSelectedObject();
    void SelectSaveObject(SDK::USaveGame* object, const std::string& sourceLabel, const std::string& slotName);
    void ValidateSelection();
    void PublishOperationResult(OperationResult result);
    void DrainOperationResults();
    void LoadSelectedSlot();
    void SaveSelectedSlot();
    void RenderSlotControls();
    void RenderObjectControls();
    void RenderProperties();

public:
    explicit SaveEditorSection(ModContext& ctx);
    void OnOpen() override;
    void Render() override;
};
