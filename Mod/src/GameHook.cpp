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
#include <cstring>
#include <intrin.h>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline __attribute__((always_inline))
#endif

std::vector<GameHook::QueuedAction> GameHook::gameThreadQueue;
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
    static thread_local uint32_t g_hookSuppressionDepth = 0;

    struct ScopedHookSuppression {
        ScopedHookSuppression() noexcept { ++g_hookSuppressionDepth; }
        ~ScopedHookSuppression() noexcept { --g_hookSuppressionDepth; }

        ScopedHookSuppression(const ScopedHookSuppression&) = delete;
        ScopedHookSuppression& operator=(const ScopedHookSuppression&) = delete;
    };
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
    const auto originalProcessEvent = std::bit_cast<ProcessEvent>(hook.oProcessEvent);
    if (g_hookSuppressionDepth > 0) [[unlikely]] {
        originalProcessEvent(pObject, pFunc, parms);
        return;
    }

    GameHook::ProcessEventContext context{pObject, pFunc, parms};
    const GameHook::HookEntry* entry = nullptr;

    const bool hasQueuedActions = GameHook::hasQueuedActions.load(std::memory_order_relaxed);
    if (hasQueuedActions || hook.hookCount) [[unlikely]] {
        const auto* slot = g_peCache.Lookup(pFunc);
        if (!slot) [[unlikely]] {
            slot = ResolveAndCache(pFunc, hook.hooks.data(), hook.hookCount);
        }

        if (hasQueuedActions && slot->isReceiveTick) [[unlikely]] {
            const ScopedHookSuppression suppressHooks;
            GameHook::ProcessGameThreadQueue();
        }

        if (slot->hookIdx >= 0) [[unlikely]] {
            entry = &hook.hooks[slot->hookIdx];
            if (entry->beforeCallback) {
                const ScopedHookSuppression suppressHooks;
                entry->beforeCallback(context);
            }
        }
    }

    originalProcessEvent(pObject, pFunc, parms);
    if (entry && entry->afterCallback) [[unlikely]] {
        const ScopedHookSuppression suppressHooks;
        entry->afterCallback(context);
    }
}

GameHook& GameHook::Get() {
    static GameHook instance;
    return instance;
}

bool GameHook::Hook() {
    processEventAddress = SDK::InSDKUtils::GetImageBase() + SDK::Offsets::ProcessEvent;
    if (!processEventAddress) {
        logger.Log("Failed to resolve ProcessEvent address");
        return false;
    }

    oProcessEvent = processEventAddress;

    if (!MemoryUtils::PlaceHook(oProcessEvent, (uintptr_t)OnProcessEvent, (uintptr_t*)&oProcessEvent)) {
        logger.Log("Failed to hook ProcessEvent");
        oProcessEvent = 0;
        processEventAddress = 0;
        return false;
    }

    hooked = true;

    if (ConfigManager::Get().GetBool("UE", "console_enabled", false)) {
        SetUEConsoleEnabled(true);
    }

    QueueAction([](const RuntimeContextSnapshot&) { GameBuildInfo::Query(); });

    return true;
}

void GameHook::Unhook() {
    hooked = false;
    MemoryUtils::Unhook(processEventAddress);
    for (uint8_t i = 0; i < hookCount; ++i)
        hooks[i] = {};
    hookCount = 0;
    g_peCache.Clear();
    EventBus::Get().Clear();
}

void GameHook::RegisterHook(std::string_view functionName, HookCallback callback, bool afterOriginal) {
    RegisterHook(HS::Hash::FNV1A(functionName), std::move(callback), afterOriginal);
}

void GameHook::UnregisterHook(std::string_view functionName) {
    UnregisterHook(HS::Hash::FNV1A(functionName));
}

void GameHook::RegisterHook(uint64_t hash, HookCallback callback, bool afterOriginal) {
    if (!callback) return;

    for (uint8_t i = 0; i < hookCount; ++i) {
        if (hooks[i].nameHash == hash) {
            if (afterOriginal) {
                hooks[i].afterCallback = std::move(callback);
            } else {
                hooks[i].beforeCallback = std::move(callback);
            }
            return;
        }
    }
    if (hookCount < MAX_HOOKS) {
        auto& entry = hooks[hookCount++];
        entry.nameHash = hash;
        if (afterOriginal) {
            entry.afterCallback = std::move(callback);
        } else {
            entry.beforeCallback = std::move(callback);
        }
        g_peCache.Clear();
    } else {
        logger.Log("Max ProcessEvent hooks reached");
    }
}

void GameHook::UnregisterHook(uint64_t hash) {
    for (uint8_t i = 0; i < hookCount; ++i) {
        if (hooks[i].nameHash == hash) {
            --hookCount;
            if (i != hookCount) {
                hooks[i] = std::move(hooks[hookCount]);
            }
            hooks[hookCount] = {};
            g_peCache.Clear();
            return;
        }
    }
}

void GameHook::SetUEConsoleEnabled(bool enabled) {
    QueueAction([enabled]([[maybe_unused]] const RuntimeContextSnapshot&) {
        auto& hook = GameHook::Get();
        SDK::UEngine* engine = SDK::UEngine::GetEngine();
        SDK::UGameViewportClient* viewport = engine ? engine->GameViewport : nullptr;

        if (enabled) {
            if (!engine || !viewport || !engine->ConsoleClass.Get()) {
                hook.logger.Log("UE Console unlock skipped: runtime was not ready");
                return;
            }

            if (!viewport->ViewportConsole) {
                SDK::UObject* newConsole = SDK::UGameplayStatics::SpawnObject(engine->ConsoleClass, viewport);
                if (!newConsole) {
                    hook.logger.Log("UE Console unlock failed: console object could not be created");
                    return;
                }

                viewport->ViewportConsole = static_cast<SDK::UConsole*>(newConsole);
            }
        } else if (viewport && viewport->ViewportConsole) {
            viewport->ViewportConsole = nullptr;
        }

        SDK::UInputSettings* inputSettings = SDK::UInputSettings::GetDefaultObj();
        if (inputSettings && inputSettings->ConsoleKeys.Num() > 0) {
            inputSettings->ConsoleKeys[0].KeyName =
                SDK::UKismetStringLibrary::Conv_StringToName(SDK::FString(enabled ? L"F2" : L"None"));
        }

        hook.logger.Log(enabled ? "UE Console unlocked - Press F2 to open console" : "UE Console locked");
    });
}

void GameHook::QueueAction(QueuedAction action) {
    std::lock_guard<std::mutex> lock(queueMutex);
    gameThreadQueue.push_back(std::move(action));
    hasQueuedActions.store(true, std::memory_order_release);
}

void GameHook::ProcessGameThreadQueue() {
    static thread_local std::vector<QueuedAction> localQueue;
    localQueue.clear();

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (gameThreadQueue.empty()) {
            hasQueuedActions.store(false, std::memory_order_release);
            return;
        }
        localQueue.swap(gameThreadQueue);
        hasQueuedActions.store(false, std::memory_order_release);
    }

    const auto snapshot = ModContext::Get().RefreshGameThreadCache();
    for (auto& action : localQueue) {
        action(snapshot);
    }
}
