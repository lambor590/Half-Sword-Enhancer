#pragma once

#include <atomic>
#include <functional>
#include <cstdint>
#include <deque>
#include <string>
#include <type_traits>
#include <vector>

#include "Core/ModContext.h"
#include "Hooks/GameHook.h"
#include "Menu/EventBus.h"
#include "Menu/GameEvent.h"

class ModContext;

struct KeybindParam {
    enum class Type : uint8_t { Int, Float, Double, Bool };

    const char* name;
    const char* displayName;
    const char* tooltip;
    Type type;
    void* valuePtr;

    float minValue = 0.0f;
    float maxValue = 0.0f;

    KeybindParam(
        const char* name, const char* displayName, int* value, int minVal = 0, int maxVal = 100,
        const char* tooltip = ""
    );

    KeybindParam(
        const char* name, const char* displayName, float* value, float minVal = 0.0f, float maxVal = 1.0f,
        const char* tooltip = ""
    );

    KeybindParam(
        const char* name, const char* displayName, double* value, double minVal = 0.0, double maxVal = 1.0,
        const char* tooltip = ""
    );

    KeybindParam(const char* name, const char* displayName, bool* value, const char* tooltip = "");
};

struct KeybindFunctionHook {
    const char* functionName;
    GameHook::HookCallback callback;
    GameHook::HookPhase phase = GameHook::HookPhase::Before;
    GameHook::HookHandle handle = GameHook::INVALID_HOOK_HANDLE;
};

enum class KeybindKind : uint8_t { Command, State };

struct KeybindEntry {
    std::string name;
    std::string tooltip;
    std::string configSection;
    int* keyPtr = nullptr;
    std::function<void(bool, const RuntimeContextSnapshot&)> callback;
    KeybindKind kind = KeybindKind::Command;
    std::function<bool()> stateGetter;
    std::function<bool()> available;
    bool applyOnToggle = false;
    std::atomic_bool isActive = false;
    bool persistParams = true;

    std::vector<GameEvent> events;
    EventBus::SubscriptionGroup eventSubscriptions;
    std::vector<KeybindFunctionHook> functionHooks;

    std::vector<KeybindParam> params;
    std::function<void()> onParamsChanged;
    const char* group = "";
    bool destructive = false;

    /// UI state -- managed by rendering.
    int pendingOriginalKey = 0;
    int pendingConflictKey = 0;
    bool configPopupOpenLastFrame = false;
    bool configDirty = false;
    float naturalWidth = 0.0f;

    bool IsState() const noexcept { return kind == KeybindKind::State; }
    bool CurrentState() const { return stateGetter ? stateGetter() : isActive.load(std::memory_order_acquire); }
    bool IsAvailable() const { return !available || available(); }
    ~KeybindEntry();

    // Definitions are assembled as aggregates, then transferred into their final stable storage before Init().
    // A registered entry must never move because runtime callbacks retain its address.
    void AdoptDefinition(KeybindEntry& source) noexcept;
    void Init();
    void Render(bool highlight = false, bool scrollIntoView = false, float cellWidth = 0.0f);
};

static_assert(!std::is_copy_constructible_v<KeybindEntry> && !std::is_move_constructible_v<KeybindEntry>);

class KeybindList {
public:
    void Add(KeybindEntry&& entry);
    void Render();
    void RequestHighlight(const KeybindEntry* entry);
    std::deque<KeybindEntry>& Entries() noexcept { return entries; }

private:
    std::deque<KeybindEntry> entries;
    const KeybindEntry* highlightedEntry = nullptr;
    double highlightUntil = 0.0;
    bool scrollHighlightedEntry = false;
};

namespace KeybindUi {
    [[nodiscard]] float CalculateKeycapWidth(const char* label, float maximumWidth);
    [[nodiscard]] bool RenderKeycap(const char* id, const char* label, float width);
}

namespace KeybindRuntime {
    // UI thread. Persists any in-progress popup edits before navigation hides their controls.
    void FlushPendingParamChanges() noexcept;
    // Game-thread only. Rehydrates enabled event/function hooks after GameHook restarts.
    void OnRuntimeStart();
    // Game-thread only. Releases runtime subscriptions without changing persisted enabled state.
    void OnRuntimeShutdown() noexcept;
}
