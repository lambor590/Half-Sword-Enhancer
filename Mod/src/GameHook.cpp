#include "Hooks/GameHook.h"

#include <Windows.h>

#include "ConfigManager.h"
#include "Core/ModContext.h"
#include "MemoryUtils.h"
#include "Menu/EventBus.h"
#include "Utils/CompileTimeHash.h"
#include "Utils/GameBuildInfo.h"

#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"

#include <algorithm>
#include <bit>
#include <condition_variable>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
    constexpr std::uint64_t RECEIVE_TICK_HASH = HS::Hash::FNV1A("ReceiveTick");

    struct ProcessEventCacheSlot {
        SDK::UFunction* key = nullptr;
        std::uint64_t nameHash = 0;
        void* hookEntry = nullptr;
        std::uint32_t registryGeneration = 0;
    };

    struct ProcessEventCache {
        static constexpr std::uintptr_t TABLE_MASK = 1024 - 1;

        alignas(64) ProcessEventCacheSlot slots[TABLE_MASK + 1]{};
    };

    thread_local ProcessEventCache processEventCache;
    thread_local std::uint8_t hookSuppressionDepth = 0;
    thread_local std::uint32_t dispatchDepth = 0;

    struct ScopedHookSuppression {
        ScopedHookSuppression() noexcept { ++hookSuppressionDepth; }
        ~ScopedHookSuppression() noexcept { --hookSuppressionDepth; }

        ScopedHookSuppression(const ScopedHookSuppression&) = delete;
        ScopedHookSuppression& operator=(const ScopedHookSuppression&) = delete;
    };
}

void __stdcall OnProcessEvent(SDK::UObject* object, SDK::UFunction* function, void* params) noexcept {
    auto& hook = GameHook::Get();
    const GameHook::DispatchLease dispatch{hook};
    const auto originalProcessEvent = std::bit_cast<ProcessEvent>(
        dispatch.DispatchHooks() ? hook.oProcessEvent
                                 : (hook.oProcessEvent ? hook.oProcessEvent : hook.processEventAddress)
    );
    if (!originalProcessEvent) return;

    if (hookSuppressionDepth > 0) [[unlikely]] {
        originalProcessEvent(object, function, params);
        return;
    }

    if (!dispatch.DispatchHooks()) [[unlikely]] {
        const ScopedHookSuppression suppressHooks;
        originalProcessEvent(object, function, params);
        return;
    }

    const bool queued = hook.hasQueuedActions.load(std::memory_order_relaxed);
    if (!queued && (!hook.hasListeners.load(std::memory_order_relaxed) || !hook.IsGameThread())) [[likely]] {
        originalProcessEvent(object, function, params);
        return;
    }

    auto& slot = processEventCache.slots
        [(reinterpret_cast<std::uintptr_t>(function) >> 4) & ProcessEventCache::TABLE_MASK];
    if (slot.key != function) [[unlikely]] {
        const std::string functionName = function->GetName();
        slot = {.key = function, .nameHash = HS::Hash::FNV1A(functionName)};
    }

    if (queued && (slot.nameHash == RECEIVE_TICK_HASH || hook.IsGameThread())) [[unlikely]] {
        const ScopedHookSuppression suppressHooks;
        hook.gameThreadId.store(GetCurrentThreadId(), std::memory_order_release);

        static thread_local std::vector<GameHook::QueuedAction> localQueue;
        localQueue.clear();
        {
            std::lock_guard lock(hook.queueMutex);
            localQueue.swap(hook.gameThreadQueue);
            hook.hasQueuedActions.store(false, std::memory_order_release);
        }

        if (!localQueue.empty()) {
            const auto snapshot = ModContext::Get().RefreshGameThreadCache();
            for (auto& action : localQueue) {
                try {
                    action(snapshot);
                } catch (...) {
                    hook.logger.Log("Queued game-thread action failed");
                }
            }
        }
    }

    if (queued && (!hook.IsGameThread() || !hook.hasListeners.load(std::memory_order_relaxed))) {
        originalProcessEvent(object, function, params);
        return;
    }

    if (slot.registryGeneration != hook.registryGeneration) {
        slot.hookEntry = hook.FindHookEntry(slot.nameHash);
        slot.registryGeneration = hook.registryGeneration;
    }

    auto* entry = static_cast<GameHook::HookEntry*>(slot.hookEntry);
    if (!entry || (entry->before.empty() && entry->after.empty())) {
        originalProcessEvent(object, function, params);
        return;
    }

    GameHook::ProcessEventContext context{object, function, params};
    if (!entry->before.empty()) {
        const ScopedHookSuppression suppressHooks;
        GameHook::DispatchListeners(entry->before, context);
    }

    if (context.IsCancelled()) [[unlikely]]
        return;

    originalProcessEvent(object, function, params);

    if (!entry->after.empty()) {
        const ScopedHookSuppression suppressHooks;
        GameHook::DispatchListeners(entry->after, context);
    }
}

GameHook& GameHook::Get() {
    static GameHook instance;
    return instance;
}

GameHook::DispatchLease::DispatchLease(GameHook& owner) noexcept : owner(owner) {
    ownsDispatch = dispatchDepth++ == 0;
    dispatchHooks = ownsDispatch ? owner.BeginDispatch() : owner.IsDispatchRunning();
}

GameHook::DispatchLease::~DispatchLease() {
    if (--dispatchDepth == 0 && ownsDispatch) owner.EndDispatch();
}

bool GameHook::Hook() {
    if (IsHooked()) return true;
    if (oProcessEvent) return false;

    hooks.reserve(32);

    processEventAddress = SDK::InSDKUtils::GetImageBase() + SDK::Offsets::ProcessEvent;
    oProcessEvent = processEventAddress;
    if (!MemoryUtils::PlaceHook(oProcessEvent, reinterpret_cast<uintptr_t>(OnProcessEvent), &oProcessEvent)) {
        logger.Log("Failed to hook ProcessEvent");
        oProcessEvent = 0;
        processEventAddress = 0;
        return false;
    }

    SetDispatchMode(DispatchMode::Running);

    if (ConfigManager::Get().GetBool("UE", "console_enabled", false)) SetUEConsoleEnabled(true);
    QueueAction([](const RuntimeContextSnapshot&) { GameBuildInfo::Query(); });
    return true;
}

void GameHook::Quiesce() noexcept {
    if (!oProcessEvent) return;

    auto state = dispatchState.load(std::memory_order_acquire);
    for (;;) {
        const auto mode = ModeOf(state);
        if (mode == DispatchMode::Blocked) return;

        if (mode == DispatchMode::Running) {
            const auto bypass = DispatchState(DispatchMode::Bypass, DispatchCount(state));
            if (!dispatchState
                     .compare_exchange_weak(state, bypass, std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            state = bypass;
        }

        if (DispatchCount(state) == 0) {
            const auto blocked = DispatchState(DispatchMode::Blocked);
            if (dispatchState
                    .compare_exchange_weak(state, blocked, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return;
            }
            continue;
        }

        dispatchState.wait(state, std::memory_order_acquire);
        state = dispatchState.load(std::memory_order_acquire);
    }
}

void GameHook::Unhook() {
    if (!oProcessEvent) return;

    Quiesce();
    if (processEventAddress) MemoryUtils::Unhook(processEventAddress);
    oProcessEvent = 0;

    SetDispatchMode(DispatchMode::Bypass);
    WaitForDispatches();

    {
        std::lock_guard lock(queueMutex);
        gameThreadQueue.clear();
        hasQueuedActions.store(false, std::memory_order_release);
    }

    EventBus::Get().Clear();
    hooks.clear();
    ++registryGeneration;
    hasListeners.store(false, std::memory_order_release);
    nextHookHandle = 1;
    gameThreadId.store(0, std::memory_order_release);
}

bool GameHook::BeginDispatch() noexcept {
    auto state = dispatchState.load(std::memory_order_acquire);
    for (;;) {
        const auto entered = state + 1;
        if (dispatchState.compare_exchange_weak(state, entered, std::memory_order_acq_rel, std::memory_order_acquire)) {
            state = entered;
            break;
        }
    }

    while (ModeOf(state) == DispatchMode::Blocked) {
        dispatchState.wait(state, std::memory_order_acquire);
        state = dispatchState.load(std::memory_order_acquire);
    }
    return ModeOf(state) == DispatchMode::Running;
}

void GameHook::EndDispatch() noexcept {
    const auto previous = dispatchState.fetch_sub(1, std::memory_order_acq_rel);
    if (DispatchCount(previous) == 1 && ModeOf(previous) != DispatchMode::Running) dispatchState.notify_all();
}

bool GameHook::IsDispatchRunning() const noexcept {
    return ModeOf(dispatchState.load(std::memory_order_acquire)) == DispatchMode::Running;
}

void GameHook::SetDispatchMode(DispatchMode mode) noexcept {
    auto state = dispatchState.load(std::memory_order_acquire);
    for (;;) {
        const auto updated = DispatchState(mode, DispatchCount(state));
        if (dispatchState.compare_exchange_weak(state, updated, std::memory_order_acq_rel, std::memory_order_acquire)) {
            dispatchState.notify_all();
            return;
        }
    }
}

void GameHook::WaitForDispatches() noexcept {
    auto state = dispatchState.load(std::memory_order_acquire);
    while (DispatchCount(state) != 0) {
        dispatchState.wait(state, std::memory_order_acquire);
        state = dispatchState.load(std::memory_order_acquire);
    }
}

bool GameHook::CanAccessRegistry() const noexcept {
    return !IsHooked() || IsGameThread();
}

GameHook::HookHandle GameHook::Subscribe(std::string_view functionName, HookPhase phase, HookCallback callback) {
    if (functionName.empty() || !callback || !CanAccessRegistry()) return INVALID_HOOK_HANDLE;

    const auto nameHash = HS::Hash::FNV1A(functionName);
    auto* entry = FindHookEntry(nameHash);
    if (!entry) {
        hooks.push_back({.nameHash = nameHash});
        ++registryGeneration;
        entry = &hooks.back();
    }
    const auto handle = nextHookHandle++;
    (phase == HookPhase::Before ? entry->before : entry->after)
        .push_back({.handle = handle, .callback = std::move(callback)});
    hasListeners.store(true, std::memory_order_release);
    return handle;
}

void GameHook::Unsubscribe(HookHandle handle) {
    if (handle == INVALID_HOOK_HANDLE || !CanAccessRegistry()) return;

    const auto remove = [handle](ListenerList& listeners) {
        const auto listener = std::find_if(listeners.begin(), listeners.end(), [handle](const HookListener& candidate) {
            return candidate.handle == handle;
        });
        if (listener == listeners.end()) return false;
        listeners.erase(listener);
        return true;
    };

    for (auto& entry : hooks) {
        if (!remove(entry.before) && !remove(entry.after)) continue;

        const bool anyListeners = std::ranges::any_of(hooks, [](const HookEntry& candidate) {
            return !candidate.before.empty() || !candidate.after.empty();
        });
        hasListeners.store(anyListeners, std::memory_order_release);
        return;
    }
}

bool GameHook::IsSubscribed(HookHandle handle) noexcept {
    if (handle == INVALID_HOOK_HANDLE || !IsHooked()) return false;
    if (!IsGameThread()) return true;
    for (const auto& entry : hooks) {
        const auto contains = [handle](const ListenerList& listeners) {
            return std::ranges::any_of(listeners, [handle](const HookListener& listener) {
                return listener.handle == handle;
            });
        };
        if (contains(entry.before) || contains(entry.after)) return true;
    }
    return false;
}

GameHook::HookHandle GameHook::SubscriptionGroup::Subscribe(
    std::string_view functionName, HookPhase phase, HookCallback callback
) {
    const auto handle = GameHook::Get().Subscribe(functionName, phase, std::move(callback));
    if (handle != INVALID_HOOK_HANDLE) handles.push_back(handle);
    return handle;
}

void GameHook::SubscriptionGroup::Reset() noexcept {
    auto& hook = GameHook::Get();
    for (const auto handle : handles)
        hook.Unsubscribe(handle);
    handles.clear();
}

bool GameHook::SubscriptionGroup::IsSubscribed() const noexcept {
    if (handles.empty()) return false;
    auto& hook = GameHook::Get();
    return std::ranges::all_of(handles, [&hook](HookHandle handle) { return hook.IsSubscribed(handle); });
}

void GameHook::SetUEConsoleEnabled(bool enabled) {
    QueueAction([enabled]([[maybe_unused]] const RuntimeContextSnapshot&) {
        auto& hook = GameHook::Get();
        SDK::UEngine* engine = SDK::UEngine::GetEngine();
        SDK::UGameViewportClient* viewport = engine ? engine->GameViewport : nullptr;

        if (enabled) {
            if (!engine || !viewport || !engine->ConsoleClass.Get()) {
                hook.logger.Log("UE Console unlock skipped: runtime was not ready");
                return;
            }

            if (!viewport->ViewportConsole) {
                SDK::UObject* newConsole = SDK::UGameplayStatics::SpawnObject(engine->ConsoleClass, viewport);
                if (!newConsole) {
                    hook.logger.Log("UE Console unlock failed: console object could not be created");
                    return;
                }
                viewport->ViewportConsole = static_cast<SDK::UConsole*>(newConsole);
            }
        } else if (viewport && viewport->ViewportConsole) {
            viewport->ViewportConsole = nullptr;
        }

        SDK::UInputSettings* inputSettings = SDK::UInputSettings::GetDefaultObj();
        if (inputSettings && inputSettings->ConsoleKeys.Num() > 0) {
            inputSettings->ConsoleKeys[0].KeyName =
                SDK::UKismetStringLibrary::Conv_StringToName(SDK::FString(enabled ? L"F2" : L"None"));
        }

        hook.logger.Log(enabled ? "UE Console unlocked - Press F2 to open console" : "UE Console locked");
    });
}

bool GameHook::QueueAction(QueuedAction action) {
    if (!action) return false;

    auto& hook = GameHook::Get();
    std::lock_guard lock(hook.queueMutex);
    if (!hook.IsHooked()) return false;
    hook.gameThreadQueue.push_back(std::move(action));
    hook.hasQueuedActions.store(true, std::memory_order_release);
    return true;
}

bool GameHook::ExecuteOnGameThreadAndWait(QueuedAction action, std::chrono::milliseconds timeout) {
    if (!action || !IsHooked()) return false;

    if (IsGameThread()) {
        try {
            action(ModContext::Get().RefreshGameThreadCache());
            return true;
        } catch (...) {
            logger.Log("Synchronous game-thread action failed");
            return false;
        }
    }

    struct Completion {
        std::mutex mutex;
        std::condition_variable signal;
        bool finished = false;
        bool success = false;
    };

    auto completion = std::make_shared<Completion>();
    if (!QueueAction([action = std::move(action), completion](const RuntimeContextSnapshot& runtime) mutable {
            bool success = false;
            try {
                action(runtime);
                success = true;
            } catch (...) {
                GameHook::Get().logger.Log("Synchronous game-thread action failed");
            }
            {
                std::lock_guard lock(completion->mutex);
                completion->success = success;
                completion->finished = true;
            }
            completion->signal.notify_one();
        })) {
        return false;
    }

    std::unique_lock lock(completion->mutex);
    if (!completion->signal.wait_for(lock, timeout, [&completion] { return completion->finished; })) return false;
    return completion->success;
}

bool GameHook::IsGameThread() const noexcept {
    const auto threadId = gameThreadId.load(std::memory_order_acquire);
    return threadId != 0 && threadId == GetCurrentThreadId();
}

bool GameHook::IsHooked() const noexcept {
    return ModeOf(dispatchState.load(std::memory_order_acquire)) == DispatchMode::Running;
}

GameHook::HookEntry* GameHook::FindHookEntry(std::uint64_t nameHash) noexcept {
    const auto entry = std::ranges::find(hooks, nameHash, &HookEntry::nameHash);
    return entry == hooks.end() ? nullptr : &*entry;
}

void GameHook::DispatchListeners(const ListenerList& listeners, ProcessEventContext& context) {
    for (const auto& listener : listeners)
        listener.callback(context);
}
