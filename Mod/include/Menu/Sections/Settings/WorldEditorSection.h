#pragma once

#include <string>
#include <vector>

#include "Menu/ICollapsibleSection.h"
#include "Utils/GuiUtils.h"
#include "Utils/PropertyBrowser.h"
#include "ComponentValidator.h"
#include "Hooks/GameHook.h"

class WorldEditorSection : public CollapsibleSection {
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

    struct QuickFilter { const char* label; const char* filter; };
    static constexpr QuickFilter QUICK_FILTERS[] = {
        {"All", ""},
        {"Sky", "Sky"},
        {"Light", "Light"},
        {"Fog", "Fog"},
        {"PostProcess", "PostProcess"},
        {"Volume", "Volume"},
        {"Water", "Water"},
    };

    void ResetState() {
        allActors.clear();
        filteredActors.clear();
        selectedActorIndex = -1;
        browseTarget = nullptr;
        browseTargetIsComponent = false;
        cachedWorld = nullptr;
        properties.clear();
        categories.clear();
        actorComboWidth = 0;
        infoText.clear();
        status = {};
        needsScan = true;
    }

    void ScanAllActors() {
        needsScan = false;

        if (!ComponentValidator::Validate(world)) {
            status.Set("World not available", true);
            return;
        }

        cachedWorld = world;
        allActors = PropertyBrowser::FindWorldActors(world);
        ApplyFilter();
    }

    void ApplyFilter() {
        filteredActors.clear();
        selectedActorIndex = -1;
        browseTarget = nullptr;
        browseTargetIsComponent = false;
        properties.clear();
        categories.clear();
        actorComboWidth = 0;

        const char* filter = actorSearchBuf[0] != '\0'
            ? actorSearchBuf
            : QUICK_FILTERS[activeQuickFilter].filter;
        size_t filterLen = std::strlen(filter);

        for (auto& wa : allActors) {
            if (filterLen == 0 || GuiUtils::MatchesFilter(wa.className.c_str(), wa.className.size(), filter, filterLen))
                filteredActors.push_back(wa);
        }

        if (filteredActors.empty()) {
            status.Set("No actors match filter", true);
            return;
        }

        float maxW = 0;
        for (const auto& wa : filteredActors) {
            float w = ImGui::CalcTextSize(wa.className.c_str()).x;
            if (w > maxW) maxW = w;
        }
        actorComboWidth = GuiUtils::ComboWidthFromText(maxW);

        int restoreIdx = 0;
        if (!previousClassName.empty()) {
            for (int i = 0; i < static_cast<int>(filteredActors.size()); ++i) {
                if (filteredActors[i].className == previousClassName) {
                    restoreIdx = i;
                    break;
                }
            }
        }
        SelectActor(restoreIdx);
    }

    void SelectActor(int index) {
        if (index < 0 || index >= static_cast<int>(filteredActors.size())) return;
        selectedActorIndex = index;
        previousClassName = filteredActors[index].className;

        auto* actor = filteredActors[index].actor;
        browseTarget = actor;
        browseTargetIsComponent = false;
        if (actor->RootComponent) {
            browseTarget = actor->RootComponent;
            browseTargetIsComponent = true;
        }

        properties = PropertyBrowser::EnumerateProperties(browseTarget->Class);
        categories = PropertyBrowser::GroupByCategory(properties);

        int supported = 0;
        for (const auto& p : properties)
            if (p.type != PropertyBrowser::PropType::Unsupported) ++supported;

        infoText = filteredActors[index].className + " > " +
            browseTarget->Class->GetName() + " (" +
            std::to_string(supported) + " editable)";
    }

    void SelectActorDirect(SDK::AActor* actor, const std::string& className) {
        if (!actor) return;
        browseTarget = actor;
        browseTargetIsComponent = false;
        if (actor->RootComponent) {
            browseTarget = actor->RootComponent;
            browseTargetIsComponent = true;
        }

        properties = PropertyBrowser::EnumerateProperties(browseTarget->Class);
        categories = PropertyBrowser::GroupByCategory(properties);

        int supported = 0;
        for (const auto& p : properties)
            if (p.type != PropertyBrowser::PropType::Unsupported) ++supported;

        infoText = className + " > " +
            browseTarget->Class->GetName() + " (" +
            std::to_string(supported) + " editable)";
    }

    void FindByClassName(const char* className) {
        if (findPending) return;
        auto* cls = SDK::UObject::FindClassFast(std::string(className));
        if (!cls) {
            status.Set(std::string(className) + " class not found", true);
            return;
        }
        findPending = true;
        status.Set("Searching...");
        std::string name = className;
        GameHook::QueueAction([this, cls, name]() {
            auto* actor = SDK::UGameplayStatics::GetActorOfClass(cachedWorld ? cachedWorld : world, cls);
            if (actor) {
                selectedActorIndex = -1;
                SelectActorDirect(actor, name);
            } else {
                status.Set(name + " instance not found", true);
            }
            findPending = false;
        });
    }

    void QueueApply() {
        if (!browseTargetIsComponent) return;
        auto* comp = static_cast<SDK::USceneComponent*>(browseTarget);
        bool isSkyLight = browseTarget->IsA(SDK::USkyLightComponent::StaticClass());
        GameHook::QueueAction([comp, isSkyLight]() {
            comp->SetVisibility(false, false);
            comp->SetVisibility(true, false);
            comp->K2_SetRelativeLocationAndRotation(
                comp->RelativeLocation, comp->RelativeRotation,
                false, nullptr, true);
            if (isSkyLight)
                static_cast<SDK::USkyLightComponent*>(comp)->RecaptureSky();
        });
    }

    int CountVisibleInCategory(const std::vector<const PropertyBrowser::PropertyInfo*>& props, size_t filterLen) {
        int count = 0;
        for (const auto* p : props) {
            if (p->type == PropertyBrowser::PropType::Unsupported) continue;
            if (filterLen > 0 && !GuiUtils::MatchesFilter(p->displayName.c_str(), p->displayName.size(), propSearchBuf, filterLen))
                continue;
            ++count;
        }
        return count;
    }

    void RenderCategory(const std::string& name, const std::vector<const PropertyBrowser::PropertyInfo*>& props, size_t filterLen) {
        int visibleCount = CountVisibleInCategory(props, filterLen);
        if (visibleCount == 0) return;

        char label[128];
        std::snprintf(label, sizeof(label), "%s (%d)", name.c_str(), visibleCount);

        if (expandState != 0) ImGui::SetNextItemOpen(expandState > 0);
        bool open = ImGui::TreeNodeEx(label, filterLen > 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        if (!open) return;

        for (const auto* p : props) {
            if (p->type == PropertyBrowser::PropType::Unsupported) continue;
            if (filterLen > 0 && !GuiUtils::MatchesFilter(p->displayName.c_str(), p->displayName.size(), propSearchBuf, filterLen))
                continue;
            if (PropertyBrowser::RenderPropertyWidget(*p, browseTarget) && liveMode)
                pendingApply = true;
        }

        ImGui::TreePop();
    }

public:
    WorldEditorSection() : CollapsibleSection("World Editor") {}
    const char* GetGroup() const noexcept override { return "Settings"; }

    void RenderContent() override {
        const SectionStyle::StyleRAII style;

        if (world != cachedWorld)
            ResetState();

        if (needsScan)
            ScanAllActors();

        char actorLabel[64];
        if (!allActors.empty())
            std::snprintf(actorLabel, sizeof(actorLabel), "Actor (%zu / %zu)", filteredActors.size(), allActors.size());
        else
            std::snprintf(actorLabel, sizeof(actorLabel), "Actor");
        ImGui::SeparatorText(actorLabel);

        for (int i = 0; i < static_cast<int>(std::size(QUICK_FILTERS)); ++i) {
            if (i > 0) ImGui::SameLine();
            bool active = (actorSearchBuf[0] == '\0' && activeQuickFilter == i);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::SmallButton(QUICK_FILTERS[i].label)) {
                activeQuickFilter = i;
                actorSearchBuf[0] = '\0';
                ApplyFilter();
            }
            if (active) ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        if (findPending) ImGui::BeginDisabled();
        if (ImGui::SmallButton("UDS"))
            FindByClassName("Ultra_Dynamic_Sky_C");
        if (findPending) ImGui::EndDisabled();

        float refreshW = ImGui::CalcTextSize("Refresh").x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - refreshW - ImGui::GetStyle().ItemSpacing.x);
        bool enterPressed = ImGui::InputTextWithHint("##ActorSearch", "Custom filter...",
            actorSearchBuf, sizeof(actorSearchBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Refresh") || enterPressed) {
            if (actorSearchBuf[0] != '\0')
                ApplyFilter();
            else
                ScanAllActors();
        }

        if (!filteredActors.empty()) {
            const char* preview = (selectedActorIndex >= 0)
                ? filteredActors[selectedActorIndex].className.c_str() : "Select...";
            ImGui::SetNextItemWidth(actorComboWidth);
            if (ImGui::BeginCombo("##ActorSelector", preview)) {
                for (int i = 0; i < static_cast<int>(filteredActors.size()); ++i) {
                    ImGui::PushID(i);
                    bool sel = (i == selectedActorIndex);
                    if (ImGui::Selectable(filteredActors[i].className.c_str(), sel))
                        SelectActor(i);
                    if (sel) ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            if (browseTarget) {
                ImGui::SameLine();
                if (!liveMode && browseTargetIsComponent) {
                    if (ImGui::Button("Apply"))
                        QueueApply();
                    ImGui::SameLine();
                }
                if (browseTargetIsComponent)
                    ImGui::Checkbox("Live", &liveMode);
            }
        }

        if (!infoText.empty())
            ImGui::TextDisabled("%s", infoText.c_str());
        status.Render();

        if (!browseTarget) return;

        ImGui::SeparatorText("Properties");

        float btnW = ImGui::CalcTextSize("+").x + ImGui::GetStyle().FramePadding.x * 2;
        float btnsW = btnW * 2 + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - btnsW - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputTextWithHint("##PropFilter", "Search properties...", propSearchBuf, sizeof(propSearchBuf));
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(btnW, 0))) expandState = 1;
        if (ImGui::IsItemHovered()) { GuiUtils::BeginStyledTooltip(); ImGui::TextUnformatted("Expand all"); GuiUtils::EndStyledTooltip(); }
        ImGui::SameLine();
        if (ImGui::Button("-", ImVec2(btnW, 0))) expandState = -1;
        if (ImGui::IsItemHovered()) { GuiUtils::BeginStyledTooltip(); ImGui::TextUnformatted("Collapse all"); GuiUtils::EndStyledTooltip(); }

        ImGui::Spacing();

        size_t propFilterLen = std::strlen(propSearchBuf);
        ImGui::BeginChild("##PropertyList", ImVec2(0, 0), ImGuiChildFlags_None);
        for (auto& [catName, catProps] : categories)
            RenderCategory(catName, catProps, propFilterLen);
        expandState = 0;
        ImGui::EndChild();

        if (pendingApply) {
            QueueApply();
            pendingApply = false;
        }
    }
};
