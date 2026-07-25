#pragma once

#include <cstring>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <variant>

#include "imgui/imgui.h"
#include "Utils/PresetUtils.h"
#include "Utils/GuiUtils.h"

enum class PresetApplyDisposition {
    Applied,
    Rejected,
    Pending,
};

template <typename T> class PresetBuildResult {
    struct FailureState {
        std::string error;
    };
    struct PendingState {};

    std::variant<T, FailureState, PendingState> state;

    explicit PresetBuildResult(T data) : state(std::move(data)) {}
    explicit PresetBuildResult(FailureState failure) : state(std::move(failure)) {}
    explicit PresetBuildResult(PendingState pending) : state(pending) {}

public:
    static PresetBuildResult Success(T data) { return PresetBuildResult(std::move(data)); }
    static PresetBuildResult Failure(std::string message) {
        return PresetBuildResult(FailureState{std::move(message)});
    }
    static PresetBuildResult Pending() { return PresetBuildResult(PendingState{}); }

    [[nodiscard]] bool IsPending() const noexcept { return std::holds_alternative<PendingState>(state); }
    [[nodiscard]] T* GetValue() noexcept { return std::get_if<T>(&state); }
    [[nodiscard]] std::string TakeError() {
        return std::move(std::get<FailureState>(state).error);
    }
};

template <typename Serializer> struct PresetSectionState {
    char presetNameBuf[512] = {};
    char presetSearchBuf[256] = {};
    PresetUtils::PresetTreeNode presetTree;
    std::filesystem::path pendingDeletePath;
    ImVec2 pendingDeletePopupAnchor{};
    std::filesystem::path editingPath;
    std::string pendingOverwriteName;
    std::filesystem::path pendingApplyPath;
    std::string pendingApplyEditingName;
    GuiUtils::StatusMessage::Token pendingProgressToken = 0;
    bool applyPending = false;
    bool savePending = false;
    bool presetListDirty = true;
    std::uint64_t catalogRevision = 0;
    GuiUtils::StatusMessage status;

    [[nodiscard]] bool IsApplyPending() const { return applyPending; }

    void ClearEditing() {
        editingPath.clear();
        presetNameBuf[0] = '\0';
    }

    void CompletePendingSave(PresetOperationResult result) {
        if (!savePending) return;

        if (result.success) {
            editingPath = std::move(result.path);
            presetListDirty = true;
            status.ClearText(pendingProgressToken);
        } else {
            status.SetError("Couldn't save preset: " + result.error);
        }
        pendingProgressToken = 0;
        savePending = false;
    }

    void CompletePendingApply(bool success, std::string error = {}) {
        if (!applyPending) return;

        if (success) {
            editingPath = std::move(pendingApplyPath);
            strncpy_s(presetNameBuf, pendingApplyEditingName.c_str(), _TRUNCATE);
            status.ClearText(pendingProgressToken);
        } else {
            status.SetError(error.empty() ? "Couldn't use this preset" : std::move(error));
        }

        pendingApplyPath.clear();
        pendingApplyEditingName.clear();
        pendingProgressToken = 0;
        applyPending = false;
    }

    void RefreshPresetTree() {
        if (!presetListDirty) Serializer::InvalidateCatalog();
        const std::uint64_t revisionBefore = Serializer::GetCatalogRevision();
        presetTree = Serializer::ListPresetsTree();
        GuiUtils::SortPresetTree(presetTree);
        const std::uint64_t revisionAfter = Serializer::GetCatalogRevision();
        presetListDirty = revisionBefore != revisionAfter;
        catalogRevision = revisionAfter;
    }

    template <typename BuildFn, typename ApplyFn>
    void RenderPresetsTab(
        BuildFn&& buildData, ApplyFn&& applyData, bool canSave = true, const char* loadLabel = "Load"
    ) {
        if (catalogRevision != Serializer::GetCatalogRevision()) presetListDirty = true;
        ImGui::PushID("presets");
        GuiUtils::PresetPanelState panelState{
            presetNameBuf,
            sizeof(presetNameBuf),
            presetSearchBuf,
            sizeof(presetSearchBuf),
            presetListDirty,
            presetTree,
            status,
            pendingDeletePath,
            pendingDeletePopupAnchor,
            editingPath,
            pendingOverwriteName,
            canSave && !applyPending && !savePending,
            !applyPending && !savePending,
            loadLabel
        };
        GuiUtils::RenderPresetPanel(
            panelState, Serializer::GetPresetsDirectory(), [this]() { RefreshPresetTree(); },
            [this, &buildData](const char* name, bool overwrite) {
                using Data = typename Serializer::Data;
                PresetBuildResult<Data> built = buildData(name, overwrite);
                if (built.IsPending()) {
                    savePending = true;
                    pendingProgressToken = status.SetInfo("Saving " + std::string(name) + "...");
                    return;
                }
                auto* data = built.GetValue();
                if (!data) {
                    auto error = built.TakeError();
                    status.SetError(error.empty() ? std::string("Nothing is ready to save") : std::move(error));
                    return;
                }
                data->name = name;
                data->id.clear();
                auto save = Serializer::SavePresetByNameResult(name, *data, overwrite);
                if (save.success) {
                    editingPath = save.path;
                    presetListDirty = true;
                    status.Clear();
                } else {
                    status.SetError("Couldn't save preset: " + save.error);
                }
            },
            [this, &applyData](const std::filesystem::path& path) {
                auto load = Serializer::LoadFromFileResult(path);
                if (load.success) {
                    status.Clear();
                    auto result = std::move(load.value);
                    const auto disposition = applyData(std::move(result));
                    if (disposition == PresetApplyDisposition::Rejected) return;
                    auto editingName = GuiUtils::PresetDisplayPath(path, Serializer::GetPresetsDirectory());
                    if (disposition == PresetApplyDisposition::Pending) {
                        pendingApplyPath = path;
                        pendingApplyEditingName = std::move(editingName);
                        applyPending = true;
                        pendingProgressToken = status.SetInfo("Loading " + pendingApplyEditingName + "...");
                        return;
                    }

                    editingPath = path;
                    strncpy_s(presetNameBuf, editingName.c_str(), _TRUNCATE);
                } else {
                    status.SetError("Couldn't open preset: " + load.error);
                }
            },
            [this](const std::filesystem::path& path) {
                auto deletion = Serializer::DeletePresetResult(path);
                if (deletion.success) {
                    presetListDirty = true;
                    status.Clear();
                    if (!editingPath.empty() && GuiUtils::PresetPathsEqual(editingPath, path)) {
                        ClearEditing();
                    }
                } else {
                    status.SetError("Couldn't delete preset: " + deletion.error);
                }
            }
        );
        ImGui::PopID();
    }
};
