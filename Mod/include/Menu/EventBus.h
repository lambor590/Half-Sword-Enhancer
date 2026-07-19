#pragma once

#include <atomic>
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

    using EventCallback = std::function<void(const RuntimeContextSnapshot&)>;

    [[nodiscard]] SubscriptionHandle Subscribe(GameEvent event, EventCallback callback);
    void Unsubscribe(SubscriptionHandle handle);

    class SubscriptionGroup {
    public:
        SubscriptionGroup() = default;
        ~SubscriptionGroup();

        SubscriptionGroup(const SubscriptionGroup&) = delete;
        SubscriptionGroup& operator=(const SubscriptionGroup&) = delete;

        [[nodiscard]] SubscriptionHandle Subscribe(GameEvent event, EventCallback callback);
        void Clear();

    private:
        std::vector<SubscriptionHandle> handles;
    };

    void Dispatch(GameEvent event);

    static constexpr std::array EVENT_NAMES{"OnWalkingOffLedge", "ReceiveTick"};
    static constexpr size_t EVENT_COUNT = EVENT_NAMES.size();

    void Clear();

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

private:
    EventBus() = default;

    struct Subscriber {
        SubscriptionHandle handle = INVALID_SUBSCRIPTION;
        EventCallback callback;
    };

    void AddSubscriber(GameEvent event, SubscriptionHandle handle, EventCallback callback);
    void RemoveSubscriber(SubscriptionHandle handle);

    std::array<std::vector<Subscriber>, EVENT_COUNT> subscribers;
    std::array<GameHook::HookHandle, EVENT_COUNT> eventHookHandles{};
    std::atomic<SubscriptionHandle> nextHandle{1};
};
