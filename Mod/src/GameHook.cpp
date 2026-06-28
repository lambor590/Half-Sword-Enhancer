#include "Hooks/GameHook.h"

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
#include <cstring>
#include <intrin.h>
#include <string>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline __attribute__((always_inline))
#endif

std::vector<GameHook::QueuedAction> GameHook::gameThreadQueue;
std::mutex GameHook::queueMutex;
std::atomic<bool> GameHook::hasQueuedActions{false};

namespace {
    constexpr uint64_t RECEIVE_TICK_HASH = HS::Hash::FNV1A("ReceiveTick");

    FORCE_INLINE size_t FibonacciHash(uintptr_t key) noexcept {
        constexpr uint64_t PHI64 = 0x9E3779B97F4A7C15ULL;
        return static_cast<size_t>((key * PHI64) >> 52);
    }
}

namespace GameHookDetail {
    struct ProcessEventCacheSlot {
        SDK::UFunction* key = nullptr;
        void* beforeListeners = nullptr;
        void* afterListeners = nullptr;
        uint32_t generation = 0;
        bool isReceiveTick = false;
    };
}

namespace {
    struct ProcessEventCache {
        static constexpr size_t TABLE_SIZE = 4096;
        static constexpr size_t TABLE_MASK = TABLE_SIZE - 1;
        static constexpr size_t MAX_PROBES = 8;

        using Slot = GameHookDetail::ProcessEventCacheSlot;

        alignas(64) Slot slots[TABLE_SIZE];
        uint32_t generation = 1;

        ProcessEventCache() noexcept { ForceClear(); }

        void Clear() noexcept {
            if (++generation == 0) {
                ForceClear();
                generation = 1;
            }
        }

        void ForceClear() noexcept {
            std::memset(slots, 0, sizeof(slots));
        }

        FORCE_INLINE const Slot* Lookup(SDK::UFunction* function) const noexcept {
            const auto raw = reinterpret_cast<uintptr_t>(function);
            const size_t idx = FibonacciHash(raw);

            _mm_prefetch(reinterpret_cast<const char*>(&slots[(idx + 4) & TABLE_MASK]), _MM_HINT_T0);

            {
                const auto& slot = slots[idx & TABLE_MASK];
                if (slot.generation != generation) return nullptr;
                if (slot.key == function) [[likely]]
                    return &slot;
            }

            for (size_t i = 1; i < MAX_PROBES; ++i) {
                const auto& slot = slots[(idx + i) & TABLE_MASK];
                if (slot.generation != generation) return nullptr;
                if (slot.key == function) return &slot;
            }
            return nullptr;
        }

        Slot* Insert(
            SDK::UFunction* function, void* beforeListeners, void* afterListeners, bool isReceiveTick
        ) noexcept {
            const size_t idx = FibonacciHash(reinterpret_cast<uintptr_t>(function));
            for (size_t i = 0; i < MAX_PROBES; ++i) {
                auto& slot = slots[(idx + i) & TABLE_MASK];
                if (slot.generation != generation || slot.key == function) {
                    slot.generation = generation;
                    slot.key = function;
                    slot.beforeListeners = beforeListeners;
                    slot.afterListeners = afterListeners;
                    slot.isReceiveTick = isReceiveTick;
                    return &slot;
                }
            }

            slots[idx & TABLE_MASK] = {function, beforeListeners, afterListeners, generation, isReceiveTick};
            return &slots[idx & TABLE_MASK];
        }
    };

    static ProcessEventCache peCache;
    static thread_local uint8_t g_hookSuppressionDepth = 0;

    struct ScopedHookSuppression {
        ScopedHookSuppression() noexcept { ++g_hookSuppressionDepth; }
        ~ScopedHookSuppression() noexcept { --g_hookSuppressionDepth; }

        ScopedHookSuppression(const ScopedHookSuppression&) = delete;
        ScopedHookSuppression& operator=(const ScopedHookSuppression&) = delete;
    };

}

void __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* parms) noexcept {
    auto& hook = GameHook::Get();
    const auto originalProcessEvent = std::bit_cast<ProcessEvent>(hook.oProcessEvent);
    if (g_hookSuppressionDepth > 0) [[unlikely]] {
        originalProcessEvent(pObject, pFunc, parms);
        return;
    }

    const bool queued = GameHook::hasQueuedActions.load(std::memory_order_relaxed);
    if (!queued && hook.hooks.empty()) [[likely]] {
        originalProcessEvent(pObject, pFunc, parms);
        return;
    }

    const auto* slot = peCache.Lookup(pFunc);
    if (!slot) [[unlikely]] {
        slot = hook.ResolveAndCache(pFunc);
    }

    if (queued && slot->isReceiveTick) [[unlikely]] {
        const auto generationBeforeQueue = peCache.generation;

        {
            const ScopedHookSuppression suppressHooks;
            GameHook::ProcessGameThreadQueue();
        }

        if (generationBeforeQueue != peCache.generation) [[unlikely]] {
            originalProcessEvent(pObject, pFunc, parms);
            return;
        }
    }

    auto* beforeListeners = static_cast<GameHook::ListenerList*>(slot->beforeListeners);
    auto* afterListeners = static_cast<GameHook::ListenerList*>(slot->afterListeners);
    if (!beforeListeners && !afterListeners) {
        originalProcessEvent(pObject, pFunc, parms);
        return;
    }

    GameHook::ProcessEventContext context{pObject, pFunc, parms};
    if (beforeListeners) {
        ++g_hookSuppressionDepth;
        GameHook::DispatchListeners(*beforeListeners, context);
        --g_hookSuppressionDepth;
    }

    if (context.IsCancelled()) [[unlikely]]
        return;

    originalProcessEvent(pObject, pFunc, parms);

    if (afterListeners) {
        ++g_hookSuppressionDepth;
        GameHook::DispatchListeners(*afterListeners, context);
        --g_hookSuppressionDepth;
    }
}

GameHook& GameHook::Get() {
    static GameHook instance;
    return instance;
}

bool GameHook::Hook() {
    hooks.reserve(32);
    hookIndex.reserve(32);
    listenerIndex.reserve(64);

    processEventAddress = SDK::InSDKUtils::GetImageBase() + SDK::Offsets::ProcessEvent;
    if (!processEventAddress) {
        logger.Log("Failed to resolve ProcessEvent address");
        return false;
    }

    oProcessEvent = processEventAddress;

    if (!MemoryUtils::PlaceHook(oProcessEvent, (uintptr_t)OnProcessEvent, (uintptr_t*)&oProcessEvent)) {
        logger.Log("Failed to hook ProcessEvent");
        oProcessEvent = 0;
        processEventAddress = 0;
        return false;
    }

    hooked = true;

    if (ConfigManager::Get().GetBool("UE", "console_enabled", false)) {
        SetUEConsoleEnabled(true);
    }

    QueueAction([](const RuntimeContextSnapshot&) { GameBuildInfo::Query(); });

    return true;
}

void GameHook::Unhook() {
    hooked = false;
    MemoryUtils::Unhook(processEventAddress);

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        gameThreadQueue.clear();
        hasQueuedActions.store(false, std::memory_order_release);
    }

    EventBus::Get().Clear();

    hooks.clear();
    hookIndex.clear();
    listenerIndex.clear();
    nextHookHandle = 1;
    InvalidateDispatchCaches();
}

GameHook::HookHandle GameHook::Subscribe(std::string_view functionName, HookPhase phase, HookCallback callback) {
    if (functionName.empty() || !callback) return INVALID_HOOK_HANDLE;

    const uint64_t nameHash = HS::Hash::FNV1A(functionName);
    const size_t previousHookCapacity = hooks.capacity();
    auto* entry = EnsureHookEntry(nameHash);

    const auto entryIndex = static_cast<size_t>(entry - hooks.data());
    auto& listeners = ListenersFor(*entry, phase);
    const bool phaseWasEmpty = listeners.Empty();
    const auto handle = AllocateHandle();
    const size_t slot = AddListener(listeners, handle, std::move(callback));

    if (listenerIndex.size() <= handle) {
        listenerIndex.resize(static_cast<size_t>(handle) + 1);
    }
    listenerIndex[handle] = {.entryIndex = entryIndex, .slot = slot, .phase = phase};

    if (phaseWasEmpty || hooks.capacity() != previousHookCapacity) {
        InvalidateDispatchCaches();
    }
    return handle;
}

void GameHook::Unsubscribe(HookHandle handle) {
    if (handle == INVALID_HOOK_HANDLE) return;
    if (handle >= listenerIndex.size()) return;

    const auto location = listenerIndex[handle];
    if (location.entryIndex == INVALID_INDEX || location.entryIndex >= hooks.size()) return;

    auto& entry = hooks[location.entryIndex];
    auto& listeners = ListenersFor(entry, location.phase);
    RemoveListenerAt(listeners, location.slot, location.entryIndex, location.phase);

    listenerIndex[handle] = {};

    if (entry.before.Empty() && entry.after.Empty()) {
        RemoveHookEntryAt(location.entryIndex);
    } else if (listeners.Empty()) {
        InvalidateDispatchCaches();
    }
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

void GameHook::QueueAction(QueuedAction action) {
    if (!action) return;

    std::lock_guard<std::mutex> lock(queueMutex);
    gameThreadQueue.push_back(std::move(action));
    hasQueuedActions.store(true, std::memory_order_release);
}

void GameHook::ProcessGameThreadQueue() {
    static thread_local std::vector<QueuedAction> localQueue;
    localQueue.clear();

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (gameThreadQueue.empty()) {
            hasQueuedActions.store(false, std::memory_order_release);
            return;
        }
        localQueue.swap(gameThreadQueue);
        hasQueuedActions.store(false, std::memory_order_release);
    }

    const auto snapshot = ModContext::Get().RefreshGameThreadCache();
    for (auto& action : localQueue) {
        try {
            action(snapshot);
        } catch (...) {
            GameHook::Get().logger.Log("Queued game-thread action failed");
        }
    }
}

GameHook::HookHandle GameHook::AllocateHandle() noexcept {
    if (nextHookHandle == INVALID_HOOK_HANDLE) ++nextHookHandle;
    return nextHookHandle++;
}

std::vector<GameHook::HookIndexEntry>::iterator GameHook::LowerBoundHookIndex(uint64_t nameHash) noexcept {
    return std::lower_bound(hookIndex.begin(), hookIndex.end(), nameHash, [](const HookIndexEntry& entry, uint64_t hash) {
        return entry.nameHash < hash;
    });
}

GameHook::HookEntry* GameHook::FindHookEntry(uint64_t nameHash) noexcept {
    const auto it = LowerBoundHookIndex(nameHash);
    return it == hookIndex.end() || it->nameHash != nameHash ? nullptr : &hooks[it->entryIndex];
}

GameHook::HookEntry* GameHook::EnsureHookEntry(uint64_t nameHash) {
    const auto it = LowerBoundHookIndex(nameHash);
    if (it != hookIndex.end() && it->nameHash == nameHash) return &hooks[it->entryIndex];

    const size_t entryIndex = hooks.size();
    hooks.push_back({.nameHash = nameHash});
    hookIndex.insert(it, {.nameHash = nameHash, .entryIndex = entryIndex});
    return &hooks.back();
}

GameHook::ListenerList& GameHook::ListenersFor(HookEntry& entry, HookPhase phase) noexcept {
    return phase == HookPhase::Before ? entry.before : entry.after;
}

size_t GameHook::AddListener(ListenerList& listeners, HookHandle handle, HookCallback callback) {
    if (listeners.Empty()) {
        listeners.firstHandle = handle;
        listeners.firstCallback = std::move(callback);
        return 0;
    }

    if (listeners.extra.empty()) {
        listeners.extra.reserve(3);
    }
    listeners.extra.push_back({.handle = handle, .callback = std::move(callback)});
    return listeners.extra.size();
}

void GameHook::RemoveListenerAt(ListenerList& listeners, size_t slot, size_t entryIndex, HookPhase phase) {
    if (slot == 0) {
        if (listeners.extra.empty()) {
            listeners.firstHandle = INVALID_HOOK_HANDLE;
            listeners.firstCallback = {};
            return;
        }

        auto promoted = std::move(listeners.extra.back());
        listeners.extra.pop_back();
        listeners.firstHandle = promoted.handle;
        listeners.firstCallback = std::move(promoted.callback);
        listenerIndex[promoted.handle] = {.entryIndex = entryIndex, .slot = 0, .phase = phase};
        return;
    }

    const size_t extraIndex = slot - 1;

    const size_t lastIndex = listeners.extra.size() - 1;
    if (extraIndex != lastIndex) {
        listeners.extra[extraIndex] = std::move(listeners.extra[lastIndex]);
        listenerIndex[listeners.extra[extraIndex].handle] = {.entryIndex = entryIndex, .slot = slot, .phase = phase};
    }
    listeners.extra.pop_back();
}

void GameHook::DispatchListeners(ListenerList& listeners, ProcessEventContext& context) {
    listeners.firstCallback(context);

    for (auto& listener : listeners.extra) {
        listener.callback(context);
    }
}

void GameHook::RemoveHookEntryAt(size_t entryIndex) noexcept {
    const uint64_t removedHash = hooks[entryIndex].nameHash;
    const size_t lastIndex = hooks.size() - 1;

    hookIndex.erase(LowerBoundHookIndex(removedHash));

    if (entryIndex != lastIndex) {
        const uint64_t movedHash = hooks[lastIndex].nameHash;
        hooks[entryIndex] = std::move(hooks[lastIndex]);

        LowerBoundHookIndex(movedHash)->entryIndex = entryIndex;
        RelocateEntryListeners(hooks[entryIndex], entryIndex);
    }

    hooks.pop_back();
    InvalidateDispatchCaches();
}

void GameHook::RelocateEntryListeners(HookEntry& entry, size_t entryIndex) noexcept {
    auto relocate = [this, entryIndex](ListenerList& listeners, HookPhase phase) {
        if (listeners.firstHandle != INVALID_HOOK_HANDLE) {
            listenerIndex[listeners.firstHandle] = {.entryIndex = entryIndex, .slot = 0, .phase = phase};
        }

        for (size_t i = 0; i < listeners.extra.size(); ++i) {
            listenerIndex[listeners.extra[i].handle] = {.entryIndex = entryIndex, .slot = i + 1, .phase = phase};
        }
    };

    relocate(entry.before, HookPhase::Before);
    relocate(entry.after, HookPhase::After);
}

void GameHook::InvalidateDispatchCaches() noexcept {
    peCache.Clear();
}

GameHookDetail::ProcessEventCacheSlot* GameHook::ResolveAndCache(SDK::UFunction* function) noexcept {
    std::string funcName = function->GetName();
    const uint64_t nameHash = HS::Hash::FNV1A(funcName);
    auto* entry = FindHookEntry(nameHash);

    ListenerList* beforeListeners = nullptr;
    ListenerList* afterListeners = nullptr;

    if (entry) {
        if (!entry->before.Empty()) beforeListeners = &entry->before;
        if (!entry->after.Empty()) afterListeners = &entry->after;
    }

    return peCache.Insert(function, beforeListeners, afterListeners, nameHash == RECEIVE_TICK_HASH);
}
