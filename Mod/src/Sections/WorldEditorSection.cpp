#include "Menu/Sections/World/WorldEditorSection.h"
#include "Menu/SectionStyle.h"

#include "Hooks/GameHook.h"
#include "SDK/Willie_BP_classes.hpp"
#include "Utils/ActorUtils.h"
#include "Utils/PresetUtils.h"

#include <cstdio>
#include <cstring>
#include <utility>

namespace {
    constexpr double PICK_TRACE_DISTANCE = 20000.0;

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

    [[nodiscard]] bool IsLiveObject(const SDK::UObject* object) {
        if (!object || !object->Class || object->Index < 0) return false;
        if (SDK::UObject::GObjects->GetByIndex(object->Index) != object) return false;
        return !(object->Flags & SDK::EObjectFlags::BeginDestroyed) &&
               !(object->Flags & SDK::EObjectFlags::FinishDestroyed);
    }

    [[nodiscard]] bool IsLiveActor(SDK::AActor* actor) {
        return IsLiveObject(actor) && !actor->IsActorBeingDestroyed();
    }

    [[nodiscard]] std::string StripDuplicateAssetObjectSuffix(std::string path) {
        const size_t dotPos = path.rfind('.');
        if (dotPos == std::string::npos) return path;

        const size_t leafStart = path.find_last_of("/.", dotPos - 1);
        const size_t packageLeafStart = leafStart == std::string::npos ? 0 : leafStart + 1;
        const std::string packageLeaf = path.substr(packageLeafStart, dotPos - packageLeafStart);
        const std::string objectName = path.substr(dotPos + 1);
        if (objectName == packageLeaf) path.erase(dotPos);
        return path;
    }

    void RenderCopyableObjectRow(
        const char* label, const SDK::UObject* object, GuiUtils::StatusMessage& status, const char* successMessage
    ) {
        if (!IsLiveObject(object)) object = nullptr;

        std::string value = object ? PresetUtils::ObjectToAbsolutePath(object) : "(null)";
        if (object && value.empty()) {
            value = object->GetFullName();
            if (value.empty()) value = object->GetName();
        }
        if (object) value = StripDuplicateAssetObjectSuffix(std::move(value));
        const std::string displayName = object ? object->GetName() : "(empty)";
        const bool canCopy = !value.empty() && value != "(null)";

        ImGui::PushID(label);
        if (!canCopy) ImGui::BeginDisabled();
        const bool clicked = ImGui::SmallButton("Copy");
        if (!canCopy) ImGui::EndDisabled();

        if (ImGui::IsItemHovered()) {
            GuiUtils::BeginStyledTooltip();
            ImGui::TextUnformatted(value.empty() ? "No value" : value.c_str());
            GuiUtils::EndStyledTooltip();
        }
        if (clicked && canCopy) {
            ImGui::SetClipboardText(value.c_str());
            status.Set(successMessage);
        }

        ImGui::SameLine();
        ImGui::TextWrapped("%s: %s", label, displayName.c_str());
        ImGui::PopID();
    }

    SDK::AActor* ActorFromHit(const SDK::FHitResult& hitResult, SDK::USceneComponent*& outComponent) {
        SDK::USceneComponent* component = hitResult.Component.Get();
        outComponent = IsLiveObject(component) ? component : nullptr;
        if (outComponent) return outComponent->GetOwner();

        auto* object = hitResult.HitObjectHandle.ReferenceObject.Get();
        if (!IsLiveObject(object)) return nullptr;
        if (object->IsA(SDK::AActor::StaticClass())) return static_cast<SDK::AActor*>(object);
        if (!object->IsA(SDK::USceneComponent::StaticClass())) return nullptr;

        component = static_cast<SDK::USceneComponent*>(object);
        outComponent = component;
        return component->GetOwner();
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
    selectedActor = nullptr;
    selectedActorLabel.clear();
    highlightMarker = {};
    browseTargets.clear();
    cachedWorld = nullptr;
    ClearBrowseTarget();
    actorComboWidth = 0;
    status = {};
    needsScan = true;
    clickPickActive = false;
    pickPending = false;
}

void WorldEditorSection::ClearBrowseTarget() {
    browseTarget = nullptr;
    browseTargetIsComponent = false;
    browseTargetIsActor = false;
    selectedTargetIndex = -1;
    properties.clear();
    categories.clear();
    visibleCategories.clear();
    visiblePropertyFilter.clear();
    visiblePropertiesReady = false;
    pendingApply = false;
    infoText.clear();
}

void WorldEditorSection::ClearUnavailableActorSelection() {
    selectedActor = nullptr;
    selectedActorIndex = -1;
    selectedActorLabel.clear();
    browseTargets.clear();
    highlightMarker = {};
    ClearBrowseTarget();
    needsScan = true;
    status.Set("Selected actor no longer available", true);
}

void WorldEditorSection::ValidateSelection() {
    if (selectedActor && !IsLiveActor(selectedActor)) {
        ClearUnavailableActorSelection();
        return;
    }

    const bool targetUnavailable =
        browseTarget &&
        (browseTargetIsActor ? !IsLiveActor(static_cast<SDK::AActor*>(browseTarget)) : !IsLiveObject(browseTarget));
    if (targetUnavailable) {
        ClearBrowseTarget();
        needsScan = true;
        status.Set("Selected target no longer available", true);
    }
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

void WorldEditorSection::ApplyFilter() {
    auto* restoreActor = IsLiveActor(selectedActor) ? selectedActor : nullptr;

    filteredActors.clear();
    selectedActorIndex = -1;
    ClearBrowseTarget();
    selectedActor = nullptr;
    selectedActorLabel.clear();
    browseTargets.clear();
    actorComboWidth = 0;

    const char* filter = actorSearchBuf[0] != '\0' ? actorSearchBuf : QUICK_FILTERS[activeQuickFilter].filter;
    size_t filterLen = std::strlen(filter);

    for (auto& wa : allActors) {
        if (!IsLiveActor(wa.actor)) continue;
        if (nearbyMode && (wa.distanceToPlayer < 0.0f || wa.distanceToPlayer > NEARBY_RANGE_METERS)) continue;
        if (filterLen == 0 || GuiUtils::MatchesFilter(wa.className.c_str(), wa.className.size(), filter, filterLen) ||
            GuiUtils::MatchesFilter(wa.instanceName.c_str(), wa.instanceName.size(), filter, filterLen))
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
    if (!IsLiveObject(component)) return;
    for (const auto& target : browseTargets)
        if (target.object == component) return;

    std::string label(static_cast<size_t>(depth) * 2, ' ');
    label += "Component: ";
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
    if (!IsLiveActor(actor)) return;

    auto addActorTarget =
        [this](SDK::AActor* targetActor, const char* prefix, const std::string* classOverride = nullptr) {
            if (!IsLiveActor(targetActor)) return;
            for (const auto& target : browseTargets)
                if (target.object == targetActor) return;

            const std::string targetClassName =
                classOverride && !classOverride->empty()
                    ? *classOverride
                    : (targetActor->Class ? targetActor->Class->GetName() : targetActor->GetName());
            std::string label = std::string(prefix) + ": " + targetClassName;
            const std::string instanceName = targetActor->GetName();
            if (!instanceName.empty() && instanceName != targetClassName) {
                label += " | ";
                label += instanceName;
            }
            browseTargets.push_back({targetActor, std::move(label), true, false});
        };

    addActorTarget(actor, "Actor", &className);

    if (actor->IsA(SDK::AWillie_BP_C::StaticClass())) {
        auto* willie = static_cast<SDK::AWillie_BP_C*>(actor);
        addActorTarget(ActorUtils::GetAIController(willie), "AI Controller");

        auto& targetedBy = willie->Targeted_By_AI;
        for (int32_t i = 0; i < targetedBy.Num(); ++i)
            addActorTarget(targetedBy[i], "Targeted By AI");
    } else if (actor->IsA(SDK::AAI_BP_C::StaticClass())) {
        auto* ai = static_cast<SDK::AAI_BP_C*>(actor);
        addActorTarget(ai->My_Pawn, "Controlled Willie");
        addActorTarget(ai->Target, "Current Target");

        auto& targets = ai->Targets_Array;
        for (int32_t i = 0; i < targets.Num(); ++i)
            addActorTarget(targets[i], "Known Target");
    }

    if (actor->RootComponent) AddSceneComponentTargets(actor->RootComponent, 0);
}

SDK::AActor* WorldEditorSection::SelectedTargetActor() const {
    if (!browseTargetIsActor || !browseTarget) return nullptr;
    auto* actor = static_cast<SDK::AActor*>(browseTarget);
    return IsLiveActor(actor) ? actor : nullptr;
}

void WorldEditorSection::SelectTarget(int index) {
    if (index < 0 || index >= static_cast<int>(browseTargets.size())) return;

    const auto& target = browseTargets[index];
    const bool targetUnavailable =
        target.isActor ? !IsLiveActor(static_cast<SDK::AActor*>(target.object)) : !IsLiveObject(target.object);
    if (targetUnavailable) {
        ClearBrowseTarget();
        needsScan = true;
        status.Set("Selected target no longer available", true);
        return;
    }

    selectedTargetIndex = index;
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
        if (PropertyBrowser::IsEditable(p.type)) ++supported;

    infoText = target.label + " > " + browseTarget->Class->GetName() + " (" + std::to_string(supported) + " editable)";
}

void WorldEditorSection::BrowseActor(SDK::AActor* actor, const std::string& className, SDK::UObject* preferredTarget) {
    if (!IsLiveActor(actor)) {
        ClearUnavailableActorSelection();
        return;
    }

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
    if (targetIndex < 0) {
        targetIndex = 0;
        for (int i = 0; i < static_cast<int>(browseTargets.size()); ++i) {
            if (browseTargets[i].isComponent) {
                targetIndex = i;
                break;
            }
        }
    }
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
    if (!IsLiveActor(actor)) return;
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

void WorldEditorSection::PickClickedActor(ImVec2 screenPos, ImVec2 viewportPos, ImVec2 viewportSize) {
    pickPending = true;
    status.Set("Picking...");

    GameHook::QueueAction([this, screenPos, viewportPos, viewportSize](const RuntimeContextSnapshot& runtime) {
        auto* world = runtime.world;
        auto* controller = runtime.controller;
        if (!world || !controller) {
            status.Set(!world ? "World not available" : "Controller not available", true);
            pickPending = false;
            return;
        }

        int32_t viewportX = 0;
        int32_t viewportY = 0;
        controller->GetViewportSize(&viewportX, &viewportY);
        const float localX = screenPos.x - viewportPos.x;
        const float localY = screenPos.y - viewportPos.y;
        const float screenX = localX * static_cast<float>(viewportX) / viewportSize.x;
        const float screenY = localY * static_cast<float>(viewportY) / viewportSize.y;

        SDK::FVector start{};
        SDK::FVector direction{};
        const bool deprojected = controller->DeprojectScreenPositionToWorld(screenX, screenY, &start, &direction);

        SDK::USceneComponent* component = nullptr;
        SDK::AActor* actor = nullptr;
        if (deprojected) {
            const SDK::FVector end = start + direction * PICK_TRACE_DISTANCE;

            SDK::TArray<SDK::AActor*> actorsToIgnore;
            if (runtime.player) actorsToIgnore.Add(runtime.player);

            SDK::FHitResult hitResult;
            if (SDK::UKismetSystemLibrary::LineTraceSingle(
                    world, start, end, SDK::ETraceTypeQuery::TraceTypeQuery1, false, actorsToIgnore,
                    SDK::EDrawDebugTrace::ForDuration, &hitResult, true, SDK::FLinearColor(0.2f, 0.7f, 1.0f, 1.0f),
                    SDK::FLinearColor(1.0f, 0.9f, 0.1f, 1.0f), 0.5f
                ))
                actor = ActorFromHit(hitResult, component);
        }

        if (!IsLiveActor(actor)) {
            status.Set("No actor under cursor", true);
            pickPending = false;
            return;
        }

        SelectActorDirect(actor, actor->Class->GetName(), component);
        status.Set("Picked: " + selectedActorLabel);
        pickPending = false;
    });
}

void WorldEditorSection::RenderClickPickOverlay() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowFocus();

    constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                                       ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if (ImGui::Begin("##WorldEditorClickPickOverlay", nullptr, FLAGS)) {
        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
        ImGui::InvisibleButton(
            "##WorldEditorClickPickTarget", viewport->Size,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight
        );

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            clickPickActive = false;
            status.Set("Pick cancelled");
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            clickPickActive = false;
            PickClickedActor(ImGui::GetIO().MousePos, viewport->Pos, viewport->Size);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void WorldEditorSection::HighlightSelected() {
    auto* actor = SelectedTargetActor();
    auto* component =
        browseTargetIsComponent && IsLiveObject(browseTarget) && browseTarget->IsA(SDK::USceneComponent::StaticClass())
            ? static_cast<SDK::USceneComponent*>(browseTarget)
            : nullptr;
    if (!actor && component) actor = component->GetOwner();
    if (!actor) actor = selectedActor;
    if (!IsLiveActor(actor)) {
        status.Set("No actor selected", true);
        return;
    }

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
    if (!IsLiveActor(actor)) {
        highlightMarker = {};
        return;
    }

    auto* component = highlightMarker.component;
    if (component && !IsLiveObject(component)) {
        highlightMarker = {};
        return;
    }

    SDK::FVector worldPos = component ? component->K2_GetComponentLocation() : actor->K2_GetActorLocation();

    auto snapshot = RenderSnapshot();
    if (!snapshot.controller) return;

    SDK::FVector2D screen{};
    if (!snapshot.controller->ProjectWorldLocationToScreen(worldPos, &screen, false)) return;

    const auto pos = ImVec2(static_cast<float>(screen.X), static_cast<float>(screen.Y));
    constexpr float RADIUS = 24.0f;
    constexpr ImU32 COLOR = IM_COL32(26, 191, 255, 255);
    auto* dl = ImGui::GetForegroundDrawList();
    dl->AddCircle(pos, RADIUS, COLOR, 48, 3.0f);
    dl->AddLine(ImVec2(pos.x - RADIUS - 8.0f, pos.y), ImVec2(pos.x - 8.0f, pos.y), COLOR, 3.0f);
    dl->AddLine(ImVec2(pos.x + 8.0f, pos.y), ImVec2(pos.x + RADIUS + 8.0f, pos.y), COLOR, 3.0f);
    dl->AddLine(ImVec2(pos.x, pos.y - RADIUS - 8.0f), ImVec2(pos.x, pos.y - 8.0f), COLOR, 3.0f);
    dl->AddLine(ImVec2(pos.x, pos.y + 8.0f), ImVec2(pos.x, pos.y + RADIUS + 8.0f), COLOR, 3.0f);
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
        if (!IsLiveActor(actor)) return;
        GameHook::QueueAction([actor](const RuntimeContextSnapshot&) {
            if (!IsLiveActor(actor)) return;
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
    if (!IsLiveObject(comp)) return;
    bool isSkyLight = browseTarget->IsA(SDK::USkyLightComponent::StaticClass());
    bool hidden = comp->bHiddenInGame;
    bool visible = comp->bVisible;
    auto collision = comp->IsA(SDK::UPrimitiveComponent::StaticClass())
                         ? static_cast<SDK::UPrimitiveComponent*>(comp)->GetCollisionEnabled()
                         : SDK::ECollisionEnabled::QueryAndPhysics;
    GameHook::QueueAction([comp, hidden, visible, collision, isSkyLight](const RuntimeContextSnapshot&) {
        if (!IsLiveObject(comp)) return;
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

void WorldEditorSection::QueueActorState(SDK::AActor* actor, bool hidden, bool collision, bool tickEnabled) {
    if (!IsLiveActor(actor)) return;
    GameHook::QueueAction([actor, hidden, collision, tickEnabled](const RuntimeContextSnapshot&) {
        if (!IsLiveActor(actor)) return;
        actor->SetActorHiddenInGame(hidden);
        actor->SetActorEnableCollision(collision);
        actor->SetActorTickEnabled(tickEnabled);
    });
}

void WorldEditorSection::QueueActorTransform(
    SDK::AActor* actor, const SDK::FVector& location, const SDK::FRotator& rotation, const SDK::FVector& scale
) {
    if (!IsLiveActor(actor)) return;
    GameHook::QueueAction([actor, location, rotation, scale](const RuntimeContextSnapshot&) {
        if (!IsLiveActor(actor)) return;
        actor->SetActorScale3D(scale);
        actor->K2_SetActorLocationAndRotation(location, rotation, false, nullptr, true);
    });
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
            if (!PropertyBrowser::IsVisible(p->type)) continue;
            if (filterLen > 0 &&
                !PropertyBrowser::PropertyMatchesFilter(*p, propSearchBuf, filterLen))
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

    bool wasPickPending = clickPickActive || pickPending;
    if (wasPickPending) ImGui::BeginDisabled();
    if (ImGui::SmallButton("Click Actor")) {
        clickPickActive = true;
        status.Set("Click an actor in the world");
    }
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
        const char* preview = (selectedActorIndex >= 0)
                                  ? filteredActors[selectedActorIndex].displayLabel.c_str()
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

void WorldEditorSection::RenderMaterialEntry(int index, SDK::UMaterialInterface* material) {
    if (!IsLiveObject(material)) material = nullptr;

    ImGui::PushID(index);

    std::string materialName = material ? material->GetName() : "(empty)";
    char label[160];
    std::snprintf(label, sizeof(label), "Slot %d | %s", index, materialName.c_str());
    const bool open = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen);
    if (!open) {
        ImGui::PopID();
        return;
    }

    if (!material) {
        ImGui::TextDisabled("No material");
        ImGui::TreePop();
        ImGui::PopID();
        return;
    }

    RenderCopyableObjectRow("Material", material, status, "Material path copied");

    auto* instance =
        material->IsA(SDK::UMaterialInstance::StaticClass()) ? static_cast<SDK::UMaterialInstance*>(material) : nullptr;
    if (!instance) {
        ImGui::TextDisabled("No material instance texture parameters");
        ImGui::TreePop();
        ImGui::PopID();
        return;
    }

    if (instance->Parent) RenderCopyableObjectRow("Parent", instance->Parent, status, "Parent material path copied");

    auto renderTextureParams = [this](auto& params, const char* fallbackName) {
        int rows = 0;
        for (int i = 0; i < params.Num(); ++i) {
            auto& param = params[i];
            std::string paramName = param.ParameterInfo.Name.ToString();
            if (paramName.empty()) paramName = fallbackName;
            RenderCopyableObjectRow(paramName.c_str(), param.ParameterValue, status, "Texture path copied");
            ++rows;
        }
        return rows;
    };

    const int textureRows =
        renderTextureParams(instance->TextureParameterValues, "Texture") +
        renderTextureParams(instance->RuntimeVirtualTextureParameterValues, "Runtime Virtual Texture") +
        renderTextureParams(instance->SparseVolumeTextureParameterValues, "Sparse Volume Texture");

    if (textureRows == 0) ImGui::TextDisabled("No texture parameters");

    ImGui::TreePop();
    ImGui::PopID();
}

void WorldEditorSection::RenderMaterialInspector() {
    if (browseTarget->IsA(SDK::UDecalComponent::StaticClass())) {
        ImGui::SeparatorText("Materials");
        RenderMaterialEntry(0, static_cast<SDK::UDecalComponent*>(browseTarget)->GetDecalMaterial());
        return;
    }

    if (!browseTarget->IsA(SDK::UPrimitiveComponent::StaticClass())) return;

    ImGui::SeparatorText("Materials");

    auto* primitive = static_cast<SDK::UPrimitiveComponent*>(browseTarget);
    const int materialCount = primitive->GetNumMaterials();
    if (materialCount <= 0) {
        ImGui::TextDisabled("No material slots");
        return;
    }

    for (int i = 0; i < materialCount; ++i)
        RenderMaterialEntry(i, primitive->GetMaterial(i));
}

void WorldEditorSection::RenderTargetControls() {
    ImGui::SeparatorText("Controls");

    if (browseTargetIsActor) {
        RenderActorControls();
        return;
    }

    if (browseTargetIsComponent) RenderComponentControls();
}

void WorldEditorSection::RenderActorControls() {
    auto* actor = SelectedTargetActor();
    if (!IsLiveActor(actor)) return;

    SDK::FVector location = actor->K2_GetActorLocation();
    SDK::FRotator rotation = actor->K2_GetActorRotation();
    SDK::FVector scale = actor->GetActorScale3D();

    if (PropertyBrowser::DragDouble3("World Location", &location.X, 1.0f, "%.1f"))
        QueueActorTransform(actor, location, rotation, scale);
    if (PropertyBrowser::DragDouble3("World Rotation", &rotation.Pitch, 0.5f, "%.1f"))
        QueueActorTransform(actor, location, rotation, scale);
    if (PropertyBrowser::DragDouble3("World Scale", &scale.X, 0.01f, "%.3f"))
        QueueActorTransform(actor, location, rotation, scale);

    bool hidden = actor->bHidden;
    bool collision = actor->GetActorEnableCollision();
    bool tickEnabled = actor->IsActorTickEnabled();

    if (ImGui::Checkbox("Hidden", &hidden)) QueueActorState(actor, hidden, collision, tickEnabled);
    ImGui::SameLine();
    if (ImGui::Checkbox("Collision", &collision)) QueueActorState(actor, hidden, collision, tickEnabled);
    ImGui::SameLine();
    if (ImGui::Checkbox("Tick", &tickEnabled)) QueueActorState(actor, hidden, collision, tickEnabled);

    if (ImGui::SmallButton("Move To Player")) {
        GameHook::QueueAction([actor](const RuntimeContextSnapshot& runtime) {
            if (!IsLiveActor(actor) || !runtime.player) return;
            actor->K2_SetActorLocationAndRotation(
                runtime.player->K2_GetActorLocation(), runtime.player->K2_GetActorRotation(), false, nullptr, true
            );
        });
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Bring Player Here")) {
        GameHook::QueueAction([actor](const RuntimeContextSnapshot& runtime) {
            if (!IsLiveActor(actor) || !runtime.player) return;
            runtime.player->K2_SetActorLocationAndRotation(
                actor->K2_GetActorLocation(), actor->K2_GetActorRotation(), false, nullptr, true
            );
        });
    }
}

void WorldEditorSection::RenderComponentControls() {
    if (!browseTarget->IsA(SDK::USceneComponent::StaticClass())) return;
    auto* comp = static_cast<SDK::USceneComponent*>(browseTarget);
    if (PropertyBrowser::DragDouble3("Relative Location", &comp->RelativeLocation.X, 1.0f, "%.1f")) pendingApply = true;
    if (PropertyBrowser::DragDouble3("Relative Rotation", &comp->RelativeRotation.Pitch, 0.5f, "%.1f"))
        pendingApply = true;
    if (PropertyBrowser::DragDouble3("Relative Scale", &comp->RelativeScale3D.X, 0.01f, "%.3f")) pendingApply = true;

    bool visible = comp->bVisible;
    if (ImGui::Checkbox("Visible", &visible)) {
        GameHook::QueueAction([comp, visible](const RuntimeContextSnapshot&) {
            if (!IsLiveObject(comp)) return;
            comp->SetVisibility(visible, false);
        });
    }
    ImGui::SameLine();
    bool hidden = comp->bHiddenInGame;
    if (ImGui::Checkbox("Hidden In Game", &hidden)) {
        GameHook::QueueAction([comp, hidden](const RuntimeContextSnapshot&) {
            if (!IsLiveObject(comp)) return;
            comp->SetHiddenInGame(hidden, false);
        });
    }

    if (browseTarget->IsA(SDK::UPrimitiveComponent::StaticClass())) {
        auto* prim = static_cast<SDK::UPrimitiveComponent*>(browseTarget);
        auto collision = prim->GetCollisionEnabled();
        static float collisionComboW = GuiUtils::CalcComboWidth("Query + Physics");
        if (GuiUtils::BeginSizedCombo("Collision Mode", CollisionLabel(collision), collisionComboW)) {
            for (int i = 0; i <= static_cast<int>(SDK::ECollisionEnabled::QueryAndProbe); ++i) {
                auto mode = static_cast<SDK::ECollisionEnabled>(i);
                if (ImGui::Selectable(CollisionLabel(mode), mode == collision)) {
                    GameHook::QueueAction([prim, mode](const RuntimeContextSnapshot&) {
                        if (!IsLiveObject(prim)) return;
                        prim->SetCollisionEnabled(mode);
                    });
                }
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
    ValidateSelection();

    const bool renderPickOverlay = clickPickActive;
    RenderActorSelector();
    RenderHighlightMarker();

    if (browseTarget) {
        RenderTargetSelector();
        RenderTargetControls();
        RenderMaterialInspector();
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

    if (renderPickOverlay) RenderClickPickOverlay();
}
