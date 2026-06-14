#pragma once

#include <functional>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "Core/ModContext.h"
#include "Hooks/GameHook.h"
#include "Menu/GameEvent.h"

class ModContext;

struct KeybindParam {
    enum class Type : uint8_t { Int, Float, Bool };

    std::string_view name;
    std::string_view displayName;
    std::string_view tooltip;
    Type type;
    void* valuePtr;

    union {
        int intMin, intMax;
        float floatMin, floatMax;
    } minValue{}, maxValue{};

    mutable std::string id;

    KeybindParam(
        std::string_view name, std::string_view displayName, int* value, int minVal = 0, int maxVal = 100,
        std::string_view tooltip = ""
    ) noexcept;

    KeybindParam(
        std::string_view name, std::string_view displayName, float* value, float minVal = 0.0f, float maxVal = 1.0f,
        std::string_view tooltip = ""
    ) noexcept;

    KeybindParam(
        std::string_view name, std::string_view displayName, bool* value, std::string_view tooltip = ""
    ) noexcept;
};

struct KeybindFunctionHook {
    std::string_view functionName;
    GameHook::HookCallback callback;
    bool afterOriginal = false;
};

struct KeybindEntry {
    std::string name;
    std::string tooltip;
    std::string configSection;
    int* keyPtr = nullptr;
    std::function<void(bool, const RuntimeContextSnapshot&)> callback;
    bool runOnToggle = false;
    bool isEnabled = false;

    std::vector<GameEvent> events;
    std::vector<KeybindFunctionHook> functionHooks;

    /// UI state -- managed by rendering.
    bool waitingForKey = false;
    int prevKey = 0;
    int pendingOriginalKey = 0;
    int pendingConflictKey = 0;
    bool popupWasOpen = false;

    std::vector<KeybindParam> params;

    /// Cached ImGui IDs to avoid per-frame string allocation.
    std::string keyId;
    std::string checkId;
    std::string popupId;
    std::string paramButtonId;
    std::string conflictPopupId;

    bool IsToggle() const noexcept { return runOnToggle || !events.empty() || !functionHooks.empty(); }
    void Init();
    void Render();
};

class KeybindList {
public:
    void Add(KeybindEntry entry);
    void Render();

private:
    std::deque<KeybindEntry> entries;
};

/// Cached config read.
namespace TooltipHelper {
    void ShowTooltip(std::string_view tooltip);
    void InvalidateCache();
}
