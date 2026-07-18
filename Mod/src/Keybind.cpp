#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ranges>
#include <string>
#include <utility>

#include "Menu/Keybind.h"
#include "Menu/EventBus.h"
#include "Hooks/GameHook.h"
#include "imgui/imgui.h"
#include "ConfigManager.h"
#include "KeybindManager.h"
#include "Logger.h"
#include "NotificationManager.h"
#include "DefaultStyle.h"
#include "Utils/GuiUtils.h"

KeybindParam::KeybindParam(
    const char* name, const char* displayName, int* value, int minVal, int maxVal, const char* tooltip
)
    : name(name), displayName(displayName), tooltip(tooltip), type(Type::Int), valuePtr(value) {
    minValue = static_cast<float>(minVal);
    maxValue = static_cast<float>(maxVal);
}

KeybindParam::KeybindParam(
    const char* name, const char* displayName, float* value, float minVal, float maxVal, const char* tooltip
)
    : name(name), displayName(displayName), tooltip(tooltip), type(Type::Float), valuePtr(value) {
    minValue = minVal;
    maxValue = maxVal;
}

KeybindParam::KeybindParam(
    const char* name, const char* displayName, double* value, double minVal, double maxVal, const char* tooltip
)
    : name(name), displayName(displayName), tooltip(tooltip), type(Type::Double), valuePtr(value) {
    minValue = static_cast<float>(minVal);
    maxValue = static_cast<float>(maxVal);
}

KeybindParam::KeybindParam(const char* name, const char* displayName, bool* value, const char* tooltip)
    : name(name), displayName(displayName), tooltip(tooltip), type(Type::Bool), valuePtr(value) {}

namespace KeybindConfig {
    void LoadKeybind(KeybindEntry& entry);
    void SaveKeybind(const KeybindEntry& entry);
    void SaveParams(const KeybindEntry& entry);
}

namespace {
    Logger g_keybindRuntimeLogger{"KeybindRuntime"};

    std::vector<KeybindEntry*>& RuntimeEntries() {
        static std::vector<KeybindEntry*> entries;
        return entries;
    }

    constexpr ImVec4 MODAL_DIM_COLOR{0, 0, 0, 0.6f};

    constexpr float ACTION_MIN_WIDTH = 72.0f;
    constexpr float PARAM_CONTROL_WIDTH = 104.0f;
    constexpr float KEYCAP_MIN_WIDTH = 48.0f;
    constexpr float KEYCAP_MAX_WIDTH = 112.0f;
    constexpr float ACTION_KEYCAP_WIDTH = 64.0f;
    constexpr float ACTION_GROUP_ROUNDING = 4.0f;
    constexpr float ACTION_GROUP_BACKGROUND_ALPHA = 0.52f;
    constexpr char PRESS_KEY_TEXT[] = "Press a key...";
    constexpr char ADD_SHORTCUT_TEXT[] = "+ Key";
    constexpr char CHANGE_KEYBIND_TEXT[] = "Choose a shortcut";
    constexpr char KEY_CONFLICT_FORMAT[] = "%s already controls %s. Choose how to use it.";
    constexpr char KEY_MULTI_CONFLICT_FORMAT[] = "%s already controls %d actions. Choose how to use it.";

    constexpr bool IsKeyUnbound(int key) noexcept {
        return key <= 0 || key == 255;
    }

    struct SliderStyleRAII {
        SliderStyleRAII() {
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, DefaultStyle::OLD_BRASS);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, DefaultStyle::BRIGHT_BRASS);
        }
        ~SliderStyleRAII() { ImGui::PopStyleColor(2); }
    };

    struct KeycapStyleRAII {
        KeycapStyleRAII() {
            ImVec4 border = DefaultStyle::OLD_BRASS;
            border.w = 0.65f;
            ImGui::PushStyleColor(ImGuiCol_Button, DefaultStyle::CLEAR);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DefaultStyle::HEADER);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, DefaultStyle::HEADER_ACTIVE);
            ImGui::PushStyleColor(ImGuiCol_Border, border);
            ImGui::PushStyleColor(ImGuiCol_Text, DefaultStyle::PARCHMENT_DARK);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        }
        ~KeycapStyleRAII() {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(5);
        }
    };

    [[nodiscard]] bool RenderCompactButton(
        const char* id, const char* label, float width, GuiUtils::ButtonTone tone = GuiUtils::ButtonTone::Default
    ) {
        ImGui::PushID(id);
        const bool pressed = GuiUtils::Button(label, tone, ImVec2((std::max)(1.0f, width), 0.0f));
        ImGui::PopID();
        return pressed;
    }

    struct SegmentInteraction {
        bool pressed = false;
        bool hovered = false;
        bool held = false;
        bool textClipped = false;
        ImVec2 min{};
        ImVec2 max{};
    };

    [[nodiscard]] SegmentInteraction RenderSegmentButton(const char* id, float width) {
        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_Button, DefaultStyle::CLEAR);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DefaultStyle::CLEAR);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, DefaultStyle::CLEAR);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        SegmentInteraction result;
        result.pressed = ImGui::Button("##segment", ImVec2((std::max)(1.0f, width), ImGui::GetFrameHeight()));
        result.hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
        result.held = ImGui::IsItemActive();
        result.min = ImGui::GetItemRectMin();
        result.max = ImGui::GetItemRectMax();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        ImGui::PopID();
        return result;
    }

    void DrawSegmentFill(const SegmentInteraction& segment, ImVec4 color, ImDrawFlags corners) {
        if (color.w <= 0.0f) return;
        ImGui::GetWindowDrawList()
            ->AddRectFilled(segment.min, segment.max, ImGui::GetColorU32(color), ACTION_GROUP_ROUNDING, corners);
    }

    void DrawActionGroupBackground(const ImVec2& min, float width) {
        ImVec4 background = DefaultStyle::DARK_WOOD;
        background.w = ACTION_GROUP_BACKGROUND_ALPHA;
        ImGui::GetWindowDrawList()->AddRectFilled(
            min, ImVec2(min.x + width, min.y + ImGui::GetFrameHeight()), ImGui::GetColorU32(background),
            ACTION_GROUP_ROUNDING
        );
    }

    [[nodiscard]] bool DrawSegmentText(
        const SegmentInteraction& segment, const char* label, const ImVec4& color, bool alignLeft = false
    ) {
        const char* visibleEnd = GuiUtils::VisibleLabelEnd(label);
        const float availableWidth =
            (std::max)(0.0f, segment.max.x - segment.min.x - ImGui::GetStyle().FramePadding.x * 2.0f);
        const bool clipped = ImGui::CalcTextSize(label, visibleEnd).x > availableWidth + 0.5f;

        std::string renderedText;
        const char* textBegin = label;
        const char* textEnd = visibleEnd;
        if (clipped) {
            constexpr std::string_view ELLIPSIS = "...";
            const float ellipsisWidth = ImGui::CalcTextSize(ELLIPSIS.data(), ELLIPSIS.data() + ELLIPSIS.size()).x;
            auto visibleBytes = static_cast<size_t>(visibleEnd - label);
            while (visibleBytes > 0 &&
                   ImGui::CalcTextSize(label, label + visibleBytes).x + ellipsisWidth > availableWidth) {
                --visibleBytes;
                while (visibleBytes > 0 && (static_cast<unsigned char>(label[visibleBytes]) & 0xC0U) == 0x80U) {
                    --visibleBytes;
                }
            }
            renderedText.assign(label, visibleBytes);
            renderedText.append(ELLIPSIS);
            textBegin = renderedText.data();
            textEnd = textBegin + renderedText.size();
        }

        const ImVec2 textSize = ImGui::CalcTextSize(textBegin, textEnd);
        const ImVec2 textPosition(
            alignLeft ? segment.min.x + ImGui::GetStyle().FramePadding.x
                      : segment.min.x + (segment.max.x - segment.min.x - textSize.x) * 0.5f,
            segment.min.y + (segment.max.y - segment.min.y - textSize.y) * 0.5f
        );
        const ImVec4 clipRect(segment.min.x + 1.0f, segment.min.y, segment.max.x - 1.0f, segment.max.y);
        ImGui::GetWindowDrawList()->AddText(
            ImGui::GetFont(), ImGui::GetFontSize(), textPosition, ImGui::GetColorU32(color), textBegin, textEnd, 0.0f,
            &clipRect
        );
        return clipped;
    }

    void RenderSegmentTooltip(const char* fullLabel, bool clipped, std::string_view helpText) {
        const bool showHelp = GuiUtils::HelpTooltipsEnabled() && !helpText.empty();
        if ((!clipped && !showHelp) ||
            !ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
            return;

        GuiUtils::BeginStyledTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        if (clipped) ImGui::TextColored(DefaultStyle::PARCHMENT, "%s", fullLabel);
        if (clipped && showHelp) ImGui::Spacing();
        if (showHelp) ImGui::TextUnformatted(helpText.data(), helpText.data() + helpText.size());
        ImGui::PopTextWrapPos();
        GuiUtils::EndStyledTooltip();
    }

    [[nodiscard]] SegmentInteraction RenderKeyButton(
        const char* id, const void* owner, int key, int& pendingOriginalKey, float width
    ) {
        const bool waitingForKey = KeybindManager::IsRebinding(owner);
        const char* keyText =
            waitingForKey ? PRESS_KEY_TEXT : (IsKeyUnbound(key) ? ADD_SHORTCUT_TEXT : KeybindManager::GetKeyName(key));

        const SegmentInteraction segment = RenderSegmentButton(id, width);
        if (segment.held || waitingForKey) {
            ImVec4 fill = DefaultStyle::OLD_BRASS;
            fill.w = waitingForKey ? 0.30f : 0.42f;
            DrawSegmentFill(segment, fill, ImDrawFlags_RoundCornersRight);
        } else if (segment.hovered) {
            ImVec4 fill = DefaultStyle::HEADER;
            fill.w = 0.72f;
            DrawSegmentFill(segment, fill, ImDrawFlags_RoundCornersRight);
        }
        const bool clipped = DrawSegmentText(segment, keyText, DefaultStyle::PARCHMENT_DARK);

        if (segment.pressed) {
            pendingOriginalKey = key;
            KeybindManager::BeginRebind(owner);
        }

        const bool showHelp = GuiUtils::HelpTooltipsEnabled();
        if ((clipped || showHelp) &&
            ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled)) [[unlikely]] {
            GuiUtils::BeginStyledTooltip();
            if (clipped) {
                ImGui::TextColored(DefaultStyle::PARCHMENT, "%s", keyText);
                if (showHelp) ImGui::Spacing();
            }
            if (showHelp && waitingForKey) {
                ImGui::TextColored(DefaultStyle::PARCHMENT, "Press any key. Escape cancels.");
                ImGui::TextColored(
                    DefaultStyle::PARCHMENT_DARK, "%s clears the shortcut.",
                    KeybindManager::GetKeyName(KeybindManager::GetUnbindKey())
                );
            } else if (showHelp) {
                ImGui::TextColored(DefaultStyle::PARCHMENT, CHANGE_KEYBIND_TEXT);
            }
            GuiUtils::EndStyledTooltip();
        }
        return segment;
    }

    void DrawSearchHighlight(const ImVec2& min, const ImVec2& max) {
        const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(ImGui::GetTime() * 5.0));
        ImVec4 accent = DefaultStyle::BRIGHT_BRASS;
        accent.w = 0.55f + pulse * 0.40f;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(
            ImVec2(min.x - 4.0f, min.y), ImVec2(min.x - 1.5f, max.y), ImGui::ColorConvertFloat4ToU32(accent), 1.0f
        );
    }

    [[nodiscard]] SegmentInteraction RenderConfigurationButton(bool popupOpen, float width) {
        const SegmentInteraction segment = RenderSegmentButton("configuration", width);
        const float size = segment.max.y - segment.min.y;
        if (segment.held || popupOpen) {
            ImVec4 fill = DefaultStyle::OLD_BRASS;
            fill.w = popupOpen ? 0.30f : 0.42f;
            DrawSegmentFill(segment, fill, ImDrawFlags_RoundCornersNone);
        } else if (segment.hovered) {
            ImVec4 fill = DefaultStyle::HEADER;
            fill.w = 0.72f;
            DrawSegmentFill(segment, fill, ImDrawFlags_RoundCornersNone);
        }

        const float left = segment.min.x + size * 0.25f;
        const float right = segment.max.x - size * 0.25f;
        const float knobRadius = (std::max)(1.25f, size * 0.065f);
        const ImU32 color = ImGui::GetColorU32(DefaultStyle::PARCHMENT_DARK);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        constexpr float KNOB_POSITIONS[] = {0.38f, 0.68f, 0.48f};
        for (int row = 0; row < 3; ++row) {
            const float y = segment.min.y + size * (0.32f + static_cast<float>(row) * 0.18f);
            const float knobX = left + (right - left) * KNOB_POSITIONS[row];
            drawList->AddLine(ImVec2(left, y), ImVec2(right, y), color, 1.0f);
            drawList->AddCircleFilled(ImVec2(knobX, y), knobRadius, color);
        }
        GuiUtils::HelpTooltip("Options");
        return segment;
    }

    struct ParamRenderResult {
        bool edited = false;
        bool committed = false;
    };

    [[nodiscard]] ParamRenderResult RenderParam(const KeybindParam& param) {
        ImGui::PushID(param.name);
        ParamRenderResult result;
        if (param.type == KeybindParam::Type::Bool) {
            std::array<char, 192> label{};
            std::snprintf(label.data(), label.size(), "%s###value", param.displayName);
            result.committed = ImGui::Checkbox(label.data(), static_cast<bool*>(param.valuePtr));
            result.edited = result.committed;
            GuiUtils::HelpTooltip(param.tooltip);
            ImGui::PopID();
            return result;
        }

        ImGui::BeginGroup();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(DefaultStyle::PARCHMENT_DARK, "%s", param.displayName);
        GuiUtils::HelpTooltip(param.tooltip);
        ImGui::SameLine();
        ImGui::PushItemWidth(PARAM_CONTROL_WIDTH);
        switch (param.type) {
            case KeybindParam::Type::Int: {
                auto* valuePtr = static_cast<int*>(param.valuePtr);
                const int minValue = static_cast<int>(param.minValue);
                const int maxValue = static_cast<int>(param.maxValue);
                const SliderStyleRAII sliderStyle;
                result.committed = GuiUtils::DebouncedDragInt("##value", valuePtr, 1.0f, minValue, maxValue);
                break;
            }
            case KeybindParam::Type::Float: {
                auto* valuePtr = static_cast<float*>(param.valuePtr);
                const auto minValue = static_cast<float>(param.minValue);
                const auto maxValue = static_cast<float>(param.maxValue);
                const SliderStyleRAII sliderStyle;
                result.committed = GuiUtils::DebouncedDragFloat("##value", valuePtr, 0.01f, minValue, maxValue, "%.2f");
                break;
            }
            case KeybindParam::Type::Double: {
                auto* valuePtr = static_cast<double*>(param.valuePtr);
                const double minValue = param.minValue;
                const double maxValue = param.maxValue;
                const SliderStyleRAII sliderStyle;
                result.committed = GuiUtils::DebouncedDragScalar(
                    "##value", ImGuiDataType_Double, valuePtr, 0.01f, &minValue, &maxValue, "%.2f"
                );
                break;
            }
            case KeybindParam::Type::Bool: break;
        }
        result.edited = ImGui::IsItemEdited();
        ImGui::PopItemWidth();
        GuiUtils::HelpTooltip(param.tooltip);
        ImGui::EndGroup();
        ImGui::PopID();
        return result;
    }

    bool RenderConfigurationPopup(
        KeybindEntry& entry, const ImVec2& anchor, bool closeRequested, bool& edited, bool& committed
    ) {
        ImGui::SetNextWindowPos(anchor, ImGuiCond_Appearing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, GuiUtils::K_POPUP_PADDING);
        const bool began = ImGui::BeginPopup(
            "Configuration",
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings
        );
        bool open = false;
        if (began) {
            if (closeRequested) {
                ImGui::CloseCurrentPopup();
            } else {
                open = true;
                for (const auto& param : entry.params) {
                    const auto result = RenderParam(param);
                    edited |= result.edited;
                    committed |= result.committed;
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        return open;
    }

    [[nodiscard]] float EntryNaturalWidth(KeybindEntry& entry) {
        if (entry.naturalWidth > 0.0f) return entry.naturalWidth;
        const float configurationWidth = entry.params.empty() ? 0.0f : ImGui::GetFrameHeight();
        const float actionWidth = (std::max)(ACTION_MIN_WIDTH, GuiUtils::ButtonNaturalWidth(entry.name.c_str()));
        entry.naturalWidth = configurationWidth + ACTION_KEYCAP_WIDTH + actionWidth;
        return entry.naturalWidth;
    }

    bool QueueEntryCallback(KeybindEntry* entry, bool enabled) {
        auto callback = entry->callback;
        return GameHook::QueueAction([callback = std::move(callback), enabled](const RuntimeContextSnapshot& runtime) {
            if (callback) callback(enabled, runtime);
        });
    }

    void SetEventsEnabled(KeybindEntry* entry, bool enabled) {
        entry->eventSubscriptions.Clear();
        if (enabled) {
            for (auto evt : entry->events) {
                (void)entry->eventSubscriptions.Subscribe(evt, [entry](EventBus::EventContext& context) {
                    if (!entry->isActive.load(std::memory_order_acquire) || !entry->callback) return;
                    entry->callback(true, context.Runtime());
                });
            }
        }
    }

    void SetFunctionHooksEnabled(KeybindEntry* entry, bool enabled) {
        for (size_t i = 0; i < entry->functionHooks.size(); ++i) {
            auto& hook = entry->functionHooks[i];
            if (enabled) {
                GameHook::QueueAction([entry, i, name = hook.functionName, callback = hook.callback,
                                       phase = hook.phase](const RuntimeContextSnapshot&) mutable {
                    if (!entry->isActive.load(std::memory_order_acquire) || i >= entry->functionHooks.size() ||
                        !callback)
                        return;
                    const auto previousHandle = entry->functionHooks[i].handle;
                    if (GameHook::Get().IsSubscribed(previousHandle)) return;
                    entry->functionHooks[i].handle = GameHook::INVALID_HOOK_HANDLE;
                    entry->functionHooks[i].handle = GameHook::Get().Subscribe(name, phase, std::move(callback));
                });
            } else {
                const auto handle = hook.handle;
                hook.handle = GameHook::INVALID_HOOK_HANDLE;
                GameHook::QueueAction([handle](const RuntimeContextSnapshot&) { GameHook::Get().Unsubscribe(handle); });
            }
        }
    }

    void SetInternalState(KeybindEntry* entry, bool active) {
        entry->isActive.store(active, std::memory_order_release);
        KeybindConfig::SaveKeybind(*entry);
        SetEventsEnabled(entry, active);
        SetFunctionHooksEnabled(entry, active);
    }

    bool SetEntryState(KeybindEntry* entry, bool active) {
        if (!entry->IsAvailable()) return false;
        if (entry->stateGetter) {
            return QueueEntryCallback(entry, active);
        }

        SetInternalState(entry, active);
        if (entry->applyOnToggle) (void)QueueEntryCallback(entry, active);
        return true;
    }

    void InvokeEntry(KeybindEntry* entry, bool fromShortcut) {
        if (entry->IsState()) {
            const bool nextState = !entry->CurrentState();
            if (SetEntryState(entry, nextState) && fromShortcut && !entry->name.empty())
                NotificationManager::NotifyStateChange(entry->name, nextState);
        } else if (QueueEntryCallback(entry, true) && fromShortcut && !entry->name.empty()) {
            NotificationManager::NotifyAction(entry->name);
        }
    }

    [[nodiscard]] SegmentInteraction RenderActionSegment(
        KeybindEntry& entry, bool available, bool active, float width
    ) {
        SegmentInteraction segment = RenderSegmentButton("action", width);
        ImVec4 fill = DefaultStyle::CLEAR;
        if (available) {
            if (segment.held || segment.pressed) {
                fill = entry.destructive ? ImVec4(0.74f, 0.23f, 0.18f, 0.55f) : DefaultStyle::OLD_BRASS;
                if (!entry.destructive) fill.w = 0.48f;
            } else if (active) {
                fill = DefaultStyle::OLD_BRASS;
                fill.w = segment.hovered ? 0.40f : 0.28f;
            } else if (segment.hovered) {
                fill = DefaultStyle::HEADER;
                fill.w = 0.72f;
            }
        }
        DrawSegmentFill(segment, fill, ImDrawFlags_RoundCornersLeft);

        const ImVec4 text = !available ? DefaultStyle::TEXT_DISABLED
                            : entry.destructive
                                ? ImVec4(0.95f, segment.hovered ? 0.72f : 0.64f, 0.56f, 1.0f)
                                : (!active && segment.hovered ? DefaultStyle::BRIGHT_BRASS : DefaultStyle::PARCHMENT);
        segment.textClipped = DrawSegmentText(segment, entry.name.c_str(), text, true);
        return segment;
    }

    void DrawSegmentedControlFrame(
        const ImVec2& min, const ImVec2& max, float actionRight, const SegmentInteraction* configuration
    ) {
        ImVec4 border = DefaultStyle::OLD_BRASS;
        border.w = 0.68f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList
            ->AddRect(min, max, ImGui::GetColorU32(border), ACTION_GROUP_ROUNDING, ImDrawFlags_RoundCornersAll, 1.0f);

        const float lineTop = min.y + 1.0f;
        const float lineBottom = max.y - 1.0f;
        drawList->AddLine(ImVec2(actionRight, lineTop), ImVec2(actionRight, lineBottom), ImGui::GetColorU32(border));
        if (configuration) {
            drawList->AddLine(
                ImVec2(configuration->max.x, lineTop), ImVec2(configuration->max.x, lineBottom),
                ImGui::GetColorU32(border)
            );
        }
    }

    void CommitParamChanges(KeybindEntry& entry) {
        if (entry.persistParams) KeybindConfig::SaveParams(entry);
        if (entry.onParamsChanged) {
            entry.onParamsChanged();
        } else if (entry.IsState() && entry.applyOnToggle && entry.CurrentState() && entry.IsAvailable()) {
            (void)QueueEntryCallback(&entry, true);
        }
    }

    void RegisterEntry(KeybindEntry& entry) {
        auto* entryPtr = &entry;
        KeybindManager::RegisterKeybind(
            entry.keyPtr, [entryPtr]() { InvokeEntry(entryPtr, true); },
            entry.name, [entryPtr]() { KeybindConfig::SaveKeybind(*entryPtr); }
        );
    }

    void ApplyKey(KeybindEntry& entry) {
        KeybindManager::UnregisterKeybind(entry.keyPtr);
        if (!IsKeyUnbound(*entry.keyPtr)) {
            RegisterEntry(entry);
        }
        KeybindConfig::SaveKeybind(entry);
    }

    void HandleKeyAssignment(KeybindEntry& entry) {
        int capturedKey = *entry.keyPtr;
        const auto result = KeybindManager::PollRebind(&entry, capturedKey);
        if (result != KeybindManager::RebindResult::Assigned) return;

        const int newKey = capturedKey;
        if (newKey != -1) {
            const int count = KeybindManager::GetBindingCount(newKey, entry.keyPtr);
            if (newKey == KeybindManager::GetToggleGuiKey() || count > 0) {
                entry.pendingConflictKey = newKey;
                ImGui::OpenPopup("Shortcut conflict");
                return;
            }
        }

        *entry.keyPtr = newKey;
        ApplyKey(entry);
    }

    void RenderConflictPopup(KeybindEntry& entry) {
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, MODAL_DIM_COLOR);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, GuiUtils::K_POPUP_PADDING);
        if (ImGui::BeginPopupModal(
                "Shortcut conflict", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings
            )) {
            const auto names = KeybindManager::GetAllBoundNames(entry.pendingConflictKey, entry.keyPtr);
            const int count = static_cast<int>(names.size());
            const bool reservedForMenu = entry.pendingConflictKey == KeybindManager::GetToggleGuiKey();

            if (reservedForMenu) {
                ImGui::TextWrapped(
                    "%s opens and closes the menu. Choose another shortcut.",
                    KeybindManager::GetKeyName(entry.pendingConflictKey)
                );
            } else if (count == 1) {
                const char* conflictName = names.empty() ? "another action" : names[0].c_str();
                ImGui::Text(KEY_CONFLICT_FORMAT, KeybindManager::GetKeyName(entry.pendingConflictKey), conflictName);
            } else {
                ImGui::Text(KEY_MULTI_CONFLICT_FORMAT, KeybindManager::GetKeyName(entry.pendingConflictKey), count);
                ImGui::Spacing();
                ImGui::Text("Used by:");
                for (size_t i = 0; i < names.size() && i < 5; ++i) {
                    ImGui::BulletText("%s", names[i].c_str());
                }
                if (names.size() > 5) {
                    ImGui::BulletText("... and %zu more", names.size() - 5);
                }
            }
            ImGui::Spacing();

            if (!reservedForMenu) {
                const std::string replaceText = !names.empty() ? "Replace " + names.front() : "Replace";
                if (ImGui::Button(replaceText.c_str())) {
                    KeybindManager::RemoveBinding(entry.pendingConflictKey, entry.keyPtr);
                    *entry.keyPtr = entry.pendingConflictKey;
                    ApplyKey(entry);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                const char* shareText = count > 1 ? "Use for All" : "Use for Both";
                if (ImGui::Button(shareText)) {
                    *entry.keyPtr = entry.pendingConflictKey;
                    ApplyKey(entry);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
            }
            if (ImGui::Button("Cancel")) {
                *entry.keyPtr = entry.pendingOriginalKey;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Try Another Key")) {
                *entry.keyPtr = entry.pendingOriginalKey;
                KeybindManager::BeginRebind(&entry);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

} // anonymous namespace

float KeybindUi::CalculateKeycapWidth(const char* label, float maximumWidth) {
    const float naturalWidth = GuiUtils::ButtonNaturalWidth(label);
    const float cappedMaximum = (std::min)(KEYCAP_MAX_WIDTH, (std::max)(1.0f, maximumWidth));
    return (std::min)((std::max)(KEYCAP_MIN_WIDTH, naturalWidth), cappedMaximum);
}

bool KeybindUi::RenderKeycap(const char* id, const char* label, float width) {
    const KeycapStyleRAII keycapStyle;
    return RenderCompactButton(id, label, width);
}


void KeybindEntry::Render(bool highlight, bool scrollIntoView, float cellWidth) {
    ImGui::PushID(this);
    const float availableWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();

    const bool hasConfiguration = !params.empty();
    const float targetWidth = (std::min)(availableWidth, cellWidth > 0.0f ? cellWidth : EntryNaturalWidth(*this));
    const float configurationWidth =
        hasConfiguration ? (std::min)(ImGui::GetFrameHeight(), (std::max)(1.0f, targetWidth - 2.0f)) : 0.0f;
    const float keyWidth = (std::min)(ACTION_KEYCAP_WIDTH, (std::max)(1.0f, targetWidth - configurationWidth - 1.0f));
    const float actionWidth = (std::max)(1.0f, targetWidth - configurationWidth - keyWidth);

    bool closeConfiguration = false;
    ImVec2 configurationAnchor{};
    SegmentInteraction configurationSegment;

    ImGui::BeginGroup();
    DrawActionGroupBackground(rowMin, targetWidth);

    const bool actionAvailable = IsAvailable();
    const bool active = IsState() && CurrentState();
    if (!actionAvailable) ImGui::BeginDisabled();
    const SegmentInteraction actionSegment = RenderActionSegment(*this, actionAvailable, active, actionWidth);
    if (actionSegment.pressed) {
        KeybindManager::CancelRebind();
        InvokeEntry(this, false);
    }
    if (!actionAvailable) ImGui::EndDisabled();
    RenderSegmentTooltip(name.c_str(), actionSegment.textClipped, tooltip);
    ImGui::SameLine(0.0f, 0.0f);

    if (hasConfiguration) {
        const bool popupOpen = ImGui::IsPopupOpen("Configuration");
        configurationSegment = RenderConfigurationButton(popupOpen, configurationWidth);
        if (configurationSegment.pressed) {
            KeybindManager::CancelRebind();
            if (popupOpen)
                closeConfiguration = true;
            else
                ImGui::OpenPopup("Configuration");
        }
        configurationAnchor =
            ImVec2(configurationSegment.min.x, configurationSegment.max.y + ImGui::GetStyle().ItemSpacing.y);
        ImGui::SameLine(0.0f, 0.0f);
    }

    (void)RenderKeyButton("shortcut", this, *keyPtr, pendingOriginalKey, keyWidth);

    ImGui::EndGroup();
    const ImVec2 rowMax = ImGui::GetItemRectMax();
    DrawSegmentedControlFrame(rowMin, rowMax, actionSegment.max.x, hasConfiguration ? &configurationSegment : nullptr);

    HandleKeyAssignment(*this);

    bool paramsEdited = false;
    bool paramsCommitted = false;
    bool popupOpen = false;
    if (hasConfiguration) {
        popupOpen =
            RenderConfigurationPopup(*this, configurationAnchor, closeConfiguration, paramsEdited, paramsCommitted);
    }
    configDirty |= paramsEdited;
    const bool shouldCommitParams = paramsCommitted || (configPopupOpenLastFrame && !popupOpen && configDirty);
    if (shouldCommitParams) {
        CommitParamChanges(*this);
        configDirty = false;
    }
    configPopupOpenLastFrame = popupOpen;

    RenderConflictPopup(*this);

    if (highlight && scrollIntoView) ImGui::SetScrollHereY(0.45f);
    if (highlight) {
        DrawSearchHighlight(ImVec2(rowMin.x, rowMin.y - 1.0f), ImVec2(rowMax.x, rowMax.y + 1.0f));
    }
    ImGui::PopID();
}

void KeybindList::Render() {
    if (highlightedEntry && ImGui::GetTime() > highlightUntil) highlightedEntry = nullptr;

    ImGui::PushID(this);
    const size_t count = entries.size();
    size_t groupBegin = 0;
    while (groupBegin < count) {
        const char* currentGroup = entries[groupBegin].group;
        size_t groupEnd = groupBegin + 1;
        while (groupEnd < count && std::strcmp(entries[groupEnd].group, currentGroup) == 0)
            ++groupEnd;

        if (currentGroup[0] != '\0') ImGui::SeparatorText(currentGroup);

        float cellWidth = 1.0f;
        for (size_t i = groupBegin; i < groupEnd; ++i) {
            cellWidth = (std::max)(cellWidth, EntryNaturalWidth(entries[i]));
        }
        cellWidth = (std::min)(cellWidth, (std::max)(1.0f, ImGui::GetContentRegionAvail().x));

        for (size_t i = groupBegin; i < groupEnd; ++i) {
            if (i > groupBegin) (void)GuiUtils::SameLineIfFits(cellWidth);
            const bool highlight = highlightedEntry == &entries[i];
            entries[i].Render(highlight, highlight && scrollHighlightedEntry, cellWidth);
            if (highlight) scrollHighlightedEntry = false;
        }
        groupBegin = groupEnd;
    }
    ImGui::PopID();
}

void KeybindList::RequestHighlight(const KeybindEntry* entry) {
    highlightedEntry = entry;
    highlightUntil = ImGui::GetTime() + 2.0;
    scrollHighlightedEntry = true;
}


void KeybindConfig::LoadKeybind(KeybindEntry& entry) {
    auto& config = ConfigManager::Get();
    const char* section = entry.configSection.c_str();
    *entry.keyPtr = config.GetInt(section, "key", *entry.keyPtr);
    if (entry.IsState() && !entry.stateGetter) {
        entry.isActive.store(config.GetBool(section, "enabled", false), std::memory_order_release);
    }

    if (!entry.persistParams) return;
    for (const auto& param : entry.params) {
        switch (param.type) {
            case KeybindParam::Type::Int: {
                auto* value = static_cast<int*>(param.valuePtr);
                *value = config.GetInt(section, param.name, *value);
                break;
            }
            case KeybindParam::Type::Float: {
                auto* value = static_cast<float*>(param.valuePtr);
                *value = config.GetFloat(section, param.name, *value);
                break;
            }
            case KeybindParam::Type::Double: {
                auto* value = static_cast<double*>(param.valuePtr);
                *value = config.GetDouble(section, param.name, *value);
                break;
            }
            case KeybindParam::Type::Bool: {
                auto* value = static_cast<bool*>(param.valuePtr);
                *value = config.GetBool(section, param.name, *value);
                break;
            }
        }
    }
}

void KeybindConfig::SaveKeybind(const KeybindEntry& entry) {
    auto& config = ConfigManager::Get();
    const char* section = entry.configSection.c_str();
    config.BatchSave([&] {
        config.SetInt(section, "key", *entry.keyPtr);
        if (entry.IsState() && !entry.stateGetter) {
            config.SetBool(section, "enabled", entry.isActive.load(std::memory_order_acquire));
        }
    });
}

void KeybindConfig::SaveParams(const KeybindEntry& entry) {
    auto& config = ConfigManager::Get();
    const char* section = entry.configSection.c_str();
    config.BatchSave([&] {
        for (const auto& param : entry.params) {
            switch (param.type) {
                case KeybindParam::Type::Int:
                    config.SetInt(section, param.name, *static_cast<int*>(param.valuePtr));
                    break;
                case KeybindParam::Type::Float:
                    config.SetFloat(section, param.name, *static_cast<float*>(param.valuePtr));
                    break;
                case KeybindParam::Type::Double:
                    config.SetDouble(section, param.name, *static_cast<double*>(param.valuePtr));
                    break;
                case KeybindParam::Type::Bool:
                    config.SetBool(section, param.name, *static_cast<bool*>(param.valuePtr));
                    break;
            }
        }
    });
}

KeybindEntry::~KeybindEntry() {
    std::erase(RuntimeEntries(), this);
}

void KeybindEntry::AdoptDefinition(KeybindEntry& source) noexcept {
    name = std::move(source.name);
    tooltip = std::move(source.tooltip);
    configSection = std::move(source.configSection);
    keyPtr = source.keyPtr;
    callback = std::move(source.callback);
    kind = source.kind;
    stateGetter = std::move(source.stateGetter);
    available = std::move(source.available);
    applyOnToggle = source.applyOnToggle;
    isActive.store(source.isActive.load(std::memory_order_acquire), std::memory_order_release);
    persistParams = source.persistParams;
    events = std::move(source.events);
    functionHooks = std::move(source.functionHooks);
    params = std::move(source.params);
    onParamsChanged = std::move(source.onParamsChanged);
    group = source.group;
    destructive = source.destructive;
}

void KeybindEntry::Init() {
    if (initialized || !keyPtr) return;
    initialized = true;

    std::erase(configSection, ' ');
    KeybindConfig::LoadKeybind(*this);

    auto& registeredEntries = RuntimeEntries();
    if (std::ranges::find(registeredEntries, this) == registeredEntries.end()) registeredEntries.push_back(this);

    if (!IsKeyUnbound(*keyPtr)) RegisterEntry(*this);

    if (isActive.load(std::memory_order_acquire)) {
        SetEventsEnabled(this, true);
        SetFunctionHooksEnabled(this, true);
        if (applyOnToggle) (void)QueueEntryCallback(this, true);
    }
}

void KeybindRuntime::FlushPendingParamChanges() noexcept {
    try {
        for (auto* entry : RuntimeEntries()) {
            if (!entry || !entry->configDirty) continue;
            CommitParamChanges(*entry);
            entry->configDirty = false;
            entry->configPopupOpenLastFrame = false;
        }
    } catch (...) {
        g_keybindRuntimeLogger.Log("Exception while saving action configuration");
    }
}

void KeybindRuntime::OnRuntimeStart() {
    for (auto* entry : RuntimeEntries()) {
        if (!entry || !entry->isActive.load(std::memory_order_acquire)) continue;
        SetEventsEnabled(entry, true);
        for (auto& functionHook : entry->functionHooks) {
            if (!GameHook::Get().IsSubscribed(functionHook.handle)) {
                functionHook.handle = GameHook::INVALID_HOOK_HANDLE;
            }
        }
        SetFunctionHooksEnabled(entry, true);
        if (entry->applyOnToggle) (void)QueueEntryCallback(entry, true);
    }
}

void KeybindRuntime::OnRuntimeShutdown() noexcept {
    try {
        auto& hook = GameHook::Get();
        for (auto* entry : RuntimeEntries()) {
            if (!entry) continue;
            entry->eventSubscriptions.Clear();
            for (auto& functionHook : entry->functionHooks) {
                hook.Unsubscribe(functionHook.handle);
                functionHook.handle = GameHook::INVALID_HOOK_HANDLE;
            }
        }
    } catch (...) {
        g_keybindRuntimeLogger.Log("Exception while resetting keybind runtime subscriptions");
    }
}

void KeybindList::Add(KeybindEntry&& entry) {
    entries.emplace_back();
    entries.back().AdoptDefinition(entry);
    entries.back().Init();
}
