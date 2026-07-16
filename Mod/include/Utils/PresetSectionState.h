#pragma once

#include <cstring>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
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
    [[nodiscard]] const std::string& GetError() const noexcept {
        static const std::string empty;
        const auto* failure = std::get_if<FailureState>(&state);
        return failure ? failure->error : empty;
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
    std::string pendingApplyLoadedName;
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
            status.Set("Saved: " + GuiUtils::PresetDisplayPath(editingPath, Serializer::GetPresetsDirectory()));
            presetListDirty = true;
        } else {
            status.Set("Couldn't save preset: " + result.error, true);
        }
        savePending = false;
    }

    void CompletePendingApply(bool success, std::string error = {}) {
        if (!applyPending) return;

        if (success) {
            editingPath = std::move(pendingApplyPath);
            strncpy_s(presetNameBuf, pendingApplyEditingName.c_str(), _TRUNCATE);
            status.Set("Loaded: " + pendingApplyLoadedName);
        } else {
            status.Set(error.empty() ? "Couldn't use this preset" : std::move(error), true);
        }

        pendingApplyPath.clear();
        pendingApplyEditingName.clear();
        pendingApplyLoadedName.clear();
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
                auto built = [&]() {
                    if constexpr (std::is_invocable_v<BuildFn&, const char*, bool>)
                        return std::invoke(buildData, name, overwrite);
                    else if constexpr (std::is_invocable_v<BuildFn&, const char*>)
                        return std::invoke(buildData, name);
                    else
                        return std::invoke(buildData);
                }();
                using BuildResult = std::remove_cvref_t<decltype(built)>;
                Data data;
                if constexpr (std::is_same_v<BuildResult, Data>) {
                    data = std::move(built);
                } else {
                    static_assert(
                        std::is_same_v<BuildResult, PresetBuildResult<Data>>,
                        "Preset build callbacks must return their data type or PresetBuildResult<data type>"
                    );
                    if (built.IsPending()) {
                        savePending = true;
                        status.Set("Saving " + std::string(name) + "...");
                        return;
                    }
                    auto* value = built.GetValue();
                    if (!value) {
                        const auto& error = built.GetError();
                        status.Set(error.empty() ? std::string("Nothing is ready to save") : error, true);
                        return;
                    }
                    data = std::move(*value);
                }
                data.name = name;
                data.id.clear();
                auto save = Serializer::SavePresetByNameResult(name, data, overwrite);
                if (save.success) {
                    editingPath = save.path;
                    status.Set("Saved: " + GuiUtils::PresetDisplayPath(editingPath, Serializer::GetPresetsDirectory()));
                    presetListDirty = true;
                } else {
                    status.Set("Couldn't save preset: " + save.error, true);
                }
            },
            [this, &applyData](const std::filesystem::path& path) {
                auto load = Serializer::LoadFromFileResult(path);
                if (load.success) {
                    auto result = std::move(load.value);
                    auto editingName = GuiUtils::PresetDisplayPath(path, Serializer::GetPresetsDirectory());
                    if (editingName.empty()) editingName = result.name;
                    const std::string loadedName = result.name;
                    const std::string statusBeforeApply = status.text;
                    using ApplyResult = std::invoke_result_t<ApplyFn&, decltype(result)&&>;
                    PresetApplyDisposition disposition = PresetApplyDisposition::Applied;
                    if constexpr (std::is_void_v<ApplyResult>) {
                        std::invoke(applyData, std::move(result));
                    } else {
                        using Result = std::remove_cvref_t<ApplyResult>;
                        static_assert(
                            std::is_same_v<Result, bool> || std::is_same_v<Result, PresetApplyDisposition>,
                            "Preset apply callbacks must return void, bool, or PresetApplyDisposition"
                        );
                        if constexpr (std::is_same_v<Result, bool>) {
                            disposition = std::invoke(applyData, std::move(result)) ? PresetApplyDisposition::Applied
                                                                                    : PresetApplyDisposition::Rejected;
                        } else {
                            disposition = std::invoke(applyData, std::move(result));
                        }
                    }
                    if (disposition == PresetApplyDisposition::Rejected) return;
                    if (disposition == PresetApplyDisposition::Pending) {
                        pendingApplyPath = path;
                        pendingApplyEditingName = std::move(editingName);
                        pendingApplyLoadedName = loadedName;
                        applyPending = true;
                        status.Set("Loading " + loadedName + "...");
                        return;
                    }

                    editingPath = path;
                    strncpy_s(presetNameBuf, editingName.c_str(), _TRUNCATE);
                    if (status.text == statusBeforeApply) status.Set("Loaded: " + loadedName);
                } else {
                    status.Set("Couldn't open preset: " + load.error, true);
                }
            },
            [this](const std::filesystem::path& path) {
                const auto presetName = PresetUtils::PathToUtf8(path.stem());
                auto deletion = Serializer::DeletePresetResult(path);
                if (deletion.success) {
                    PresetUtils::CleanEmptyDirectories(Serializer::GetPresetsDirectory());
                    presetListDirty = true;
                    if (!editingPath.empty() && GuiUtils::PresetPathsEqual(editingPath, path)) {
                        ClearEditing();
                    }
                    status.Set("Deleted: " + presetName);
                } else {
                    status.Set("Couldn't delete preset: " + deletion.error, true);
                }
            }
        );
        ImGui::PopID();
    }
};
