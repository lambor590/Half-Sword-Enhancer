#pragma once

#include <algorithm>
#include <array>
#include <cfloat>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "DefaultStyle.h"
#include "Utils/PresetUtils.h"
#include "Menu/SectionConfig.h"
#include "Utils/GameConstants.h"
#include "SDK/Basic.hpp"
#include "SDK/Enum_Ranks_structs.hpp"
#include "SDK/Enum_MaterialLayer_structs.hpp"

namespace GuiUtils {
    inline constexpr ImVec2 K_TOOLTIP_PADDING{8.0f, 6.0f};
    inline constexpr ImVec2 K_POPUP_PADDING{10.0f, 8.0f};
    inline constexpr float K_DRAG_WIDTH = 120.0f;
    inline constexpr float K_COMBO_MIN_WIDTH = 90.0f;
    inline constexpr float K_COMBO_MAX_WIDTH = 320.0f;
    inline constexpr float K_COMBO_SEARCH_MIN_WIDTH = 140.0f;
    inline constexpr float K_COLOR_FIELD_MIN_WIDTH = 220.0f;
    inline constexpr float K_COLOR_FIELD_MAX_WIDTH = 480.0f;
    inline constexpr float K_COLOR_FIELD_WIDTH_RATIO = 0.7f;
    inline constexpr float K_INPUT_MAX_WIDTH = 360.0f;
    inline bool g_helpTooltipsEnabled = true;

    struct WidthSpec {
        float min = 0.0f;
        float preferred = 0.0f;
        float max = FLT_MAX;
    };

    [[nodiscard]] inline float ResolveControlWidth(WidthSpec spec, float intrinsicWidth = 0.0f) noexcept {
        const float available = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
        const float maximum = (std::min)(available, spec.max > 0.0f ? spec.max : available);
        const float minimum = (std::min)(maximum, (std::max)(1.0f, spec.min));
        const float preferred = spec.preferred > 0.0f ? spec.preferred : intrinsicWidth;
        return (std::clamp)(preferred > 0.0f ? preferred : minimum, minimum, maximum);
    }

    [[nodiscard]] inline float ResolveInputWidth(float preferred = FLT_MAX) noexcept {
        return ResolveControlWidth({0.0f, preferred, K_INPUT_MAX_WIDTH});
    }

    inline void SetNextInputWidth(float preferred = FLT_MAX) noexcept {
        ImGui::SetNextItemWidth(ResolveInputWidth(preferred));
    }

    inline void SetNextFieldWidth(WidthSpec spec) noexcept {
        ImGui::SetNextItemWidth(ResolveControlWidth(spec, spec.preferred));
    }

    inline void TextDisabledWrapped(std::string_view text) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%.*s", static_cast<int>(text.size()), text.empty() ? "" : text.data());
        ImGui::PopTextWrapPos();
    }

    inline void SetNextColorFieldWidth(const char* label) noexcept {
        const float available = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
        const float visibleLabelWidth = label && label[0] != '\0' ? ImGui::CalcTextSize(label, nullptr, true).x +
                                                                        ImGui::GetStyle().ItemInnerSpacing.x
                                                                  : 0.0f;
        const float maximum = (std::max)(1.0f, available - visibleLabelWidth);
        const float preferred =
            (std::clamp)(available * K_COLOR_FIELD_WIDTH_RATIO, K_COLOR_FIELD_MIN_WIDTH, K_COLOR_FIELD_MAX_WIDTH);
        ImGui::SetNextItemWidth((std::min)(maximum, preferred));
    }

    [[nodiscard]] inline bool SameLineIfFits(float nextItemWidth) noexcept {
        const float nextItemLeft = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x;
        const float contentRight = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        if (nextItemLeft + nextItemWidth > contentRight) return false;
        ImGui::SameLine();
        return true;
    }

    inline void SetHelpTooltipsEnabled(bool enabled) noexcept {
        g_helpTooltipsEnabled = enabled;
    }

    [[nodiscard]] inline bool HelpTooltipsEnabled() noexcept {
        return g_helpTooltipsEnabled;
    }

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

    inline void ClippedTextTooltip(const char* text, const char* textEnd = nullptr) {
        if (!text || !ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled)) return;
        if (!textEnd) textEnd = text + std::strlen(text);
        BeginStyledTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        ImGui::TextUnformatted(text, textEnd);
        ImGui::PopTextWrapPos();
        EndStyledTooltip();
    }

    inline void HelpTooltip(std::string_view tooltip) {
        if (!g_helpTooltipsEnabled || tooltip.empty() ||
            !ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
            return;
        BeginStyledTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        ImGui::TextUnformatted(tooltip.data(), tooltip.data() + tooltip.size());
        ImGui::PopTextWrapPos();
        EndStyledTooltip();
    }

    [[nodiscard]] inline bool CheckboxWithTooltip(const char* label, bool* value, const char* tooltip) {
        bool changed = ImGui::Checkbox(label, value);
        HelpTooltip(tooltip);
        return changed;
    }

    [[nodiscard]] inline float ComboWidthFromText(float maxTextWidth) noexcept {
        const auto& style = ImGui::GetStyle();
        return maxTextWidth + ImGui::GetFrameHeight() + style.FramePadding.x * 2.0f + style.ItemInnerSpacing.x;
    }

    inline void PrepareNextCombo(float controlWidth, float popupWidth, ImGuiComboFlags flags = 0) noexcept {
        float popupMaxHeight = FLT_MAX;
        if ((flags & ImGuiComboFlags_HeightLargest) == 0) {
            int maxItems = 8;
            if (flags & ImGuiComboFlags_HeightSmall)
                maxItems = 4;
            else if (flags & ImGuiComboFlags_HeightLarge)
                maxItems = 20;

            const auto& style = ImGui::GetStyle();
            popupMaxHeight = (ImGui::GetFontSize() + style.ItemSpacing.y) * maxItems - style.ItemSpacing.y +
                             style.WindowPadding.y * 2.0f;
        }

        constexpr float VIEWPORT_MARGIN = 32.0f;
        const float popupLimit =
            (std::max)(controlWidth, ImGui::GetMainViewport()->WorkSize.x - VIEWPORT_MARGIN);
        const float resolvedPopupWidth = (std::min)((std::max)(controlWidth, popupWidth), popupLimit);
        ImGui::SetNextItemWidth(controlWidth);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(resolvedPopupWidth, 0.0f), ImVec2(popupLimit, popupMaxHeight)
        );
    }

    inline void PrepareNextCombo(float width, ImGuiComboFlags flags = 0) noexcept {
        const float controlWidth = ResolveControlWidth({K_COMBO_MIN_WIDTH, width, K_COMBO_MAX_WIDTH}, width);
        PrepareNextCombo(controlWidth, width, flags);
    }

    inline void ShowClippedComboPreviewTooltip(
        const char* preview, float controlWidth, ImGuiComboFlags flags, bool comboOpen
    ) {
        if (comboOpen || !preview || (flags & ImGuiComboFlags_NoPreview) != 0) return;
        const auto& style = ImGui::GetStyle();
        const float previewWidth =
            (std::max)(0.0f, controlWidth - ImGui::GetFrameHeight() - style.FramePadding.x * 2.0f);
        if (ImGui::CalcTextSize(preview).x > previewWidth) ClippedTextTooltip(preview);
    }

    [[nodiscard]] inline bool BeginSizedCombo(
        const char* label, const char* preview, float width, ImGuiComboFlags flags = 0
    ) noexcept {
        const float controlWidth = ResolveControlWidth({K_COMBO_MIN_WIDTH, width, K_COMBO_MAX_WIDTH}, width);
        PrepareNextCombo(controlWidth, width, flags);
        const bool open = ImGui::BeginCombo(label, preview, flags);
        ShowClippedComboPreviewTooltip(preview, controlWidth, flags, open);
        return open;
    }

    [[nodiscard]] inline bool BeginSizedCombo(
        const char* label, const char* preview, WidthSpec width, ImGuiComboFlags flags = 0
    ) noexcept {
        const float intrinsicWidth = preview ? ComboWidthFromText(ImGui::CalcTextSize(preview).x) : K_COMBO_MIN_WIDTH;
        const float controlWidth = ResolveControlWidth(width, intrinsicWidth);
        const float popupWidth = width.preferred > 0.0f ? width.preferred : intrinsicWidth;
        PrepareNextCombo(controlWidth, popupWidth, flags);
        const bool open = ImGui::BeginCombo(label, preview, flags);
        ShowClippedComboPreviewTooltip(preview, controlWidth, flags, open);
        return open;
    }

    inline void SetComboSearchWidth(float width) noexcept {
        const float searchWidth = width - ImGui::GetStyle().WindowPadding.x * 2.0f;
        ImGui::SetNextItemWidth(
            ResolveControlWidth({K_COMBO_SEARCH_MIN_WIDTH, searchWidth, K_INPUT_MAX_WIDTH}, searchWidth)
        );
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

    inline bool RenderEnumCombo(
        const char* label, int& value, const std::vector<std::string>& names, float maxTextWidthEm
    ) {
        if (names.empty()) return false;
        if (value < 0 || value >= static_cast<int>(names.size())) value = 0;

        bool changed = false;
        if (BeginSizedCombo(label, names[value].c_str(), ComboWidthFromText(maxTextWidthEm * ImGui::GetFontSize()))) {
            for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                if (ImGui::Selectable(names[i].c_str(), value == i)) {
                    value = i;
                    changed = true;
                }
                if (value == i) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
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
        const size_t limit = nameLen - filterLen;
        const char first = LOWER_TABLE[static_cast<unsigned char>(filter[0])];
        for (size_t i = 0; i <= limit; ++i) {
            if (LOWER_TABLE[static_cast<unsigned char>(name[i])] != first) continue;
            size_t j = 1;
            while (j < filterLen && LOWER_TABLE[static_cast<unsigned char>(name[i + j])] ==
                                        LOWER_TABLE[static_cast<unsigned char>(filter[j])])
                ++j;
            if (j == filterLen) return true;
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
        if (BeginSizedCombo(label, TIER_LABELS[tier], CachedTierComboWidth())) {
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

    enum class ButtonTone : uint8_t { Default, Primary, Danger, Quiet };

    [[nodiscard]] inline const char* VisibleLabelEnd(const char* label) noexcept {
        if (const char* idMarker = std::strstr(label, "##")) return idMarker;
        return label + std::strlen(label);
    }

    [[nodiscard]] inline float ButtonNaturalWidth(const char* label) noexcept {
        const char* visibleEnd = VisibleLabelEnd(label);
        return ImGui::CalcTextSize(label, visibleEnd).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    }

    [[nodiscard]] inline float CheckboxNaturalWidth(const char* label) noexcept {
        const char* visibleEnd = VisibleLabelEnd(label);
        const float labelWidth = ImGui::CalcTextSize(label, visibleEnd).x;
        return ImGui::GetFrameHeight() + (labelWidth > 0.0f ? ImGui::GetStyle().ItemInnerSpacing.x + labelWidth : 0.0f);
    }

    [[nodiscard]] inline bool SameLineIfFitsButton(const char* label) noexcept {
        return SameLineIfFits(ButtonNaturalWidth(label));
    }

    [[nodiscard]] inline bool SameLineIfFitsCheckbox(const char* label) noexcept {
        return SameLineIfFits(CheckboxNaturalWidth(label));
    }

    [[nodiscard]] inline bool Button(
        const char* label, ButtonTone tone = ButtonTone::Default, ImVec2 size = ImVec2(0.0f, 0.0f)
    ) {
        const char* visibleEnd = VisibleLabelEnd(label);
        const float naturalWidth = ImGui::CalcTextSize(label, visibleEnd).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        const float available = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
        if (size.x < 0.0f)
            size.x = available;
        else if (size.x == 0.0f)
            size.x = ResolveControlWidth({0.0f, naturalWidth, FLT_MAX}, naturalWidth);
        else
            size.x = (std::min)(size.x, available);

        const bool clipped = naturalWidth > size.x + 0.5f;
        std::string renderedLabel;
        const char* effectiveLabel = label;
        if (clipped) {
            constexpr std::string_view ELLIPSIS = "...";
            const float availableTextWidth = (std::max)(0.0f, size.x - ImGui::GetStyle().FramePadding.x * 2.0f);
            const float ellipsisWidth = ImGui::CalcTextSize(ELLIPSIS.data(), ELLIPSIS.data() + ELLIPSIS.size()).x;
            size_t visibleBytes = static_cast<size_t>(visibleEnd - label);
            while (visibleBytes > 0 &&
                   ImGui::CalcTextSize(label, label + visibleBytes).x + ellipsisWidth > availableTextWidth) {
                --visibleBytes;
                while (visibleBytes > 0 && (static_cast<unsigned char>(label[visibleBytes]) & 0xC0U) == 0x80U) {
                    --visibleBytes;
                }
            }

            renderedLabel.assign(label, visibleBytes);
            renderedLabel.append(ELLIPSIS);
            renderedLabel.append("###");
            renderedLabel.append(label);
            effectiveLabel = renderedLabel.c_str();
        }

        int pushedColors = 0;
        switch (tone) {
            case ButtonTone::Default: break;
            case ButtonTone::Primary: {
                const bool disabled = (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
                ImGui::PushStyleColor(
                    ImGuiCol_Button, disabled ? DefaultStyle::LIGHT_WOOD : DefaultStyle::OLD_BRASS
                );
                ImGui::PushStyleColor(
                    ImGuiCol_ButtonHovered, disabled ? DefaultStyle::LIGHT_WOOD : DefaultStyle::BRIGHT_BRASS
                );
                ImGui::PushStyleColor(
                    ImGuiCol_ButtonActive, disabled ? DefaultStyle::LIGHT_WOOD : DefaultStyle::PARCHMENT_DARK
                );
                ImGui::PushStyleColor(
                    ImGuiCol_Text, disabled ? DefaultStyle::PARCHMENT_DARK : DefaultStyle::DARK_INK
                );
                pushedColors = 4;
                break;
            }
            case ButtonTone::Danger:
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.16f, 0.13f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.74f, 0.23f, 0.18f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.42f, 0.10f, 0.08f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, DefaultStyle::PARCHMENT);
                pushedColors = 4;
                break;
            case ButtonTone::Quiet:
                ImGui::PushStyleColor(ImGuiCol_Button, DefaultStyle::CLEAR);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DefaultStyle::HEADER);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, DefaultStyle::HEADER_ACTIVE);
                ImGui::PushStyleColor(ImGuiCol_Text, DefaultStyle::PARCHMENT_DARK);
                pushedColors = 4;
                break;
        }

        const bool pressed = ImGui::Button(effectiveLabel, size);
        if (pushedColors > 0) ImGui::PopStyleColor(pushedColors);
        if (clipped) ClippedTextTooltip(label, visibleEnd);
        return pressed;
    }

    [[nodiscard]] inline bool Button(const char* label, ButtonTone tone, WidthSpec width) {
        const float resolved = ResolveControlWidth(width, ButtonNaturalWidth(label));
        return Button(label, tone, ImVec2(resolved, 0.0f));
    }

    enum class CalloutTone : uint8_t { Info, Success, Warning, Error };

    inline constexpr std::array CALLOUT_BACKGROUNDS = {
        ImVec4{0.12f, 0.24f, 0.32f, 0.72f},
        ImVec4{0.12f, 0.28f, 0.16f, 0.72f},
        ImVec4{0.32f, 0.23f, 0.08f, 0.72f},
        ImVec4{0.32f, 0.10f, 0.08f, 0.72f},
    };

    struct CalloutResult {
        bool actionClicked = false;
        bool dismissed = false;
    };

    inline CalloutResult RenderCallout(
        const char* id, std::string_view message, CalloutTone tone, bool dismissible = false,
        const char* actionLabel = nullptr
    ) {
        const ImVec4 background = CALLOUT_BACKGROUNDS[static_cast<size_t>(tone)];

        const auto& style = ImGui::GetStyle();
        const float messageWidth =
            message.empty() ? 0.0f : ImGui::CalcTextSize(message.data(), message.data() + message.size()).x;
        float actionWidth = actionLabel ? ButtonNaturalWidth(actionLabel) : 0.0f;
        if (dismissible) {
            if (actionLabel) actionWidth += style.ItemSpacing.x;
            actionWidth += ButtonNaturalWidth("Dismiss");
        }
        constexpr float CALLOUT_MAX_WIDTH = 560.0f;
        const float preferredWidth = (std::max)(messageWidth, actionWidth) + K_TOOLTIP_PADDING.x * 2.0f;
        const float calloutWidth = ResolveControlWidth({0.0f, preferredWidth, CALLOUT_MAX_WIDTH}, preferredWidth);

        CalloutResult result;
        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, K_TOOLTIP_PADDING);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);

        constexpr ImGuiChildFlags CHILD_FLAGS =
            ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_NavFlattened;
        constexpr ImGuiWindowFlags WINDOW_FLAGS =
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;
        const bool visible = ImGui::BeginChild("##callout", ImVec2(calloutWidth, 0.0f), CHILD_FLAGS, WINDOW_FLAGS);
        if (visible) {
            if (!message.empty()) {
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextUnformatted(message.data(), message.data() + message.size());
                ImGui::PopTextWrapPos();
            }

            if (actionLabel || dismissible) {
                if (actionLabel) result.actionClicked = Button(actionLabel, ButtonTone::Primary);
                if (actionLabel && dismissible) (void)SameLineIfFits(ButtonNaturalWidth("Dismiss"));
                if (dismissible) result.dismissed = Button("Dismiss", ButtonTone::Quiet);
            }
        }
        ImGui::EndChild();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();
        ImGui::PopID();
        return result;
    }

    inline void RenderInlineCallout(std::string_view message, CalloutTone tone) {
        if (message.empty()) return;

        const auto& style = ImGui::GetStyle();
        const ImVec2 anchorMinimum = ImGui::GetItemRectMin();
        const ImVec2 anchorMaximum = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float contentRight = (std::min)(
            ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x, drawList->GetClipRectMax().x
        );
        const float availableWidth = contentRight - anchorMaximum.x - style.ItemSpacing.x;
        if (availableWidth <= 1.0f) return;

        const ImVec2 textSize = ImGui::CalcTextSize(message.data(), message.data() + message.size());
        const float intrinsicWidth = textSize.x + K_TOOLTIP_PADDING.x * 2.0f;
        const float width = (std::min)(intrinsicWidth, availableWidth);
        const float height = (std::max)(1.0f, anchorMaximum.y - anchorMinimum.y);
        const ImVec2 minimum{anchorMaximum.x + style.ItemSpacing.x, anchorMinimum.y};
        const ImVec2 maximum{minimum.x + width, minimum.y + height};
        const float horizontalPadding = (std::min)(K_TOOLTIP_PADDING.x, width * 0.25f);
        const ImVec2 textPosition{minimum.x + horizontalPadding, minimum.y + (height - textSize.y) * 0.5f};
        const ImVec4 clipRect{
            minimum.x + horizontalPadding, minimum.y, maximum.x - horizontalPadding, maximum.y
        };

        drawList->AddRectFilled(
            minimum, maximum, ImGui::GetColorU32(CALLOUT_BACKGROUNDS[static_cast<size_t>(tone)]), 4.0f
        );
        drawList->AddText(
            ImGui::GetFont(), ImGui::GetFontSize(), textPosition, ImGui::GetColorU32(ImGuiCol_Text), message.data(),
            message.data() + message.size(), 0.0f, &clipRect
        );

        if (intrinsicWidth > width + 0.5f && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(minimum, maximum)) {
            ImGui::SetTooltip("%.*s", static_cast<int>(message.size()), message.data());
        }
    }

    inline void RenderUnderlineTabs(const char* id, int& activeTab, const char* const* labels, int count) {
        if (!labels || count <= 0) return;
        activeTab = std::clamp(activeTab, 0, count - 1);

        if (ImGui::BeginTabBar(id, ImGuiTabBarFlags_FittingPolicyResizeDown)) {
            for (int i = 0; i < count; ++i) {
                if (ImGui::BeginTabItem(labels[i])) {
                    activeTab = i;
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
        ImGui::Spacing();
    }

    struct StatusMessage {
        using Token = std::uint64_t;

        std::string text;
        const char* resultText = nullptr;
        std::chrono::steady_clock::time_point resultDeadline{};
        Token revision = 0;
        CalloutTone tone = CalloutTone::Info;

        void SetError(std::string msg) {
            resultText = nullptr;
            text = std::move(msg);
            tone = CalloutTone::Error;
            ++revision;
        }

        Token SetInfo(std::string msg) {
            resultText = nullptr;
            text = std::move(msg);
            tone = CalloutTone::Info;
            return ++revision;
        }

        template <std::size_t N> void Notify(const char (&msg)[N]) noexcept {
            static_assert(N > 1);
            text.clear();
            ++revision;
            resultText = msg;
            resultDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        }

        void Clear() noexcept {
            text.clear();
            resultText = nullptr;
            ++revision;
        }

        void ClearText() noexcept {
            text.clear();
            ++revision;
        }

        void ClearText(Token token) noexcept {
            if (token != 0 && token == revision) ClearText();
        }

        void RenderResult() {
            if (!resultText) return;
            if (std::chrono::steady_clock::now() > resultDeadline) {
                resultText = nullptr;
                return;
            }
            RenderInlineCallout(resultText, CalloutTone::Success);
        }

        void Render() {
            if (text.empty()) return;

            ImGui::PushID(this);
            if (RenderCallout("##status", text, tone, tone == CalloutTone::Error).dismissed) ClearText();
            ImGui::PopID();
        }
    };

    inline void RenderColorEditor(const char* label, SDK::FLinearColor& color) {
        SetNextColorFieldWidth(label);
        float col[4] = {color.R, color.G, color.B, color.A};
        if (ImGui::ColorEdit4(label, col)) color = {col[0], col[1], col[2], col[3]};
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
        if (BeginSizedCombo(label, preview, materialComboW)) {
            for (int i = 0; i < GameConstants::MATERIAL_LAYER_COUNT; ++i) {
                if (ImGui::Selectable(GameConstants::MATERIAL_LAYER_NAMES[i], val == i))
                    mat = static_cast<SDK::Enum_MaterialLayer>(i);
                if (val == i) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    using PresetUtils::PresetPathsEqual;

    [[nodiscard]] inline std::string PresetDisplayPath(
        const std::filesystem::path& path, const std::filesystem::path& presetsDir, bool includeExtension = false
    ) {
        auto relative = path.lexically_relative(presetsDir);
        if (relative.empty() || (!relative.empty() && *relative.begin() == std::filesystem::path{".."}))
            relative = path.filename();
        if (!includeExtension) relative.replace_extension();
        return PresetUtils::PathToUtf8(relative);
    }

    [[nodiscard]] inline bool TryPresetPathFromName(
        const std::filesystem::path& presetsDir, const char* name, std::filesystem::path& path, std::string& error
    ) {
        std::filesystem::path relative;
        if (!PresetUtils::TryNormalizePresetRelativePath(name ? name : "", relative, error)) {
            path.clear();
            return false;
        }
        path = (presetsDir / relative).lexically_normal();
        return true;
    }

    inline void SortPresetTree(PresetUtils::PresetTreeNode& node) {
        auto compareText = [](const std::string& lhs, const std::string& rhs) {
            std::string lhsKey = lhs;
            std::string rhsKey = rhs;
            for (char& c : lhsKey)
                c = LOWER_TABLE[static_cast<unsigned char>(c)];
            for (char& c : rhsKey)
                c = LOWER_TABLE[static_cast<unsigned char>(c)];
            return lhsKey == rhsKey ? lhs < rhs : lhsKey < rhsKey;
        };

        std::sort(node.children.begin(), node.children.end(), [&](const auto& lhs, const auto& rhs) {
            return compareText(lhs.name, rhs.name);
        });
        std::sort(node.presets.begin(), node.presets.end(), [](const auto& lhs, const auto& rhs) {
            if (PresetPathsEqual(lhs.path, rhs.path))
                return PresetUtils::PathToUtf8(lhs.path) < PresetUtils::PathToUtf8(rhs.path);
            return PresetUtils::PathLess(lhs.path, rhs.path);
        });
        for (auto& child : node.children)
            SortPresetTree(child);
    }

    [[nodiscard]] inline const PresetListEntry* FindPresetTreeEntryByPath(
        const PresetUtils::PresetTreeNode& node, const std::filesystem::path& path
    ) {
        for (const auto& preset : node.presets)
            if (PresetPathsEqual(preset.path, path)) return &preset;
        for (const auto& child : node.children) {
            if (const auto* found = FindPresetTreeEntryByPath(child, path)) return found;
        }
        return nullptr;
    }

    struct PresetTreeAction {
        enum class Type : uint8_t { None, Load, Delete };
        Type type = Type::None;
        std::filesystem::path path;
        ImVec2 popupAnchor{};
    };

    [[nodiscard]] inline PresetTreeAction RenderPresetRow(
        const PresetListEntry& preset, const std::string& displayLabel, const char* loadLabel
    ) {
        PresetTreeAction action;
        const auto& style = ImGui::GetStyle();
        const float loadWidth = ImGui::CalcTextSize(loadLabel).x + style.FramePadding.x * 2.0f;
        const float deleteWidth = ImGui::CalcTextSize("Delete").x + style.FramePadding.x * 2.0f;
        const float buttonsWidth = loadWidth + deleteWidth + style.ItemSpacing.x * 2.0f;
        const float labelWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x - buttonsWidth);
        ImGui::PushID(&preset);
        const std::string effectiveLabel = preset.valid ? displayLabel : "[Unavailable] " + displayLabel;
        if (!preset.valid) ImGui::BeginDisabled();
        const bool rowClicked = ImGui::Selectable(
            effectiveLabel.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(labelWidth, 0.0f)
        );
        if (!preset.valid) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (!preset.error.empty()) ImGui::SetItemTooltip("%s", preset.error.c_str());
        }
        if (preset.valid && rowClicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            action = {PresetTreeAction::Type::Load, preset.path};

        ImGui::SameLine();
        if (!preset.valid) ImGui::BeginDisabled();
        if (ImGui::Button(loadLabel)) action = {PresetTreeAction::Type::Load, preset.path};
        if (!preset.valid) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            action = {PresetTreeAction::Type::Delete, preset.path, ImVec2((min.x + max.x) * 0.5f, min.y)};
        }
        ImGui::PopID();
        return action;
    }

    [[nodiscard]] inline bool PresetEntryMatchesFilter(
        const PresetListEntry& preset, const std::filesystem::path& presetsDir, const char* filter, size_t filterLen
    ) {
        const auto displayPath = PresetDisplayPath(preset.path, presetsDir);
        return MatchesFilter(preset.name.c_str(), preset.name.size(), filter, filterLen) ||
               MatchesFilter(displayPath.c_str(), displayPath.size(), filter, filterLen);
    }

    [[nodiscard]] inline bool PresetTreeHasMatches(
        const PresetUtils::PresetTreeNode& node, const std::filesystem::path& presetsDir, const char* filter,
        size_t filterLen
    ) {
        for (const auto& preset : node.presets)
            if (PresetEntryMatchesFilter(preset, presetsDir, filter, filterLen)) return true;
        for (const auto& child : node.children)
            if (PresetTreeHasMatches(child, presetsDir, filter, filterLen)) return true;
        return false;
    }

    [[nodiscard]] inline PresetTreeAction RenderPresetTree(
        const PresetUtils::PresetTreeNode& node, const std::filesystem::path& presetsDir, const char* loadLabel
    ) {
        PresetTreeAction action;
        for (const auto& child : node.children) {
            if (ImGui::TreeNode(child.name.c_str())) {
                auto childAction = RenderPresetTree(child, presetsDir, loadLabel);
                if (childAction.type != PresetTreeAction::Type::None) action = std::move(childAction);
                ImGui::TreePop();
            }
        }

        for (const auto& preset : node.presets) {
            auto rowAction = RenderPresetRow(preset, preset.name, loadLabel);
            if (rowAction.type != PresetTreeAction::Type::None) action = std::move(rowAction);
        }
        return action;
    }

    [[nodiscard]] inline PresetTreeAction RenderFilteredPresetTree(
        const PresetUtils::PresetTreeNode& node, const std::filesystem::path& presetsDir, const char* filter,
        size_t filterLen, const char* loadLabel
    ) {
        PresetTreeAction action;
        for (const auto& preset : node.presets) {
            if (!PresetEntryMatchesFilter(preset, presetsDir, filter, filterLen)) continue;
            auto rowAction = RenderPresetRow(preset, PresetDisplayPath(preset.path, presetsDir), loadLabel);
            if (rowAction.type != PresetTreeAction::Type::None) action = std::move(rowAction);
        }
        for (const auto& child : node.children) {
            auto childAction = RenderFilteredPresetTree(child, presetsDir, filter, filterLen, loadLabel);
            if (childAction.type != PresetTreeAction::Type::None) action = std::move(childAction);
        }
        return action;
    }

    struct PresetPanelState {
        char* nameBuf;
        size_t nameBufSize;
        char* searchBuf;
        size_t searchBufSize;
        bool& listDirty;
        PresetUtils::PresetTreeNode& tree;
        StatusMessage& status;
        std::filesystem::path& pendingDeletePath;
        ImVec2& pendingDeletePopupAnchor;
        std::filesystem::path& editingPath;
        std::string& pendingOverwriteName;
        bool canSave = true;
        bool canInteract = true;
        const char* loadLabel = "Load";
    };

    template <typename RefreshFn, typename SaveFn, typename LoadFn, typename DeleteFn>
    inline void RenderPresetPanel(
        PresetPanelState& state, const std::filesystem::path& presetsDir, RefreshFn&& refreshTree, SaveFn&& onSave,
        LoadFn&& onLoad, DeleteFn&& onDelete
    ) {
        ImGui::SeparatorText("Preset");

        // Refresh only on catalog changes/user request. The resulting tree is
        // also the render-thread cache used for overwrite labels, avoiding a
        // filesystem stat on every frame.
        if (state.listDirty) refreshTree();

        const bool hasDraft = state.nameBuf[0] != '\0' || !state.editingPath.empty();
        if (!hasDraft || !state.canInteract) ImGui::BeginDisabled();
        if (ImGui::Button("New Preset")) {
            state.nameBuf[0] = '\0';
            state.editingPath.clear();
            state.status.Clear();
        }
        if (!hasDraft || !state.canInteract) ImGui::EndDisabled();
        ImGui::SameLine();
        if (state.editingPath.empty()) {
            ImGui::TextDisabled("New preset");
        } else {
            const auto displayPath = PresetDisplayPath(state.editingPath, presetsDir, true);
            ImGui::TextDisabled("Current:");
            ImGui::SameLine();
            ImGui::TextWrapped("%s", displayPath.c_str());
        }

        std::filesystem::path targetPath;
        std::string targetValidationError;
        const bool targetValid = TryPresetPathFromName(presetsDir, state.nameBuf, targetPath, targetValidationError);
        const bool updating =
            targetValid && !state.editingPath.empty() && PresetPathsEqual(targetPath, state.editingPath);
        const auto* targetEntry = targetValid ? FindPresetTreeEntryByPath(state.tree, targetPath) : nullptr;
        const bool targetExists = targetEntry != nullptr;
        const bool targetCanBeOverwritten = !targetEntry || targetEntry->valid;
        const bool overwriting = targetExists && !updating;
        const char* saveLabel =
            updating ? "Update" : (overwriting ? "Overwrite" : (state.editingPath.empty() ? "Save" : "Save As"));
        const auto& style = ImGui::GetStyle();
        const float saveWidth = ImGui::CalcTextSize(saveLabel).x + style.FramePadding.x * 2.0f;
        const float inputWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x - saveWidth - style.ItemSpacing.x);
        SetNextInputWidth(inputWidth);
        if (!state.canInteract) ImGui::BeginDisabled();
        const bool saveFromEnter = ImGui::InputTextWithHint(
            "##PresetName", "name or folder/name...", state.nameBuf, state.nameBufSize,
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
        );
        if (!state.canInteract) ImGui::EndDisabled();
        ImGui::SameLine();
        const bool canSave =
            state.nameBuf[0] != '\0' && targetValid && targetCanBeOverwritten && state.canSave && state.canInteract;
        if (!canSave) ImGui::BeginDisabled();
        const bool saveFromButton = ImGui::Button(saveLabel);
        if (!canSave) ImGui::EndDisabled();
        if (canSave && (saveFromEnter || saveFromButton)) {
            if (updating) {
                onSave(state.nameBuf, true);
            } else {
                std::error_code targetFilesystemError;
                const bool existsNow =
                    overwriting || std::filesystem::is_regular_file(targetPath, targetFilesystemError);
                if (targetFilesystemError) {
                    state.status.SetError("Couldn't check that preset name");
                } else if (existsNow) {
                    if (!targetExists) refreshTree();
                    const auto* currentTarget = FindPresetTreeEntryByPath(state.tree, targetPath);
                    if (!currentTarget || !currentTarget->valid) {
                        state.status.SetError("This preset is damaged. Delete it before reusing this name.");
                    } else {
                        state.pendingOverwriteName = state.nameBuf;
                        ImGui::OpenPopup("##overwrite_preset_confirm");
                    }
                } else {
                    onSave(state.nameBuf, false);
                }
            }
        }
        if (state.nameBuf[0] != '\0' && !targetValid) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextWrapped("%s", targetValidationError.c_str());
            ImGui::PopStyleColor();
        }
        if (targetValid && !targetCanBeOverwritten) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextWrapped("This preset is damaged. Delete it before reusing this name.");
            ImGui::PopStyleColor();
        }

        if (ImGui::BeginPopupModal(
                "##overwrite_preset_confirm", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar
            )) {
            std::filesystem::path pendingTargetPath;
            std::string pendingTargetError;
            const bool pendingTargetValid = TryPresetPathFromName(
                presetsDir, state.pendingOverwriteName.c_str(), pendingTargetPath, pendingTargetError
            );
            const auto* pendingTarget =
                pendingTargetValid ? FindPresetTreeEntryByPath(state.tree, pendingTargetPath) : nullptr;
            ImGui::TextUnformatted("A preset already exists at:");
            ImGui::TextWrapped(
                "%s", pendingTargetValid ? PresetDisplayPath(pendingTargetPath, presetsDir, true).c_str()
                                         : pendingTargetError.c_str()
            );
            ImGui::Spacing();
            ImGui::TextUnformatted("Replace it with the current values?");
            const bool canConfirmOverwrite = pendingTargetValid && pendingTarget && pendingTarget->valid;
            if (!canConfirmOverwrite) ImGui::BeginDisabled();
            if (ImGui::Button("Overwrite")) {
                onSave(state.pendingOverwriteName.c_str(), true);
                state.pendingOverwriteName.clear();
                ImGui::CloseCurrentPopup();
            }
            if (!canConfirmOverwrite) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                state.pendingOverwriteName.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (!state.pendingOverwriteName.empty() && !ImGui::IsPopupOpen("##overwrite_preset_confirm")) {
            state.pendingOverwriteName.clear();
        }

        ImGui::SeparatorText("Saved Presets");

        const float refreshWidth = ImGui::CalcTextSize("Refresh List").x + style.FramePadding.x * 2.0f;
        const float searchWidth =
            (std::max)(1.0f, ImGui::GetContentRegionAvail().x - refreshWidth - style.ItemSpacing.x);
        SetNextInputWidth(searchWidth);
        ImGui::InputTextWithHint("##PresetSearch", "Search presets...", state.searchBuf, state.searchBufSize);
        ImGui::SameLine();
        if (ImGui::Button("Refresh List")) refreshTree();

        if (state.tree.presets.empty() && state.tree.children.empty()) {
            ImGui::TextDisabled("No saved presets");
        } else {
            const size_t filterLen = std::strlen(state.searchBuf);
            const bool hasMatches =
                filterLen == 0 || PresetTreeHasMatches(state.tree, presetsDir, state.searchBuf, filterLen);
            PresetTreeAction action;
            if (!state.canInteract) ImGui::BeginDisabled();
            if (!hasMatches) {
                ImGui::TextDisabled("No matching presets");
            } else {
                ImGui::BeginChild(
                    "##presetList", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 8), ImGuiChildFlags_Borders
                );
                action =
                    filterLen == 0
                        ? RenderPresetTree(state.tree, presetsDir, state.loadLabel)
                        : RenderFilteredPresetTree(state.tree, presetsDir, state.searchBuf, filterLen, state.loadLabel);
                ImGui::EndChild();
            }
            if (!state.canInteract) ImGui::EndDisabled();

            if (action.type == PresetTreeAction::Type::Load)
                onLoad(action.path);
            else if (action.type == PresetTreeAction::Type::Delete) {
                state.pendingDeletePath = action.path;
                state.pendingDeletePopupAnchor = action.popupAnchor;
                ImGui::OpenPopup("##delete_preset_confirm");
            }
        }

        ImVec2 popupAnchor = state.pendingDeletePopupAnchor;
        popupAnchor.y -= ImGui::GetStyle().ItemSpacing.y;
        ImGui::SetNextWindowPos(popupAnchor, ImGuiCond_Always, ImVec2(0.5f, 1.0f));
        const float viewportWidth = ImGui::GetMainViewport()->WorkSize.x;
        const float popupMaxWidth = (std::max)(260.0f, (std::min)(560.0f, viewportWidth - 32.0f));
        ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 0.0f), ImVec2(popupMaxWidth, FLT_MAX));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, K_POPUP_PADDING);
        if (ImGui::BeginPopup(
                "##delete_preset_confirm", ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar |
                                               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove
            )) {
            const auto relativePath = state.pendingDeletePath.lexically_relative(presetsDir);
            const auto displayPath =
                PresetUtils::PathToUtf8(relativePath.empty() ? state.pendingDeletePath.filename() : relativePath);

            ImGui::TextUnformatted("Delete preset?");
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + popupMaxWidth - K_POPUP_PADDING.x * 2.0f);
            ImGui::TextWrapped("%s", displayPath.c_str());
            ImGui::PopTextWrapPos();

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
        if (!state.pendingDeletePath.empty() && !ImGui::IsPopupOpen("##delete_preset_confirm")) {
            state.pendingDeletePath.clear();
        }

        ImGui::Spacing();
        if (Button("Open Presets Folder")) (void)PresetUtils::OpenInExplorer(presetsDir);
    }

    inline void RenderOverrideCount(int count) {
        if (count > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%d active)", count);
        }
    }

    inline void RenderPreviewControls(PreviewConfig& preview, const char* itemType = "preview") {
        char label[64];
        std::snprintf(label, sizeof(label), "See changes on a nearby %s immediately", itemType);
        (void)CheckboxWithTooltip("Preview Changes", &preview.livePreview, label);
        if (preview.livePreview) {
            ImGui::SameLine();
            std::snprintf(label, sizeof(label), "Keep the %s turning for easier viewing", itemType);
            (void)CheckboxWithTooltip("Rotate Preview", &preview.autoRotate, label);
            if (preview.autoRotate) {
                ImGui::SetNextItemWidth(K_DRAG_WIDTH);
                DebouncedDragFloat("Preview Speed", &preview.rotationSpeed, 1.0f, -360.0f, 360.0f, "%.0f deg/s");
            }
        }
    }

    /// Caller must call ImGui::EndChild() after content.
    inline void BeginScrollWithFooter(const char* id, int footerRows = 1) {
        const auto& style = ImGui::GetStyle();
        const float rowCount = static_cast<float>(footerRows);
        const float footerHeight = ImGui::GetFrameHeight() * rowCount + style.ItemSpacing.y * (rowCount + 1.0f);
        const float scrollHeight = (std::max)(1.0f, ImGui::GetContentRegionAvail().y - footerHeight);
        ImGui::BeginChild(id, ImVec2(0.0f, scrollHeight));
    }

    template <typename RenderFn> inline void RenderClippedList(int itemCount, int includeIndex, RenderFn&& renderItem) {
        ImGuiListClipper clipper;
        clipper.Begin(itemCount);
        if (includeIndex >= 0 && includeIndex < itemCount) clipper.IncludeItemByIndex(includeIndex);
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                renderItem(i);
            }
        }
    }

    template <typename Entry>
    inline void RenderGlobalModuleCombo(
        const char* label, SDK::UClass*& current, const std::vector<Entry>& options, char* filterBuf,
        float& cachedWidth, bool allowNone = true, std::string* currentPath = nullptr
    ) {
        const char* preview = "None";
        bool foundPath = false;
        int selectedIndex = -1;
        for (int i = 0; i < static_cast<int>(options.size()); ++i) {
            const auto& e = options[static_cast<size_t>(i)];
            if ((currentPath && e.path == *currentPath) || (!currentPath && e.cls == current)) {
                preview = e.name.c_str();
                selectedIndex = i;
                if (currentPath) {
                    current = e.cls;
                    foundPath = true;
                }
                break;
            }
        }
        if (currentPath && !foundPath) current = nullptr;

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

        if (!BeginSizedCombo(label, preview, cachedWidth)) return;

        SetComboSearchWidth(cachedWidth);
        ImGui::InputTextWithHint("##filter", "Search parts...", filterBuf, 64);

        const size_t filterLen = std::strlen(filterBuf);
        const bool hasFilter = filterLen > 0;

        if (hasFilter) {
            int visible = 0;
            for (const auto& e : options)
                if (MatchesFilter(e.name.c_str(), e.name.size(), filterBuf, filterLen)) ++visible;
            ImGui::TextDisabled("Showing %d of %d", visible, static_cast<int>(options.size()));
        }

        ImGui::Separator();

        if (allowNone && ImGui::Selectable("None", currentPath ? currentPath->empty() : current == nullptr)) {
            if (currentPath) currentPath->clear();
            current = nullptr;
        }

        auto renderEntry = [&](int idx) {
            const auto& e = options[static_cast<size_t>(idx)];
            if (hasFilter && !MatchesFilter(e.name.c_str(), e.name.size(), filterBuf, filterLen)) return;
            char display[128];
            std::snprintf(display, sizeof(display), "%-36s [%s]", e.name.c_str(), e.sourceType);
            const bool selected = currentPath ? e.path == *currentPath : e.cls == current;
            if (ImGui::Selectable(display, selected)) {
                if (currentPath) *currentPath = e.path;
                current = e.cls;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        };

        if (hasFilter) {
            for (int i = 0; i < static_cast<int>(options.size()); ++i) {
                renderEntry(i);
            }
        } else {
            RenderClippedList(static_cast<int>(options.size()), selectedIndex, renderEntry);
        }
        ImGui::EndCombo();
    }

    template <typename Entry>
    inline void RenderModuleIndexCombo(
        const char* label, int32_t& moduleIndex, const std::vector<Entry>& available, char* filterBuf,
        float& cachedWidth
    ) {
        if (available.empty()) {
            ImGui::TextDisabled("No %s options available", label);
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

        if (!BeginSizedCombo(label, preview, cachedWidth)) return;

        SetComboSearchWidth(cachedWidth);
        ImGui::InputTextWithHint("##filter", "Search parts...", filterBuf, 64);

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

        auto renderEntry = [&](int i) {
            const auto& entry = available[static_cast<size_t>(i)];
            if (hasFilter && !MatchesFilter(entry.name.c_str(), entry.name.size(), filterBuf, filterLen)) return;
            bool selected = (moduleIndex == i + 1);
            if (ImGui::Selectable(entry.name.c_str(), selected)) moduleIndex = i + 1;
            if (selected) ImGui::SetItemDefaultFocus();
        };

        if (hasFilter) {
            for (int i = 0; i < static_cast<int>(available.size()); ++i) {
                renderEntry(i);
            }
        } else {
            RenderClippedList(static_cast<int>(available.size()), moduleIndex - 1, renderEntry);
        }
        ImGui::EndCombo();
    }

}
