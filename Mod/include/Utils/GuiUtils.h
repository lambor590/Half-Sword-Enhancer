#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <vector>

#include "imgui/imgui.h"
#include "ConfigManager.h"
#include "DefaultStyle.h"
#include "Utils/PresetUtils.h"
#include "Menu/SectionConfig.h"
#include "Utils/OverrideTypes.h"
#include "Utils/GameConstants.h"
#include "SDK/Basic.hpp"
#include "SDK/Enum_Ranks_structs.hpp"
#include "SDK/Enum_MaterialLayer_structs.hpp"

namespace GuiUtils {
    inline constexpr ImVec2 K_TOOLTIP_PADDING{8.0f, 6.0f};
    inline constexpr ImVec2 K_POPUP_PADDING{10.0f, 8.0f};
    inline constexpr float K_DRAG_WIDTH = 120.0f;

    inline bool DebouncedDragFloat(
        const char* label, float* value, float speed, float minValue = 0.0f, float maxValue = 0.0f,
        const char* format = "%.3f"
    ) {
        ImGui::DragFloat(label, value, speed, minValue, maxValue, format);
        return ImGui::IsItemDeactivatedAfterEdit();
    }

    inline bool DebouncedDragFloat3(
        const char* label, float value[3], float speed, float minValue = 0.0f, float maxValue = 0.0f,
        const char* format = "%.3f"
    ) {
        ImGui::DragFloat3(label, value, speed, minValue, maxValue, format);
        return ImGui::IsItemDeactivatedAfterEdit();
    }

    inline bool DebouncedDragInt(
        const char* label, int* value, float speed, int minValue = 0, int maxValue = 0, const char* format = "%d"
    ) {
        ImGui::DragInt(label, value, speed, minValue, maxValue, format);
        return ImGui::IsItemDeactivatedAfterEdit();
    }

    inline bool DebouncedDragScalar(
        const char* label, ImGuiDataType dataType, void* value, float speed, const void* minValue = nullptr,
        const void* maxValue = nullptr, const char* format = nullptr
    ) {
        ImGui::DragScalar(label, dataType, value, speed, minValue, maxValue, format);
        return ImGui::IsItemDeactivatedAfterEdit();
    }

    inline bool DebouncedSliderFloat(
        const char* label, float* value, float minValue, float maxValue, const char* format = "%.3f"
    ) {
        ImGui::SliderFloat(label, value, minValue, maxValue, format);
        return ImGui::IsItemDeactivatedAfterEdit();
    }

    inline void StoreEdited(double& target, float value) noexcept {
        if (ImGui::IsItemEdited()) target = value;
    }

    inline void StoreEdited(SDK::FVector& target, const float value[3]) noexcept {
        if (ImGui::IsItemEdited()) target = {value[0], value[1], value[2]};
    }

    inline void StoreEdited(SDK::FRotator& target, const float value[3]) noexcept {
        if (ImGui::IsItemEdited()) target = {value[0], value[1], value[2]};
    }

    inline void BeginStyledTooltip() noexcept {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, K_TOOLTIP_PADDING);
        ImGui::BeginTooltip();
    }

    inline void EndStyledTooltip() noexcept {
        ImGui::EndTooltip();
        ImGui::PopStyleVar();
    }

    [[nodiscard]] inline bool CheckboxWithTooltip(const char* label, bool* value, const char* tooltip) {
        bool changed = ImGui::Checkbox(label, value);
        if (ImGui::IsItemHovered()) {
            BeginStyledTooltip();
            ImGui::Text("%s", tooltip);
            EndStyledTooltip();
        }
        return changed;
    }

    [[nodiscard]] inline float ComboWidthFromText(float maxTextWidth) noexcept {
        return maxTextWidth + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2;
    }

    [[nodiscard]] inline float CalcComboWidth(const char* widestItem) {
        return ComboWidthFromText(ImGui::CalcTextSize(widestItem).x);
    }

    [[nodiscard]] inline float CalcComboWidth(const char* const* items, int count) {
        float maxW = 0;
        for (int i = 0; i < count; ++i) {
            float w = ImGui::CalcTextSize(items[i]).x;
            if (w > maxW) maxW = w;
        }
        return ComboWidthFromText(maxW);
    }

    [[nodiscard]] inline float CalcComboWidth(const char* (*getter)(void* data, int idx), void* data, int count) {
        float maxW = 0;
        for (int i = 0; i < count; ++i) {
            float w = ImGui::CalcTextSize(getter(data, i)).x;
            if (w > maxW) maxW = w;
        }
        return ComboWidthFromText(maxW);
    }

    inline constexpr auto LOWER_TABLE = [] {
        std::array<char, 256> t{};
        for (int i = 0; i < 256; ++i)
            t[i] = static_cast<char>(i);
        for (int i = 'A'; i <= 'Z'; ++i)
            t[i] = static_cast<char>(i + 32);
        return t;
    }();

    [[nodiscard]] inline bool MatchesFilter(const char* name, size_t nameLen, const char* filter, size_t filterLen) {
        if (filterLen == 0) return true;
        if (filterLen > nameLen) return false;
        char filterLower[128];
        size_t fl = filterLen < 128 ? filterLen : 127;
        for (size_t j = 0; j < fl; ++j)
            filterLower[j] = LOWER_TABLE[static_cast<unsigned char>(filter[j])];
        const size_t limit = nameLen - fl;
        for (size_t i = 0; i <= limit; ++i) {
            if (LOWER_TABLE[static_cast<unsigned char>(name[i])] != filterLower[0]) continue;
            size_t j = 1;
            while (j < fl && LOWER_TABLE[static_cast<unsigned char>(name[i + j])] == filterLower[j])
                ++j;
            if (j == fl) return true;
        }
        return false;
    }

    inline constexpr const char* TIER_LABELS[] = {"Tier 0", "Tier 1", "Tier 2", "Tier 3", "Tier 4",
                                                  "Tier 5", "Tier 6", "Tier 7", "Tier 8"};

    [[nodiscard]] inline float CachedTierComboWidth() {
        static float w = CalcComboWidth(TIER_LABELS, 9);
        return w;
    }

    inline void RenderFreeTierCombo(const char* label, int& tier) {
        ImGui::SetNextItemWidth(CachedTierComboWidth());
        if (ImGui::BeginCombo(label, TIER_LABELS[tier])) {
            for (int t = 0; t <= 8; ++t) {
                if (ImGui::Selectable(TIER_LABELS[t], t == tier)) tier = t;
                if (t == tier) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    inline void RenderPriceDrag(const char* label, double& price, float speed = 1.0f) {
        ImGui::SetNextItemWidth(K_DRAG_WIDTH);
        auto val = static_cast<float>(price);
        DebouncedDragFloat(label, &val, speed, 0.0f, 0.0f, "%.1f");
        StoreEdited(price, val);
    }

    inline void RenderOverrideDrag(const char* label, RuntimeOverride& ovr, float speed = 0.1f) {
        ImGui::PushID(label);
        ImGui::Checkbox("##en", &ovr.enabled);
        ImGui::SameLine();
        if (!ovr.enabled) ImGui::BeginDisabled();
        auto val = static_cast<float>(ovr.value);
        ImGui::SetNextItemWidth(K_DRAG_WIDTH);
        DebouncedDragFloat(label, &val, speed, 0.0f, 0.0f, "%.3f");
        StoreEdited(ovr.value, val);
        if (!ovr.enabled) ImGui::EndDisabled();
        ImGui::PopID();
    }

    inline void RenderOverrideInt(const char* label, IntOverride& ovr, float speed = 0.1f) {
        ImGui::PushID(label);
        ImGui::Checkbox("##en", &ovr.enabled);
        ImGui::SameLine();
        if (!ovr.enabled) ImGui::BeginDisabled();
        ImGui::SetNextItemWidth(K_DRAG_WIDTH);
        DebouncedDragInt(label, &ovr.value, speed, 0, 0);
        if (!ovr.enabled) ImGui::EndDisabled();
        ImGui::PopID();
    }

    inline void RenderOverrideBool(const char* label, BoolOverride& ovr) {
        static constexpr const char* TRISTATE[] = {"Default", "No", "Yes"};
        static float tristateW = CalcComboWidth(TRISTATE, 3);

        int current = ovr.enabled ? (ovr.value ? 2 : 1) : 0;
        ImGui::PushID(label);
        ImGui::SetNextItemWidth(tristateW);
        if (ImGui::Combo(label, &current, TRISTATE, 3)) {
            ovr.enabled = (current != 0);
            ovr.value = (current == 2);
        }
        ImGui::PopID();
    }

    inline constexpr float UNDERLINE_TAB_HPAD = 12.0f;
    inline constexpr float UNDERLINE_TAB_VPAD = 4.0f;
    inline constexpr float UNDERLINE_THICKNESS = 2.0f;
    inline constexpr float UNDERLINE_GAP = 6.0f;
    inline constexpr float TAB_SPACING = 4.0f;

    inline void RenderUnderlineTabs(const char* id, int& activeTab, const char* const* labels, int count) {
        ImGui::PushID(id);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float textH = ImGui::GetTextLineHeight();
        float rowH = textH + UNDERLINE_TAB_VPAD * 2;
        float availWidth = ImGui::GetContentRegionAvail().x;

        static const ImU32 ACTIVE_COL = ImGui::ColorConvertFloat4ToU32(DefaultStyle::OLD_BRASS);
        static const ImU32 INACTIVE_COL = ImGui::ColorConvertFloat4ToU32(DefaultStyle::TEXT_DISABLED);
        static const ImU32 HOVER_COL = ImGui::ColorConvertFloat4ToU32(DefaultStyle::PARCHMENT_DARK);
        static const ImU32 BASELINE_COL = ImGui::ColorConvertFloat4ToU32(
            ImVec4(DefaultStyle::MEDIUM_WOOD.x, DefaultStyle::MEDIUM_WOOD.y, DefaultStyle::MEDIUM_WOOD.z, 0.35f)
        );

        float x = cursor.x;
        for (int i = 0; i < count; ++i) {
            ImVec2 textSize = ImGui::CalcTextSize(labels[i]);
            float tabW = textSize.x + UNDERLINE_TAB_HPAD * 2;

            ImGui::PushID(i);
            ImGui::SetCursorScreenPos(ImVec2(x, cursor.y));
            if (ImGui::InvisibleButton("##tab", ImVec2(tabW, rowH))) activeTab = i;
            bool hovered = ImGui::IsItemHovered();
            if (i < count - 1) ImGui::SameLine(0, TAB_SPACING);

            bool isActive = (i == activeTab);
            ImU32 textCol = isActive ? ACTIVE_COL : (hovered ? HOVER_COL : INACTIVE_COL);
            ImVec2 textPos(x + UNDERLINE_TAB_HPAD, cursor.y + UNDERLINE_TAB_VPAD);
            dl->AddText(textPos, textCol, labels[i]);

            if (isActive) {
                float lineY = cursor.y + rowH;
                dl->AddLine(ImVec2(x, lineY), ImVec2(x + tabW, lineY), ACTIVE_COL, UNDERLINE_THICKNESS);
            }

            x += tabW + TAB_SPACING;
            ImGui::PopID();
        }

        float baselineY = cursor.y + rowH + 1.0f;
        dl->AddLine(ImVec2(cursor.x, baselineY), ImVec2(cursor.x + availWidth, baselineY), BASELINE_COL, 1.0f);

        ImGui::SetCursorScreenPos(ImVec2(cursor.x, baselineY + UNDERLINE_GAP));
        ImGui::Spacing();
        ImGui::PopID();
    }

    struct StatusMessage {
        std::string text;
        double time = 0.0;
        bool isError = false;

        void Set(const std::string& msg, bool error = false) {
            text = msg;
            time = ImGui::GetTime();
            isError = error;
        }

        void Render() {
            if (text.empty()) return;
            if (ImGui::GetTime() - time > 3.0) {
                text.clear();
                return;
            }
            ImVec4 color = isError ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
            ImGui::TextColored(color, "%s", text.c_str());
        }
    };

    inline void RenderColorEditor(const char* label, SDK::FLinearColor& color) {
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.75f);
        float col[4] = {color.R, color.G, color.B, color.A};
        if (ImGui::ColorEdit4(label, col)) color = {col[0], col[1], col[2], col[3]};
        ImGui::PopItemWidth();
    }

    inline void RenderFreeTierCombo(const char* label, SDK::Enum_Ranks& tier) {
        int val = static_cast<int>(tier);
        RenderFreeTierCombo(label, val);
        tier = static_cast<SDK::Enum_Ranks>(val);
    }

    inline void RenderMaterialCombo(const char* label, SDK::Enum_MaterialLayer& mat) {
        static float materialComboW =
            CalcComboWidth(GameConstants::MATERIAL_LAYER_NAMES, GameConstants::MATERIAL_LAYER_COUNT);

        int val = static_cast<int>(mat);
        const char* preview = (val >= 0 && val < GameConstants::MATERIAL_LAYER_COUNT)
                                  ? GameConstants::MATERIAL_LAYER_NAMES[val]
                                  : "Unknown";
        ImGui::SetNextItemWidth(materialComboW);
        if (ImGui::BeginCombo(label, preview)) {
            for (int i = 0; i < GameConstants::MATERIAL_LAYER_COUNT; ++i) {
                if (ImGui::Selectable(GameConstants::MATERIAL_LAYER_NAMES[i], val == i))
                    mat = static_cast<SDK::Enum_MaterialLayer>(i);
                if (val == i) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    struct PresetTreeAction {
        enum class Type : uint8_t { None, Load, Delete };
        Type type = Type::None;
        std::filesystem::path path;
    };

    [[nodiscard]] inline PresetTreeAction RenderPresetTree(const PresetUtils::PresetTreeNode& node) {
        PresetTreeAction action;
        static const float LOAD_W = ImGui::CalcTextSize("Load").x + ImGui::GetStyle().FramePadding.x * 2;
        static const float DEL_W = ImGui::CalcTextSize("Del").x + ImGui::GetStyle().FramePadding.x * 2;
        static const float BUTTONS_WIDTH = LOAD_W + DEL_W + ImGui::GetStyle().ItemSpacing.x * 2;

        for (const auto& child : node.children) {
            if (ImGui::TreeNode(child.name.c_str())) {
                auto childAction = RenderPresetTree(child);
                if (childAction.type != PresetTreeAction::Type::None) action = std::move(childAction);
                ImGui::TreePop();
            }
        }

        for (size_t i = 0; i < node.presets.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            float textW = ImGui::GetContentRegionAvail().x - BUTTONS_WIDTH;

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(node.presets[i].name.c_str());
            if (textW > 0) ImGui::SameLine(textW);
            if (ImGui::Button("Load")) action = {PresetTreeAction::Type::Load, node.presets[i].path};
            ImGui::SameLine();
            if (ImGui::Button("Del")) action = {PresetTreeAction::Type::Delete, node.presets[i].path};
            ImGui::PopID();
        }

        return action;
    }

    struct PresetPanelState {
        char* nameBuf;
        size_t nameBufSize;
        bool& listDirty;
        PresetUtils::PresetTreeNode& tree;
        StatusMessage& status;
        std::filesystem::path& pendingDeletePath;
        bool canSave = true;
    };

    using PresetCallback = std::function<void()>;
    using PresetNameCallback = std::function<void(const char*)>;
    using PresetPathCallback = std::function<void(const std::filesystem::path&)>;

    inline void RenderPresetPanel(
        PresetPanelState& state, const std::filesystem::path& presetsDir, const PresetCallback& refreshTree,
        PresetNameCallback onSave, PresetPathCallback onLoad, PresetPathCallback onDelete
    ) {
        ImGui::SeparatorText("Save");
        static float btnWidth = ImGui::CalcTextSize("Save").x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - btnWidth - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputTextWithHint("##PresetName", "folder/name...", state.nameBuf, state.nameBufSize);
        ImGui::SameLine();
        bool canSave = state.nameBuf[0] != '\0' && state.canSave;
        if (!canSave) ImGui::BeginDisabled();
        if (ImGui::Button("Save")) onSave(state.nameBuf);
        if (!canSave) ImGui::EndDisabled();

        ImGui::SeparatorText("Presets");
        if (state.listDirty) refreshTree();

        if (state.tree.presets.empty() && state.tree.children.empty()) {
            ImGui::TextDisabled("No saved presets");
        } else {
            ImGui::BeginChild(
                "##presetList", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 8), ImGuiChildFlags_Borders
            );
            auto action = RenderPresetTree(state.tree);
            ImGui::EndChild();

            if (action.type == PresetTreeAction::Type::Load)
                onLoad(action.path);
            else if (action.type == PresetTreeAction::Type::Delete) {
                state.pendingDeletePath = action.path;
                ImGui::OpenPopup("##delete_preset_confirm");
            }
        }

        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0.6f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, K_POPUP_PADDING);
        if (ImGui::BeginPopupModal(
                "##delete_preset_confirm", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings
            )) {
            const auto relativePath = state.pendingDeletePath.lexically_relative(presetsDir);
            const auto displayPath =
                (relativePath.empty() ? state.pendingDeletePath.filename() : relativePath).string();

            ImGui::Text("Delete preset?");
            ImGui::Spacing();
            ImGui::TextWrapped("%s", displayPath.c_str());
            ImGui::Spacing();

            if (ImGui::Button("Delete")) {
                onDelete(state.pendingDeletePath);
                state.pendingDeletePath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                state.pendingDeletePath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::Spacing();
        if (ImGui::Button("Open Presets Folder", ImVec2(-1, 0))) PresetUtils::OpenInExplorer(presetsDir);
    }

    inline void RenderOverrideCount(int count) {
        if (count > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%d active)", count);
        }
    }

    inline void RenderPreviewControls(PreviewConfig& preview, const char* itemType = "preview") {
        char label[64];
        std::snprintf(label, sizeof(label), "Auto-spawn a %s that updates as you edit", itemType);
        (void)CheckboxWithTooltip("Live Preview", &preview.livePreview, label);
        if (preview.livePreview) {
            ImGui::SameLine();
            std::snprintf(label, sizeof(label), "Continuously rotate the %s", itemType);
            (void)CheckboxWithTooltip("Auto-Rotate", &preview.autoRotate, label);
            if (preview.autoRotate) {
                ImGui::SetNextItemWidth(K_DRAG_WIDTH);
                DebouncedDragFloat("Rotation Speed", &preview.rotationSpeed, 1.0f, -360.0f, 360.0f, "%.0f deg/s");
            }
        }
    }

    /// Caller must call ImGui::EndChild() after content.
    inline float BeginScrollWithFooter(const char* id) {
        float footerH = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        float scrollH = ImGui::GetContentRegionAvail().y - footerH;
        if (scrollH < 100.0f) scrollH = 100.0f;
        ImGui::BeginChild(id, ImVec2(0, scrollH));
        return scrollH;
    }

    template <typename Entry>
    inline void RenderGlobalModuleCombo(
        const char* label, SDK::UClass*& current, const std::vector<Entry>& options, char* filterBuf,
        float& cachedWidth, bool allowNone = true
    ) {
        const char* preview = "None";
        for (const auto& e : options)
            if (e.cls == current) {
                preview = e.name.c_str();
                break;
            }

        if (cachedWidth == 0.0f) {
            float maxW = 0;
            for (const auto& e : options) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%-36s [%s]", e.name.c_str(), e.sourceType);
                float w = ImGui::CalcTextSize(buf).x;
                if (w > maxW) maxW = w;
            }
            cachedWidth = ComboWidthFromText(maxW);
        }

        ImGui::SetNextItemWidth(cachedWidth);
        if (!ImGui::BeginCombo(label, preview)) return;

        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##filter", "Search modules...", filterBuf, 64);

        const size_t filterLen = std::strlen(filterBuf);
        const bool hasFilter = filterLen > 0;

        if (hasFilter) {
            int visible = 0;
            for (const auto& e : options)
                if (MatchesFilter(e.name.c_str(), e.name.size(), filterBuf, filterLen)) ++visible;
            ImGui::TextDisabled("Showing %d of %d", visible, static_cast<int>(options.size()));
        }

        ImGui::Separator();

        if (allowNone && ImGui::Selectable("None", current == nullptr)) current = nullptr;

        char display[128];
        for (const auto& e : options) {
            if (hasFilter && !MatchesFilter(e.name.c_str(), e.name.size(), filterBuf, filterLen)) continue;
            std::snprintf(display, sizeof(display), "%-36s [%s]", e.name.c_str(), e.sourceType);
            if (ImGui::Selectable(display, e.cls == current)) current = e.cls;
            if (e.cls == current) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    template <typename Entry>
    inline void RenderModuleIndexCombo(
        const char* label, int32_t& moduleIndex, const std::vector<Entry>& available, char* filterBuf,
        float& cachedWidth
    ) {
        if (available.empty()) {
            ImGui::TextDisabled("No %s modules available", label);
            return;
        }

        const char* preview = "None";
        if (moduleIndex > 0 && moduleIndex <= static_cast<int32_t>(available.size()))
            preview = available[moduleIndex - 1].name.c_str();

        if (cachedWidth == 0.0f) {
            float maxW = 0;
            for (const auto& e : available) {
                float w = ImGui::CalcTextSize(e.name.c_str()).x;
                if (w > maxW) maxW = w;
            }
            cachedWidth = ComboWidthFromText(maxW);
        }

        ImGui::SetNextItemWidth(cachedWidth);
        if (!ImGui::BeginCombo(label, preview)) return;

        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##filter", "Search modules...", filterBuf, 64);

        const size_t filterLen = std::strlen(filterBuf);
        const bool hasFilter = filterLen > 0;

        if (hasFilter) {
            int visible = 0;
            for (const auto& e : available)
                if (MatchesFilter(e.name.c_str(), e.name.size(), filterBuf, filterLen)) ++visible;
            ImGui::TextDisabled("Showing %d of %d", visible, static_cast<int>(available.size()));
        }

        ImGui::Separator();

        if (ImGui::Selectable("None", moduleIndex <= 0)) moduleIndex = 0;

        for (int i = 0; i < static_cast<int>(available.size()); ++i) {
            if (hasFilter && !MatchesFilter(available[i].name.c_str(), available[i].name.size(), filterBuf, filterLen))
                continue;
            bool selected = (moduleIndex == i + 1);
            if (ImGui::Selectable(available[i].name.c_str(), selected)) moduleIndex = i + 1;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

}
