#include "Menu/Sections/World/WorldEditorSection.h"

#include "Hooks/GameHook.h"
#include "Menu/SectionStyle.h"
#include "SDK/Willie_BP_classes.hpp"
#include "Utils/ActorUtils.h"
#include "Utils/BlueprintRegistry.h"
#include "Utils/PresetUtils.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <utility>

namespace {
    constexpr double PICK_TRACE_DISTANCE = 20000.0;

    constexpr const char* CollisionLabel(SDK::ECollisionEnabled mode) noexcept {
        switch (mode) {
            case SDK::ECollisionEnabled::NoCollision: return "Off";
            case SDK::ECollisionEnabled::QueryOnly: return "Detection Only";
            case SDK::ECollisionEnabled::PhysicsOnly: return "Physical Blocking Only";
            case SDK::ECollisionEnabled::QueryAndPhysics: return "Detection & Physical Blocking";
            case SDK::ECollisionEnabled::ProbeOnly: return "Contact Events Only";
            case SDK::ECollisionEnabled::QueryAndProbe: return "Detection & Contact Events";
            default: return "Unknown";
        }
    }

    [[nodiscard]] std::string FriendlyObjectName(std::string_view rawName) {
        if (rawName.ends_with("_C")) rawName.remove_suffix(2);
        return BlueprintRegistry::CleanDisplayName(rawName);
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

        const size_t leafStart = dotPos == 0 ? std::string::npos : path.find_last_of("/.", dotPos - 1);
        const size_t packageLeafStart = leafStart == std::string::npos ? 0 : leafStart + 1;
        const std::string_view packageLeaf{path.data() + packageLeafStart, dotPos - packageLeafStart};
        const std::string_view objectName{path.data() + dotPos + 1, path.size() - dotPos - 1};
        if (objectName == packageLeaf) path.erase(dotPos);
        return path;
    }

    template <std::size_t N> void RenderCopyableObjectRow(
        const char* label, const SDK::UObject* object, GuiUtils::StatusMessage& resultStatus, ImGuiID& resultSource,
        const char (&successMessage)[N]
    ) {
        if (!IsLiveObject(object)) object = nullptr;

        const std::string displayName = object ? FriendlyObjectName(object->GetName()) : "None";
        const bool canCopy = object != nullptr;

        ImGui::PushID(label);
        ImGui::TextWrapped("%s: %s", label, displayName.c_str());
        (void)GuiUtils::SameLineIfFitsButton("Copy");
        if (!canCopy) ImGui::BeginDisabled();
        const bool clicked = ImGui::SmallButton("Copy");
        const ImGuiID rowId = ImGui::GetItemID();
        const bool hovered = ImGui::IsItemHovered();
        if (!canCopy) ImGui::EndDisabled();

        std::string value;
        if ((clicked || hovered) && object)
            value = StripDuplicateAssetObjectSuffix(PresetUtils::ObjectToAbsolutePath(object));
        if (clicked && !value.empty()) {
            ImGui::SetClipboardText(value.c_str());
            resultSource = rowId;
            resultStatus.Notify(successMessage);
        }
        if (resultSource == rowId) resultStatus.RenderResult();
        if (hovered) {
            GuiUtils::BeginStyledTooltip();
            ImGui::TextUnformatted(value.empty() ? "No value" : value.c_str());
            GuiUtils::EndStyledTooltip();
        }
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
    ++selectionGeneration;
    {
        std::lock_guard lock(selectionResultMutex);
        selectionResults.clear();
    }
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
    status.Clear();
    needsScan = true;
    clickPickActive = false;
    pickPending = false;
    findPending = false;
}

void WorldEditorSection::ClearBrowseTarget() {
    browseTarget = nullptr;
    browseTargetIsComponent = false;
    browseTargetIsActor = false;
    selectedTargetIndex = -1;
    propertyPanel.Clear();
    pendingApply = false;
    infoText.clear();
    copyStatus.Clear();
    copyResultSource = 0;
}

void WorldEditorSection::ClearUnavailableActorSelection() {
    selectedActor = nullptr;
    selectedActorIndex = -1;
    selectedActorLabel.clear();
    browseTargets.clear();
    highlightMarker = {};
    ClearBrowseTarget();
    needsScan = true;
    status.SetError("The selected object is no longer available");
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
        status.SetError("The selected part is no longer available");
    }
}

void WorldEditorSection::ScanAllActors() {
    needsScan = false;
    const auto playerWorld = RenderPlayerWorld();
    auto* world = playerWorld.world;

    if (!world) {
        status.SetError("Enter a map before selecting objects");
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
        status.SetError(nearbyMode ? "No nearby objects match the filter" : "No objects match the filter");
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
    label += "Part: ";
    const std::string className = component->Class ? component->Class->GetName() : component->GetName();
    const std::string instanceName = component->GetName();
    label += FriendlyObjectName(instanceName.empty() ? className : instanceName);
    if (selectedActor && component == selectedActor->RootComponent) label += " (main)";

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
            std::string label = std::string(prefix) + ": " + FriendlyObjectName(targetClassName);
            browseTargets.push_back({targetActor, std::move(label), true, false});
        };

    addActorTarget(actor, "Object", &className);

    if (actor->IsA(SDK::AWillie_BP_C::StaticClass())) {
        auto* willie = static_cast<SDK::AWillie_BP_C*>(actor);
        addActorTarget(ActorUtils::GetAIController(willie), "NPC Behavior");

        auto& targetedBy = willie->Targeted_By_AI;
        for (int32_t i = 0; i < targetedBy.Num(); ++i)
            addActorTarget(targetedBy[i], "Targeted By");
    } else if (actor->IsA(SDK::AAI_BP_C::StaticClass())) {
        auto* ai = static_cast<SDK::AAI_BP_C*>(actor);
        addActorTarget(ai->My_Pawn, "Controlled NPC");
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
        status.SetError("The selected part is no longer available.");
        return;
    }

    selectedTargetIndex = index;
    browseTarget = target.object;
    browseTargetIsActor = target.isActor;
    browseTargetIsComponent = target.isComponent;
    copyStatus.Clear();
    copyResultSource = 0;

    propertyPanel.SetType(browseTarget->Class);
    const int supported = propertyPanel.EditableCount();

    infoText = target.label + " - " + std::to_string(supported) + " settings available";
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

void WorldEditorSection::PublishSelectionResult(SelectionResult result) {
    std::lock_guard lock(selectionResultMutex);
    selectionResults.push_back(std::move(result));
}

void WorldEditorSection::DrainSelectionResults(SDK::UWorld* world) {
    std::vector<SelectionResult> results;
    {
        std::lock_guard lock(selectionResultMutex);
        results.swap(selectionResults);
    }

    for (auto& result : results) {
        if (result.generation != selectionGeneration) continue;
        if (result.operation == SelectionOperation::Pick)
            pickPending = false;
        else
            findPending = false;

        if (!result.error.empty() || result.world != world || !IsLiveActor(result.actor)) {
            if (result.error.empty())
                status.SetError("The selected object is no longer available");
            else
                status.SetError(std::move(result.error));
            continue;
        }

        SelectActorDirect(result.actor, result.className, result.preferredTarget);
        status.Clear();
    }
}

void WorldEditorSection::PickClickedActor(ImVec2 screenPos, ImVec2 viewportPos, ImVec2 viewportSize) {
    if (pickPending || findPending) return;
    pickPending = true;
    status.SetInfo("Choose an object in the world");
    const auto generation = ++selectionGeneration;

    const bool queued = GameHook::QueueAction([this, screenPos, viewportPos, viewportSize,
                                               generation](const RuntimeContextSnapshot& runtime) {
        auto* world = runtime.world;
        auto* controller = runtime.controller;
        if (!world || !controller) {
            PublishSelectionResult({
                .operation = SelectionOperation::Pick,
                .generation = generation,
                .world = world,
                .error = !world ? "Enter a map before selecting objects" : "Player controls are unavailable",
            });
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
            PublishSelectionResult({
                .operation = SelectionOperation::Pick,
                .generation = generation,
                .world = world,
                .error = "No editable object under the cursor",
            });
            return;
        }

        PublishSelectionResult({
            .operation = SelectionOperation::Pick,
            .generation = generation,
            .world = world,
            .actor = actor,
            .preferredTarget = component,
            .className = actor->Class->GetName(),
        });
    });
    if (!queued) {
        pickPending = false;
        status.SetError("Could not start world selection");
    }
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
            status.Clear();
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
        status.SetError("No object selected");
        return;
    }

    highlightMarker = {actor, component, ImGui::GetTime() + 3.0};
    status.Clear();
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
    if (findPending || pickPending) return;
    auto* cls = SDK::UObject::FindClassFast(std::string(className));
    if (!cls) {
        status.SetError("This type of object is unavailable in the current map.");
        return;
    }
    findPending = true;
    status.SetInfo("Finding object...");
    const auto generation = ++selectionGeneration;
    std::string searchName = className;
    const bool queued =
        GameHook::QueueAction([this, cls, searchName, generation](const RuntimeContextSnapshot& runtime) {
            auto* world = runtime.world;
            auto* actor = world ? SDK::UGameplayStatics::GetActorOfClass(world, cls) : nullptr;
            if (actor) {
                PublishSelectionResult({
                    .operation = SelectionOperation::Find,
                    .generation = generation,
                    .world = world,
                    .actor = actor,
                    .className = searchName,
                });
            } else {
                PublishSelectionResult({
                    .operation = SelectionOperation::Find,
                    .generation = generation,
                    .world = world,
                    .error = "No matching object was found in the current map.",
                });
            }
        });
    if (!queued) {
        findPending = false;
        status.SetError("Could not find objects");
    }
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
    GameHook::QueueAction([actor, location, rotation, scale](const RuntimeContextSnapshot&) {
        if (!IsLiveActor(actor)) return;
        actor->SetActorScale3D(scale);
        actor->K2_SetActorLocationAndRotation(location, rotation, false, nullptr, true);
    });
}

void WorldEditorSection::RenderActorSelector() {
    char actorLabel[64];
    if (!allActors.empty())
        std::snprintf(actorLabel, sizeof(actorLabel), "Objects (%zu / %zu)", filteredActors.size(), allActors.size());
    else
        std::snprintf(actorLabel, sizeof(actorLabel), "Objects");
    ImGui::SeparatorText(actorLabel);

    bool wasPickPending = clickPickActive || pickPending;
    if (wasPickPending) ImGui::BeginDisabled();
    if (ImGui::SmallButton("Select from World")) {
        clickPickActive = true;
        status.SetInfo("Click an object in the world");
    }
    if (wasPickPending) ImGui::EndDisabled();
    (void)GuiUtils::SameLineIfFitsButton("Show Selection");
    if (!selectedActor) ImGui::BeginDisabled();
    if (ImGui::SmallButton("Show Selection")) HighlightSelected();
    if (!selectedActor) ImGui::EndDisabled();
    (void)GuiUtils::SameLineIfFitsButton("Nearby Only");
    bool wasNearbyMode = nearbyMode;
    if (wasNearbyMode) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::SmallButton("Nearby Only")) {
        nearbyMode = !nearbyMode;
        ScanAllActors();
    }
    if (wasNearbyMode) ImGui::PopStyleColor();

    for (int i = 0; i < static_cast<int>(std::size(QUICK_FILTERS)); ++i) {
        if (i > 0) (void)GuiUtils::SameLineIfFitsButton(QUICK_FILTERS[i].label);
        bool active = (actorSearchBuf[0] == '\0' && activeQuickFilter == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::SmallButton(QUICK_FILTERS[i].label)) {
            activeQuickFilter = i;
            actorSearchBuf[0] = '\0';
            ApplyFilter();
        }
        if (active) ImGui::PopStyleColor();
    }
    (void)GuiUtils::SameLineIfFitsButton("Select Sky");
    if (findPending) ImGui::BeginDisabled();
    if (ImGui::SmallButton("Select Sky")) FindByClassName("Ultra_Dynamic_Sky_C");
    if (findPending) ImGui::EndDisabled();

    const auto& style = ImGui::GetStyle();
    const float refreshWidth = GuiUtils::ButtonNaturalWidth("Find Objects");
    const float searchWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x - refreshWidth - style.ItemSpacing.x);
    GuiUtils::SetNextInputWidth(searchWidth);
    bool enterPressed = ImGui::InputTextWithHint(
        "##ActorSearch", "Search objects...", actorSearchBuf, sizeof(actorSearchBuf),
        ImGuiInputTextFlags_EnterReturnsTrue
    );
    ImGui::SameLine();
    if (ImGui::Button("Find Objects") || enterPressed) {
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
            if (!liveMode && browseTargetIsComponent) {
                (void)GuiUtils::SameLineIfFitsButton("Update Object");
                if (ImGui::Button("Update Object")) QueueApply();
            }
            if (browseTargetIsComponent) {
                (void)GuiUtils::SameLineIfFitsCheckbox("Update Instantly");
                ImGui::Checkbox("Update Instantly", &liveMode);
            }
        }
    }

    if (!infoText.empty()) GuiUtils::TextDisabledWrapped(infoText);
    status.Render();
}

void WorldEditorSection::RenderTargetSelector() {
    if (browseTargets.empty()) return;

    ImGui::SeparatorText("Selected Part");

    const char* preview = (selectedTargetIndex >= 0 && selectedTargetIndex < static_cast<int>(browseTargets.size()))
                              ? browseTargets[selectedTargetIndex].label.c_str()
                              : "Select...";
    if (GuiUtils::BeginSizedCombo(
            "##TargetSelector", preview, {GuiUtils::K_COMBO_MIN_WIDTH, 0.0f, SectionStyle::FIELD_MAX_WIDTH}
        )) {
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

    std::string materialName = material ? FriendlyObjectName(material->GetName()) : "None";
    char label[160];
    std::snprintf(label, sizeof(label), "Material %d | %s", index + 1, materialName.c_str());
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

    RenderCopyableObjectRow(
        "Material", material, copyStatus, copyResultSource, "Material location copied."
    );

    auto* instance =
        material->IsA(SDK::UMaterialInstance::StaticClass()) ? static_cast<SDK::UMaterialInstance*>(material) : nullptr;
    if (!instance) {
        ImGui::TextDisabled("This material has no replaceable textures.");
        ImGui::TreePop();
        ImGui::PopID();
        return;
    }

    if (instance->Parent)
        RenderCopyableObjectRow(
            "Base Material", instance->Parent, copyStatus, copyResultSource, "Base material location copied."
        );

    auto renderTextureParams = [this](auto& params, const char* fallbackName) {
        int rows = 0;
        for (int i = 0; i < params.Num(); ++i) {
            auto& param = params[i];
            std::string paramName = param.ParameterInfo.Name.ToString();
            if (paramName.empty()) paramName = fallbackName;
            ImGui::PushID(static_cast<const void*>(&param));
            RenderCopyableObjectRow(
                paramName.c_str(), param.ParameterValue, copyStatus, copyResultSource, "Texture location copied."
            );
            ImGui::PopID();
            ++rows;
        }
        return rows;
    };

    const int textureRows = renderTextureParams(instance->TextureParameterValues, "Texture") +
                            renderTextureParams(instance->RuntimeVirtualTextureParameterValues, "Terrain Texture") +
                            renderTextureParams(instance->SparseVolumeTextureParameterValues, "Volume Texture");

    if (textureRows == 0) ImGui::TextDisabled("This material has no replaceable textures.");

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
        ImGui::TextDisabled("This part has no materials.");
        return;
    }

    for (int i = 0; i < materialCount; ++i)
        RenderMaterialEntry(i, primitive->GetMaterial(i));
}

void WorldEditorSection::RenderActorControls() {
    auto* actor = SelectedTargetActor();
    if (!actor) return;

    SDK::FVector location = actor->K2_GetActorLocation();
    SDK::FRotator rotation = actor->K2_GetActorRotation();
    SDK::FVector scale = actor->GetActorScale3D();

    if (PropertyBrowser::DragDouble3("Position", &location.X, 1.0f, "%.1f"))
        QueueActorTransform(actor, location, rotation, scale);
    if (PropertyBrowser::DragDouble3("Rotation", &rotation.Pitch, 0.5f, "%.1f"))
        QueueActorTransform(actor, location, rotation, scale);
    if (PropertyBrowser::DragDouble3("Scale", &scale.X, 0.01f, "%.3f"))
        QueueActorTransform(actor, location, rotation, scale);

    bool hidden = actor->bHidden;
    bool collision = actor->GetActorEnableCollision();
    bool tickEnabled = actor->IsActorTickEnabled();

    if (ImGui::Checkbox("Hidden", &hidden)) QueueActorState(actor, hidden, collision, tickEnabled);
    (void)GuiUtils::SameLineIfFitsCheckbox("Collision");
    if (ImGui::Checkbox("Collision", &collision)) QueueActorState(actor, hidden, collision, tickEnabled);
    (void)GuiUtils::SameLineIfFitsCheckbox("Behavior Active");
    if (ImGui::Checkbox("Behavior Active", &tickEnabled)) QueueActorState(actor, hidden, collision, tickEnabled);

    if (ImGui::SmallButton("Move To Player")) {
        GameHook::QueueAction([actor](const RuntimeContextSnapshot& runtime) {
            if (!IsLiveActor(actor) || !runtime.player) return;
            actor->K2_SetActorLocationAndRotation(
                runtime.player->K2_GetActorLocation(), runtime.player->K2_GetActorRotation(), false, nullptr, true
            );
        });
    }
    (void)GuiUtils::SameLineIfFitsButton("Bring Player Here");
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
    auto* comp = static_cast<SDK::USceneComponent*>(browseTarget);
    if (PropertyBrowser::DragDouble3("Position Within Object", &comp->RelativeLocation.X, 1.0f, "%.1f"))
        pendingApply = true;
    if (PropertyBrowser::DragDouble3("Rotation Within Object", &comp->RelativeRotation.Pitch, 0.5f, "%.1f"))
        pendingApply = true;
    if (PropertyBrowser::DragDouble3("Scale Within Object", &comp->RelativeScale3D.X, 0.01f, "%.3f"))
        pendingApply = true;

    bool visible = comp->bVisible;
    if (ImGui::Checkbox("Visible", &visible)) {
        GameHook::QueueAction([comp, visible](const RuntimeContextSnapshot&) {
            if (!IsLiveObject(comp)) return;
            comp->SetVisibility(visible, false);
        });
    }
    (void)GuiUtils::SameLineIfFitsCheckbox("Hidden During Play");
    bool hidden = comp->bHiddenInGame;
    if (ImGui::Checkbox("Hidden During Play", &hidden)) {
        GameHook::QueueAction([comp, hidden](const RuntimeContextSnapshot&) {
            if (!IsLiveObject(comp)) return;
            comp->SetHiddenInGame(hidden, false);
        });
    }

    if (browseTarget->IsA(SDK::UPrimitiveComponent::StaticClass())) {
        auto* prim = static_cast<SDK::UPrimitiveComponent*>(browseTarget);
        auto collision = prim->GetCollisionEnabled();
        static float collisionComboW = GuiUtils::CalcComboWidth("Detection & Physical Blocking");
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

    if (ImGui::SmallButton("Reset Position, Rotation, and Scale")) {
        comp->RelativeLocation = {};
        comp->RelativeRotation = {};
        comp->RelativeScale3D = {1.0, 1.0, 1.0};
        QueueApply();
    }
}

void WorldEditorSection::Render() {
    auto* world = RenderWorld();

    if (world != cachedWorld) ResetState();
    DrainSelectionResults(world);
    if (needsScan) ScanAllActors();
    ValidateSelection();

    const bool renderPickOverlay = clickPickActive;
    RenderActorSelector();
    RenderHighlightMarker();

    if (browseTarget) {
        RenderTargetSelector();
        ImGui::SeparatorText("Object Controls");
        if (browseTargetIsActor)
            RenderActorControls();
        else if (browseTargetIsComponent)
            RenderComponentControls();
        RenderMaterialInspector();
        PropertyBrowser::RenderPanel(
            propertyPanel, reinterpret_cast<std::byte*>(browseTarget), "##PropFilter", "##PropertyList",
            [this] {
                if (liveMode) pendingApply = true;
            },
            false, true
        );

        if (pendingApply) {
            QueueApply();
            pendingApply = false;
        }
    }

    if (renderPickOverlay) RenderClickPickOverlay();
}
