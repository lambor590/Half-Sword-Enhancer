#pragma once

#include <string>
#include <vector>

#include "Menu/Section.h"
#include "Utils/GuiUtils.h"
#include "Utils/PropertyBrowser.h"

class WorldEditorSection : public Section {
private:
    std::vector<PropertyBrowser::WorldActor> allActors;
    std::vector<PropertyBrowser::WorldActor> filteredActors;
    int selectedActorIndex = -1;
    SDK::UObject* browseTarget = nullptr;
    bool browseTargetIsComponent = false;
    SDK::UWorld* cachedWorld = nullptr;
    std::vector<PropertyBrowser::PropertyInfo> properties;
    PropertyBrowser::CategoryMap categories;
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

    struct QuickFilter {
        const char* label;
        const char* filter;
    };
    static constexpr QuickFilter QUICK_FILTERS[] = {
        {"All", ""},          {"Sky", "Sky"},     {"Light", "Light"}, {"Fog", "Fog"}, {"PostProcess", "PostProcess"},
        {"Volume", "Volume"}, {"Water", "Water"},
    };

    void ResetState();
    void ScanAllActors();
    void ApplyFilter();
    void SelectActor(int index);
    void SelectActorDirect(SDK::AActor* actor, const std::string& className);
    void FindByClassName(const char* className);
    void QueueApply();
    int CountVisibleInCategory(const std::vector<const PropertyBrowser::PropertyInfo*>& props, size_t filterLen);
    void RenderCategory(
        const std::string& categoryName, const std::vector<const PropertyBrowser::PropertyInfo*>& props,
        size_t filterLen
    );
    void RenderActorSelector();
    void RenderPropertyToolbar();

public:
    const char* GetGroup() const noexcept override { return "Environment"; }

    explicit WorldEditorSection(ModContext& ctx);
    void Render() override;
};
