#include "Menu/EventBus.h"

#include "Core/ModContext.h"
#include "Hooks/GameHook.h"

#include <algorithm>
#include <utility>

EventBus& EventBus::Get() {
    static EventBus instance;
    return instance;
}

EventBus::SubscriptionHandle EventBus::Subscribe(GameEvent event, EventCallback callback) {
    if (!callback) return INVALID_SUBSCRIPTION;

    const auto handle = nextHandle.fetch_add(1, std::memory_order_relaxed);
    if (handle == INVALID_SUBSCRIPTION) return INVALID_SUBSCRIPTION;

    return GameHook::QueueAction([this, event, handle,
                                  cb = std::move(callback)](const RuntimeContextSnapshot&) mutable {
        AddSubscriber(event, handle, std::move(cb));
    })
               ? handle
               : INVALID_SUBSCRIPTION;
}

void EventBus::Unsubscribe(SubscriptionHandle handle) {
    if (handle == INVALID_SUBSCRIPTION) return;

    GameHook::QueueAction([this, handle](const RuntimeContextSnapshot&) { RemoveSubscriber(handle); });
}

EventBus::SubscriptionGroup::~SubscriptionGroup() {
    Clear();
}

EventBus::SubscriptionGroup::SubscriptionGroup(SubscriptionGroup&& other) noexcept : handles(std::move(other.handles)) {
    other.handles.clear();
}

EventBus::SubscriptionGroup& EventBus::SubscriptionGroup::operator=(SubscriptionGroup&& other) noexcept {
    if (this == &other) return *this;

    Clear();
    handles = std::move(other.handles);
    other.handles.clear();
    return *this;
}

EventBus::SubscriptionHandle EventBus::SubscriptionGroup::Subscribe(GameEvent event, EventCallback callback) {
    const auto handle = EventBus::Get().Subscribe(event, std::move(callback));
    if (handle != INVALID_SUBSCRIPTION) handles.push_back(handle);
    return handle;
}

void EventBus::SubscriptionGroup::Clear() {
    for (const auto handle : handles) {
        EventBus::Get().Unsubscribe(handle);
    }
    handles.clear();
}

void EventBus::Dispatch(GameEvent event, GameHook::ProcessEventContext& processEvent) {
    const auto runtime = ModContext::Get().RefreshGameThreadCache();
    EventContext context{event, runtime, processEvent, event == GameEvent::OffLedge};
    auto& list = subscribers[static_cast<size_t>(event)];
    for (auto& subscriber : list) {
        subscriber.callback(context);
    }
}

void EventBus::Clear() {
    for (auto& hookHandle : eventHookHandles) {
        GameHook::Get().Unsubscribe(hookHandle);
        hookHandle = GameHook::INVALID_HOOK_HANDLE;
    }

    for (auto& list : subscribers) {
        list.clear();
    }
    nextHandle.store(1, std::memory_order_relaxed);
}

void EventBus::AddSubscriber(GameEvent event, SubscriptionHandle handle, EventCallback callback) {
    const auto idx = static_cast<size_t>(event);
    auto& list = subscribers[idx];
    const bool wasEmpty = list.empty();
    if (list.capacity() == 0) {
        list.reserve(event == GameEvent::OffLedge ? 32 : 4);
    }
    list.push_back({.handle = handle, .callback = std::move(callback)});

    if (wasEmpty) {
        eventHookHandles[idx] = GameHook::Get().Subscribe(
            GetEventFunctionName(event), GameHook::HookPhase::Before,
            [this, event](GameHook::ProcessEventContext& context) { Dispatch(event, context); }
        );
    }
}

void EventBus::RemoveSubscriber(SubscriptionHandle handle) {
    if (handle == INVALID_SUBSCRIPTION) return;

    for (size_t eventIndex = 0; eventIndex < subscribers.size(); ++eventIndex) {
        auto& list = subscribers[eventIndex];
        const auto subscriber = std::ranges::find(list, handle, &Subscriber::handle);
        if (subscriber == list.end()) continue;

        if (subscriber != list.end() - 1) *subscriber = std::move(list.back());
        list.pop_back();
        if (list.empty()) {
            GameHook::Get().Unsubscribe(eventHookHandles[eventIndex]);
            eventHookHandles[eventIndex] = GameHook::INVALID_HOOK_HANDLE;
        }
        return;
    }
}
