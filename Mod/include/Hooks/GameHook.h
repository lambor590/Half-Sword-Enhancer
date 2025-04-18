#pragma once

#include <Windows.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <thread>
#include <vector>

#include "Logger.h"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Basic.hpp"
#include "MemoryUtils.h"

typedef void* (__stdcall* ProcessEvent)(SDK::UObject*, SDK::UFunction*, void*);

class GameHook
{
protected:
    inline static GameHook* s_instance = nullptr;

    GameHook() = default;

public:
    static GameHook& Get() {
        if (!s_instance) {
            s_instance = new GameHook();
        }
        return *s_instance;
    }

    void Hook();
    void Unhook() const;

    void RegisterHook(const std::string& functionName, std::function<void()> callback) {
        size_t hash = std::hash<std::string>{}(functionName);
        hookMap[hash] = { functionName, std::move(callback) };
    }

    void UnregisterHook(const std::string& functionName) {
        size_t hash = std::hash<std::string>{}(functionName);
        hookMap.erase(hash);
    }

    enum class GameEvent {
        BeginFight,
        OffLedge
    };

    void RegisterEvent(GameEvent event, std::function<void()> callback) {
        auto& vec = eventCallbacks[event];
        bool first = vec.empty();
        vec.push_back(std::move(callback));
        if (first) {
            const std::string& funcName = EventNames.at(event);
            RegisterHook(funcName, [this, event]() {
                for (auto& fn : eventCallbacks[event]) fn();
            });
        }
    }

    GameHook(const GameHook&) = delete;
    GameHook& operator=(const GameHook&) = delete;

private:
    struct HookData {
        std::string name;
        std::function<void()> callback;
    };

    Logger logger{ "GameHook" };
    uintptr_t oProcessEvent = NULL;
    std::unordered_map<size_t, HookData> hookMap;
    std::unordered_map<GameEvent, std::vector<std::function<void()>>> eventCallbacks;
    static inline const std::unordered_map<GameEvent, std::string> EventNames = {
        { GameEvent::BeginFight, "ExecuteUbergraph_UI_BeginFight" },
        { GameEvent::OffLedge,    "OnWalkingOffLedge" }
    };

    friend void* __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* Parms);
};