#pragma once

#include <cstring>
#include <filesystem>
#include <string>

#include "imgui/imgui.h"
#include "Utils/PresetUtils.h"
#include "Utils/GuiUtils.h"

template <typename Serializer> struct PresetSectionState {
    char presetNameBuf[128] = {};
    PresetUtils::PresetTreeNode presetTree;
    bool presetListDirty = true;
    GuiUtils::StatusMessage status;

    void RefreshPresetTree() {
        presetTree = Serializer::ListPresetsTree();
        presetListDirty = false;
    }

    template <typename BuildFn, typename ApplyFn>
    void RenderPresetsTab(BuildFn&& buildData, ApplyFn&& applyData, bool canSave = true) {
        ImGui::PushID("presets");
        GuiUtils::PresetPanelState panelState{presetNameBuf, sizeof(presetNameBuf), presetListDirty, presetTree, status,
                                              canSave};
        GuiUtils::RenderPresetPanel(
            panelState, Serializer::GetPresetsDirectory(), [this]() { RefreshPresetTree(); },
            [this, &buildData](const char* name) {
                auto data = buildData();
                data.name = name;
                if (Serializer::SavePresetByName(name, data)) {
                    status.Set("Saved: " + std::string(name));
                    presetListDirty = true;
                } else {
                    status.Set("Error saving preset", true);
                }
            },
            [this, &applyData](const std::filesystem::path& path) {
                auto result = Serializer::LoadFromFile(path);
                if (result.success) {
                    strncpy_s(presetNameBuf, result.name.c_str(), _TRUNCATE);
                    std::string loadedName = std::move(result.name);
                    applyData(std::move(result));
                    status.Set("Loaded: " + loadedName);
                } else {
                    status.Set("Error: " + result.error, true);
                }
            },
            [this](const std::filesystem::path& path) {
                Serializer::DeletePreset(path);
                PresetUtils::CleanEmptyDirectories(Serializer::GetPresetsDirectory());
                presetListDirty = true;
            }
        );
        ImGui::PopID();
    }
};
