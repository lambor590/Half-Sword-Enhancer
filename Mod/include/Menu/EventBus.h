#pragma once

/// Integrates with GameHook's ProcessEvent dispatch to fire callbacks on game events.

#include <functional>
#include <vector>

#include "Core/ModContext.h"
#include "Menu/GameEvent.h"

/// All subscriptions and dispatches happen on the game thread via GameHook::QueueAction.
class EventBus {
public:
    static EventBus& Get();

    /// Callbacks fire on the game thread.
    void Subscribe(GameEvent event, void* id, std::function<void(const RuntimeContextSnapshot&)> callback);

    void Unsubscribe(GameEvent event, void* id);

    /// Called by GameHook's ProcessEvent path.
    void Dispatch(GameEvent event);

    /// Returns the UE function name that maps to this GameEvent for ProcessEvent hooking.
    static constexpr const char* GetEventFunctionName(GameEvent event) noexcept {
        constexpr const char* EVENT_NAMES[] = {
            "OnWalkingOffLedge",
            "ReceiveTick",
        };
        return EVENT_NAMES[static_cast<uint8_t>(event)];
    }

    static constexpr size_t EVENT_COUNT = 2;

    /// Remove all subscriptions and unregister all hooks. Called during Unhook.
    void Clear();

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

private:
    EventBus() = default;

    struct Subscriber {
        void* id;
        std::function<void(const RuntimeContextSnapshot&)> callback;
    };

    std::vector<Subscriber> subscribers[EVENT_COUNT];
};
