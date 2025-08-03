#pragma once

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
    void Unhook() const;

    void RegisterHook(const std::string& functionName, std::function<void()> callback);

    void UnregisterHook(const std::string& functionName);

    void UnlockUEConsole();
    void LockUEConsole();

    static void QueueAction(std::function<void()> action);
    static void ProcessGameThreadQueue();

    enum class GameEvent : uint8_t {
        BeginFight,
        InAbyss,
        OffLedge,
        OnTick
    };

    void RegisterEvent(GameEvent event, void* id, std::function<void()> callback) {
        uint8_t idx = static_cast<uint8_t>(event);
        auto& vec = eventCallbacks[idx];
        bool first = vec.empty();
        vec.emplace_back(id, std::move(callback));
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
    }

    void UnregisterEvent(GameEvent event, void* id) {
        uint8_t idx = static_cast<uint8_t>(event);
        auto& vec = eventCallbacks[idx];
        vec.erase(std::remove_if(vec.begin(), vec.end(), 
            [id](const auto& p) { return p.first == id; }), vec.end());
        if (vec.empty()) {
            const char* funcName = GetEventFunctionName(event);
            UnregisterHook(funcName);
        }
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
    

    GameHook(const GameHook&) = delete;
    GameHook& operator=(const GameHook&) = delete;

private:

    Logger logger{ "GameHook" };
    uintptr_t oProcessEvent = NULL;
    std::unordered_map<uint64_t, std::function<void()>> hookMap;
    std::array<std::vector<std::pair<void*, std::function<void()>>>, 4> eventCallbacks;
    
    static std::queue<std::function<void()>> gameThreadQueue;
    static std::mutex queueMutex;
    
    static constexpr size_t HOOK_MAP_RESERVE_SIZE = 64;
    
    friend void* __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* Parms);
};