#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Menu/Section.h"
#include "Utils/ArmorPresetSerializer.h"
#include "Utils/GuiUtils.h"
#include "Utils/PresetPickerState.h"
#include "Utils/SaveEditorModel.h"
#include "Utils/WeaponPresetSerializer.h"

class SaveEditorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::Player, "Save Editor", "Edit values stored in Half Sword save files."
    };

private:
    struct OperationResult {
        std::variant<std::monostate, SaveEditorModel::Document, SaveEditorModel::ValueNode> payload;
        std::string message;
        std::string info;
        std::string gripMeshPath;
        SaveEditorModel::NodeId targetNodeId{};
        int weaponCoa = 0;
        SaveEditorModel::PresetTargetKind targetKind = SaveEditorModel::PresetTargetKind::None;
        bool success = false;
        bool refreshSlots = false;
        bool refreshBackups = false;
    };

    struct AsyncState {
        std::mutex mutex;
        std::optional<OperationResult> pendingResult;

        void Publish(OperationResult result);
        std::optional<OperationResult> TakeResult();
    };

    struct BackupEntry {
        std::filesystem::path path;
        std::filesystem::file_time_type writeTime;
    };

    enum class Panel : std::uint8_t {
        None,
        Preset,
        Backups,
        SaveCopy,
        ConfirmSave,
        DiscardOpen,
        DiscardAll,
        Restore,
        DeleteOldest,
        DeleteAll
    };

    std::vector<std::string> saveSlots;
    int selectedSlotIndex = -1;
    bool slotsNeedRefresh = true;
    std::shared_ptr<SaveEditorModel::Document> document;
    std::shared_ptr<AsyncState> asyncState = std::make_shared<AsyncState>();
    std::vector<BackupEntry> backups;
    PresetPickerState<WeaponPresetSerializer> weaponPresetPicker;
    PresetPickerState<ArmorPresetSerializer> armorPresetPicker;
    GuiUtils::StatusMessage status;
    char copySlotName[256] = {};
    char propertyFilter[160] = {};
    int expandState = 0;
    int backupsToDelete = 1;
    std::uint8_t backupRetentionIndex = 0;
    bool operationPending = false;
    bool backupsEnabled = true;
    bool backupsNeedRefresh = true;
    bool showChangedOnly = false;
    bool filterHasMatches = false;
    bool pendingSaveCopy = false;
    bool pendingCopyTargetExists = false;
    bool pendingTargetChanged = false;
    std::size_t dirtyValueCount = 0;
    std::size_t pendingDeleteCount = 0;
    std::string pendingTargetSlot;
    std::string pendingOpenSlot;
    std::string backupCatalogError;
    std::string latestBackupTimestamp;
    std::string panelError;
    std::string classMismatchWarning;
    std::filesystem::path pendingRestorePath;
    const SaveEditorModel::ValueNode* pendingPresetNode = nullptr;
    GuiUtils::StatusMessage::Token operationStatusToken = 0;
    Panel panel = Panel::None;
    ImVec2 panelAnchor{};
    ImVec2 panelPivot{0.5f, 0.0f};
    bool panelOpenRequested = false;

    static const std::filesystem::path& SaveGameDirectory();
    static const std::filesystem::path& BackupRootDirectory();
    static std::string FriendlyClassName(std::string_view classPath);
    static bool NodeMatchesFilterText(const SaveEditorModel::ValueNode& node, std::string_view filter);

    void RefreshSlots();
    void QueueLoad(std::string slotName);
    void QueueSave(std::string targetSlot, std::optional<bool> expectedTargetExists = std::nullopt);
    void QueueRestore(std::filesystem::path backupPath);
    void DrainResults();
    void AcceptDocument(SaveEditorModel::Document loaded);
    void RebuildFilterMatches();
    bool CollectFilterMatches(SaveEditorModel::ValueNode& node, std::string_view filter, bool ancestorMatches);
    void RefreshBackups();
    void ApplyPresetResult(OperationResult& result);
    void OpenPanel(Panel nextPanel);
    void ClosePanel();
    void ResetPanel();
    void RequestPreset(const SaveEditorModel::ValueNode& node);
    void QueueWeaponPreset(SaveEditorModel::NodeId nodeId, SaveEditorModel::PresetTargetKind kind);
    void QueueArmorPreset(SaveEditorModel::NodeId nodeId, SaveEditorModel::PresetTargetKind kind);
    static bool TrySaveFileExists(std::string_view slotName, bool& exists, std::string& error);
    static bool CreateBackup(std::string_view slotName, std::string& error);
    static bool ListBackups(std::string_view slotName, std::vector<BackupEntry>& entries, std::string& error);
    static bool DeleteBackups(
        std::string_view slotName, const std::vector<BackupEntry>& entries, std::size_t count, std::string& error
    );
    static bool PruneBackups(std::string_view slotName, int retention, std::string& error);
    static SaveEditorModel::LoadResult LoadBackupDocument(
        const std::filesystem::path& backupPath, std::string_view targetSlot, int userIndex
    );
    static bool DeleteTemporarySave(std::string_view slotName, int userIndex);
    bool ValidateSaveTarget(std::string_view targetSlot, std::string& error) const;

    void RenderSaveSelector();
    void RenderValues(std::size_t footerDirtyValueCount, const char* saveLabel);
    std::ptrdiff_t RenderNode(SaveEditorModel::ValueNode& node);
    std::ptrdiff_t RenderLeaf(SaveEditorModel::ValueNode& node);
    void RenderScalar(SaveEditorModel::ValueNode& node);
    void RenderTextValue(SaveEditorModel::ValueNode& node, const char* hint);
    void RenderEnumValue(SaveEditorModel::ValueNode& node);
    void RenderSaveActions(std::size_t visibleDirtyValueCount, const char* saveLabel);
    void RenderPanel();
    void RenderPresetPanel();
    void RenderBackupPanel();
    void RenderSaveCopyPanel();

public:
    explicit SaveEditorSection(ModContext& ctx);
    void OnOpen() override;
    void Render() override;
};
