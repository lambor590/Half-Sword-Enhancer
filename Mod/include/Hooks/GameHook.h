#pragma once

#include <Windows.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <thread>
#include <vector>
#include <utility>
#include <string_view>
#include <array>

#include "Logger.h"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Basic.hpp"
#include "MemoryUtils.h"

typedef void* (__stdcall* ProcessEvent)(SDK::UObject*, SDK::UFunction*, void*);

constexpr uint64_t fnv1a_hash(std::string_view str) noexcept {
    constexpr uint64_t offset_basis = 14695981039346656037ULL;
    constexpr uint64_t prime = 1099511628211ULL;
    
    uint64_t hash = offset_basis;
    for (char c : str) {
        hash ^= static_cast<uint64_t>(c);
        hash *= prime;
    }
    return hash;
}

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

    void RegisterHook(const std::string& functionName, std::function<void()> callback) {
        auto [hookClass, hookFunc] = ParseFunctionName(functionName);
        uint64_t hash = fnv1a_hash(hookFunc);
        hookMap[hash] = { std::string(hookClass), std::string(hookFunc), std::move(callback) };
    }

    void UnregisterHook(const std::string& functionName) {
        auto [_, hookFunc] = ParseFunctionName(functionName);
        uint64_t hash = fnv1a_hash(hookFunc);
        hookMap.erase(hash);
    }

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
            "Willie_BP_C::ReceiveTick"
        };
        return EventNames[static_cast<uint8_t>(event)];
    }

    GameHook(const GameHook&) = delete;
    GameHook& operator=(const GameHook&) = delete;

private:
    static constexpr std::pair<std::string_view, std::string_view> ParseFunctionName(std::string_view functionName) noexcept {
        auto pos = functionName.find("::");
        if (pos != std::string_view::npos) {
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
    std::unordered_map<uint64_t, HookData> hookMap;
    std::array<std::vector<std::pair<void*, std::function<void()>>>, 4> eventCallbacks;

    friend void* __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* Parms);
};