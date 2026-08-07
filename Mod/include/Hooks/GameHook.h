#pragma once

#include <atomic>
#include <chrono>
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

using ProcessEvent = void(__stdcall*)(SDK::UObject*, SDK::UFunction*, void*);

class GameHook {
public:
    static GameHook& Get();

    [[nodiscard]] bool Hook();
    void Quiesce() noexcept;
    void Unhook();

    void SetUEConsoleEnabled(bool enabled);

    using QueuedAction = std::function<void(const RuntimeContextSnapshot&)>;

    static bool QueueAction(QueuedAction action);
    [[nodiscard]] bool ExecuteOnGameThreadAndWait(
        QueuedAction action, std::chrono::milliseconds timeout = std::chrono::seconds(5)
    );
    [[nodiscard]] bool IsGameThread() const noexcept;
    [[nodiscard]] bool IsHooked() const noexcept;

    enum class HookPhase : std::uint8_t { Before, After };

    using HookHandle = std::uint64_t;
    static constexpr HookHandle INVALID_HOOK_HANDLE = 0;

    struct ProcessEventContext {
        ProcessEventContext(SDK::UObject* object, SDK::UFunction* function, void* params) noexcept
            : object(object), function(function), params(params) {}

        SDK::UObject* object = nullptr;
        SDK::UFunction* function = nullptr;
        void* params = nullptr;

        void Cancel() noexcept { cancelled = true; }
        [[nodiscard]] bool IsCancelled() const noexcept { return cancelled; }

        template <typename T> [[nodiscard]] T* Params() const noexcept { return static_cast<T*>(params); }

    private:
        bool cancelled = false;
    };

    using HookCallback = std::function<void(ProcessEventContext&)>;

    [[nodiscard]] HookHandle Subscribe(std::string_view functionName, HookPhase phase, HookCallback callback);
    void Unsubscribe(HookHandle handle);
    [[nodiscard]] bool IsSubscribed(HookHandle handle) noexcept;

    class SubscriptionGroup {
    public:
        [[nodiscard]] HookHandle Subscribe(std::string_view functionName, HookPhase phase, HookCallback callback);
        void Reset() noexcept;
        [[nodiscard]] bool IsSubscribed() const noexcept;

        SubscriptionGroup() = default;
        SubscriptionGroup(const SubscriptionGroup&) = delete;
        SubscriptionGroup& operator=(const SubscriptionGroup&) = delete;

    private:
        std::vector<HookHandle> handles;
    };

    GameHook(const GameHook&) = delete;
    GameHook& operator=(const GameHook&) = delete;

private:
    enum class DispatchMode : std::uint64_t { Bypass, Running, Blocked };

    class DispatchLease {
    public:
        explicit DispatchLease(GameHook& owner) noexcept;
        ~DispatchLease();
        [[nodiscard]] bool DispatchHooks() const noexcept { return dispatchHooks; }

        DispatchLease(const DispatchLease&) = delete;
        DispatchLease& operator=(const DispatchLease&) = delete;

    private:
        GameHook& owner;
        bool dispatchHooks = false;
        bool ownsDispatch = false;
    };

    struct HookListener {
        HookHandle handle = INVALID_HOOK_HANDLE;
        HookCallback callback;
    };

    using ListenerList = std::vector<HookListener>;

    struct HookEntry {
        std::uint64_t nameHash = 0;
        ListenerList before;
        ListenerList after;
    };

    static constexpr std::uint64_t DISPATCH_MODE_SHIFT = 62;
    static constexpr std::uint64_t DISPATCH_COUNT_MASK = (std::uint64_t{1} << DISPATCH_MODE_SHIFT) - 1;

    GameHook() = default;

    static constexpr std::uint64_t DispatchState(DispatchMode mode, std::uint64_t count = 0) noexcept {
        return (static_cast<std::uint64_t>(mode) << DISPATCH_MODE_SHIFT) | count;
    }
    static constexpr DispatchMode ModeOf(std::uint64_t state) noexcept {
        return static_cast<DispatchMode>(state >> DISPATCH_MODE_SHIFT);
    }
    static constexpr std::uint64_t DispatchCount(std::uint64_t state) noexcept { return state & DISPATCH_COUNT_MASK; }

    [[nodiscard]] bool BeginDispatch() noexcept;
    void EndDispatch() noexcept;
    [[nodiscard]] bool IsDispatchRunning() const noexcept;
    void SetDispatchMode(DispatchMode mode) noexcept;
    void WaitForDispatches() noexcept;
    [[nodiscard]] bool CanAccessRegistry() const noexcept;
    [[nodiscard]] HookEntry* FindHookEntry(std::uint64_t nameHash) noexcept;
    static void DispatchListeners(const ListenerList& listeners, ProcessEventContext& context);

    Logger logger{"GameHook"};
    uintptr_t oProcessEvent = 0;
    uintptr_t processEventAddress = 0;
    std::atomic<std::uint64_t> dispatchState{DispatchState(DispatchMode::Bypass)};
    std::atomic<std::uint32_t> gameThreadId{0};
    std::atomic<bool> hasListeners{false};
    std::vector<HookEntry> hooks;
    HookHandle nextHookHandle = 1;
    std::uint32_t registryGeneration = 1;

    std::vector<QueuedAction> gameThreadQueue;
    std::mutex queueMutex;
    std::atomic<bool> hasQueuedActions{false};

    friend void __stdcall OnProcessEvent(SDK::UObject* object, SDK::UFunction* function, void* params) noexcept;
};
