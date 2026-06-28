#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "Core/ModContext.h"
#include "Hooks/GameHook.h"
#include "Menu/GameEvent.h"

class EventBus {
public:
    static EventBus& Get();

    using SubscriptionHandle = uint64_t;
    static constexpr SubscriptionHandle INVALID_SUBSCRIPTION = 0;

    class EventContext {
    public:
        EventContext(
            GameEvent event,
            const RuntimeContextSnapshot& runtime,
            GameHook::ProcessEventContext& processEvent,
            bool cancellable
        ) noexcept
            : event(event), runtime(runtime), processEvent(processEvent), cancellable(cancellable) {}

        [[nodiscard]] GameEvent Event() const noexcept { return event; }
        [[nodiscard]] const RuntimeContextSnapshot& Runtime() const noexcept { return runtime; }
        [[nodiscard]] const GameHook::ProcessEventContext& ProcessEvent() const noexcept { return processEvent; }
        [[nodiscard]] bool CanCancel() const noexcept { return cancellable; }
        [[nodiscard]] bool IsCancelled() const noexcept { return processEvent.IsCancelled(); }

        bool Cancel() const noexcept {
            if (!cancellable) return false;

            processEvent.Cancel();
            return true;
        }

        template <typename T>
        [[nodiscard]] T* Params() const noexcept {
            return processEvent.Params<T>();
        }

    private:
        GameEvent event;
        const RuntimeContextSnapshot& runtime;
        GameHook::ProcessEventContext& processEvent;
        bool cancellable = false;
    };

    using EventCallback = std::function<void(EventContext&)>;

    [[nodiscard]] SubscriptionHandle Subscribe(GameEvent event, EventCallback callback);
    void Unsubscribe(SubscriptionHandle handle);

    class SubscriptionGroup {
    public:
        SubscriptionGroup() = default;
        ~SubscriptionGroup();

        SubscriptionGroup(const SubscriptionGroup&) = delete;
        SubscriptionGroup& operator=(const SubscriptionGroup&) = delete;
        SubscriptionGroup(SubscriptionGroup&& other) noexcept;
        SubscriptionGroup& operator=(SubscriptionGroup&& other) noexcept;

        [[nodiscard]] SubscriptionHandle Subscribe(GameEvent event, EventCallback callback);
        void Clear();

    private:
        std::vector<SubscriptionHandle> handles;
    };

    void Dispatch(GameEvent event, GameHook::ProcessEventContext& processEvent);

    static constexpr const char* GetEventFunctionName(GameEvent event) noexcept {
        constexpr const char* EVENT_NAMES[] = {
            "OnWalkingOffLedge",
            "ReceiveTick",
        };
        return EVENT_NAMES[static_cast<uint8_t>(event)];
    }

    static constexpr size_t EVENT_COUNT = 2;

    void Clear();

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

private:
    static constexpr size_t INVALID_INDEX = static_cast<size_t>(-1);

    EventBus() = default;

    struct Subscriber {
        SubscriptionHandle handle = INVALID_SUBSCRIPTION;
        EventCallback callback;
    };

    struct SubscriberLocation {
        size_t eventIndex = INVALID_INDEX;
        size_t subscriberIndex = INVALID_INDEX;
    };

    void AddSubscriber(GameEvent event, SubscriptionHandle handle, EventCallback callback);
    void RemoveSubscriber(SubscriptionHandle handle);

    std::array<std::vector<Subscriber>, EVENT_COUNT> subscribers;
    std::array<GameHook::HookHandle, EVENT_COUNT> eventHookHandles{};
    std::vector<SubscriberLocation> subscriberIndex;
    SubscriptionHandle nextHandle = 1;
};
