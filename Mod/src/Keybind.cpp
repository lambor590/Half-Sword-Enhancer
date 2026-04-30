#include <string>
#include <algorithm>

#include "Menu/Keybind.h"
#include "Menu/EventBus.h"
#include "Hooks/GameHook.h"
#include "imgui/imgui.h"
#include "ConfigManager.h"
#include "KeybindManager.h"
#include "DefaultStyle.h"
#include "Utils/GuiUtils.h"

KeybindParam::KeybindParam(
    std::string_view name, std::string_view displayName, int* value, int minVal, int maxVal, std::string_view tooltip
) noexcept
    : name(name), displayName(displayName), tooltip(tooltip), type(Type::Int), valuePtr(value) {
    minValue.intMin = minVal;
    maxValue.intMax = maxVal;
    id = "##param_" + std::string(name);
}

KeybindParam::KeybindParam(
    std::string_view name, std::string_view displayName, float* value, float minVal, float maxVal,
    std::string_view tooltip
) noexcept
    : name(name), displayName(displayName), tooltip(tooltip), type(Type::Float), valuePtr(value) {
    minValue.floatMin = minVal;
    maxValue.floatMax = maxVal;
    id = "##param_" + std::string(name);
}

KeybindParam::KeybindParam(
    std::string_view name, std::string_view displayName, bool* value, std::string_view tooltip
) noexcept
    : name(name), displayName(displayName), tooltip(tooltip), type(Type::Bool), valuePtr(value) {
    id = "##param_" + std::string(name);
}

namespace {

    constexpr ImVec4 DISABLED_BUTTON_COLOR{0.18f, 0.13f, 0.09f, 0.50f};
    constexpr ImVec4 DISABLED_TEXT_COLOR{0.55f, 0.45f, 0.35f, 0.60f};
    constexpr ImVec4 DISABLED_BORDER_COLOR{0.28f, 0.20f, 0.12f, 0.40f};
    constexpr ImVec4 DISABLED_NAME_COLOR{0.60f, 0.55f, 0.48f, 0.70f};
    constexpr ImVec4 MODAL_DIM_COLOR{0, 0, 0, 0.6f};

    constexpr float BUTTON_WIDTH_PADDING = 28.0f;
    constexpr float PARAMETER_BUTTON_OFFSET = 95.0f;
    constexpr float ITEM_WIDTH_160 = 160.0f;
    constexpr float FRAME_BORDER_SIZE = 1.0f;

    constexpr std::string_view PRESS_KEY_TEXT = "Press a key...";
    constexpr std::string_view CONFIGURE_FORMAT = "Configure %s";
    constexpr std::string_view CHANGE_KEYBIND_TEXT = "Change keybind";
    constexpr std::string_view KEY_CONFLICT_FORMAT = "Key %s is already bound to %s. What do you want to do?";
    constexpr std::string_view KEY_MULTI_CONFLICT_FORMAT =
        "Key %s is already bound to %d functions. What do you want to do?";

    constexpr bool IsKeyUnbound(int key) noexcept {
        return key == -1 || key == 255;
    }

    struct ButtonStyleRAII {
        explicit ButtonStyleRAII(bool disabled) : pushCount(disabled ? 4 : 0) {
            if (disabled) {
                ImGui::PushStyleColor(ImGuiCol_Button, DISABLED_BUTTON_COLOR);
                ImGui::PushStyleColor(ImGuiCol_Text, DISABLED_TEXT_COLOR);
                ImGui::PushStyleColor(ImGuiCol_Border, DISABLED_BORDER_COLOR);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, FRAME_BORDER_SIZE);
            }
        }
        ~ButtonStyleRAII() {
            if (pushCount) {
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
            }
        }
        int pushCount;
    };

    struct SliderStyleRAII {
        SliderStyleRAII() {
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, DefaultStyle::OLD_BRASS);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, DefaultStyle::BRIGHT_BRASS);
        }
        ~SliderStyleRAII() { ImGui::PopStyleColor(2); }
    };

    void RenderKeyButton(const char* id, bool& waitingForKey, int key, int& pendingOriginalKey) {
        const char* keyText = waitingForKey ? PRESS_KEY_TEXT.data() : KeybindManager::GetKeyName(key);
        const ButtonStyleRAII style(IsKeyUnbound(key));

        const float textWidth = ImGui::CalcTextSize(keyText).x;
        ImGui::SetNextItemWidth(textWidth + BUTTON_WIDTH_PADDING);
        ImGui::PushID(id);

        if (ImGui::Button(keyText)) {
            waitingForKey = true;
            pendingOriginalKey = key;
        }

        ImGui::PopID();

        if (ImGui::IsItemHovered()) [[unlikely]] {
            GuiUtils::BeginStyledTooltip();
            ImGui::TextColored(DefaultStyle::PARCHMENT, CHANGE_KEYBIND_TEXT.data());
            GuiUtils::EndStyledTooltip();
        }
    }

    void RenderName(std::string_view name, bool isDisabled) {
        ImGui::TextColored(
            isDisabled ? DISABLED_NAME_COLOR : DefaultStyle::PARCHMENT, "%.*s", static_cast<int>(name.size()),
            name.data()
        );
    }

    bool RenderParametersButton(const char* buttonId, const std::string& name) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - PARAMETER_BUTTON_OFFSET);

        const bool clicked = ImGui::Button(buttonId);

        if (ImGui::IsItemHovered()) {
            GuiUtils::BeginStyledTooltip();
            ImGui::Text(CONFIGURE_FORMAT.data(), name.c_str());
            GuiUtils::EndStyledTooltip();
        }
        return clicked;
    }

    void RenderParam(const KeybindParam& param) {
        ImGui::PushItemWidth(ITEM_WIDTH_160);
        ImGui::AlignTextToFramePadding();

        ImGui::TextColored(
            DefaultStyle::PARCHMENT_DARK, "%.*s", static_cast<int>(param.displayName.size()), param.displayName.data()
        );
        TooltipHelper::ShowTooltip(param.tooltip);
        ImGui::SameLine();

        switch (param.type) {
            case KeybindParam::Type::Int: {
                auto* valuePtr = static_cast<int*>(param.valuePtr);
                const SliderStyleRAII sliderStyle;
                GuiUtils::DebouncedDragInt(param.id.c_str(), valuePtr, 1.0f);
                break;
            }
            case KeybindParam::Type::Float: {
                auto* valuePtr = static_cast<float*>(param.valuePtr);
                const SliderStyleRAII sliderStyle;
                GuiUtils::DebouncedDragFloat(param.id.c_str(), valuePtr, 0.01f, 0.0f, 0.0f, "%.2f");
                break;
            }
            case KeybindParam::Type::Bool: {
                auto* valuePtr = static_cast<bool*>(param.valuePtr);
                ImGui::Checkbox(param.id.c_str(), valuePtr);
                break;
            }
        }

        ImGui::PopItemWidth();
    }

    void QueueEntryCallback(KeybindEntry* entry, bool enabled) {
        GameHook::QueueAction([entry, enabled](const RuntimeContextSnapshot& runtime) {
            entry->callback(enabled, runtime);
        });
    }

    void SetEventsEnabled(KeybindEntry* entry, bool enabled) {
        for (auto evt : entry->events) {
            if (enabled) {
                EventBus::Get().Subscribe(evt, entry, [entry](const RuntimeContextSnapshot& runtime) {
                    entry->callback(entry->isEnabled, runtime);
                });
            } else {
                EventBus::Get().Unsubscribe(evt, entry);
            }
        }
    }

    void SetEnabledState(KeybindEntry* entry, bool enabled) {
        entry->isEnabled = enabled;
        KeybindConfig::SaveKeybind(*entry);
        SetEventsEnabled(entry, enabled);
    }

    void ToggleEntry(KeybindEntry* entry, bool enabled) {
        SetEnabledState(entry, enabled);
        if (entry->runOnToggle) {
            QueueEntryCallback(entry, entry->isEnabled);
        }
    }

    void RegisterEntry(KeybindEntry& entry) {
        auto* entryPtr = &entry;
        KeybindManager::RegisterKeybind(
            entry.keyPtr,
            [entryPtr]() {
                if (entryPtr->IsToggle()) {
                    ToggleEntry(entryPtr, !entryPtr->isEnabled);
                } else {
                    QueueEntryCallback(entryPtr, true);
                }
            },
            entry.name, entry.IsToggle(),
            [entryPtr]() {
                if (entryPtr->IsToggle() && entryPtr->isEnabled) {
                    SetEnabledState(entryPtr, false);
                    return;
                }
                KeybindConfig::SaveKeybind(*entryPtr);
            }
        );
    }

    void ApplyKey(KeybindEntry& entry) {
        KeybindManager::UnregisterKeybind(entry.keyPtr);
        if (IsKeyUnbound(*entry.keyPtr)) {
            if (entry.IsToggle() && entry.isEnabled) {
                SetEnabledState(&entry, false);
            }
        } else {
            RegisterEntry(entry);
        }
        entry.prevKey = *entry.keyPtr;
        KeybindConfig::SaveKeybind(entry);
    }

    void HandleKeyAssignment(KeybindEntry& entry) {
        if (!KeybindManager::HandleKeyPress(entry.waitingForKey, *entry.keyPtr)) return;

        const int newKey = *entry.keyPtr;
        if (newKey != -1) {
            const int count = KeybindManager::GetBindingCount(newKey, entry.keyPtr);
            if (count > 0) {
                *entry.keyPtr = entry.pendingOriginalKey;
                entry.pendingConflictKey = newKey;
                ImGui::OpenPopup(entry.conflictPopupId.c_str());
                return;
            }
        }

        ApplyKey(entry);
    }

    void RenderConflictPopup(KeybindEntry& entry) {
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, MODAL_DIM_COLOR);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, GuiUtils::K_POPUP_PADDING);
        if (ImGui::BeginPopupModal(
                entry.conflictPopupId.c_str(), nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings
            )) {
            const int count = KeybindManager::GetBindingCount(entry.pendingConflictKey, entry.keyPtr);
            const auto names = KeybindManager::GetAllBoundNames(entry.pendingConflictKey, entry.keyPtr);

            if (count == 1) {
                const char* conflictName = names.empty() ? "Unknown" : names[0].c_str();
                ImGui::Text(
                    KEY_CONFLICT_FORMAT.data(), KeybindManager::GetKeyName(entry.pendingConflictKey), conflictName
                );
            } else {
                ImGui::Text(
                    KEY_MULTI_CONFLICT_FORMAT.data(), KeybindManager::GetKeyName(entry.pendingConflictKey), count
                );
                ImGui::Spacing();
                ImGui::Text("Currently bound to:");
                for (size_t i = 0; i < names.size() && i < 5; ++i) {
                    ImGui::BulletText("%s", names[i].c_str());
                }
                if (names.size() > 5) {
                    ImGui::BulletText("... and %zu more", names.size() - 5);
                }
            }
            ImGui::Spacing();

            const char* replaceText = (count > 1) ? "Remove One" : "Replace";
            if (ImGui::Button(replaceText)) {
                KeybindManager::RemoveBinding(entry.pendingConflictKey, entry.keyPtr);
                *entry.keyPtr = entry.pendingConflictKey;
                ApplyKey(entry);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Share Key")) {
                *entry.keyPtr = entry.pendingConflictKey;
                ApplyKey(entry);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                *entry.keyPtr = entry.pendingOriginalKey;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Choose Another")) {
                *entry.keyPtr = entry.pendingOriginalKey;
                entry.waitingForKey = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

} // anonymous namespace


namespace {
    struct TooltipState {
        bool enabled = true;
        uint8_t counter = 0;

        [[nodiscard]] bool IsEnabled() noexcept {
            if (++counter >= 60) [[unlikely]] {
                enabled = g_ConfigManager.GetBool("GUI", "tooltips_enabled", true);
                counter = 0;
            }
            return enabled;
        }

        void Refresh() noexcept {
            enabled = g_ConfigManager.GetBool("GUI", "tooltips_enabled", true);
            counter = 0;
        }
    };

    thread_local TooltipState g_tooltipState;
}

void TooltipHelper::ShowTooltip(std::string_view tooltip) {
    if (tooltip.empty()) [[unlikely]]
        return;
    if (!g_tooltipState.IsEnabled()) [[likely]]
        return;

    if (ImGui::IsItemHovered()) [[unlikely]] {
        GuiUtils::BeginStyledTooltip();
        ImGui::TextColored(DefaultStyle::PARCHMENT, "%.*s", static_cast<int>(tooltip.size()), tooltip.data());
        GuiUtils::EndStyledTooltip();
    }
}

void TooltipHelper::InvalidateCache() {
    g_tooltipState.Refresh();
}


void KeybindUI::RenderKeybind(KeybindEntry& entry) {
    RenderKeyButton(entry.keyId.c_str(), entry.waitingForKey, *entry.keyPtr, entry.pendingOriginalKey);
    ImGui::SameLine();

    if (entry.IsToggle()) {
        bool currentEnabled = entry.isEnabled;
        if (ImGui::Checkbox(entry.checkId.c_str(), &currentEnabled) && currentEnabled != entry.isEnabled) {
            ToggleEntry(&entry, currentEnabled);
        }
        ImGui::SameLine();
        RenderName(entry.name, !entry.isEnabled && IsKeyUnbound(*entry.keyPtr));
        TooltipHelper::ShowTooltip(entry.tooltip);
    } else {
        RenderName(entry.name, IsKeyUnbound(*entry.keyPtr));
        TooltipHelper::ShowTooltip(entry.tooltip);
    }

    if (!entry.params.empty()) {
        if (RenderParametersButton(entry.paramButtonId.c_str(), entry.name)) {
            ImGui::OpenPopup(entry.popupId.c_str());
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, GuiUtils::K_POPUP_PADDING);
        if (ImGui::BeginPopup(entry.popupId.c_str())) {
            for (const auto& param : entry.params) {
                RenderParam(param);
            }
            ImGui::EndPopup();
            entry.popupWasOpen = true;
        } else if (entry.popupWasOpen) {
            KeybindConfig::SaveParams(entry);
            entry.popupWasOpen = false;
        }
        ImGui::PopStyleVar();
    }

    HandleKeyAssignment(entry);
    RenderConflictPopup(entry);
}

void KeybindUI::RenderKeybindList(KeybindEntries& entries) {
    const size_t count = entries.size();
    for (size_t i = 0; i < count; ++i) {
        RenderKeybind(entries[i]);
        if (i + 1 < count) {
            ImGui::Spacing();
        }
    }
}


static std::string NormalizeSection(std::string_view name) {
    std::string s(name);
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

void KeybindConfig::LoadKeybind(KeybindEntry& entry) {
    auto section = NormalizeSection(entry.configSection);
    *entry.keyPtr = g_ConfigManager.GetInt(section, "key", *entry.keyPtr);
    entry.prevKey = *entry.keyPtr;

    if (entry.IsToggle()) {
        entry.isEnabled = g_ConfigManager.GetBool(section, "enabled", false);
    }

    LoadParams(entry);
}

void KeybindConfig::SaveKeybind(const KeybindEntry& entry) {
    auto section = NormalizeSection(entry.configSection);
    g_ConfigManager.SetInt(section, "key", *entry.keyPtr);
    if (entry.IsToggle()) {
        g_ConfigManager.SetBool(section, "enabled", entry.isEnabled);
    }
    g_ConfigManager.SaveConfig();
}

void KeybindConfig::LoadParam(const KeybindParam& param, std::string_view configSection) {
    auto section = NormalizeSection(configSection);
    switch (param.type) {
        case KeybindParam::Type::Int: {
            auto* ptr = static_cast<int*>(param.valuePtr);
            *ptr = g_ConfigManager.GetInt(section, std::string(param.name), *ptr);
            break;
        }
        case KeybindParam::Type::Float: {
            auto* ptr = static_cast<float*>(param.valuePtr);
            *ptr = g_ConfigManager.GetFloat(section, std::string(param.name), *ptr);
            break;
        }
        case KeybindParam::Type::Bool: {
            auto* ptr = static_cast<bool*>(param.valuePtr);
            *ptr = g_ConfigManager.GetBool(section, std::string(param.name), *ptr);
            break;
        }
    }
}

void KeybindConfig::SaveParam(const KeybindParam& param, std::string_view configSection) {
    auto section = NormalizeSection(configSection);
    switch (param.type) {
        case KeybindParam::Type::Int:
            g_ConfigManager.SetInt(section, std::string(param.name), *static_cast<int*>(param.valuePtr));
            break;
        case KeybindParam::Type::Float:
            g_ConfigManager.SetFloat(section, std::string(param.name), *static_cast<float*>(param.valuePtr));
            break;
        case KeybindParam::Type::Bool:
            g_ConfigManager.SetBool(section, std::string(param.name), *static_cast<bool*>(param.valuePtr));
            break;
    }
}

void KeybindConfig::LoadParams(KeybindEntry& entry) {
    for (const auto& param : entry.params) {
        LoadParam(param, entry.configSection);
    }
}

void KeybindConfig::SaveParams(const KeybindEntry& entry) {
    for (const auto& param : entry.params) {
        SaveParam(param, entry.configSection);
    }
    g_ConfigManager.SaveConfig();
}

void InitKeybindEntry(KeybindEntry& entry) {
    std::string base = "##Kb_" + entry.name;
    entry.keyId = base + "_key";
    entry.checkId = base;
    entry.popupId = base + "_params";
    entry.paramButtonId = "Config##" + base;
    entry.conflictPopupId = base + "_conflict";

    KeybindConfig::LoadKeybind(entry);

    if (!IsKeyUnbound(*entry.keyPtr)) RegisterEntry(entry);

    if (entry.isEnabled) SetEventsEnabled(&entry, true);
}

void AddKeybind(KeybindEntries& keybinds, KeybindEntry entry) {
    keybinds.push_back(std::move(entry));
    InitKeybindEntry(keybinds.back());
}
