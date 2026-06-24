#include "Menu/EventBus.h"
#include "Core/ModContext.h"
#include "Hooks/GameHook.h"

EventBus& EventBus::Get() {
    static EventBus instance;
    return instance;
}

void EventBus::Subscribe(GameEvent event, void* id, std::function<void(const RuntimeContextSnapshot&)> callback) {
    GameHook::QueueAction([this, event, id, cb = std::move(callback)](const RuntimeContextSnapshot&) mutable {
        auto idx = static_cast<uint8_t>(event);
        auto& vec = subscribers[idx];
        bool first = vec.empty();
        vec.push_back({id, std::move(cb)});
        if (first) {
            GameHook::Get().RegisterHook(
                GetEventFunctionName(event), [this, event](GameHook::ProcessEventContext&) { Dispatch(event); }
            );
        }
    });
}

void EventBus::Unsubscribe(GameEvent event, void* id) {
    GameHook::QueueAction([this, event, id](const RuntimeContextSnapshot&) {
        auto idx = static_cast<uint8_t>(event);
        auto& vec = subscribers[idx];
        for (size_t i = 0; i < vec.size(); ++i) {
            if (vec[i].id == id) {
                vec[i] = std::move(vec.back());
                vec.pop_back();
                break;
            }
        }
        if (vec.empty()) {
            GameHook::Get().UnregisterHook(GetEventFunctionName(event));
        }
    });
}

void EventBus::Dispatch(GameEvent event) {
    auto runtime = ModContext::Get().RefreshGameThreadCache();

    auto idx = static_cast<uint8_t>(event);
    auto& vec = subscribers[idx];
    for (auto& [_, cb] : vec) {
        cb(runtime);
    }
}

void EventBus::Clear() {
    for (auto& vec : subscribers) {
        vec.clear();
    }
}
