#include "Menu/Sections/World/WorldEditorSection.h"
#include "Menu/SectionStyle.h"

#include "Hooks/GameHook.h"
#include "SDK/Willie_BP_classes.hpp"

#include <cstdio>
#include <cstring>
#include <utility>

namespace {
    const char* CollisionLabel(SDK::ECollisionEnabled mode) noexcept {
        switch (mode) {
            case SDK::ECollisionEnabled::NoCollision: return "No Collision";
            case SDK::ECollisionEnabled::QueryOnly: return "Query Only";
            case SDK::ECollisionEnabled::PhysicsOnly: return "Physics Only";
            case SDK::ECollisionEnabled::QueryAndPhysics: return "Query + Physics";
            case SDK::ECollisionEnabled::ProbeOnly: return "Probe Only";
            case SDK::ECollisionEnabled::QueryAndProbe: return "Query + Probe";
            default: return "Unknown";
        }
    }

    bool SameLineIfRoom(const char* nextLabel) {
        const auto& style = ImGui::GetStyle();
        float width = ImGui::CalcTextSize(nextLabel).x + style.FramePadding.x * 2.0f + style.ItemSpacing.x;
        if (ImGui::GetContentRegionAvail().x < width) return false;
        ImGui::SameLine();
        return true;
    }
} // namespace

WorldEditorSection::WorldEditorSection(ModContext& ctx) : Section(ctx, SECTION) {}

void WorldEditorSection::OnOpen() {
    needsScan = true;
}

void WorldEditorSection::ResetState() {
    allActors.clear();
    filteredActors.clear();
    selectedActorIndex = -1;
    browseTarget = nullptr;
    browseTargetIsComponent = false;
    browseTargetIsActor = false;
    selectedActor = nullptr;
    selectedTargetIndex = -1;
    selectedActorLabel.clear();
    highlightMarker = {};
    browseTargets.clear();
    cachedWorld = nullptr;
    properties.clear();
    categories.clear();
    visibleCategories.clear();
    visiblePropertyFilter.clear();
    visiblePropertiesReady = false;
    actorComboWidth = 0;
    infoText.clear();
    status = {};
    needsScan = true;
    pickPending = false;
}

void WorldEditorSection::ScanAllActors() {
    needsScan = false;
    const auto playerWorld = RenderPlayerWorld();
    auto* world = playerWorld.world;

    if (!world) {
        status.Set("World not available", true);
        return;
    }

    cachedWorld = world;
    allActors = PropertyBrowser::FindWorldActors(world, playerWorld.player);
    ApplyFilter();
}

bool WorldEditorSection::ActorMatchesFilter(
    const PropertyBrowser::WorldActor& actor, const char* filter, size_t filterLen
) const {
    return GuiUtils::MatchesFilter(actor.className.c_str(), actor.className.size(), filter, filterLen) ||
           GuiUtils::MatchesFilter(actor.instanceName.c_str(), actor.instanceName.size(), filter, filterLen);
}

void WorldEditorSection::ApplyFilter() {
    auto* restoreActor = selectedActor;

    filteredActors.clear();
    selectedActorIndex = -1;
    browseTarget = nullptr;
    browseTargetIsComponent = false;
    browseTargetIsActor = false;
    selectedActor = nullptr;
    selectedActorLabel.clear();
    selectedTargetIndex = -1;
    browseTargets.clear();
    properties.clear();
    categories.clear();
    visibleCategories.clear();
    visiblePropertyFilter.clear();
    visiblePropertiesReady = false;
    actorComboWidth = 0;

    const char* filter = actorSearchBuf[0] != '\0' ? actorSearchBuf : QUICK_FILTERS[activeQuickFilter].filter;
    size_t filterLen = std::strlen(filter);

    for (auto& wa : allActors) {
        if (nearbyMode && (wa.distanceToPlayer < 0.0f || wa.distanceToPlayer > NEARBY_RANGE_METERS)) continue;
        if (filterLen == 0 || ActorMatchesFilter(wa, filter, filterLen))
            filteredActors.push_back(wa);
    }

    if (filteredActors.empty()) {
        status.Set(nearbyMode ? "No nearby actors match filter" : "No actors match filter", true);
        return;
    }

    float maxW = 0;
    for (const auto& wa : filteredActors) {
        float w = ImGui::CalcTextSize(wa.displayLabel.c_str()).x;
        if (w > maxW) maxW = w;
    }
    actorComboWidth = GuiUtils::ComboWidthFromText(maxW);

    int restoreIdx = -1;
    if (restoreActor) {
        for (int i = 0; i < static_cast<int>(filteredActors.size()); ++i) {
            if (filteredActors[i].actor == restoreActor) {
                restoreIdx = i;
                break;
            }
        }
    }
    SelectActor(restoreIdx >= 0 ? restoreIdx : 0);
}

void WorldEditorSection::AddSceneComponentTargets(SDK::USceneComponent* component, int depth) {
    if (!component) return;
    for (const auto& target : browseTargets)
        if (target.object == component) return;

    std::string label(static_cast<size_t>(depth) * 2, ' ');
    const std::string className = component->Class ? component->Class->GetName() : component->GetName();
    const std::string instanceName = component->GetName();
    label += className;
    if (!instanceName.empty() && instanceName != className) {
        label += " | ";
        label += instanceName;
    }
    if (selectedActor && component == selectedActor->RootComponent) label += " (root)";

    browseTargets.push_back({component, std::move(label), false, true});

    auto& children = component->AttachChildren;
    for (int32_t i = 0; i < children.Num(); ++i)
        AddSceneComponentTargets(children[i], depth + 1);
}

void WorldEditorSection::BuildBrowseTargets(SDK::AActor* actor, const std::string& className) {
    browseTargets.clear();
    selectedTargetIndex = -1;
    if (!actor) return;

    std::string label = "Actor: " + className;
    const std::string instanceName = actor->GetName();
    if (!instanceName.empty() && instanceName != className) {
        label += " | ";
        label += instanceName;
    }
    browseTargets.push_back({actor, std::move(label), true, false});
    if (actor->RootComponent) AddSceneComponentTargets(actor->RootComponent, 0);
}

void WorldEditorSection::SelectTarget(int index) {
    if (index < 0 || index >= static_cast<int>(browseTargets.size())) return;
    selectedTargetIndex = index;

    const auto& target = browseTargets[index];
    browseTarget = target.object;
    browseTargetIsActor = target.isActor;
    browseTargetIsComponent = target.isComponent;

    properties = PropertyBrowser::EnumerateProperties(browseTarget->Class);
    categories = PropertyBrowser::GroupByCategory(properties);
    visibleCategories.clear();
    visiblePropertyFilter.clear();
    visiblePropertiesReady = false;

    int supported = 0;
    for (const auto& p : properties)
        if (p.type != PropertyBrowser::PropType::Unsupported) ++supported;

    infoText = target.label + " > " + browseTarget->Class->GetName() + " (" + std::to_string(supported) + " editable)";
}

void WorldEditorSection::BrowseActor(SDK::AActor* actor, const std::string& className, SDK::UObject* preferredTarget) {
    selectedActor = actor;
    BuildBrowseTargets(actor, className);

    int targetIndex = -1;
    if (preferredTarget) {
        for (int i = 0; i < static_cast<int>(browseTargets.size()); ++i) {
            if (browseTargets[i].object == preferredTarget) {
                targetIndex = i;
                break;
            }
        }
    }
    if (targetIndex < 0) targetIndex = browseTargets.size() > 1 ? 1 : 0;
    SelectTarget(targetIndex);
}

void WorldEditorSection::SelectActor(int index) {
    if (index < 0 || index >= static_cast<int>(filteredActors.size())) return;
    selectedActorIndex = index;
    selectedActorLabel = filteredActors[index].displayLabel;
    BrowseActor(filteredActors[index].actor, filteredActors[index].className);
}

void WorldEditorSection::SelectActorDirect(
    SDK::AActor* actor, const std::string& className, SDK::UObject* preferredTarget
) {
    if (!actor || !actor->Class) return;
    const std::string resolvedClassName = className.empty() ? actor->Class->GetName() : className;
    selectedActorIndex = -1;
    selectedActorLabel.clear();
    for (int i = 0; i < static_cast<int>(filteredActors.size()); ++i) {
        if (filteredActors[i].actor != actor) continue;
        selectedActorIndex = i;
        selectedActorLabel = filteredActors[i].displayLabel;
        break;
    }

    if (selectedActorLabel.empty())
        selectedActorLabel = PropertyBrowser::BuildActorDisplayLabel(resolvedClassName, actor->GetName(), -1.0f);
    BrowseActor(actor, resolvedClassName, preferredTarget);
}

void WorldEditorSection::PickLookingAt() {
    if (pickPending) return;
    pickPending = true;
    status.Set("Picking...");

    GameHook::QueueAction([this](const RuntimeContextSnapshot& runtime) {
        auto* world = runtime.world;
        if (!world) {
            status.Set("World not available", true);
            pickPending = false;
            return;
        }

        auto* camera = SDK::UGameplayStatics::GetPlayerCameraManager(world, 0);
        if (!camera) {
            status.Set("Camera not available", true);
            pickPending = false;
            return;
        }

        const SDK::FVector start = camera->GetCameraLocation();
        const SDK::FVector end = start + SDK::UKismetMathLibrary::GetForwardVector(camera->GetCameraRotation()) * 20000.0;

        SDK::TArray<SDK::EObjectTypeQuery> objectTypes;
        for (int i = 0; i < static_cast<int>(SDK::EObjectTypeQuery::ObjectTypeQuery_MAX); ++i)
            objectTypes.Add(static_cast<SDK::EObjectTypeQuery>(i));

        SDK::TArray<SDK::AActor*> actorsToIgnore;
        if (runtime.player) actorsToIgnore.Add(runtime.player);

        SDK::FHitResult hitResult;
        bool hit = SDK::UKismetSystemLibrary::LineTraceSingleForObjects(
            world, start, end, objectTypes, true, actorsToIgnore, SDK::EDrawDebugTrace::ForDuration, &hitResult, true,
            SDK::FLinearColor(0.2f, 0.7f, 1.0f, 1.0f), SDK::FLinearColor(1.0f, 0.9f, 0.1f, 1.0f), 0.5f
        );

        auto* component = hit ? hitResult.Component.Get() : nullptr;
        auto* actor = component ? component->GetOwner() : nullptr;
        if (!actor || !actor->Class || actor->IsActorBeingDestroyed()) {
            status.Set("No actor under reticle", true);
            pickPending = false;
            return;
        }

        SelectActorDirect(actor, actor->Class->GetName(), component);
        status.Set("Picked: " + selectedActorLabel);
        pickPending = false;
    });
}

void WorldEditorSection::HighlightSelected() {
    auto* actor = selectedActor;
    if (!actor) {
        status.Set("No actor selected", true);
        return;
    }

    auto* component = browseTargetIsComponent && browseTarget && browseTarget->IsA(SDK::USceneComponent::StaticClass())
                          ? static_cast<SDK::USceneComponent*>(browseTarget)
                          : nullptr;
    highlightMarker = {actor, component, ImGui::GetTime() + 3.0};
    status.Set("Highlighting selection...");
}

void WorldEditorSection::RenderHighlightMarker() {
    if (!highlightMarker.actor && !highlightMarker.component) return;

    const double now = ImGui::GetTime();
    if (now >= highlightMarker.endTime) {
        highlightMarker = {};
        return;
    }

    auto* actor = highlightMarker.actor;
    if (!actor || !actor->Class || actor->IsActorBeingDestroyed()) {
        highlightMarker = {};
        return;
    }

    auto* component = highlightMarker.component;
    SDK::FVector worldPos =
        component ? component->K2_GetComponentLocation() : actor->K2_GetActorLocation();

    auto snapshot = RenderSnapshot();
    if (!snapshot.controller) return;

    SDK::FVector2D screen{};
    if (!snapshot.controller->ProjectWorldLocationToScreen(worldPos, &screen, false)) return;

    const auto pos = ImVec2(static_cast<float>(screen.X), static_cast<float>(screen.Y));
    constexpr float radius = 24.0f;
    constexpr ImU32 color = IM_COL32(26, 191, 255, 255);
    auto* dl = ImGui::GetForegroundDrawList();
    dl->AddCircle(pos, radius, color, 48, 3.0f);
    dl->AddLine(ImVec2(pos.x - radius - 8.0f, pos.y), ImVec2(pos.x - 8.0f, pos.y), color, 3.0f);
    dl->AddLine(ImVec2(pos.x + 8.0f, pos.y), ImVec2(pos.x + radius + 8.0f, pos.y), color, 3.0f);
    dl->AddLine(ImVec2(pos.x, pos.y - radius - 8.0f), ImVec2(pos.x, pos.y - 8.0f), color, 3.0f);
    dl->AddLine(ImVec2(pos.x, pos.y + 8.0f), ImVec2(pos.x, pos.y + radius + 8.0f), color, 3.0f);
}

void WorldEditorSection::FindByClassName(const char* className) {
    if (findPending) return;
    auto* cls = SDK::UObject::FindClassFast(std::string(className));
    if (!cls) {
        status.Set(std::string(className) + " class not found", true);
        return;
    }
    findPending = true;
    status.Set("Searching...");
    std::string searchName = className;
    GameHook::QueueAction([this, cls, searchName](const RuntimeContextSnapshot& runtime) {
        auto* world = runtime.world;
        auto* actor = world ? SDK::UGameplayStatics::GetActorOfClass(world, cls) : nullptr;
        if (actor) {
            selectedActorIndex = -1;
            SelectActorDirect(actor, searchName);
        } else {
            status.Set(searchName + " instance not found", true);
        }
        findPending = false;
    });
}

void WorldEditorSection::QueueApply() {
    if (browseTargetIsActor) {
        auto* actor = static_cast<SDK::AActor*>(browseTarget);
        GameHook::QueueAction([actor](const RuntimeContextSnapshot&) {
            if (!actor || actor->IsActorBeingDestroyed()) return;
            actor->SetActorHiddenInGame(actor->bHidden);
            actor->SetActorEnableCollision(actor->GetActorEnableCollision());
            actor->SetActorScale3D(actor->GetActorScale3D());
            actor->K2_SetActorLocationAndRotation(
                actor->K2_GetActorLocation(), actor->K2_GetActorRotation(), false, nullptr, true
            );
        });
        return;
    }

    if (!browseTargetIsComponent) return;
    auto* comp = static_cast<SDK::USceneComponent*>(browseTarget);
    bool isSkyLight = browseTarget->IsA(SDK::USkyLightComponent::StaticClass());
    bool hidden = comp->bHiddenInGame;
    bool visible = comp->bVisible;
    auto collision = comp->IsA(SDK::UPrimitiveComponent::StaticClass())
                         ? static_cast<SDK::UPrimitiveComponent*>(comp)->GetCollisionEnabled()
                         : SDK::ECollisionEnabled::QueryAndPhysics;
    GameHook::QueueAction([comp, hidden, visible, collision, isSkyLight](const RuntimeContextSnapshot&) {
        comp->SetHiddenInGame(hidden, false);
        comp->SetVisibility(false, false);
        comp->SetVisibility(true, false);
        comp->SetVisibility(visible, false);
        comp->K2_SetRelativeLocationAndRotation(comp->RelativeLocation, comp->RelativeRotation, false, nullptr, true);
        comp->SetRelativeScale3D(comp->RelativeScale3D);
        if (comp->IsA(SDK::UPrimitiveComponent::StaticClass())) {
            auto* prim = static_cast<SDK::UPrimitiveComponent*>(comp);
            prim->SetCollisionEnabled(collision);
        }
        if (isSkyLight) static_cast<SDK::USkyLightComponent*>(comp)->RecaptureSky();
    });
}

void WorldEditorSection::QueueActorState(bool hidden, bool collision, bool tickEnabled) {
    auto* actor = selectedActor;
    GameHook::QueueAction([actor, hidden, collision, tickEnabled](const RuntimeContextSnapshot&) {
        if (!actor || actor->IsActorBeingDestroyed()) return;
        actor->SetActorHiddenInGame(hidden);
        actor->SetActorEnableCollision(collision);
        actor->SetActorTickEnabled(tickEnabled);
    });
}

void WorldEditorSection::QueueComponentCollision(SDK::ECollisionEnabled collision) {
    if (!browseTarget || !browseTarget->IsA(SDK::UPrimitiveComponent::StaticClass())) return;
    auto* prim = static_cast<SDK::UPrimitiveComponent*>(browseTarget);
    GameHook::QueueAction([prim, collision](const RuntimeContextSnapshot&) { prim->SetCollisionEnabled(collision); });
}

int WorldEditorSection::CountVisibleInCategory(
    const std::vector<const PropertyBrowser::PropertyInfo*>& props, size_t filterLen
) {
    int count = 0;
    for (const auto* p : props) {
        if (p->type == PropertyBrowser::PropType::Unsupported) continue;
        if (filterLen > 0 &&
            !GuiUtils::MatchesFilter(p->displayName.c_str(), p->displayName.size(), propSearchBuf, filterLen))
            continue;
        ++count;
    }
    return count;
}

void WorldEditorSection::RenderCategory(
    const std::string& categoryName, const std::vector<const PropertyBrowser::PropertyInfo*>& props, size_t filterLen
) {
    char label[128];
    std::snprintf(label, sizeof(label), "%s (%zu)", categoryName.c_str(), props.size());

    if (expandState != 0) ImGui::SetNextItemOpen(expandState > 0);
    bool open = ImGui::TreeNodeEx(label, filterLen > 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    if (!open) return;

    for (const auto* p : props) {
        if (PropertyBrowser::RenderPropertyWidget(*p, reinterpret_cast<std::byte*>(browseTarget)) && liveMode)
            pendingApply = true;
    }

    ImGui::TreePop();
}

void WorldEditorSection::RebuildVisibleProperties(size_t filterLen) {
    visibleCategories.clear();
    visiblePropertyFilter.assign(propSearchBuf, filterLen);
    visiblePropertiesReady = true;

    for (auto& [catName, catProps] : categories) {
        VisibleCategory visible;
        visible.name = catName;
        visible.props.reserve(catProps.size());
        for (const auto* p : catProps) {
            if (p->type == PropertyBrowser::PropType::Unsupported) continue;
            if (filterLen > 0 &&
                !GuiUtils::MatchesFilter(p->displayName.c_str(), p->displayName.size(), propSearchBuf, filterLen))
                continue;
            visible.props.push_back(p);
        }
        if (!visible.props.empty()) visibleCategories.push_back(std::move(visible));
    }
}

void WorldEditorSection::RenderActorSelector() {
    char actorLabel[64];
    if (!allActors.empty())
        std::snprintf(actorLabel, sizeof(actorLabel), "Actor (%zu / %zu)", filteredActors.size(), allActors.size());
    else
        std::snprintf(actorLabel, sizeof(actorLabel), "Actor");
    ImGui::SeparatorText(actorLabel);

    bool wasPickPending = pickPending;
    if (wasPickPending) ImGui::BeginDisabled();
    if (ImGui::SmallButton("Pick Looking At")) PickLookingAt();
    if (wasPickPending) ImGui::EndDisabled();
    SameLineIfRoom("Highlight");
    if (!selectedActor) ImGui::BeginDisabled();
    if (ImGui::SmallButton("Highlight")) HighlightSelected();
    if (!selectedActor) ImGui::EndDisabled();
    SameLineIfRoom("Nearby");
    bool wasNearbyMode = nearbyMode;
    if (wasNearbyMode) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::SmallButton("Nearby")) {
        nearbyMode = !nearbyMode;
        ScanAllActors();
    }
    if (wasNearbyMode) ImGui::PopStyleColor();

    for (int i = 0; i < static_cast<int>(std::size(QUICK_FILTERS)); ++i) {
        if (i > 0) SameLineIfRoom(QUICK_FILTERS[i].label);
        bool active = (actorSearchBuf[0] == '\0' && activeQuickFilter == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::SmallButton(QUICK_FILTERS[i].label)) {
            activeQuickFilter = i;
            actorSearchBuf[0] = '\0';
            ApplyFilter();
        }
        if (active) ImGui::PopStyleColor();
    }
    SameLineIfRoom("UDS");
    if (findPending) ImGui::BeginDisabled();
    if (ImGui::SmallButton("UDS")) FindByClassName("Ultra_Dynamic_Sky_C");
    if (findPending) ImGui::EndDisabled();

    float refreshW = ImGui::CalcTextSize("Refresh").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - refreshW - ImGui::GetStyle().ItemSpacing.x);
    bool enterPressed = ImGui::InputTextWithHint(
        "##ActorSearch", "Custom filter...", actorSearchBuf, sizeof(actorSearchBuf),
        ImGuiInputTextFlags_EnterReturnsTrue
    );
    ImGui::SameLine();
    if (ImGui::Button("Refresh") || enterPressed) {
        if (actorSearchBuf[0] != '\0')
            ApplyFilter();
        else
            ScanAllActors();
    }

    if (!filteredActors.empty()) {
        const char* preview =
            (selectedActorIndex >= 0) ? filteredActors[selectedActorIndex].displayLabel.c_str()
                                      : (!selectedActorLabel.empty() ? selectedActorLabel.c_str() : "Select...");
        if (GuiUtils::BeginSizedCombo("##ActorSelector", preview, actorComboWidth)) {
            GuiUtils::RenderClippedList(static_cast<int>(filteredActors.size()), selectedActorIndex, [&](int i) {
                ImGui::PushID(i);
                bool sel = (i == selectedActorIndex);
                if (ImGui::Selectable(filteredActors[i].displayLabel.c_str(), sel)) SelectActor(i);
                if (sel) ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            });
            ImGui::EndCombo();
        }
        if (browseTarget) {
            ImGui::SameLine();
            if (!liveMode && browseTargetIsComponent) {
                if (ImGui::Button("Apply")) QueueApply();
                ImGui::SameLine();
            }
            if (browseTargetIsComponent) ImGui::Checkbox("Live", &liveMode);
        }
    }

    if (!infoText.empty()) ImGui::TextDisabled("%s", infoText.c_str());
    status.Render();
}

void WorldEditorSection::RenderTargetSelector() {
    if (browseTargets.empty()) return;

    ImGui::SeparatorText("Target");

    const char* preview = (selectedTargetIndex >= 0 && selectedTargetIndex < static_cast<int>(browseTargets.size()))
                              ? browseTargets[selectedTargetIndex].label.c_str()
                              : "Select...";
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo("##TargetSelector", preview)) {
        for (int i = 0; i < static_cast<int>(browseTargets.size()); ++i) {
            ImGui::PushID(i);
            bool selected = i == selectedTargetIndex;
            if (ImGui::Selectable(browseTargets[i].label.c_str(), selected)) SelectTarget(i);
            if (selected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
}

void WorldEditorSection::RenderTargetControls() {
    if (!browseTarget) return;

    ImGui::SeparatorText("Controls");

    if (browseTargetIsActor) {
        RenderActorControls();
        return;
    }

    if (browseTargetIsComponent) RenderComponentControls();
}

void WorldEditorSection::RenderActorControls() {
    if (!selectedActor) return;

    bool hidden = selectedActor->bHidden;
    bool collision = selectedActor->GetActorEnableCollision();
    bool tickEnabled = selectedActor->IsActorTickEnabled();

    if (ImGui::Checkbox("Hidden", &hidden)) QueueActorState(hidden, collision, tickEnabled);
    ImGui::SameLine();
    if (ImGui::Checkbox("Collision", &collision)) QueueActorState(hidden, collision, tickEnabled);
    ImGui::SameLine();
    if (ImGui::Checkbox("Tick", &tickEnabled)) QueueActorState(hidden, collision, tickEnabled);

    if (ImGui::SmallButton("Move To Player")) {
        auto* actor = selectedActor;
        GameHook::QueueAction([actor](const RuntimeContextSnapshot& runtime) {
            if (!actor || actor->IsActorBeingDestroyed() || !runtime.player) return;
            actor->K2_SetActorLocationAndRotation(
                runtime.player->K2_GetActorLocation(), runtime.player->K2_GetActorRotation(), false, nullptr, true
            );
        });
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Bring Player Here")) {
        auto* actor = selectedActor;
        GameHook::QueueAction([actor](const RuntimeContextSnapshot& runtime) {
            if (!actor || actor->IsActorBeingDestroyed() || !runtime.player) return;
            runtime.player->K2_SetActorLocationAndRotation(
                actor->K2_GetActorLocation(), actor->K2_GetActorRotation(), false, nullptr, true
            );
        });
    }
}

void WorldEditorSection::RenderComponentControls() {
    auto* comp = static_cast<SDK::USceneComponent*>(browseTarget);
    if (PropertyBrowser::DragDouble3("Relative Location", &comp->RelativeLocation.X, 1.0f, "%.1f")) pendingApply = true;
    if (PropertyBrowser::DragDouble3("Relative Rotation", &comp->RelativeRotation.Pitch, 0.5f, "%.1f"))
        pendingApply = true;
    if (PropertyBrowser::DragDouble3("Relative Scale", &comp->RelativeScale3D.X, 0.01f, "%.3f")) pendingApply = true;

    bool visible = comp->bVisible;
    if (ImGui::Checkbox("Visible", &visible)) {
        GameHook::QueueAction([comp, visible](const RuntimeContextSnapshot&) { comp->SetVisibility(visible, false); });
    }
    ImGui::SameLine();
    bool hidden = comp->bHiddenInGame;
    if (ImGui::Checkbox("Hidden In Game", &hidden)) {
        GameHook::QueueAction([comp, hidden](const RuntimeContextSnapshot&) { comp->SetHiddenInGame(hidden, false); });
    }

    if (browseTarget->IsA(SDK::UPrimitiveComponent::StaticClass())) {
        auto* prim = static_cast<SDK::UPrimitiveComponent*>(browseTarget);
        auto collision = prim->GetCollisionEnabled();
        static float collisionComboW = GuiUtils::CalcComboWidth("Query + Physics");
        if (GuiUtils::BeginSizedCombo("Collision Mode", CollisionLabel(collision), collisionComboW)) {
            for (int i = 0; i <= static_cast<int>(SDK::ECollisionEnabled::QueryAndProbe); ++i) {
                auto mode = static_cast<SDK::ECollisionEnabled>(i);
                if (ImGui::Selectable(CollisionLabel(mode), mode == collision)) QueueComponentCollision(mode);
                if (mode == collision) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    if (ImGui::SmallButton("Reset Relative Transform")) {
        comp->RelativeLocation = {};
        comp->RelativeRotation = {};
        comp->RelativeScale3D = {1.0, 1.0, 1.0};
        QueueApply();
    }
}

void WorldEditorSection::RenderPropertyToolbar() {
    ImGui::SeparatorText("Properties");

    float btnW = ImGui::CalcTextSize("+").x + ImGui::GetStyle().FramePadding.x * 2;
    float btnsW = btnW * 2 + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - btnsW - ImGui::GetStyle().ItemSpacing.x);
    if (ImGui::InputTextWithHint("##PropFilter", "Search properties...", propSearchBuf, sizeof(propSearchBuf))) {
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

void WorldEditorSection::Render() {
    const SectionStyle::StyleRAII style;
    auto* world = RenderWorld();

    if (world != cachedWorld) ResetState();
    if (needsScan) ScanAllActors();

    RenderActorSelector();
    RenderHighlightMarker();

    if (!browseTarget) return;

    RenderTargetSelector();
    RenderTargetControls();
    RenderPropertyToolbar();

    ImGui::Spacing();

    size_t propFilterLen = std::strlen(propSearchBuf);
    if (!visiblePropertiesReady || visiblePropertyFilter.size() != propFilterLen ||
        std::memcmp(visiblePropertyFilter.data(), propSearchBuf, propFilterLen) != 0)
        RebuildVisibleProperties(propFilterLen);
    ImGui::BeginChild("##PropertyList", ImVec2(0, 0), ImGuiChildFlags_None);
    for (auto& category : visibleCategories)
        RenderCategory(category.name, category.props, propFilterLen);
    expandState = 0;
    ImGui::EndChild();

    if (pendingApply) {
        QueueApply();
        pendingApply = false;
    }
}
