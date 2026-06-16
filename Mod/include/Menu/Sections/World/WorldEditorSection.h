#pragma once

#include <string>
#include <vector>

#include "Menu/Section.h"
#include "Utils/GuiUtils.h"
#include "Utils/PropertyBrowser.h"

class WorldEditorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{MenuTab::World, "World Editor", "Environment"};

private:
    std::vector<PropertyBrowser::WorldActor> allActors;
    std::vector<PropertyBrowser::WorldActor> filteredActors;
    int selectedActorIndex = -1;
    SDK::UObject* browseTarget = nullptr;
    bool browseTargetIsComponent = false;
    bool browseTargetIsActor = false;
    SDK::AActor* selectedActor = nullptr;
    SDK::UWorld* cachedWorld = nullptr;
    std::vector<PropertyBrowser::PropertyInfo> properties;
    PropertyBrowser::CategoryMap categories;
    struct VisibleCategory {
        std::string name;
        std::vector<const PropertyBrowser::PropertyInfo*> props;
    };
    std::vector<VisibleCategory> visibleCategories;
    std::string visiblePropertyFilter;
    bool visiblePropertiesReady = false;
    char actorSearchBuf[64] = "";
    char propSearchBuf[128] = "";
    std::string infoText;
    GuiUtils::StatusMessage status;
    std::string previousClassName;
    float actorComboWidth = 0;
    bool needsScan = true;
    bool liveMode = true;
    bool pendingApply = false;
    int activeQuickFilter = 0;
    int expandState = 0;
    bool findPending = false;
    int selectedTargetIndex = -1;

    struct BrowseTarget {
        SDK::UObject* object = nullptr;
        std::string label;
        bool isActor = false;
        bool isComponent = false;
    };
    std::vector<BrowseTarget> browseTargets;

    struct QuickFilter {
        const char* label;
        const char* filter;
    };
    static constexpr QuickFilter QUICK_FILTERS[] = {
        {"All", ""},          {"Sky", "Sky"},     {"Light", "Light"},   {"Fog", "Fog"},     {"Post", "PostProcess"},
        {"Volume", "Volume"}, {"Water", "Water"}, {"Camera", "Camera"}, {"Decal", "Decal"}, {"Audio", "Audio"},
        {"Prop", "Prop"},
    };

    void ResetState();
    void ScanAllActors();
    void ApplyFilter();
    void AddSceneComponentTargets(SDK::USceneComponent* component, int depth);
    void BuildBrowseTargets(SDK::AActor* actor, const std::string& className);
    void BrowseActor(SDK::AActor* actor, const std::string& className);
    void SelectTarget(int index);
    void SelectActor(int index);
    void SelectActorDirect(SDK::AActor* actor, const std::string& className);
    void FindByClassName(const char* className);
    void QueueApply();
    void QueueActorState(bool hidden, bool collision, bool tickEnabled);
    void QueueComponentCollision(SDK::ECollisionEnabled collision);
    int CountVisibleInCategory(const std::vector<const PropertyBrowser::PropertyInfo*>& props, size_t filterLen);
    void RenderCategory(
        const std::string& categoryName, const std::vector<const PropertyBrowser::PropertyInfo*>& props,
        size_t filterLen
    );
    void RebuildVisibleProperties(size_t filterLen);
    void RenderActorSelector();
    void RenderTargetSelector();
    void RenderTargetControls();
    void RenderActorControls();
    void RenderComponentControls();
    void RenderPropertyToolbar();

public:
    explicit WorldEditorSection(ModContext& ctx);
    void OnOpen() override;
    void Render() override;
};
