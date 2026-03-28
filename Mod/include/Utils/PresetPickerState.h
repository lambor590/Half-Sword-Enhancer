#pragma once

#include <vector>
#include <filesystem>

#include "imgui/imgui.h"
#include "Utils/PresetUtils.h"

template <typename Serializer> struct PresetPickerState {
    std::vector<PresetListEntry> entries;
    int selectedIndex = -1;
    bool dirty = true;

    void Refresh() {
        entries.clear();
        Flatten(Serializer::ListPresetsTree());
        dirty = false;
        if (selectedIndex >= static_cast<int>(entries.size())) selectedIndex = -1;
    }

    bool Render(const char* label, const char* noneLabel = "None") {
        if (dirty) Refresh();
        const char* preview = selectedIndex < 0 ? noneLabel : entries[selectedIndex].name.c_str();
        bool changed = false;
        if (ImGui::BeginCombo(label, preview)) {
            if (ImGui::Selectable(noneLabel, selectedIndex < 0)) {
                selectedIndex = -1;
                changed = true;
            }
            for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                if (ImGui::Selectable(entries[i].name.c_str(), i == selectedIndex)) {
                    selectedIndex = i;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    [[nodiscard]] bool HasSelection() const noexcept {
        return selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size());
    }

    [[nodiscard]] const std::filesystem::path& SelectedPath() const { return entries[selectedIndex].path; }

    void Invalidate() noexcept { dirty = true; }

private:
    void Flatten(const PresetUtils::PresetTreeNode& node) {
        for (const auto& p : node.presets)
            entries.push_back(p);
        for (const auto& child : node.children)
            Flatten(child);
    }
};
