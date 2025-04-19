#pragma once

#include <Windows.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <thread>
#include <vector>
#include <utility>

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
        auto [hookClass, hookFunc] = ParseFunctionName(functionName);
        size_t hash = std::hash<std::string>{}(hookFunc);
        hookMap[hash] = { std::move(hookClass), std::move(hookFunc), std::move(callback) };
    }

    void UnregisterHook(const std::string& functionName) {
        auto [_, hookFunc] = ParseFunctionName(functionName);
        size_t hash = std::hash<std::string>{}(hookFunc);
        hookMap.erase(hash);
    }

    enum class GameEvent {
        BeginFight,
        InAbyss,
        OffLedge
    };

    void RegisterEvent(GameEvent event, void* id, std::function<void()> callback) {
        auto& vec = eventCallbacks[event];
        bool first = vec.empty();
        vec.emplace_back(id, std::move(callback));
        if (first) {
            const std::string& funcName = EventNames.at(event);
            RegisterHook(funcName, [this, event]() {
                for (auto& [key, fn] : eventCallbacks[event]) fn();
            });
        }
    }

    void UnregisterEvent(GameEvent event, void* id) {
        auto it = eventCallbacks.find(event);
        if (it == eventCallbacks.end()) return;
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(), [&](auto& p){ return p.first == id; }), vec.end());
        if (vec.empty()) {
            const std::string& funcName = EventNames.at(event);
            UnregisterHook(funcName);
        }
    }

    static const std::string& GetEventFunctionName(GameEvent event) {
        return EventNames.at(event);
    }

    GameHook(const GameHook&) = delete;
    GameHook& operator=(const GameHook&) = delete;

private:
    static std::pair<std::string, std::string> ParseFunctionName(const std::string& functionName) {
        auto pos = functionName.find("::");
        if (pos != std::string::npos) {
            return {functionName.substr(0, pos), functionName.substr(pos + 2)};
        }
        return {"", functionName};
    }

    struct HookData {
        std::string className;
        std::string funcName;
        std::function<void()> callback;
    };

    Logger logger{ "GameHook" };
    uintptr_t oProcessEvent = NULL;
    std::unordered_map<size_t, HookData> hookMap;
    std::unordered_map<GameEvent, std::vector<std::pair<void*, std::function<void()>>>> eventCallbacks;
    static inline const std::unordered_map<GameEvent, std::string> EventNames = {
        { GameEvent::BeginFight, "ExecuteUbergraph_UI_BeginFight" },
        { GameEvent::InAbyss, "ExecuteUbergraph_Abyss_Map_Open_Intermediate" },
        { GameEvent::OffLedge, "OnWalkingOffLedge" }
    };

    friend void* __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* Parms);
};