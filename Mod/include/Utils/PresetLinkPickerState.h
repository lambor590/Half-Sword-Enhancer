#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

#include "ConfigManager.h"
#include "Menu/Preset.h"
#include "Utils/PresetLinkState.h"
#include "Utils/PresetPickerState.h"
#include "Utils/GuiUtils.h"

template <typename Serializer> struct PresetLinkPickerState {
    using Data = typename Serializer::Data;
    using Link = PresetLink<Data>;
    using ResolveResult = PresetResolveResult<Data>;
    using State = PresetLinkState<Serializer>;
    enum class CaptureMode : uint8_t { Copy, Reference };

    PresetLinkPickerState() = default;

    explicit PresetLinkPickerState(Link initialLink) { SetLink(std::move(initialLink)); }

    [[nodiscard]] bool Render(const char* label, const char* noneLabel = "None") {
        return Render(label, ConfigManager::GetAppDataPath(), noneLabel);
    }

    [[nodiscard]] bool Render(
        const char* label, const std::filesystem::path& appDataRoot, const char* noneLabel = "None"
    ) {
        RefreshIfCatalogChanged(appDataRoot);

        ImGui::PushID(this);
        if (picker.Render(label, noneLabel)) operationError.clear();

        RenderCurrentLink();

        bool changed = false;
        const bool hasSelection = picker.HasSelection();
        if (!hasSelection) ImGui::BeginDisabled();
        if (GuiUtils::Button("Use a Copy")) {
            const auto result = CaptureSelected(CaptureMode::Copy, appDataRoot);
            changed = result.success;
        }
        (void)GuiUtils::SameLineIfFitsButton("Keep Reference");
        GuiUtils::HelpTooltip("Keep these values even if the saved preset changes");
        if (GuiUtils::Button("Keep Reference")) {
            const auto result = CaptureSelected(CaptureMode::Reference, appDataRoot);
            changed = result.success;
        }
        GuiUtils::HelpTooltip("Use future changes made to the saved preset");
        if (!hasSelection) ImGui::EndDisabled();

        if (HasLink()) {
            if (ImGui::Button("Remove Preset")) {
                Clear();
                changed = true;
            }
            if (state.IsBroken()) {
                (void)GuiUtils::SameLineIfFitsButton("Check Again");
                if (ImGui::Button("Check Again")) {
                    (void)Resolve(appDataRoot);
                    changed = true;
                }
                GuiUtils::HelpTooltip("Look for the saved preset again");
            }
        }

        if (!state.GetDiagnostic().empty()) {
            GuiUtils::RenderCallout("preset-link-diagnostic", state.GetDiagnostic(), GuiUtils::CalloutTone::Error);
        }
        if (!operationError.empty()) {
            GuiUtils::RenderCallout("preset-link-error", operationError, GuiUtils::CalloutTone::Error);
        }

        ImGui::PopID();
        return changed;
    }

    [[nodiscard]] PresetOperationResult CaptureSelected(CaptureMode mode, const std::filesystem::path& appDataRoot) {
        PresetOperationResult result;
        if (!picker.HasSelection()) {
            result.error = "Select a preset first";
            SetOperationError(result.error);
            return result;
        }

        const PresetListEntry& selected = *picker.SelectedEntry();
        Link next;
        if (mode == CaptureMode::Copy) {
            auto loaded = Serializer::LoadFromFileResult(selected.path, appDataRoot);
            if (!loaded.success) {
                result.path = loaded.path;
                result.error = std::move(loaded.error);
                SetOperationError(result.error);
                return result;
            }
            next = MakePresetCopyLink(std::move(loaded.value));
        } else {
            next = MakePresetReferenceLink<Data>(selected.id);
        }

        State candidate;
        auto verification = candidate.AssignAndResolve(std::move(next), appDataRoot);
        result.path = verification.path.empty() ? selected.path : std::move(verification.path);
        if (!verification.success) {
            result.error = PresetLinkResolution::FormatDiagnostic(verification);
            SetOperationError(result.error);
            return result;
        }

        state = std::move(candidate);
        operationError.clear();
        lastPresetCatalogRevision = GetPresetCatalogRevision();
        result.success = true;
        return result;
    }

    void SetLink(Link loadedLink) { SetLink(std::move(loadedLink), ConfigManager::GetAppDataPath()); }

    void SetLink(Link loadedLink, const std::filesystem::path& appDataRoot) {
        operationError.clear();
        (void)state.AssignAndResolve(std::move(loadedLink), appDataRoot);
        lastPresetCatalogRevision = GetPresetCatalogRevision();
        RestorePickerSelection(appDataRoot);
    }

    [[nodiscard]] ResolveResult Resolve() { return Resolve(ConfigManager::GetAppDataPath()); }

    [[nodiscard]] ResolveResult Resolve(const std::filesystem::path& appDataRoot) {
        operationError.clear();
        return state.Resolve(appDataRoot);
    }

    bool RefreshIfCatalogChanged() { return RefreshIfCatalogChanged(ConfigManager::GetAppDataPath()); }

    bool RefreshIfCatalogChanged(const std::filesystem::path& appDataRoot) {
        const uint64_t presetCatalogRevision = GetPresetCatalogRevision();
        if (presetCatalogRevision != lastPresetCatalogRevision) {
            lastPresetCatalogRevision = presetCatalogRevision;
            picker.Invalidate();
            if (HasLink()) (void)Resolve(appDataRoot);
            return true;
        }
        return false;
    }

    void Clear() {
        operationError.clear();
        state.Clear();
    }

    void MarkBroken(std::string detail) { state.MarkBroken(std::move(detail)); }
    void MarkHealthy() { state.MarkHealthy(); }

    [[nodiscard]] bool HasLink() const noexcept { return state.HasLink(); }
    [[nodiscard]] bool IsBroken() const noexcept { return state.IsBroken(); }
    [[nodiscard]] const Link& GetLink() const noexcept { return state.GetLink(); }
    [[nodiscard]] const std::string& GetDiagnostic() const noexcept { return state.GetDiagnostic(); }
    [[nodiscard]] PresetPickerState<Serializer>& GetPicker() noexcept { return picker; }
    [[nodiscard]] const PresetPickerState<Serializer>& GetPicker() const noexcept { return picker; }

private:
    PresetPickerState<Serializer> picker;
    State state;
    uint64_t lastPresetCatalogRevision = 0;
    std::string operationError;

    void SetOperationError(std::string message) { operationError = std::move(message); }

    void RestorePickerSelection(const std::filesystem::path& appDataRoot) {
        picker.selectedIndex = -1;
        picker.selectedPath.clear();
        picker.Invalidate();

        if (const auto* reference = GetPresetReference(state.GetLink())) {
            auto source = Serializer::FindPresetById(reference->id, appDataRoot);
            if (source.success) picker.selectedPath = std::move(source.path);
        }
    }

    void RenderCurrentLink() const {
        ImGui::Spacing();
        ImGui::TextDisabled("Preset");
        ImGui::SameLine();

        if (!HasLink()) {
            ImGui::TextDisabled("[None]");
            return;
        }

        const bool copied = GetPresetCopy(state.GetLink()) != nullptr;
        const ImVec4 badgeColor = state.IsBroken() ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f)
                                  : copied         ? ImVec4(0.45f, 0.8f, 1.0f, 1.0f)
                                                   : ImVec4(0.45f, 1.0f, 0.6f, 1.0f);
        const char* badge = state.IsBroken() ? "[Unavailable]" : (copied ? "[Copy]" : "[Reference]");
        ImGui::TextColored(badgeColor, "%s", badge);

        std::string description;
        if (const auto* copy = GetPresetCopy(state.GetLink()))
            description = copy->name.empty() ? "Saved copy" : copy->name;
        else if (GetPresetReference(state.GetLink())) {
            description = state.IsBroken() || state.GetResolvedPath().empty()
                              ? "Saved preset unavailable"
                              : PresetUtils::PathToUtf8(state.GetResolvedPath().filename());
        }
        if (!description.empty()) ImGui::TextWrapped("%s", description.c_str());
    }
};
