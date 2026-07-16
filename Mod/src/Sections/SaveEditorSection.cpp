#include "Menu/Sections/Player/SaveEditorSection.h"

#include "Hooks/GameHook.h"
#include "SDK/Engine_classes.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <cstdio>
#include <utility>

namespace {
    constexpr const char* PLAYER_PROGRESS_CLASS = "SG_PlayerProgression_C";
    constexpr const char* DEFAULT_PLAYER_SLOT = "SG Gauntlet Progress";
    constexpr GuiUtils::WidthSpec SAVE_FIELD_WIDTH{140.0f, 320.0f, 480.0f};

    [[nodiscard]] std::filesystem::path SaveGameDir() {
        char* localAppData = nullptr;
        size_t len = 0;
        if (_dupenv_s(&localAppData, &len, "LOCALAPPDATA") != 0 || !localAppData) return {};

        std::filesystem::path dir;
        if (localAppData[0] != '\0') dir = std::filesystem::path(localAppData) / "HalfSwordUE5" / "Saved" / "SaveGames";
        std::free(localAppData);
        return dir;
    }

    [[nodiscard]] std::wstring Widen(std::string_view value) {
        return {value.begin(), value.end()};
    }

    [[nodiscard]] bool IsLiveSaveGame(SDK::USaveGame* object) {
        return PropertyBrowser::IsLiveObject(object) && object->IsA(SDK::USaveGame::StaticClass());
    }

    [[nodiscard]] bool IsPlayerProgressionSave(SDK::UObject* object) {
        auto* cls = SDK::UObject::FindClassFast(PLAYER_PROGRESS_CLASS);
        return cls && object && object->IsA(cls);
    }

    [[nodiscard]] std::string SaveObjectLabel(SDK::USaveGame* object, bool playerProgression) {
        if (!IsLiveSaveGame(object)) return "Unavailable Save";
        if (playerProgression) return "Player Progress";

        std::string label = object->Class ? object->Class->GetName() : "Other Save";
        if (label.ends_with("_C")) label.resize(label.size() - 2);
        if (label.starts_with("SG_")) label.erase(0, 3);
        std::ranges::replace(label, '_', ' ');
        return label;
    }
} // namespace

SaveEditorSection::SaveEditorSection(ModContext& ctx) : Section(ctx, SECTION) {}

void SaveEditorSection::OnOpen() {
    slotsNeedRefresh = true;
    objectsNeedScan = true;
}

void SaveEditorSection::SetSlotName(const std::string& slotName) {
    strncpy_s(slotNameBuf, sizeof(slotNameBuf), slotName.c_str(), _TRUNCATE);
}

void SaveEditorSection::RefreshSlots() {
    slotsNeedRefresh = false;
    saveSlots.clear();

    const auto dir = SaveGameDir();
    std::error_code existsEc;
    if (dir.empty() || !std::filesystem::exists(dir, existsEc) || existsEc) {
        selectedSlotIndex = -1;
        return;
    }

    std::error_code iterEc;
    for (const auto& entry : std::filesystem::directory_iterator(dir, iterEc)) {
        std::error_code fileEc;
        if (!entry.is_regular_file(fileEc) || fileEc || entry.path().extension() != ".sav") continue;

        SaveSlot slot;
        slot.slotName = entry.path().stem().string();
        slot.sizeBytes = entry.file_size(fileEc);
        if (fileEc) slot.sizeBytes = 0;
        saveSlots.push_back(std::move(slot));
    }

    std::ranges::sort(saveSlots, [](const SaveSlot& a, const SaveSlot& b) { return a.slotName < b.slotName; });

    const std::string currentSlot = slotNameBuf;
    selectedSlotIndex = -1;
    for (int i = 0; i < static_cast<int>(saveSlots.size()); ++i) {
        if (saveSlots[i].slotName == currentSlot) {
            selectedSlotIndex = i;
            break;
        }
    }
    if (selectedSlotIndex < 0) {
        for (int i = 0; i < static_cast<int>(saveSlots.size()); ++i) {
            if (saveSlots[i].slotName == DEFAULT_PLAYER_SLOT) {
                selectedSlotIndex = i;
                break;
            }
        }
    }
    if (selectedSlotIndex < 0 && !saveSlots.empty()) selectedSlotIndex = 0;
    if (selectedSlotIndex >= 0) SetSlotName(saveSlots[selectedSlotIndex].slotName);
}

void SaveEditorSection::ScanLiveSaves() {
    objectsNeedScan = false;
    saveObjects.clear();

    auto* saveClass = SDK::USaveGame::StaticClass();
    if (!saveClass) return;

    const int count = SDK::UObject::GObjects->Num();
    for (int i = 0; i < count; ++i) {
        auto* object = SDK::UObject::GObjects->GetByIndex(i);
        if (!PropertyBrowser::IsLiveObject(object) || object->IsDefaultObject() || !object->IsA(saveClass)) continue;

        const bool playerProgression = IsPlayerProgressionSave(object);
        if (!showAllSaveGames && !playerProgression) continue;

        auto* save = static_cast<SDK::USaveGame*>(object);
        SaveObjectEntry entry;
        entry.object = save;
        entry.playerProgression = playerProgression;
        entry.label = SaveObjectLabel(save, playerProgression);
        saveObjects.push_back(std::move(entry));
    }

    std::ranges::sort(saveObjects, [](const SaveObjectEntry& a, const SaveObjectEntry& b) {
        if (a.playerProgression != b.playerProgression) return a.playerProgression > b.playerProgression;
        return a.label < b.label;
    });

    if (!selectedObject && !saveObjects.empty()) {
        SelectSaveObject(saveObjects.front().object, "Current game", "");
    }
}

void SaveEditorSection::ClearSelectedObject() {
    selectedObject = nullptr;
    selectedObjectLabel.clear();
    selectedSourceLabel.clear();
    propertyPanel.Clear();
}

void SaveEditorSection::SelectSaveObject(
    SDK::USaveGame* object, const std::string& sourceLabel, const std::string& slotName
) {
    if (!IsLiveSaveGame(object)) {
        ClearSelectedObject();
        status.Set("This save is unavailable.", true);
        return;
    }

    selectedObject = object;
    selectedObjectLabel = SaveObjectLabel(object, IsPlayerProgressionSave(object));
    selectedSourceLabel = sourceLabel;
    if (!slotName.empty()) SetSlotName(slotName);

    propertyPanel.SetType(object->Class);
    const int editableCount = propertyPanel.EditableCount();

    status.Set("Ready to edit " + selectedObjectLabel + " (" + std::to_string(editableCount) + " values).");
}

void SaveEditorSection::ValidateSelection() {
    if (selectedObject && !IsLiveSaveGame(selectedObject)) {
        ClearSelectedObject();
        objectsNeedScan = true;
        status.Set("The selected save is no longer available.", true);
    }
}

void SaveEditorSection::PublishOperationResult(OperationResult result) {
    std::lock_guard lock(operationResultMutex);
    operationResults.push_back(std::move(result));
}

void SaveEditorSection::DrainOperationResults() {
    std::vector<OperationResult> results;
    {
        std::lock_guard lock(operationResultMutex);
        results.swap(operationResults);
    }

    for (auto& result : results) {
        if (result.operation == Operation::Load) {
            pendingLoad = false;
            if (result.object) {
                SelectSaveObject(result.object, "Opened from " + result.slotName, result.slotName);
                objectsNeedScan = true;
                continue;
            }
        } else {
            pendingSave = false;
            slotsNeedRefresh = true;
        }
        status.Set(result.message, result.error);
    }
}

void SaveEditorSection::LoadSelectedSlot() {
    if (pendingLoad) return;

    std::string slotName = slotNameBuf;
    if (slotName.empty()) {
        status.Set("Enter a save name.", true);
        return;
    }

    pendingLoad = true;
    std::wstring wideSlot = Widen(slotName);

    const bool queued = GameHook::QueueAction([this, slotName = std::move(slotName),
                                               wideSlot = std::move(wideSlot)](const RuntimeContextSnapshot&) {
        SDK::FString slot(wideSlot.c_str());
        if (!SDK::UGameplayStatics::DoesSaveGameExist(slot, 0)) {
            PublishOperationResult({
                .operation = Operation::Load,
                .message = "No save named '" + slotName + "' was found.",
                .error = true,
            });
            return;
        }

        auto* save = SDK::UGameplayStatics::LoadGameFromSlot(slot, 0);
        if (!save || !save->IsA(SDK::USaveGame::StaticClass())) {
            PublishOperationResult({
                .operation = Operation::Load,
                .message = "Could not open the selected save",
                .error = true,
            });
            return;
        }

        PublishOperationResult({
            .operation = Operation::Load,
            .object = save,
            .slotName = slotName,
        });
    });
    if (!queued) {
        pendingLoad = false;
        status.Set("Could not open the selected save", true);
    }
}

void SaveEditorSection::SaveSelectedSlot() {
    if (pendingSave) return;
    if (!IsLiveSaveGame(selectedObject)) {
        status.Set("Choose a save to edit.", true);
        return;
    }

    std::string slotName = slotNameBuf;
    if (slotName.empty()) {
        status.Set("Enter a save name.", true);
        return;
    }

    auto* save = selectedObject;
    pendingSave = true;
    std::wstring wideSlot = Widen(slotName);

    const bool queued = GameHook::QueueAction([this, save, slotName = std::move(slotName),
                                               wideSlot = std::move(wideSlot)](const RuntimeContextSnapshot&) {
        if (!IsLiveSaveGame(save)) {
            PublishOperationResult({
                .operation = Operation::Save,
                .message = "The selected save is no longer available",
                .error = true,
            });
            return;
        }

        const bool ok = SDK::UGameplayStatics::SaveGameToSlot(save, SDK::FString(wideSlot.c_str()), 0);
        PublishOperationResult({
            .operation = Operation::Save,
            .message = ok ? "Saved as '" + slotName + "'." : "The changes could not be saved.",
            .error = !ok,
        });
    });
    if (!queued) {
        pendingSave = false;
        status.Set("Could not save the changes", true);
    }
}

void SaveEditorSection::RenderSlotControls() {
    ImGui::SeparatorText("Saved Games");
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Choose a saved game or enter a new name.");
    ImGui::PopTextWrapPos();
    const bool operationPending = pendingLoad || pendingSave;

    const char* preview = selectedSlotIndex >= 0 && selectedSlotIndex < static_cast<int>(saveSlots.size())
                              ? saveSlots[selectedSlotIndex].slotName.c_str()
                              : "(none)";
    ImGui::TextUnformatted("Open Save");
    if (operationPending) ImGui::BeginDisabled();
    if (GuiUtils::BeginSizedCombo("##SaveSlotSelector", preview, SAVE_FIELD_WIDTH)) {
        for (int i = 0; i < static_cast<int>(saveSlots.size()); ++i) {
            char label[192];
            std::snprintf(
                label, sizeof(label), "%s (%.1f KB)", saveSlots[i].slotName.c_str(),
                static_cast<double>(saveSlots[i].sizeBytes) / 1024.0
            );
            const bool selected = i == selectedSlotIndex;
            if (ImGui::Selectable(label, selected)) {
                selectedSlotIndex = i;
                SetSlotName(saveSlots[i].slotName);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (operationPending) ImGui::EndDisabled();

    if (saveSlots.empty()) {
        GuiUtils::RenderCallout(
            "save-slots-empty", "No saved games were found. You can still enter a save name manually.",
            GuiUtils::CalloutTone::Info
        );
    }

    ImGui::TextUnformatted("Save As");
    if (operationPending) ImGui::BeginDisabled();
    GuiUtils::SetNextFieldWidth(SAVE_FIELD_WIDTH);
    ImGui::InputText("##TargetSlot", slotNameBuf, sizeof(slotNameBuf));
    if (operationPending) ImGui::EndDisabled();

    if (operationPending) ImGui::BeginDisabled();
    if (GuiUtils::Button("Refresh Saves")) RefreshSlots();
    (void)GuiUtils::SameLineIfFitsButton("Open Selected Save");
    const bool canLoad = slotNameBuf[0] != '\0';
    if (!canLoad) ImGui::BeginDisabled();
    if (GuiUtils::Button("Open Selected Save", GuiUtils::ButtonTone::Primary)) LoadSelectedSlot();
    if (!canLoad) ImGui::EndDisabled();
    if (operationPending) ImGui::EndDisabled();
}

void SaveEditorSection::RenderObjectControls() {
    ImGui::SeparatorText("Edit Save");
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Open a saved game or use the one currently active, then change its values below.");
    ImGui::PopTextWrapPos();
    const bool operationPending = pendingLoad || pendingSave;

    bool rescan = false;
    if (operationPending) ImGui::BeginDisabled();
    if (ImGui::Button("Refresh Available Saves")) rescan = true;
    (void)GuiUtils::SameLineIfFitsCheckbox("Include Other Saves");
    if (ImGui::Checkbox("Include Other Saves", &showAllSaveGames)) rescan = true;
    GuiUtils::HelpTooltip("Also show saves that do not contain player progress.");
    if (rescan) ScanLiveSaves();

    const char* preview = selectedObject ? selectedObjectLabel.c_str() : "(none)";
    ImGui::TextUnformatted("Save to Edit");
    if (GuiUtils::BeginSizedCombo("##LiveSaveSelector", preview, SAVE_FIELD_WIDTH)) {
        for (int i = 0; i < static_cast<int>(saveObjects.size()); ++i) {
            const bool selected = saveObjects[i].object == selectedObject;
            if (ImGui::Selectable(saveObjects[i].label.c_str(), selected)) {
                SelectSaveObject(saveObjects[i].object, "Current game", "");
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (operationPending) ImGui::EndDisabled();

    if (selectedObject) {
        ImGui::TextDisabled("%s", selectedSourceLabel.empty() ? "Current game" : selectedSourceLabel.c_str());
        const bool canSave = !operationPending && slotNameBuf[0] != '\0';
        if (!canSave) ImGui::BeginDisabled();
        if (GuiUtils::Button("Save Changes", GuiUtils::ButtonTone::Primary)) ImGui::OpenPopup("Replace Save");
        if (!canSave) ImGui::EndDisabled();
    } else {
        GuiUtils::RenderCallout(
            "save-object-empty", "No save selected. Open a saved game or choose the current save to begin.",
            GuiUtils::CalloutTone::Info
        );
    }

    if (ImGui::BeginPopupModal("Replace Save", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped(
            "Save these changes as '%s'? An existing save with that name will be replaced.", slotNameBuf
        );
        ImGui::Spacing();
        if (GuiUtils::Button("Replace Save", GuiUtils::ButtonTone::Danger)) {
            SaveSelectedSlot();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (GuiUtils::Button("Cancel", GuiUtils::ButtonTone::Quiet)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (pendingLoad || pendingSave) {
        GuiUtils::RenderCallout(
            "save-operation-progress", pendingLoad ? "Opening save..." : "Saving changes...",
            GuiUtils::CalloutTone::Info
        );
    }
    status.Render();
}

void SaveEditorSection::RenderProperties() {
    if (!selectedObject) return;
    if (pendingLoad || pendingSave) ImGui::BeginDisabled();
    PropertyBrowser::RenderPanel(
        propertyPanel, reinterpret_cast<std::byte*>(selectedObject), "##SavePropFilter", "##SavePropertyList", true
    );
    if (pendingLoad || pendingSave) ImGui::EndDisabled();
}

void SaveEditorSection::Render() {
    DrainOperationResults();
    if (slotsNeedRefresh) RefreshSlots();
    if (objectsNeedScan) ScanLiveSaves();
    ValidateSelection();

    RenderSlotControls();
    RenderObjectControls();
    RenderProperties();
}
