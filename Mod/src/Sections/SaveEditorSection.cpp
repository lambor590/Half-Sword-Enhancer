#include "Menu/Sections/Player/SaveEditorSection.h"
#include "Menu/SectionStyle.h"

#include "Hooks/GameHook.h"
#include "SDK/Engine_classes.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <cstdio>
#include <utility>

namespace {
    constexpr const char* PLAYER_PROGRESS_CLASS = "SG_PlayerProgression_C";
    constexpr const char* DEFAULT_PLAYER_SLOT = "SG Gauntlet Progress";

    [[nodiscard]] std::filesystem::path SaveGameDir() {
        char* localAppData = nullptr;
        size_t len = 0;
        if (_dupenv_s(&localAppData, &len, "LOCALAPPDATA") != 0 || !localAppData)
            return {};

        std::filesystem::path dir;
        if (localAppData[0] != '\0')
            dir = std::filesystem::path(localAppData) / "HalfSwordUE5" / "Saved" / "SaveGames";
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
        if (!IsLiveSaveGame(object)) return "(invalid)";

        std::string label = object->Class ? object->Class->GetName() : object->GetName();
        const std::string instanceName = object->GetName();
        if (!instanceName.empty() && instanceName != label) {
            label += " | ";
            label += instanceName;
        }
        if (playerProgression) label += " | player progression";
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
        SelectSaveObject(saveObjects.front().object, "live object", "");
    }
}

void SaveEditorSection::ClearSelectedObject() {
    selectedObject = nullptr;
    selectedObjectLabel.clear();
    selectedSourceLabel.clear();
    properties.clear();
    categories.clear();
    visibleCategories.clear();
    visiblePropertyFilter.clear();
    visiblePropertiesReady = false;
}

void SaveEditorSection::SelectSaveObject(
    SDK::USaveGame* object, const std::string& sourceLabel, const std::string& slotName
) {
    if (!IsLiveSaveGame(object)) {
        ClearSelectedObject();
        status.Set("Save object not available", true);
        return;
    }

    selectedObject = object;
    selectedObjectLabel = SaveObjectLabel(object, IsPlayerProgressionSave(object));
    selectedSourceLabel = sourceLabel;
    if (!slotName.empty()) SetSlotName(slotName);

    properties = PropertyBrowser::EnumerateProperties(object->Class);
    categories = PropertyBrowser::GroupByCategory(properties);
    visibleCategories.clear();
    visiblePropertyFilter.clear();
    visiblePropertiesReady = false;

    int editableCount = 0;
    for (const auto& prop : properties)
        if (PropertyBrowser::IsEditable(prop.type)) ++editableCount;

    status.Set("Selected " + selectedObjectLabel + " (" + std::to_string(editableCount) + " editable)");
}

void SaveEditorSection::ValidateSelection() {
    if (selectedObject && !IsLiveSaveGame(selectedObject)) {
        ClearSelectedObject();
        objectsNeedScan = true;
        status.Set("Selected save object no longer available", true);
    }
}

void SaveEditorSection::LoadSelectedSlot() {
    if (pendingLoad) return;

    std::string slotName = slotNameBuf;
    if (slotName.empty()) {
        status.Set("Slot name required", true);
        return;
    }

    pendingLoad = true;
    status.Set("Loading " + slotName + "...");
    std::wstring wideSlot = Widen(slotName);

    GameHook::QueueAction([this, slotName = std::move(slotName), wideSlot = std::move(wideSlot)](
                              const RuntimeContextSnapshot&
                          ) {
        SDK::FString slot(wideSlot.c_str());
        if (!SDK::UGameplayStatics::DoesSaveGameExist(slot, 0)) {
            status.Set("Slot not found: " + slotName, true);
            pendingLoad = false;
            return;
        }

        auto* save = SDK::UGameplayStatics::LoadGameFromSlot(slot, 0);
        if (!save || !save->IsA(SDK::USaveGame::StaticClass())) {
            status.Set("Could not load save slot", true);
            pendingLoad = false;
            return;
        }

        SelectSaveObject(save, "slot: " + slotName, slotName);
        objectsNeedScan = true;
        pendingLoad = false;
    });
}

void SaveEditorSection::SaveSelectedSlot() {
    if (pendingSave) return;
    if (!IsLiveSaveGame(selectedObject)) {
        status.Set("No save object selected", true);
        return;
    }

    std::string slotName = slotNameBuf;
    if (slotName.empty()) {
        status.Set("Slot name required", true);
        return;
    }

    auto* save = selectedObject;
    pendingSave = true;
    status.Set("Saving " + slotName + "...");
    std::wstring wideSlot = Widen(slotName);

    GameHook::QueueAction([this, save, slotName = std::move(slotName), wideSlot = std::move(wideSlot)](
                              const RuntimeContextSnapshot&
                          ) {
        if (!IsLiveSaveGame(save)) {
            status.Set("Save object no longer available", true);
            pendingSave = false;
            return;
        }

        const bool ok = SDK::UGameplayStatics::SaveGameToSlot(save, SDK::FString(wideSlot.c_str()), 0);
        status.Set(ok ? "Saved " + slotName : "Save failed", !ok);
        slotsNeedRefresh = true;
        pendingSave = false;
    });
}

void SaveEditorSection::RebuildVisibleProperties(size_t filterLen) {
    visibleCategories.clear();
    visiblePropertyFilter.assign(propSearchBuf, filterLen);
    visiblePropertiesReady = true;

    for (auto& [catName, catProps] : categories) {
        VisibleCategory visible;
        visible.name = catName;
        visible.props.reserve(catProps.size());
        for (const auto* prop : catProps) {
            if (!PropertyBrowser::IsVisible(prop->type)) continue;
            if (filterLen > 0 && !PropertyBrowser::PropertyMatchesFilter(*prop, propSearchBuf, filterLen)) continue;
            visible.props.push_back(prop);
        }
        if (!visible.props.empty()) visibleCategories.push_back(std::move(visible));
    }
}

void SaveEditorSection::RenderSlotControls() {
    ImGui::SeparatorText("Save Slots");

    if (ImGui::SmallButton("Refresh")) RefreshSlots();
    ImGui::SameLine();
    if (pendingLoad) ImGui::BeginDisabled();
    if (ImGui::SmallButton("Load")) LoadSelectedSlot();
    if (pendingLoad) ImGui::EndDisabled();

    const char* preview = selectedSlotIndex >= 0 && selectedSlotIndex < static_cast<int>(saveSlots.size())
                              ? saveSlots[selectedSlotIndex].slotName.c_str()
                              : "(none)";
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo("##SaveSlotSelector", preview)) {
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

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputText("Target Slot", slotNameBuf, sizeof(slotNameBuf));
}

void SaveEditorSection::RenderObjectControls() {
    ImGui::SeparatorText("Save Objects");

    bool rescan = false;
    if (ImGui::SmallButton("Rescan")) rescan = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("All", &showAllSaveGames)) rescan = true;
    if (rescan) ScanLiveSaves();

    const char* preview = selectedObject ? selectedObjectLabel.c_str() : "(none)";
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo("##LiveSaveSelector", preview)) {
        for (int i = 0; i < static_cast<int>(saveObjects.size()); ++i) {
            const bool selected = saveObjects[i].object == selectedObject;
            if (ImGui::Selectable(saveObjects[i].label.c_str(), selected)) {
                SelectSaveObject(saveObjects[i].object, "live object", "");
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (selectedObject) {
        if (pendingSave || slotNameBuf[0] == '\0') ImGui::BeginDisabled();
        if (ImGui::SmallButton("Save")) SaveSelectedSlot();
        if (pendingSave || slotNameBuf[0] == '\0') ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", selectedSourceLabel.empty() ? "live object" : selectedSourceLabel.c_str());
    } else {
        ImGui::TextDisabled("No save object selected");
    }

    status.Render();
}

void SaveEditorSection::RenderPropertyToolbar() {
    ImGui::SeparatorText("Properties");

    float btnW = ImGui::CalcTextSize("+").x + ImGui::GetStyle().FramePadding.x * 2;
    float btnsW = btnW * 2 + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - btnsW - ImGui::GetStyle().ItemSpacing.x);
    if (ImGui::InputTextWithHint("##SavePropFilter", "Search properties...", propSearchBuf, sizeof(propSearchBuf))) {
        visiblePropertiesReady = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("+", ImVec2(btnW, 0))) expandState = 1;
    if (ImGui::IsItemHovered()) {
        GuiUtils::BeginStyledTooltip();
        ImGui::TextUnformatted("Expand all");
        GuiUtils::EndStyledTooltip();
    }
    ImGui::SameLine();
    if (ImGui::Button("-", ImVec2(btnW, 0))) expandState = -1;
    if (ImGui::IsItemHovered()) {
        GuiUtils::BeginStyledTooltip();
        ImGui::TextUnformatted("Collapse all");
        GuiUtils::EndStyledTooltip();
    }
}

void SaveEditorSection::RenderCategory(
    const std::string& categoryName, const std::vector<const PropertyBrowser::PropertyInfo*>& props, size_t filterLen
) {
    char label[128];
    std::snprintf(label, sizeof(label), "%s (%zu)", categoryName.c_str(), props.size());

    if (expandState != 0) ImGui::SetNextItemOpen(expandState > 0);
    const bool open = ImGui::TreeNodeEx(label, filterLen > 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    if (!open) return;

    for (const auto* prop : props)
        PropertyBrowser::RenderPropertyWidget(*prop, reinterpret_cast<std::byte*>(selectedObject));

    ImGui::TreePop();
}

void SaveEditorSection::RenderProperties() {
    if (!selectedObject) return;

    RenderPropertyToolbar();

    const size_t filterLen = std::strlen(propSearchBuf);
    if (!visiblePropertiesReady || visiblePropertyFilter.size() != filterLen ||
        std::memcmp(visiblePropertyFilter.data(), propSearchBuf, filterLen) != 0)
        RebuildVisibleProperties(filterLen);

    ImGui::BeginChild("##SavePropertyList", ImVec2(0, 0), ImGuiChildFlags_None);
    for (auto& category : visibleCategories)
        RenderCategory(category.name, category.props, filterLen);
    expandState = 0;
    if (visibleCategories.empty()) ImGui::TextDisabled("No properties match filter");
    ImGui::EndChild();
}

void SaveEditorSection::Render() {
    const SectionStyle::StyleRAII style;

    if (slotsNeedRefresh) RefreshSlots();
    if (objectsNeedScan) ScanLiveSaves();
    ValidateSelection();

    RenderSlotControls();
    RenderObjectControls();
    RenderProperties();
}
