#pragma once

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "imgui/imgui.h"
#include "Utils/GuiUtils.h"
#include "Utils/PresetUtils.h"

template <typename Serializer> struct PresetPickerState {
    std::vector<PresetListEntry> entries;
    std::vector<std::string> displayLabels;
    int selectedIndex = -1;
    bool dirty = true;
    float comboWidth = 0.0f;
    std::filesystem::path selectedPath;
    char filterBuf[128] = {};
    std::uint64_t catalogRevision = 0;

    void Refresh(const char* noneLabel = "None") {
        const std::uint64_t revisionBefore = Serializer::GetCatalogRevision();
        std::filesystem::path previousPath = selectedPath;
        if (previousPath.empty() && selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size()))
            previousPath = entries[static_cast<size_t>(selectedIndex)].path;

        entries.clear();
        PresetUtils::FlattenPresetTree(Serializer::ListPresetsTree(), entries);
        const auto& presetsDir = Serializer::GetPresetsDirectory();
        std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
            return PresetUtils::PathLess(lhs.path, rhs.path);
        });

        displayLabels.clear();
        displayLabels.reserve(entries.size());
        float maxW = ImGui::CalcTextSize(noneLabel).x;
        for (const auto& entry : entries) {
            auto label = GuiUtils::PresetDisplayPath(entry.path, presetsDir);
            if (!entry.valid) label = "[Unavailable] " + label;
            auto& storedLabel = displayLabels.emplace_back(std::move(label));
            const float width = ImGui::CalcTextSize(storedLabel.c_str()).x;
            if (width > maxW) maxW = width;
        }
        comboWidth = GuiUtils::ComboWidthFromText(maxW);

        selectedIndex = -1;
        selectedPath.clear();
        if (!previousPath.empty()) {
            for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                if (!entries[static_cast<size_t>(i)].valid) continue;
                if (!GuiUtils::PresetPathsEqual(entries[static_cast<size_t>(i)].path, previousPath)) continue;
                selectedIndex = i;
                selectedPath = entries[static_cast<size_t>(i)].path;
                break;
            }
        }
        const std::uint64_t revisionAfter = Serializer::GetCatalogRevision();
        dirty = revisionBefore != revisionAfter;
        catalogRevision = revisionAfter;
    }

    bool Render(const char* label, const char* noneLabel = "None") {
        const auto selectionBeforeRefresh = selectedPath;
        if (dirty || catalogRevision != Serializer::GetCatalogRevision()) Refresh(noneLabel);
        bool changed = !selectionBeforeRefresh.empty() && selectedPath.empty();
        const char* preview = HasSelection() ? displayLabels[static_cast<size_t>(selectedIndex)].c_str() : noneLabel;

        ImGui::PushID(this);
        const bool comboOpen = GuiUtils::BeginSizedCombo(label, preview, comboWidth);
        if (comboOpen) {
            GuiUtils::SetComboSearchWidth(comboWidth);
            ImGui::InputTextWithHint("##PresetFilter", "Search presets...", filterBuf, sizeof(filterBuf));
            const size_t filterLen = std::strlen(filterBuf);
            if (filterLen > 0) {
                int visibleCount = 0;
                for (size_t i = 0; i < entries.size(); ++i)
                    if (MatchesFilter(i, filterLen)) ++visibleCount;
                ImGui::TextDisabled("Showing %d of %d", visibleCount, static_cast<int>(entries.size()));
            }
            ImGui::Separator();

            if (ImGui::Selectable(noneLabel, !HasSelection())) {
                selectedIndex = -1;
                selectedPath.clear();
                changed = true;
            }
            for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                const auto index = static_cast<size_t>(i);
                if (!MatchesFilter(index, filterLen)) continue;

                ImGui::PushID(i);
                const bool selected = i == selectedIndex;
                if (!entries[index].valid) ImGui::BeginDisabled();
                if (ImGui::Selectable(displayLabels[index].c_str(), selected)) {
                    selectedIndex = i;
                    selectedPath = entries[index].path;
                    changed = true;
                }
                if (!entries[index].valid) ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled)) {
                    if (!entries[index].error.empty()) ImGui::SetItemTooltip("%s", entries[index].error.c_str());
                }
                if (selected) ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
            if (entries.empty()) ImGui::TextDisabled("No saved presets");
            ImGui::EndCombo();
        }

        const bool canClear = HasSelection();
        if (!canClear) ImGui::BeginDisabled();
        if (ImGui::Button("Clear Selection")) {
            selectedIndex = -1;
            selectedPath.clear();
            changed = true;
        }
        if (!canClear) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) ImGui::SetItemTooltip("Clear preset selection");

        (void)GuiUtils::SameLineIfFitsButton("Refresh List");
        if (ImGui::Button("Refresh List")) {
            const auto selectionBefore = selectedPath;
            Serializer::InvalidateCatalog();
            Refresh(noneLabel);
            if (!GuiUtils::PresetPathsEqual(selectionBefore, selectedPath)) changed = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) ImGui::SetItemTooltip("Find saved presets again");
        ImGui::PopID();
        return changed;
    }

    [[nodiscard]] bool HasSelection() const noexcept {
        return selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size()) && !selectedPath.empty() &&
               entries[static_cast<size_t>(selectedIndex)].valid &&
               entries[static_cast<size_t>(selectedIndex)].path == selectedPath;
    }

    [[nodiscard]] const std::filesystem::path& SelectedPath() const { return selectedPath; }
    [[nodiscard]] const PresetListEntry* SelectedEntry() const noexcept {
        return HasSelection() ? &entries[static_cast<size_t>(selectedIndex)] : nullptr;
    }

    void Invalidate() noexcept { dirty = true; }

private:
    [[nodiscard]] bool MatchesFilter(size_t index, size_t filterLen) const {
        if (filterLen == 0) return true;
        const auto& display = displayLabels[index];
        const auto& name = entries[index].name;
        return GuiUtils::MatchesFilter(display.c_str(), display.size(), filterBuf, filterLen) ||
               GuiUtils::MatchesFilter(name.c_str(), name.size(), filterBuf, filterLen);
    }
};
