#include "Menu/Sections/World/MapLoaderSection.h"
#include "Hooks/GameHook.h"

#include "Utils/MapRegistry.h"
#include "Utils/EquipmentApplication.h"
#include "Utils/GuiUtils.h"
#include "Utils/GameClass.h"
#include "Utils/PresetApplication.h"
#include "Utils/PresetLinkResolution.h"
#include "Utils/PresetUtils.h"
#include "Utils/Spawner.h"
#include "Utils/SpawnWorkflow.h"
#include "Utils/GameConstants.h"
#include "SDK/GI_Settings_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"

#include <algorithm>
#include <cstdio>
#include <numbers>
#include <string_view>

namespace {
    constexpr auto AUTO_SPAWN_TIMEOUT = std::chrono::seconds(60);

    [[nodiscard]] std::string FriendlyMapName(const MapRegistry& registry, std::string_view name) {
        const auto separator = name.rfind('/');
        const auto leaf = name.substr(separator == std::string_view::npos ? 0 : separator + 1);
        for (const auto& map : registry.GetMaps()) {
            const auto mapSeparator = map.packageName.rfind('/');
            const auto mapLeaf = std::string_view(map.packageName).substr(
                mapSeparator == std::string::npos ? 0 : mapSeparator + 1
            );
            if (map.packageName == name || mapLeaf == leaf) return map.displayName;
        }

        std::string result(leaf);
        std::ranges::replace(result, '_', ' ');
        return result;
    }

    struct AutoSpawnNpcBatch {
        int remaining = 0;
        int succeeded = 0;
        std::function<void(int)> onComplete;

        void FinishOne(bool success) {
            if (remaining <= 0) return;
            if (success) ++succeeded;
            if (--remaining != 0 || !onComplete) return;
            auto completion = std::move(onComplete);
            completion(succeeded);
        }
    };

}

void MapLoaderSection::RefreshLevelName() {
    auto* world = RenderWorld();

    {
        std::lock_guard lock(levelNameMutex);
        if (pendingLevelName) {
            if (pendingLevelName->world == world) {
                cachedLevelName = std::move(pendingLevelName->name);
                observedLevelWorld = world;
            } else {
                observedLevelWorld = nullptr;
            }
            pendingLevelName.reset();
        }
    }

    if (!world) {
        observedLevelWorld = nullptr;
        cachedLevelName.clear();
        return;
    }

    if (world == observedLevelWorld) return;
    cachedLevelName.clear();
    if (levelNameRequestPending.exchange(true, std::memory_order_acq_rel)) return;
    observedLevelWorld = world;

    if (GameHook::QueueAction([this](const RuntimeContextSnapshot& runtime) {
            LevelNameResult result{.world = runtime.world};
            if (runtime.world) result.name = runtime.world->GetName();
            {
                std::lock_guard lock(levelNameMutex);
                pendingLevelName = std::move(result);
            }
            levelNameRequestPending.store(false, std::memory_order_release);
        }))
        return;

    observedLevelWorld = nullptr;
    levelNameRequestPending.store(false, std::memory_order_release);
}

void MapLoaderSection::RebuildFilter(MapRegistry& reg) {
    if (!filterDirty) return;
    filterDirty = false;

    const auto& maps = reg.GetMaps();
    const auto& cats = reg.GetCategories();
    filteredIndices.clear();

    const size_t filterLen = std::strlen(searchBuffer);
    const bool hasCategory = selectedCategoryIndex > 0 && selectedCategoryIndex <= static_cast<int>(cats.size());
    const char* selectedCat = hasCategory ? cats[selectedCategoryIndex - 1].c_str() : nullptr;

    const bool hasFilter = filterLen > 0;
    const bool unfiltered = !hasCategory && !hasFilter;

    if (unfiltered) {
        filteredIndices.resize(static_cast<int>(maps.size()));
        for (int i = 0; i < static_cast<int>(maps.size()); ++i)
            filteredIndices[i] = i;
        cachedComboW = GuiUtils::ComboWidthFromText(reg.GetMaxDisplayNameWidth());
    } else {
        float maxW = 0;
        for (int i = 0; i < static_cast<int>(maps.size()); ++i) {
            const auto& map = maps[i];
            if (hasCategory && map.category != selectedCat) continue;
            if (hasFilter &&
                !GuiUtils::MatchesFilter(map.displayName.c_str(), map.displayName.size(), searchBuffer, filterLen))
                continue;
            filteredIndices.push_back(i);
            float w = ImGui::CalcTextSize(map.displayName.c_str()).x;
            if (w > maxW) maxW = w;
        }
        cachedComboW = GuiUtils::ComboWidthFromText(maxW);
    }

    selectedFilteredIndex = 0;
}

void MapLoaderSection::LoadMap(std::string_view packageName, ResultTarget target) {
    const auto generation = BeginAction(target);
    auto scenario = BuildScenarioPreset(std::string(packageName));
    const bool queued = GameHook::QueueAction(
        [this, scenario = std::move(scenario), target, generation](const RuntimeContextSnapshot& runtime) {
            if (!IsActionCurrent(target, generation)) return;

            std::string error;
            if (RunScenarioImpl(runtime, scenario, target, generation, error)) return;
            FinishAutoSpawnAttempt(
                generation, error.empty() ? "The scenario could not be started." : std::move(error)
            );
        }
    );
    if (!queued) SetActionError(target, generation, "Could not open the selected map");
}

bool MapLoaderSection::RunScenarioImpl(
    const RuntimeContextSnapshot& runtime, const MapScenarioPresetData& scenario, ResultTarget target,
    std::uint64_t generation, std::string& error
) {
    if (!runtime.world) {
        error = "The current map is no longer available.";
        return false;
    }
    if (auto validation = scenario.ValidateForSave(); !validation) {
        error = "This scenario is incomplete: " + validation.error;
        return false;
    }

    std::wstring wideName;
    if (!PresetUtils::TryUtf8ToWide(scenario.packageName, wideName)) {
        error = "The destination path is not valid.";
        return false;
    }
    if (!runtime.controller || !runtime.controller->Class->GetFunction("PlayerController", "ClientTravel")) {
        error = "The game is not ready to open another map.";
        return false;
    }

    const auto& options = scenario.preLoad;
    auto* gameInstance = runtime.world->OwningGameInstance;
    if (!GameClass::IsGameSettings(gameInstance)) {
        error = "A new session cannot be started from this map.";
        return false;
    }
    auto* gi = static_cast<SDK::UGI_Settings_C*>(gameInstance);

    std::shared_ptr<const PreparedAutoSpawn> prepared;
    if (scenario.autoSpawn.enabled) {
        std::string prepareError;
        prepared = PrepareAutoSpawn(scenario.autoSpawn, prepareError);
        if (!prepared) {
            error = std::move(prepareError);
            return false;
        }
    }

    SDK::UClass* playerClass = nullptr;
    if (scenario.autoSpawn.enabled) {
        playerClass = runtime.player ? runtime.player->Class : nullptr;
        if (playerClass && !GameClass::IsWillieClass(playerClass)) playerClass = nullptr;
    }

    const bool needsAutoSpawn = static_cast<bool>(prepared);
    {
        std::lock_guard lock(autoSpawnMutex);
        if (generation != autoSpawnGeneration || latestActionGeneration != generation ||
            latestActionTarget != target)
            return false;
        if (prepared)
            pendingAutoSpawn = PendingAutoSpawn{
                .prepared = std::move(prepared),
                .sourceWorld = runtime.world,
                .playerClass = playerClass,
                .deadline = std::chrono::steady_clock::now() + AUTO_SPAWN_TIMEOUT,
                .generation = generation,
            };
    }
    if (needsAutoSpawn) StartAutoSpawnSubscription();

    gi->Fresh_Start_Map__Temp_ = options.freshStart;
    gi->Tutorial_Enabled = options.tutorial;
    gi->Free_Mode_Activated = options.freeMode;
    gi->Free_Mode_Carnage = options.carnage;
    gi->Free_Mode_Foes_Amount = options.foesAmount;
    gi->Free_Mode_Tier = static_cast<SDK::Enum_Ranks>(options.foeTier);
    gi->Combatants_Amount = options.combatantsAmount;
    gi->Current_Opponent_TIer_to_Spawn = static_cast<SDK::Enum_Ranks>(options.opponentTier);

    runtime.controller
        ->ClientTravel(SDK::FString(wideName.c_str()), SDK::ETravelType::TRAVEL_Absolute, false, SDK::FGuid{});
    error.clear();
    return true;
}

std::shared_ptr<const MapLoaderSection::PreparedAutoSpawn> MapLoaderSection::PrepareAutoSpawn(
    const MapScenarioPresetData::AutoSpawnOptions& options, std::string& error
) {
    const auto& appDataRoot = ConfigManager::GetAppDataPath();
    PresetResolveContext context;
    const auto failure = [&error](std::string_view component, const auto& cause) {
        error = std::string(component) + ": " +
                (cause.error.empty() ? "could not be loaded" : PresetLinkResolution::FormatDiagnostic(cause));
        return std::shared_ptr<const PreparedAutoSpawn>{};
    };

    if (options.enabled && options.npcCount > 0 && IsEmptyPresetLink(options.npcPreset)) {
        error = "Choose an NPC setup before adding NPCs.";
        return {};
    }

    PreparedAutoSpawn prepared;
    if (!IsEmptyPresetLink(options.playerPreset)) {
        auto player = PresetLinkResolution::Resolve<PlayerPresetSerializer>(options.playerPreset, appDataRoot, context);
        if (!player.success || !player.value) return failure("Player preset", player);
        prepared.player = std::move(*player.value);
    }

    if (!IsEmptyPresetLink(options.loadoutPreset)) {
        auto loadout = LoadoutPresetResolver{appDataRoot}.Resolve(options.loadoutPreset, context);
        if (!loadout.success || !loadout.value) return failure("Loadout preset", loadout);
        prepared.loadout = std::move(*loadout.value);
    }

    if (!IsEmptyPresetLink(options.npcPreset) && options.npcCount > 0) {
        auto npc = NPCPresetResolver(appDataRoot).Resolve(options.npcPreset, context);
        if (!npc.success || !npc.value) return failure("NPC preset", npc);
        prepared.npc = std::move(npc.value->preset);
        prepared.npcLoadout = std::move(npc.value->loadout);
        prepared.npcCount = options.npcCount;
    }

    error.clear();
    return std::make_shared<PreparedAutoSpawn>(std::move(prepared));
}

bool MapLoaderSection::IsAutoSpawnAttemptCurrent(std::uint64_t generation) {
    std::lock_guard lock(autoSpawnMutex);
    return generation == autoSpawnGeneration;
}

std::uint64_t MapLoaderSection::BeginAction(ResultTarget target) {
    std::uint64_t generation = 0;
    {
        std::lock_guard lock(autoSpawnMutex);
        generation = ++autoSpawnGeneration;
        latestActionGeneration = generation;
        latestActionTarget = target;
        pendingAutoSpawn.reset();
        autoSpawnFeedback.reset();
        autoSpawnSubscriptions.Clear();
    }
    actionStatusTarget = target;
    actionStatus.ClearText();
    actionStatusToken = actionStatus.revision;
    return generation;
}

bool MapLoaderSection::IsActionCurrent(ResultTarget target, std::uint64_t generation) {
    std::lock_guard lock(autoSpawnMutex);
    return latestActionGeneration == generation && latestActionTarget == target;
}

void MapLoaderSection::FinishAutoSpawnAttempt(std::uint64_t generation, std::string error) {
    std::lock_guard lock(autoSpawnMutex);
    if (generation != autoSpawnGeneration) return;
    if (pendingAutoSpawn && pendingAutoSpawn->generation == generation) pendingAutoSpawn.reset();
    ++autoSpawnGeneration;
    autoSpawnFeedback = AutoSpawnFeedback{std::move(error), latestActionTarget, generation};
    autoSpawnSubscriptions.Clear();
}

void MapLoaderSection::SetActionError(ResultTarget target, std::uint64_t generation, std::string error) {
    if (!IsActionCurrent(target, generation)) return;
    actionStatusTarget = target;
    actionStatus.SetError(std::move(error));
    actionStatusToken = actionStatus.revision;
}

void MapLoaderSection::RenderActionStatus(ResultTarget target) {
    if (actionStatusTarget != target) return;
    actionStatus.Render();
}

void MapLoaderSection::FlushAutoSpawnFeedback() {
    std::optional<AutoSpawnFeedback> feedback;
    {
        std::lock_guard lock(autoSpawnMutex);
        feedback = std::move(autoSpawnFeedback);
        autoSpawnFeedback.reset();
    }
    if (!feedback || !IsActionCurrent(feedback->target, feedback->generation)) return;

    actionStatusTarget = feedback->target;
    if (feedback->error.empty())
        actionStatus.ClearText(actionStatusToken);
    else {
        actionStatus.SetError(std::move(feedback->error));
        actionStatusToken = actionStatus.revision;
    }
}

void MapLoaderSection::SpawnAutoNPCs(
    SDK::UWorld* w, SDK::AWillie_BP_C* willie, const NPCPresetData& npcPreset,
    const std::optional<ResolvedLoadoutPresetData>& npcLoadout, int npcCount, std::function<void(int)> onComplete
) {
    if (npcCount <= 0) {
        onComplete(0);
        return;
    }

    auto baseRequestResult = SpawnWorkflow::BuildNPCSpawnParams(npcPreset, npcLoadout);
    if (!baseRequestResult) {
        onComplete(0);
        return;
    }
    auto request = std::move(*baseRequestResult);
    auto batch = std::make_shared<AutoSpawnNpcBatch>();
    batch->remaining = npcCount;
    batch->onComplete = std::move(onComplete);
    request.onComplete = [batch](const SpawnWorkflow::SpawnResult& result) { batch->FinishOne(result.success); };
    const SDK::FTransform baseTransform = willie->GetTransform();
    const auto forward = willie->GetActorForwardVector();
    const auto forwardDistance = static_cast<float>(npcPreset.spawnDistanceForward);
    const auto upDistance = static_cast<float>(npcPreset.spawnDistanceUp);
    const float angleStep = 2.0f * std::numbers::pi_v<float> / static_cast<float>(npcCount);
    const double scale = npcPreset.spawnScale;
    const int playerTeam = willie->Team_Int;
    const bool snapToGround = npcPreset.snapToGround;
    for (int n = 0; n < npcCount; ++n) {
        const auto index = static_cast<float>(n);
        const float angle = angleStep * index;
        const float radialDistance = 100.0f * index;
        SDK::FTransform npcTransform = baseTransform;
        npcTransform.Translation.X += forward.X * forwardDistance + std::cos(angle) * radialDistance;
        npcTransform.Translation.Y += forward.Y * forwardDistance + std::sin(angle) * radialDistance;
        npcTransform.Translation.Z += upDistance;
        npcTransform.Scale3D = {scale, scale, scale};

        (void)SpawnWorkflow::SpawnNPCAt(w, playerTeam, npcTransform, snapToGround, request);
    }
}

void MapLoaderSection::ApplyPreparedAutoSpawn(
    SDK::UWorld* world, SDK::AWillie_BP_C* willie, const std::shared_ptr<const PreparedAutoSpawn>& prepared,
    std::uint64_t generation
) {
    if (!IsAutoSpawnAttemptCurrent(generation)) return;
    if (!prepared || !world || !willie) {
        FinishAutoSpawnAttempt(generation, "The destination is no longer available");
        return;
    }

    const bool playerSucceeded =
        !prepared->player || PresetApplication::ApplyPlayerOverrides(willie, prepared->player->overrides);
    auto spawnNpcs = [this, world, willie, prepared, generation, playerSucceeded](bool loadoutSucceeded) {
        if (!IsAutoSpawnAttemptCurrent(generation)) return;

        const auto complete = [this, prepared, generation, playerSucceeded, loadoutSucceeded](int succeeded) {
            if (!IsAutoSpawnAttemptCurrent(generation)) return;
            const bool partial =
                !playerSucceeded || !loadoutSucceeded || succeeded < prepared->npcCount;
            if (!partial) {
                FinishAutoSpawnAttempt(generation);
                return;
            }
            std::string message;
            if (!playerSucceeded) message += "The player appearance could not be applied. ";
            if (!loadoutSucceeded) message += "The player equipment could not be equipped. ";
            if (prepared->npc && prepared->npcCount > 0) {
                message += "Added " + std::to_string(succeeded) + "/" + std::to_string(prepared->npcCount) + " NPCs.";
            } else {
                message += "Some starting characters could not be added.";
            }
            FinishAutoSpawnAttempt(generation, std::move(message));
        };

        if (!prepared->npc || prepared->npcCount <= 0) {
            complete(0);
            return;
        }
        if (!world || !willie) {
            complete(0);
            return;
        }
        SpawnAutoNPCs(world, willie, *prepared->npc, prepared->npcLoadout, prepared->npcCount, std::move(complete));
    };

    if (!prepared->loadout) {
        spawnNpcs(true);
        return;
    }

    std::string error;
    const bool started = EquipmentApplication::ApplyPlayerLoadout(
        world, willie, *prepared->loadout, &error, [spawnNpcs](bool success) mutable { spawnNpcs(success); }
    );
    if (!started) spawnNpcs(false);
}

void MapLoaderSection::AdvancePendingAutoSpawn(const RuntimeContextSnapshot& runtime) {
    auto* controller = runtime.controller;
    auto* world = runtime.world;
    PendingAutoSpawn pending;
    {
        std::lock_guard lock(autoSpawnMutex);
        if (!pendingAutoSpawn) return;
        pending = *pendingAutoSpawn;
    }

    if (!world || !controller || (pending.sourceWorld && pending.sourceWorld == world)) {
        if (std::chrono::steady_clock::now() >= pending.deadline) {
            FinishAutoSpawnAttempt(pending.generation, "The destination did not become ready in time");
        }
        return;
    }
    auto* willie = runtime.player;
    bool spawnedPlayer = false;
    if (!willie) {
        auto* gameInstance = world->OwningGameInstance;
        if (!GameClass::IsGameSettings(gameInstance)) {
            FinishAutoSpawnAttempt(pending.generation, "The selected player setup is unavailable");
            return;
        }
        auto* gi = static_cast<SDK::UGI_Settings_C*>(gameInstance);

        auto* willieClass =
            pending.playerClass ? pending.playerClass : Spawner::LoadClass(GameConstants::WILLIE_BP_PATH);
        if (!GameClass::IsWillieClass(willieClass)) {
            FinishAutoSpawnAttempt(pending.generation, "Could not prepare the player");
            return;
        }

        const auto transform = controller->GetTransform();
        auto* newActor = Spawner::DeferredSpawn(world, willieClass, transform, [&](SDK::AActor* actor) {
            if (!actor) return;
            auto* newWillie = static_cast<SDK::AWillie_BP_C*>(actor);
            newWillie->Player = true;
            newWillie->Team_Int = 0;
            newWillie->Character_Passport = gi->Player_Character;

            if (pending.prepared->player) {
                const auto& overrides = pending.prepared->player->overrides;
                if (overrides.heightRate.enabled)
                    newWillie->Character_Passport.Height_21_0EB204DF4978B92AD0ED188FD32EEC7B =
                        overrides.heightRate.value;
                if (overrides.muscleRate.enabled)
                    newWillie->Character_Passport.Weight_23_65E4C6534D14653F96EB739F159E58CD =
                        overrides.muscleRate.value;
            }
        });
        if (!GameClass::IsWillie(newActor)) {
            if (newActor) newActor->K2_DestroyActor();
            FinishAutoSpawnAttempt(pending.generation, "Could not add the player to the destination");
            return;
        }
        willie = static_cast<SDK::AWillie_BP_C*>(newActor);
        spawnedPlayer = true;
    }

    std::unique_lock lock(autoSpawnMutex);
    if (!pendingAutoSpawn || pendingAutoSpawn->generation != pending.generation ||
        pending.generation != autoSpawnGeneration) {
        lock.unlock();
        if (spawnedPlayer && willie) willie->K2_DestroyActor();
        return;
    }

    pendingAutoSpawn.reset();
    autoSpawnSubscriptions.Clear();
    lock.unlock();

    if (spawnedPlayer) {
        controller->Possess(willie);
        willie->Set_Up_Armor(true, false);
    }
    ApplyPreparedAutoSpawn(world, willie, pending.prepared, pending.generation);
}

void MapLoaderSection::SpawnPlayer() {
    const auto generation = BeginAction(ResultTarget::SpawnPlayer);
    std::string prepareError;
    MapScenarioPresetData::AutoSpawnOptions options{
        .enabled = optAutoSpawn,
        .npcCount = 0,
        .playerPreset = playerPresetLink.GetLink(),
        .loadoutPreset = loadoutPresetLink.GetLink(),
    };
    auto prepared = optAutoSpawn ? PrepareAutoSpawn(options, prepareError) : std::make_shared<PreparedAutoSpawn>();
    if (!prepared) {
        if (prepareError.empty()) prepareError = "The starting player could not be prepared.";
        SetActionError(ResultTarget::SpawnPlayer, generation, std::move(prepareError));
        return;
    }

    {
        std::lock_guard lock(autoSpawnMutex);
        if (latestActionGeneration != generation || autoSpawnGeneration != generation ||
            latestActionTarget != ResultTarget::SpawnPlayer)
            return;
        pendingAutoSpawn = PendingAutoSpawn{
            .prepared = std::move(prepared),
            .sourceWorld = nullptr,
            .playerClass = nullptr,
            .deadline = std::chrono::steady_clock::now() + AUTO_SPAWN_TIMEOUT,
            .generation = generation,
        };
    }

    const bool queued = GameHook::QueueAction([this, generation](const RuntimeContextSnapshot& runtime) {
        if (!IsAutoSpawnAttemptCurrent(generation)) return;
        StartAutoSpawnSubscription();
        AdvancePendingAutoSpawn(runtime);
    });
    if (!queued) {
        FinishAutoSpawnAttempt(generation, "Could not add the player to the destination map");
        return;
    }
    if (!IsActionCurrent(ResultTarget::SpawnPlayer, generation)) return;
    actionStatusTarget = ResultTarget::SpawnPlayer;
    actionStatusToken = actionStatus.SetInfo("The player will be added when the destination opens.");
}

void MapLoaderSection::RenderPreLoadOptions() {
    if (!ImGui::TreeNode("Starting Conditions")) return;

    ImGui::TextDisabled("Choose how the destination map should begin.");

    ImGui::Checkbox("Fresh Start", &optFreshStart);
    GuiUtils::HelpTooltip("Start the map as if entering it for the first time.");
    (void)GuiUtils::SameLineIfFitsCheckbox("Tutorial");
    ImGui::Checkbox("Tutorial", &optTutorial);
    GuiUtils::HelpTooltip("Show the game's tutorial in the destination map.");

    ImGui::SeparatorText("Free Mode");
    ImGui::Checkbox("Free Mode", &optFreeMode);
    GuiUtils::HelpTooltip("Start without a fixed campaign objective.");
    if (!optFreeMode) ImGui::BeginDisabled();
    ImGui::Checkbox("Carnage", &optCarnage);
    GuiUtils::HelpTooltip("Use Carnage rules in Free Mode.");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    GuiUtils::DebouncedDragInt("Enemies", &optFoesAmount, 0.2f, 0, 7);
    GuiUtils::HelpTooltip("Choose how many enemies appear in Free Mode (0-7).");
    GuiUtils::RenderFreeTierCombo("Enemy Tier", optFoeTier);
    GuiUtils::HelpTooltip("Choose the strength of Free Mode enemies.");
    if (!optFreeMode) ImGui::EndDisabled();

    ImGui::SeparatorText("Combat");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    GuiUtils::DebouncedDragInt("Starting Fighters", &optCombatantsAmount, 0.2f, 0, 7);
    GuiUtils::HelpTooltip("Choose how many fighters are present when the map starts (0-7).");
    GuiUtils::RenderFreeTierCombo("Opponent Tier", optOpponentTier);
    GuiUtils::HelpTooltip("Choose the strength of opponents present when the map starts.");

    ImGui::SeparatorText("Starting Characters");
    ImGui::Checkbox("Add Player", &optAutoSpawn);
    GuiUtils::HelpTooltip("Enter the map with the selected player appearance and equipment.");

    if (optAutoSpawn) {
        (void)playerPresetLink.Render("Player Appearance", "None (use save)");
        (void)loadoutPresetLink.Render("Player Equipment");

        ImGui::SeparatorText("NPCs");
        (void)npcPresetLink.Render("NPC Setup");
        const bool hasNPCPreset = npcPresetLink.HasLink();
        if (!hasNPCPreset) optAutoNPCCount = 0;
        if (!hasNPCPreset) ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
        GuiUtils::DebouncedDragInt("Number of NPCs", &optAutoNPCCount, 0.2f, 0, 10);
        if (!hasNPCPreset) ImGui::EndDisabled();
        if (!hasNPCPreset) ImGui::TextDisabled("Choose an NPC setup to add NPCs.");
    }

    ImGui::TreePop();
}

void MapLoaderSection::RenderMapSelector(MapRegistry& reg) {
    const auto& maps = reg.GetMaps();
    const auto& cats = reg.GetCategories();

    if (ImGui::InputTextWithHint("##MapSearch", "Search maps...", searchBuffer, sizeof(searchBuffer)))
        filterDirty = true;

    if (cats.size() > 1) {
        if (cachedCatComboW == 0.0f) {
            float maxW = ImGui::CalcTextSize("All").x;
            for (const auto& c : cats) {
                float w = ImGui::CalcTextSize(c.c_str()).x;
                if (w > maxW) maxW = w;
            }
            cachedCatComboW = GuiUtils::ComboWidthFromText(maxW);
        }

        const char* catPreview = selectedCategoryIndex == 0 ? "All" : cats[selectedCategoryIndex - 1].c_str();
        if (GuiUtils::BeginSizedCombo("Category", catPreview, cachedCatComboW)) {
            if (ImGui::Selectable("All", selectedCategoryIndex == 0)) {
                selectedCategoryIndex = 0;
                filterDirty = true;
            }
            for (int i = 0; i < static_cast<int>(cats.size()); ++i) {
                bool selected = (selectedCategoryIndex == i + 1);
                if (ImGui::Selectable(cats[i].c_str(), selected)) {
                    selectedCategoryIndex = i + 1;
                    filterDirty = true;
                }
            }
            ImGui::EndCombo();
        }
    }

    RebuildFilter(reg);

    if (filteredIndices.empty()) {
        ImGui::TextDisabled("No maps match the current search and category.");
        if (searchBuffer[0] != '\0' || selectedCategoryIndex != 0) {
            if (GuiUtils::Button("Clear Filters", GuiUtils::ButtonTone::Quiet)) {
                searchBuffer[0] = '\0';
                selectedCategoryIndex = 0;
                filterDirty = true;
            }
        }
    } else {
        if (selectedFilteredIndex >= static_cast<int>(filteredIndices.size())) selectedFilteredIndex = 0;

        const char* preview = maps[filteredIndices[selectedFilteredIndex]].displayName.c_str();
        if (GuiUtils::BeginSizedCombo("Map", preview, cachedComboW)) {
            GuiUtils::RenderClippedList(static_cast<int>(filteredIndices.size()), selectedFilteredIndex, [&](int i) {
                const auto& entry = maps[filteredIndices[i]];
                bool selected = (i == selectedFilteredIndex);
                if (ImGui::Selectable(entry.displayName.c_str(), selected)) {
                    selectedFilteredIndex = i;
                    packageOverride.clear();
                }
            });
            ImGui::EndCombo();
        }
    }
}

std::string_view MapLoaderSection::CurrentPackageName(const MapRegistry& reg) const {
    if (!packageOverride.empty()) return packageOverride;
    const auto& maps = reg.GetMaps();
    if (selectedFilteredIndex < 0 || selectedFilteredIndex >= static_cast<int>(filteredIndices.size())) return {};
    const int mapIndex = filteredIndices[static_cast<size_t>(selectedFilteredIndex)];
    if (mapIndex < 0 || mapIndex >= static_cast<int>(maps.size())) return {};
    return maps[static_cast<size_t>(mapIndex)].packageName;
}

bool MapLoaderSection::AutoSpawnLinksHealthy() const noexcept {
    if (!optAutoSpawn) return true;
    const bool npcLinkHealthy = !(optAutoSpawn && optAutoNPCCount > 0) ||
                                (npcPresetLink.HasLink() && !npcPresetLink.IsBroken());
    return !playerPresetLink.IsBroken() && !loadoutPresetLink.IsBroken() && npcLinkHealthy;
}

MapScenarioPresetData MapLoaderSection::BuildScenarioPreset(std::string packageName) const {
    MapScenarioPresetData data;
    data.packageName = std::move(packageName);
    data.preLoad = {
        .freshStart = optFreshStart,
        .tutorial = optTutorial,
        .freeMode = optFreeMode,
        .carnage = optCarnage,
        .foesAmount = optFoesAmount,
        .foeTier = optFoeTier,
        .combatantsAmount = optCombatantsAmount,
        .opponentTier = optOpponentTier,
    };
    data.autoSpawn = {
        .enabled = optAutoSpawn,
        .npcCount = optAutoNPCCount,
        .playerPreset = playerPresetLink.GetLink(),
        .loadoutPreset = loadoutPresetLink.GetLink(),
        .npcPreset = npcPresetLink.GetLink(),
    };
    return data;
}

PresetApplyDisposition MapLoaderSection::ApplyScenarioPreset(MapScenarioPresetData data) {
    packageOverride = std::move(data.packageName);
    optFreshStart = data.preLoad.freshStart;
    optTutorial = data.preLoad.tutorial;
    optFreeMode = data.preLoad.freeMode;
    optCarnage = data.preLoad.carnage;
    optFoesAmount = data.preLoad.foesAmount;
    optFoeTier = data.preLoad.foeTier;
    optCombatantsAmount = data.preLoad.combatantsAmount;
    optOpponentTier = data.preLoad.opponentTier;
    optAutoSpawn = data.autoSpawn.enabled;
    optAutoNPCCount = data.autoSpawn.npcCount;
    const auto& appDataRoot = ConfigManager::GetAppDataPath();
    playerPresetLink.SetLink(std::move(data.autoSpawn.playerPreset), appDataRoot);
    loadoutPresetLink.SetLink(std::move(data.autoSpawn.loadoutPreset), appDataRoot);
    npcPresetLink.SetLink(std::move(data.autoSpawn.npcPreset), appDataRoot);
    return PresetApplyDisposition::Applied;
}

void MapLoaderSection::RenderScenarioPresets(MapRegistry& reg) {
    if (!ImGui::TreeNode("Saved Scenarios")) return;

    scenarioPresets.status.Render();
    const auto packageName = CurrentPackageName(reg);
    ImGui::TextDisabled("Save the destination and starting conditions for later.");
    const auto destinationName = FriendlyMapName(reg, packageName);
    ImGui::TextDisabled("Destination: %s", packageName.empty() ? "None" : destinationName.c_str());
    playerPresetLink.RefreshIfCatalogChanged();
    loadoutPresetLink.RefreshIfCatalogChanged();
    npcPresetLink.RefreshIfCatalogChanged();
    const bool linksHealthy = AutoSpawnLinksHealthy();
    if (!linksHealthy) {
        GuiUtils::RenderCallout(
            "scenario-links-broken",
            "Some starting choices are missing. Choose replacements before saving or starting.",
            GuiUtils::CalloutTone::Warning
        );
    }
    scenarioPresets.RenderPresetsTab(
        [this, &reg](const char*, bool) {
            auto data = BuildScenarioPreset(std::string(CurrentPackageName(reg)));
            return PresetBuildResult<MapScenarioPresetData>::Success(std::move(data));
        },
        [this](MapScenarioPresetData data) { return ApplyScenarioPreset(std::move(data)); },
        !packageName.empty() && linksHealthy
    );
    ImGui::TreePop();
}

std::atomic<MapLoaderSection*> MapLoaderSection::runtimeInstance{nullptr};

void MapLoaderSection::StartAutoSpawnSubscription() {
    std::lock_guard lock(autoSpawnMutex);
    autoSpawnSubscriptions.Clear();
    if (!pendingAutoSpawn || pendingAutoSpawn->generation != autoSpawnGeneration) return;
    (void)autoSpawnSubscriptions.Subscribe(GameEvent::OnTick, [this](const RuntimeContextSnapshot& runtime) {
        AdvancePendingAutoSpawn(runtime);
    });
}

void MapLoaderSection::OnRuntimeStart() {
    auto* instance = runtimeInstance.load(std::memory_order_acquire);
    if (!instance) return;
    instance->StartAutoSpawnSubscription();
}

void MapLoaderSection::OnRuntimeShutdown() noexcept {
    if (auto* instance = runtimeInstance.load(std::memory_order_acquire)) {
        {
            std::lock_guard lock(instance->autoSpawnMutex);
            instance->autoSpawnSubscriptions.Clear();
            ++instance->autoSpawnGeneration;
            instance->latestActionGeneration = 0;
            instance->latestActionTarget = ResultTarget::SelectedMap;
            instance->pendingAutoSpawn.reset();
            instance->autoSpawnFeedback.reset();
        }
        instance->actionStatusResetPending.store(true, std::memory_order_release);
    }
}

MapLoaderSection::MapLoaderSection(ModContext& ctx) : Section(ctx, SECTION) {
    runtimeInstance.store(this, std::memory_order_release);
}

void MapLoaderSection::Render() {
    if (actionStatusResetPending.exchange(false, std::memory_order_acq_rel)) {
        actionStatus.Clear();
        actionStatusTarget = ResultTarget::SelectedMap;
        actionStatusToken = actionStatus.revision;
    }
    auto* player = RenderPlayer();
    FlushAutoSpawnFeedback();

    auto& reg = MapRegistry::Get();
    auto scanState = reg.GetState();

    if (scanState == ScanState::NotStarted || scanState == ScanState::Scanning) {
        if (scanState == ScanState::NotStarted) reg.RequestScan();
        GuiUtils::RenderCallout("map-scan-progress", "Finding available maps...", GuiUtils::CalloutTone::Info);
        return;
    }

    if (scanState == ScanState::Failed) {
        const auto result = GuiUtils::RenderCallout(
            "map-scan-failed", "No maps were found. Try finding them again.", GuiUtils::CalloutTone::Error, false,
            "Retry"
        );
        if (result.actionClicked) reg.RequestRescan();
        return;
    }

    RefreshLevelName();
    if (!cachedLevelName.empty()) {
        ImGui::TextDisabled("Current map");
        (void)GuiUtils::SameLineIfFits(ImGui::CalcTextSize(cachedLevelName.c_str()).x);
        const auto currentMapName = FriendlyMapName(reg, cachedLevelName);
        ImGui::TextUnformatted(currentMapName.c_str());
        ImGui::Spacing();
    }

    bool hasPlayer = player != nullptr;

    const bool showSpawnPlayerAction =
        !hasPlayer || (actionStatusTarget == ResultTarget::SpawnPlayer && !actionStatus.text.empty());
    if (showSpawnPlayerAction) {
        if (!hasPlayer)
            GuiUtils::RenderCallout(
                "map-no-player", "No player is active in this map.", GuiUtils::CalloutTone::Warning
            );

        if (hasPlayer) ImGui::BeginDisabled();
        if (GuiUtils::Button("Spawn Player", GuiUtils::ButtonTone::Primary)) SpawnPlayer();
        if (hasPlayer) ImGui::EndDisabled();
        RenderActionStatus(ResultTarget::SpawnPlayer);
    }

    ImGui::SeparatorText("Destination");
    if (!packageOverride.empty()) {
        const std::string message = "Scenario destination: " + FriendlyMapName(reg, packageOverride) +
                                    ". Choosing another map below will replace it.";
        const auto result = GuiUtils::RenderCallout(
            "staged-map-destination", message, GuiUtils::CalloutTone::Info, false, "Use Selected Map"
        );
        if (result.actionClicked) packageOverride.clear();
    }
    RenderMapSelector(reg);

    ImGui::Spacing();

    RenderPreLoadOptions();
    ImGui::Spacing();

    const auto packageName = CurrentPackageName(reg);
    const bool linksHealthy = AutoSpawnLinksHealthy();
    const bool canLoad = !packageName.empty() && linksHealthy;
    const char* loadLabel = packageOverride.empty() ? "Play Selected Map" : "Start Scenario";
    if (!canLoad) ImGui::BeginDisabled();
    if (GuiUtils::Button(loadLabel, GuiUtils::ButtonTone::Primary))
        LoadMap(packageName, ResultTarget::SelectedMap);
    if (!canLoad) ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (packageName.empty())
            ImGui::SetItemTooltip("Choose a destination map first");
        else if (!linksHealthy)
            ImGui::SetItemTooltip("Replace the missing saved choices before starting.");
        else
            ImGui::SetItemTooltip("End the current session and begin in the selected map.");
    }
    RenderActionStatus(ResultTarget::SelectedMap);

    ImGui::Spacing();
    const bool canRestart = !cachedLevelName.empty() && linksHealthy;
    if (!canRestart) ImGui::BeginDisabled();
    if (ImGui::Button("Restart Current Map")) LoadMap(cachedLevelName, ResultTarget::RestartMap);
    if (!canRestart) ImGui::EndDisabled();
    if (!canRestart && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetItemTooltip(
            linksHealthy ? "The current map is not available yet."
                         : "Replace the missing saved choices before restarting."
        );
    RenderActionStatus(ResultTarget::RestartMap);

    ImGui::Spacing();
    RenderScenarioPresets(reg);

    if (ImGui::TreeNode("Advanced")) {
        ImGui::Text("Custom Map Path");
        ImGui::InputTextWithHint("##CustomMapPath", "/Game/Maps/...", customPathBuffer, sizeof(customPathBuffer));
        const bool canLoadCustomPath = customPathBuffer[0] != '\0' && linksHealthy;
        if (!canLoadCustomPath) ImGui::BeginDisabled();
        if (GuiUtils::Button("Open Custom Map", GuiUtils::ButtonTone::Primary)) {
            packageOverride = customPathBuffer;
            LoadMap(packageOverride, ResultTarget::CustomMap);
        }
        if (!canLoadCustomPath) ImGui::EndDisabled();
        if (!canLoadCustomPath &&
            ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetItemTooltip(
                customPathBuffer[0] == '\0' ? "Enter a custom map path first"
                                            : "Replace the missing saved choices before opening the map."
            );
        RenderActionStatus(ResultTarget::CustomMap);

        ImGui::Spacing();
        if (ImGui::Button("Find Maps Again")) {
            reg.RequestRescan();
            selectedFilteredIndex = 0;
            selectedCategoryIndex = 0;
            searchBuffer[0] = '\0';
            filterDirty = true;
            cachedCatComboW = 0.0f;
            playerPresetLink.GetPicker().Invalidate();
            loadoutPresetLink.GetPicker().Invalidate();
            npcPresetLink.GetPicker().Invalidate();
            scenarioPresets.presetListDirty = true;
        }
        char catalogStatus[64];
        std::snprintf(catalogStatus, sizeof(catalogStatus), "%zu maps available", reg.GetMaps().size());
        (void)GuiUtils::SameLineIfFits(ImGui::CalcTextSize(catalogStatus).x);
        ImGui::TextDisabled("%s", catalogStatus);
        ImGui::TreePop();
    }
}
