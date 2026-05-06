#include "Hooks/GameHook.h"
#include "ConfigManager.h"
#include "Core/ModContext.h"
#include "Menu/EventBus.h"
#include "MemoryUtils.h"
#include "Utils/CompileTimeHash.h"
#include "Utils/GameBuildInfo.h"

#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"

#include <bit>
#include <chrono>
#include <cstring>
#include <intrin.h>
#include <thread>

#ifdef _MSC_VER
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline __attribute__((always_inline))
#endif

std::queue<GameHook::QueuedAction> GameHook::gameThreadQueue;
std::mutex GameHook::queueMutex;
std::atomic<bool> GameHook::hasQueuedActions{false};

namespace {
    // Fibonacci hash: multiply by golden ratio constant, then shift to extract
    // high bits. This spreads clustered pointers (from pool allocators) across
    // the full table range. UFunction pointers from UE5's allocator tend to
    // differ only in low bits; a multiplicative hash uses ALL bits of the
    // pointer, yielding near-perfect distribution.
    FORCE_INLINE size_t FibonacciHash(uintptr_t key) noexcept {
        constexpr uint64_t PHI64 = 0x9E3779B97F4A7C15ULL;
        return static_cast<size_t>((key * PHI64) >> 52);
    }

    struct ProcessEventCache {
        static constexpr size_t TABLE_SIZE = 4096;
        static constexpr size_t TABLE_MASK = TABLE_SIZE - 1;
        static constexpr size_t MAX_PROBES = 8;
        static constexpr int8_t EMPTY = -2;
        static constexpr int8_t NOT_HOOKED = -1;
        static constexpr uint64_t RECEIVE_TICK_HASH = HS::Hash::FNV1A("ReceiveTick");

        struct alignas(16) Slot {
            SDK::UFunction* key;
            int8_t hookIdx;
            bool isReceiveTick;
        };

        static_assert(sizeof(Slot) == 16);

        alignas(64) Slot slots[TABLE_SIZE];

        ProcessEventCache() { Clear(); }

        void Clear() noexcept {
            std::memset(slots, 0, sizeof(slots));
            for (size_t i = 0; i < TABLE_SIZE; ++i) {
                slots[i].hookIdx = EMPTY;
            }
        }

        FORCE_INLINE const Slot* Lookup(SDK::UFunction* function) const noexcept {
            const auto raw = reinterpret_cast<uintptr_t>(function);
            const size_t idx = FibonacciHash(raw);

            // Prefetch the second cache line we might touch (idx+4 slots ahead).
            // The first cache line will be demand-loaded; the second is speculative.
            _mm_prefetch(reinterpret_cast<const char*>(&slots[(idx + 4) & TABLE_MASK]), _MM_HINT_T0);

            {
                const auto& s = slots[idx & TABLE_MASK];
                if (s.key == function) [[likely]]
                    return &s;
                if (s.hookIdx == EMPTY) return nullptr;
            }

            for (size_t i = 1; i < MAX_PROBES; ++i) {
                const auto& s = slots[(idx + i) & TABLE_MASK];
                if (s.key == function) return &s;
                if (s.hookIdx == EMPTY) return nullptr;
            }
            return nullptr;
        }

        Slot* Insert(SDK::UFunction* function, int8_t hookIdx, bool isReceiveTick) noexcept {
            const size_t idx = FibonacciHash(reinterpret_cast<uintptr_t>(function));
            for (size_t i = 0; i < MAX_PROBES; ++i) {
                auto& s = slots[(idx + i) & TABLE_MASK];
                if (s.hookIdx == EMPTY || s.key == function) {
                    s.key = function;
                    s.hookIdx = hookIdx;
                    s.isReceiveTick = isReceiveTick;
                    return &s;
                }
            }
            slots[idx & TABLE_MASK] = {function, hookIdx, isReceiveTick};
            return &slots[idx & TABLE_MASK];
        }
    };

    static ProcessEventCache g_peCache;
    static thread_local bool g_inProcessEventHook = false;
}

// Slow path: resolve a UFunction we haven't seen before. Marked noinline
// so the fast path in OnProcessEvent stays compact (fewer registers spilled,
// smaller icache footprint, better branch prediction).
__declspec(noinline) static const ProcessEventCache::Slot* ResolveAndCache(
    SDK::UFunction* function, const GameHook::HookEntry* hookArray, uint8_t hookCount
) noexcept {
    std::string funcName = function->GetName();
    uint64_t nameHash = HS::Hash::FNV1A(funcName.c_str());

    int8_t resolved = ProcessEventCache::NOT_HOOKED;
    for (uint8_t i = 0; i < hookCount; ++i) {
        if (hookArray[i].nameHash == nameHash) {
            resolved = static_cast<int8_t>(i);
            break;
        }
    }
    return g_peCache.Insert(function, resolved, nameHash == ProcessEventCache::RECEIVE_TICK_HASH);
}

void __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* parms) noexcept {
    auto& hook = GameHook::Get();
    if (g_inProcessEventHook) [[unlikely]] {
        std::bit_cast<ProcessEvent>(hook.oProcessEvent)(pObject, pFunc, parms);
        return;
    }

    g_inProcessEventHook = true;

    const bool hasQueuedActions = GameHook::hasQueuedActions.load(std::memory_order_relaxed);
    if (hasQueuedActions || hook.hookCount) [[unlikely]] {
        const auto* slot = g_peCache.Lookup(pFunc);
        if (!slot) [[unlikely]] {
            slot = ResolveAndCache(pFunc, hook.hooks.data(), hook.hookCount);
        }

        if (hasQueuedActions && slot->isReceiveTick) [[unlikely]] {
            GameHook::ProcessGameThreadQueue();
        }

        if (slot->hookIdx >= 0) [[unlikely]] {
            hook.hooks[slot->hookIdx].callback();
        }
    }

    std::bit_cast<ProcessEvent>(hook.oProcessEvent)(pObject, pFunc, parms);
    g_inProcessEventHook = false;
}

GameHook& GameHook::Get() {
    static GameHook instance;
    return instance;
}

void GameHook::Hook() {
    logger.Log("Hooking ProcessEvent");

    processEventAddress = SDK::InSDKUtils::GetImageBase() + SDK::Offsets::ProcessEvent;
    oProcessEvent = processEventAddress;

    MemoryUtils::PlaceHook(oProcessEvent, (uintptr_t)OnProcessEvent, (uintptr_t*)&oProcessEvent);

    hooked = true;

    if (ConfigManager::Get().GetBool("UE", "console_enabled", false)) {
        UnlockUEConsole();
    }

    QueueAction([](const RuntimeContextSnapshot&) { GameBuildInfo::Query(); });

    logger.Log("ProcessEvent hooked successfully!");
}

void GameHook::Unhook() {
    hooked = false;
    MemoryUtils::Unhook(processEventAddress);
    for (uint8_t i = 0; i < hookCount; ++i)
        hooks[i] = {};
    hookCount = 0;
    g_peCache.Clear();
    EventBus::Get().Clear();
    logger.Log("ProcessEvent unhooked successfully!");
}

void GameHook::RegisterHook(std::string_view functionName, const std::function<void()>& callback) {
    RegisterHook(HS::Hash::FNV1A(functionName), std::move(callback));
}

void GameHook::UnregisterHook(std::string_view functionName) {
    UnregisterHook(HS::Hash::FNV1A(functionName));
}

void GameHook::RegisterHook(GameEvent event, const std::function<void()>& callback) {
    RegisterHook(EventBus::GetEventFunctionName(event), std::move(callback));
}

void GameHook::UnregisterHook(GameEvent event) {
    UnregisterHook(EventBus::GetEventFunctionName(event));
}

void GameHook::RegisterHook(uint64_t hash, const std::function<void()>& callback) {
    for (uint8_t i = 0; i < hookCount; ++i) {
        if (hooks[i].nameHash == hash) {
            hooks[i].callback = std::move(callback);
            g_peCache.Clear();
            return;
        }
    }
    if (hookCount < MAX_HOOKS) {
        hooks[hookCount++] = {hash, std::move(callback)};
        g_peCache.Clear();
    }
}

void GameHook::UnregisterHook(uint64_t hash) {
    for (uint8_t i = 0; i < hookCount; ++i) {
        if (hooks[i].nameHash == hash) {
            hooks[i] = std::move(hooks[--hookCount]);
            hooks[hookCount] = {};
            g_peCache.Clear();
            return;
        }
    }
}

void GameHook::UnlockUEConsole() {
    SDK::UEngine* engine = SDK::UEngine::GetEngine();
    while (!engine) {
        logger.Log("Waiting for UEngine instance...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        engine = SDK::UEngine::GetEngine();
    }

    SDK::UInputSettings* inputSettings = SDK::UInputSettings::GetDefaultObj();
    if (inputSettings && inputSettings->ConsoleKeys.Num() > 0) {
        inputSettings->ConsoleKeys[0].KeyName = SDK::UKismetStringLibrary::Conv_StringToName(SDK::FString(L"F2"));
    }

    SDK::UGameViewportClient* viewport = engine->GameViewport;
    if (viewport) {
        if (!viewport->ViewportConsole) {
            SDK::UObject* newConsole = SDK::UGameplayStatics::SpawnObject(engine->ConsoleClass, engine->GameViewport);
            if (newConsole) {
                viewport->ViewportConsole = static_cast<SDK::UConsole*>(newConsole);
                logger.Log("Console object created successfully");
            }
        }
        logger.Log("Viewport input settings configured");
    }

    logger.Log("UE Console unlocked - Press F2 to open console");
}

void GameHook::LockUEConsole() {
    SDK::UEngine* engine = SDK::UEngine::GetEngine();
    if (!engine) {
        logger.Log("Failed to get UEngine instance");
        return;
    }

    SDK::UGameViewportClient* viewport = engine->GameViewport;
    if (viewport && viewport->ViewportConsole) {
        viewport->ViewportConsole = nullptr;
        logger.Log("Console object destroyed");
    }

    SDK::UInputSettings* inputSettings = SDK::UInputSettings::GetDefaultObj();
    if (inputSettings && inputSettings->ConsoleKeys.Num() > 0) {
        inputSettings->ConsoleKeys[0].KeyName = SDK::UKismetStringLibrary::Conv_StringToName(SDK::FString(L"None"));
        logger.Log("Console key disabled");
    }

    logger.Log("UE Console locked");
}

void GameHook::QueueAction(QueuedAction action) {
    std::lock_guard<std::mutex> lock(queueMutex);
    gameThreadQueue.push(std::move(action));
    hasQueuedActions.store(true, std::memory_order_release);
}

void GameHook::ProcessGameThreadQueue() {
    std::queue<QueuedAction> localQueue;

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (gameThreadQueue.empty()) return;
        localQueue.swap(gameThreadQueue);
        hasQueuedActions.store(false, std::memory_order_release);
    }

    while (!localQueue.empty()) {
        auto snapshot = ModContext::Get().RefreshGameThreadCache();
        localQueue.front()(snapshot);
        localQueue.pop();
    }
}
