#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "Menu/Section.h"
#include "Utils/GuiUtils.h"
#include "Utils/PropertyBrowser.h"

class WorldEditorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::World, "World Editor", "Move, hide, and customize objects in the current map."
    };

private:
    std::vector<PropertyBrowser::WorldActor> allActors;
    std::vector<PropertyBrowser::WorldActor> filteredActors;
    int selectedActorIndex = -1;
    SDK::UObject* browseTarget = nullptr;
    bool browseTargetIsComponent = false;
    bool browseTargetIsActor = false;
    SDK::AActor* selectedActor = nullptr;
    SDK::UWorld* cachedWorld = nullptr;
    PropertyBrowser::PanelState propertyPanel;
    char actorSearchBuf[64] = "";
    std::string infoText;
    std::string selectedActorLabel;
    GuiUtils::StatusMessage status;
    float actorComboWidth = 0;
    bool needsScan = true;
    bool liveMode = true;
    bool pendingApply = false;
    bool nearbyMode = false;
    bool clickPickActive = false;
    bool pickPending = false;
    int activeQuickFilter = 0;
    bool findPending = false;
    int selectedTargetIndex = -1;

    enum class SelectionOperation : std::uint8_t { Pick, Find };

    struct SelectionResult {
        SelectionOperation operation = SelectionOperation::Pick;
        std::uint64_t generation = 0;
        SDK::UWorld* world = nullptr;
        SDK::AActor* actor = nullptr;
        SDK::UObject* preferredTarget = nullptr;
        std::string className;
        std::string error;
    };
    std::uint64_t selectionGeneration = 0;
    std::mutex selectionResultMutex;
    std::vector<SelectionResult> selectionResults;

    struct BrowseTarget {
        SDK::UObject* object = nullptr;
        std::string label;
        bool isActor = false;
        bool isComponent = false;
    };
    std::vector<BrowseTarget> browseTargets;

    struct HighlightMarker {
        SDK::AActor* actor = nullptr;
        SDK::USceneComponent* component = nullptr;
        double endTime = 0.0;
    };
    HighlightMarker highlightMarker;

    struct QuickFilter {
        const char* label;
        const char* filter;
    };
    static constexpr QuickFilter QUICK_FILTERS[] = {
        {"All", ""},
        {"Sky", "Sky"},
        {"Lights", "Light"},
        {"Fog", "Fog"},
        {"Effects", "PostProcess"},
        {"Areas", "Volume"},
        {"Water", "Water"},
        {"Cameras", "Camera"},
        {"Surface Details", "Decal"},
        {"Audio", "Audio"},
        {"Props", "Prop"},
    };
    static constexpr float NEARBY_RANGE_METERS = 40.0f;

    void ResetState();
    void ClearBrowseTarget();
    void ClearUnavailableActorSelection();
    void ValidateSelection();
    void ScanAllActors();
    void ApplyFilter();
    void AddSceneComponentTargets(SDK::USceneComponent* component, int depth);
    void BuildBrowseTargets(SDK::AActor* actor, const std::string& className);
    SDK::AActor* SelectedTargetActor() const;
    void BrowseActor(SDK::AActor* actor, const std::string& className, SDK::UObject* preferredTarget = nullptr);
    void SelectTarget(int index);
    void SelectActor(int index);
    void SelectActorDirect(SDK::AActor* actor, const std::string& className, SDK::UObject* preferredTarget = nullptr);
    void PublishSelectionResult(SelectionResult result);
    void DrainSelectionResults(SDK::UWorld* world);
    void PickClickedActor(ImVec2 screenPos, ImVec2 viewportPos, ImVec2 viewportSize);
    void RenderClickPickOverlay();
    void HighlightSelected();
    void RenderHighlightMarker();
    void FindByClassName(const char* className);
    void QueueApply();
    void QueueActorState(SDK::AActor* actor, bool hidden, bool collision, bool tickEnabled);
    void QueueActorTransform(
        SDK::AActor* actor, const SDK::FVector& location, const SDK::FRotator& rotation, const SDK::FVector& scale
    );
    void RenderActorSelector();
    void RenderTargetSelector();
    void RenderTargetControls();
    void RenderActorControls();
    void RenderComponentControls();
    void RenderMaterialInspector();
    void RenderMaterialEntry(int index, SDK::UMaterialInterface* material);

public:
    explicit WorldEditorSection(ModContext& ctx);
    void OnOpen() override;
    void Render() override;
};
