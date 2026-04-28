#include "Menu/EventBus.h"
#include "Core/ModContext.h"
#include "Hooks/GameHook.h"

EventBus& EventBus::Get() {
    static EventBus instance;
    return instance;
}

void EventBus::Subscribe(GameEvent event, void* id, std::function<void()> callback) {
    GameHook::QueueAction([this, event, id, cb = std::move(callback)]() mutable {
        uint8_t idx = static_cast<uint8_t>(event);
        auto& vec = subscribers[idx];
        bool first = vec.empty();
        vec.push_back({id, std::move(cb)});
        if (first) {
            GameHook::Get().RegisterHook(event, [this, event]() { Dispatch(event); });
        }
    });
}

void EventBus::Unsubscribe(GameEvent event, void* id) {
    GameHook::QueueAction([this, event, id]() {
        uint8_t idx = static_cast<uint8_t>(event);
        auto& vec = subscribers[idx];
        for (size_t i = 0; i < vec.size(); ++i) {
            if (vec[i].id == id) {
                vec[i] = std::move(vec.back());
                vec.pop_back();
                break;
            }
        }
        if (vec.empty()) {
            GameHook::Get().UnregisterHook(event);
        }
    });
}

void EventBus::Dispatch(GameEvent event) {
    ModContext::Get().RefreshCache();

    uint8_t idx = static_cast<uint8_t>(event);
    auto& vec = subscribers[idx];
    for (auto& [_, cb] : vec) {
        cb();
    }
}

void EventBus::Clear() {
    for (auto& vec : subscribers) {
        vec.clear();
    }
}
