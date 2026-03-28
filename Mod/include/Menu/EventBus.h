#pragma once

/// Lightweight game-event pub/sub system.
/// Replaces the event registration previously layered into IMenuFunction and GameHook.
/// Integrates with GameHook's ProcessEvent dispatch to fire callbacks on game events.

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

/// Game events that can be subscribed to via the EventBus.
enum class GameEvent : uint8_t {
    BeginFight,
    InAbyss,
    OffLedge,
    OnTick,
};

/// Simple game-event pub/sub bus.
/// Subscribe with an opaque id (typically `this` or `&entry`) so you can unsubscribe later.
/// All subscriptions and dispatches happen on the game thread via GameHook::QueueAction.
class EventBus {
public:
    static EventBus& Get();

    /// Subscribe to a game event. The callback fires on the game thread.
    /// @param event  The event to listen for.
    /// @param id     Opaque subscriber identifier (used for unsubscribe).
    /// @param callback  Invoked each time the event fires.
    void Subscribe(GameEvent event, void* id, std::function<void()> callback);

    /// Unsubscribe from a game event.
    /// @param event  The event to stop listening to.
    /// @param id     The same identifier passed to Subscribe.
    void Unsubscribe(GameEvent event, void* id);

    /// Dispatch all callbacks for the given event. Called by GameHook's ProcessEvent path.
    void Dispatch(GameEvent event);

    /// Returns the UE function name that maps to this GameEvent for ProcessEvent hooking.
    static constexpr const char* GetEventFunctionName(GameEvent event) noexcept {
        constexpr const char* EVENT_NAMES[] = {
            "ExecuteUbergraph_UI_BeginFight",
            "ExecuteUbergraph_Abyss_Map_Open_Intermediate",
            "OnWalkingOffLedge",
            "ReceiveTick",
        };
        return EVENT_NAMES[static_cast<uint8_t>(event)];
    }

    /// Total number of GameEvent values.
    static constexpr size_t EVENT_COUNT = 4;

    /// Remove all subscriptions and unregister all hooks. Called during Unhook.
    void Clear();

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

private:
    EventBus() = default;

    struct Subscriber {
        void* id;
        std::function<void()> callback;
    };

    std::vector<Subscriber> subscribers[EVENT_COUNT];
};
