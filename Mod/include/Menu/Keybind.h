#pragma once

/// Lightweight data-driven keybind system.
/// Replaces the old IMenuFunction class hierarchy
/// with plain structs and free utility functions.

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "Hooks/GameHook.h"

class ModContext;

/// Describes a single configurable parameter attached to a keybind action.
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

/// A single keybind entry: key assignment, toggle state, callback, and optional parameters.
struct KeybindEntry {
    std::string name;
    std::string tooltip;
    std::string configSection;
    int* keyPtr = nullptr;
    std::function<void(bool)> callback;
    bool toggleable = false;
    bool isEnabled = false;
    bool gameThread = false;

    /// Game events this keybind subscribes to (fires callback periodically).
    std::vector<GameHook::GameEvent> events;

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
};

/// Tooltip helper (cached config read).
namespace TooltipHelper {
    void ShowTooltip(std::string_view tooltip);
    void InvalidateCache();
}

/// Keybind UI rendering utilities.
namespace KeybindUI {
    /// Render a single keybind row: key button + toggle checkbox + name + params popup.
    void RenderKeybind(KeybindEntry& entry);

    /// Render all keybinds in a section with spacing.
    void RenderKeybindList(std::vector<KeybindEntry>& entries);
}

/// Keybind persistence utilities.
namespace KeybindConfig {
    /// Load key assignment and enabled state from ConfigManager.
    void LoadKeybind(KeybindEntry& entry);

    /// Save key assignment and enabled state to ConfigManager.
    void SaveKeybind(const KeybindEntry& entry);

    /// Load a parameter value from ConfigManager.
    void LoadParam(const KeybindParam& param, std::string_view configSection);

    /// Save a parameter value to ConfigManager.
    void SaveParam(const KeybindParam& param, std::string_view configSection);

    /// Load all parameters for a keybind entry.
    void LoadParams(KeybindEntry& entry);

    /// Save all parameters for a keybind entry.
    void SaveParams(const KeybindEntry& entry);
}

/// Initialize a keybind entry: generate ImGui IDs, load config, register with
/// KeybindManager and GameHook events. Call once after populating the entry fields.
void InitKeybindEntry(KeybindEntry& entry);
