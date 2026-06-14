#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string_view>
#include <vector>

#include "Logger.h"
#include "Core/ModContext.h"

namespace SDK {
    class UObject;
    class UFunction;
}

using ProcessEvent = void(__stdcall*)(SDK::UObject*, SDK::UFunction*, void*);

class GameHook {
public:
    static GameHook& Get();

    [[nodiscard]] bool Hook();
    void Unhook();

    void SetUEConsoleEnabled(bool enabled);

    using QueuedAction = std::function<void(const RuntimeContextSnapshot&)>;

    static void QueueAction(QueuedAction action);
    static void ProcessGameThreadQueue();

    bool IsHooked() const noexcept { return hooked; }

    struct ProcessEventContext {
        SDK::UObject* object = nullptr;
        SDK::UFunction* function = nullptr;
        void* params = nullptr;

        template <typename T>
        [[nodiscard]] T* Params() const noexcept {
            return static_cast<T*>(params);
        }
    };

    using HookCallback = std::function<void(ProcessEventContext&)>;

    void RegisterHook(std::string_view functionName, HookCallback callback, bool afterOriginal = false);
    void UnregisterHook(std::string_view functionName);

    struct HookEntry {
        uint64_t nameHash = 0;
        HookCallback beforeCallback;
        HookCallback afterCallback;
    };

    GameHook(const GameHook&) = delete;
    GameHook& operator=(const GameHook&) = delete;

private:
    GameHook() = default;

    void RegisterHook(uint64_t hash, HookCallback callback, bool afterOriginal);
    void UnregisterHook(uint64_t hash);
    Logger logger{"GameHook"};
    uintptr_t oProcessEvent = 0;
    uintptr_t processEventAddress = 0;
    bool hooked = false;

    static constexpr size_t MAX_HOOKS = 32;
    std::array<HookEntry, MAX_HOOKS> hooks{};
    uint8_t hookCount = 0;

    static std::vector<QueuedAction> gameThreadQueue;
    static std::mutex queueMutex;
    static std::atomic<bool> hasQueuedActions;

    friend void __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* parms) noexcept;
};
