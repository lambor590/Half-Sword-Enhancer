#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string_view>

#include "Logger.h"
#include "Menu/GameEvent.h"

namespace SDK {
    class UObject;
    class UFunction;
}

using ProcessEvent = void(__stdcall*)(SDK::UObject*, SDK::UFunction*, void*);

class GameHook {
public:
    static GameHook& Get();

    void Hook();
    void Unhook();

    void UnlockUEConsole();
    void LockUEConsole();

    static void QueueAction(const std::function<void()>& action);
    static void ProcessGameThreadQueue();

    bool IsHooked() const noexcept { return hooked; }

    void RegisterHook(std::string_view functionName, const std::function<void()>& callback);
    void UnregisterHook(std::string_view functionName);

    /// Maps GameEvent to its UE function name.
    void RegisterHook(GameEvent event, const std::function<void()>& callback);
    void UnregisterHook(GameEvent event);

    struct HookEntry {
        uint64_t nameHash = 0;
        std::function<void()> callback;
    };

    GameHook(const GameHook&) = delete;
    GameHook& operator=(const GameHook&) = delete;

private:
    GameHook() = default;

    void RegisterHook(uint64_t hash, const std::function<void()>& callback);
    void UnregisterHook(uint64_t hash);

    Logger logger{"GameHook"};
    uintptr_t oProcessEvent = 0;
    uintptr_t processEventAddress = 0;
    bool hooked = false;

    static constexpr size_t MAX_HOOKS = 16;
    std::array<HookEntry, MAX_HOOKS> hooks{};
    uint8_t hookCount = 0;

    static std::queue<std::function<void()>> gameThreadQueue;
    static std::mutex queueMutex;
    static std::atomic<bool> hasQueuedActions;

    friend void __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* parms) noexcept;
};
