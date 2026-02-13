#pragma once

#include <array>
#include <queue>
#include <mutex>
#include <functional>

#include "Utils/CommonHeaders.h"
#include "Logger.h"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Basic.hpp"
#include "MemoryUtils.h"

typedef void* (__stdcall* ProcessEvent)(SDK::UObject*, SDK::UFunction*, void*);


class GameHook
{
private:
    GameHook() = default;

public:
    static GameHook& Get() {
        static GameHook instance;
        return instance;
    }

    void Hook();
    void Unhook();

    void UnlockUEConsole();
    void LockUEConsole();

    static void QueueAction(std::function<void()> action);
    static void ProcessGameThreadQueue();

    static void SetInputEnabled(bool enabled);

    bool IsHooked() const noexcept { return hooked; }

    enum class GameEvent : uint8_t {
        BeginFight,
        InAbyss,
        OffLedge,
        OnTick
    };

    void RegisterEvent(GameEvent event, void* id, std::function<void()> callback) {
        QueueAction([this, event, id, cb = std::move(callback)]() mutable {
            uint8_t idx = static_cast<uint8_t>(event);
            auto& vec = eventCallbacks[idx];
            bool first = vec.empty();
            vec.emplace_back(id, std::move(cb));
            if (first) {
                const char* funcName = GetEventFunctionName(event);
                RegisterHook(funcName, [this, event]() {
                    uint8_t eventIdx = static_cast<uint8_t>(event);
                    auto& callbacks = eventCallbacks[eventIdx];
                    for (auto& pair : callbacks) {
                        pair.second();
                    }
                });
            }
        });
    }

    void UnregisterEvent(GameEvent event, void* id) {
        QueueAction([this, event, id]() {
            uint8_t idx = static_cast<uint8_t>(event);
            auto& vec = eventCallbacks[idx];
            for (size_t i = 0; i < vec.size(); ++i) {
                if (vec[i].first == id) {
                    vec[i] = std::move(vec.back());
                    vec.pop_back();
                    break;
                }
            }
            if (vec.empty()) {
                const char* funcName = GetEventFunctionName(event);
                UnregisterHook(funcName);
            }
        });
    }

    static constexpr const char* GetEventFunctionName(GameEvent event) noexcept {
        constexpr const char* EventNames[] = {
            "ExecuteUbergraph_UI_BeginFight",
            "ExecuteUbergraph_Abyss_Map_Open_Intermediate", 
            "OnWalkingOffLedge",
            "ReceiveTick"
        };
        return EventNames[static_cast<uint8_t>(event)];
    }
    

    struct HookEntry {
        uint64_t nameHash = 0;
        std::function<void()> callback;
    };

    GameHook(const GameHook&) = delete;
    GameHook& operator=(const GameHook&) = delete;

private:
    void RegisterHook(const std::string& functionName, std::function<void()> callback);
    void RegisterHook(uint64_t hash, std::function<void()> callback);
    void UnregisterHook(const std::string& functionName);
    void UnregisterHook(uint64_t hash);

    Logger logger{ "GameHook" };
    uintptr_t oProcessEvent = NULL;
    uintptr_t processEventAddress = 0;
    bool hooked = false;

    static constexpr size_t MAX_HOOKS = 16;
    alignas(64) std::array<HookEntry, MAX_HOOKS> hooks{};
    uint8_t hookCount = 0;

    alignas(64) std::array<std::vector<std::pair<void*, std::function<void()>>>, 4> eventCallbacks;

    static std::queue<std::function<void()>> gameThreadQueue;
    static std::mutex queueMutex;

    friend void* __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* Parms) noexcept;
};