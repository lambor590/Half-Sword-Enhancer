#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string_view>
#include <vector>

#include "Core/ModContext.h"
#include "Logger.h"

namespace SDK {
    class UObject;
    class UFunction;
}

namespace GameHookDetail {
    struct ProcessEventCacheSlot;
}

using ProcessEvent = void(__stdcall*)(SDK::UObject*, SDK::UFunction*, void*);

class GameHook {
public:
    static GameHook& Get();

    [[nodiscard]] bool Hook();
    void Unhook();

    void SetUEConsoleEnabled(bool enabled);

    using QueuedAction = std::function<void(const RuntimeContextSnapshot&)>;

    static void QueueAction(QueuedAction action);
    static void ProcessGameThreadQueue();

    bool IsHooked() const noexcept { return hooked; }

    enum class HookPhase : uint8_t { Before, After };

    using HookHandle = uint64_t;
    static constexpr HookHandle INVALID_HOOK_HANDLE = 0;

    struct ProcessEventContext {
        ProcessEventContext() = default;
        ProcessEventContext(SDK::UObject* object, SDK::UFunction* function, void* params) noexcept
            : object(object), function(function), params(params) {}

        SDK::UObject* object = nullptr;
        SDK::UFunction* function = nullptr;
        void* params = nullptr;

        void Cancel() noexcept {
            cancelled = true;
        }

        [[nodiscard]] bool IsCancelled() const noexcept {
            return cancelled;
        }

        template <typename T>
        [[nodiscard]] T* Params() const noexcept {
            return static_cast<T*>(params);
        }

    private:
        bool cancelled = false;
    };

    using HookCallback = std::function<void(ProcessEventContext&)>;

    [[nodiscard]] HookHandle Subscribe(std::string_view functionName, HookPhase phase, HookCallback callback);
    void Unsubscribe(HookHandle handle);

    GameHook(const GameHook&) = delete;
    GameHook& operator=(const GameHook&) = delete;

private:
    struct HookListener {
        HookHandle handle = INVALID_HOOK_HANDLE;
        HookCallback callback;
    };

    struct ListenerList {
        HookHandle firstHandle = INVALID_HOOK_HANDLE;
        HookCallback firstCallback;
        std::vector<HookListener> extra;

        [[nodiscard]] bool Empty() const noexcept { return firstHandle == INVALID_HOOK_HANDLE; }
    };

    struct HookEntry {
        uint64_t nameHash = 0;
        ListenerList before;
        ListenerList after;
    };

    struct HookIndexEntry {
        uint64_t nameHash = 0;
        size_t entryIndex = 0;
    };

    static constexpr size_t INVALID_INDEX = static_cast<size_t>(-1);

    struct ListenerLocation {
        size_t entryIndex = INVALID_INDEX;
        size_t slot = INVALID_INDEX;
        HookPhase phase = HookPhase::Before;
    };

    GameHook() = default;

    [[nodiscard]] HookHandle AllocateHandle() noexcept;
    [[nodiscard]] std::vector<HookIndexEntry>::iterator LowerBoundHookIndex(uint64_t nameHash) noexcept;
    [[nodiscard]] HookEntry* FindHookEntry(uint64_t nameHash) noexcept;
    [[nodiscard]] HookEntry* EnsureHookEntry(uint64_t nameHash);
    [[nodiscard]] static ListenerList& ListenersFor(HookEntry& entry, HookPhase phase) noexcept;
    [[nodiscard]] static size_t AddListener(ListenerList& listeners, HookHandle handle, HookCallback callback);
    void RemoveListenerAt(ListenerList& listeners, size_t slot, size_t entryIndex, HookPhase phase);
    static void DispatchListeners(ListenerList& listeners, ProcessEventContext& context);
    void RemoveHookEntryAt(size_t entryIndex) noexcept;
    void RelocateEntryListeners(HookEntry& entry, size_t entryIndex) noexcept;
    void InvalidateDispatchCaches() noexcept;
    [[nodiscard]] GameHookDetail::ProcessEventCacheSlot* ResolveAndCache(SDK::UFunction* function) noexcept;

    Logger logger{"GameHook"};
    uintptr_t oProcessEvent = 0;
    uintptr_t processEventAddress = 0;
    bool hooked = false;
    std::vector<HookEntry> hooks;
    std::vector<HookIndexEntry> hookIndex;
    std::vector<ListenerLocation> listenerIndex;
    HookHandle nextHookHandle = 1;

    static std::vector<QueuedAction> gameThreadQueue;
    static std::mutex queueMutex;
    static std::atomic<bool> hasQueuedActions;

    friend void __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* parms) noexcept;
};
