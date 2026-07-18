#include "Menu/Sections/Player/SaveEditorSection.h"

#include "ConfigManager.h"
#include "Hooks/GameHook.h"
#include "SDK/Engine_classes.hpp"
#include "Utils/PresetApplication.h"
#include "Utils/PresetUtils.h"

#include <Windows.h>
#include <ShlObj.h>
#include <KnownFolders.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iterator>
#include <limits>
#include <ranges>
#include <type_traits>
#include <utility>

namespace {
    constexpr GuiUtils::WidthSpec SAVE_COMBO_WIDTH{140.0f, 0.0f, GuiUtils::K_COMBO_MAX_WIDTH};
    constexpr GuiUtils::WidthSpec SAVE_SCALAR_WIDTH{90.0f, GuiUtils::K_DRAG_WIDTH, GuiUtils::K_DRAG_WIDTH};
    constexpr GuiUtils::WidthSpec SAVE_ENUM_WIDTH{120.0f, 0.0f, 240.0f};
    constexpr GuiUtils::WidthSpec SAVE_TEXT_WIDTH{160.0f, 240.0f, 320.0f};
    constexpr GuiUtils::WidthSpec BACKUP_COUNT_WIDTH{72.0f, 96.0f, 120.0f};
    constexpr float SAVE_SEARCH_WIDTH = 320.0f;
    constexpr char BACKUP_CONFIG_SECTION[] = "SaveEditor";
    constexpr std::string_view BACKUP_ROOT_NAME = "HSE Save Backups";
    constexpr char SAVE_EDITOR_PANEL[] = "##SaveEditorPanel";
    constexpr float SAVE_EDITOR_PANEL_VIEWPORT_MARGIN = 32.0f;
    constexpr ImGuiWindowFlags PANEL_FLAGS = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar |
                                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;

    struct RetentionOption {
        int count;
        const char* label;
    };

    constexpr std::array RETENTION_OPTIONS{
        RetentionOption{10, "Keep latest 10"}, RetentionOption{25, "Keep latest 25"},
        RetentionOption{50, "Keep latest 50"}, RetentionOption{0, "Keep all"}
    };

    template <typename PresetData> struct PresetTask {
        PresetData preset;
        SaveEditorModel::NodeId targetNodeId{};
        SaveEditorModel::PresetTargetKind targetKind = SaveEditorModel::PresetTargetKind::None;
    };

    struct MappedField {
        std::string_view target;
        std::string_view source;
    };

    struct IndexedField {
        std::string_view source;
        std::int64_t index;
    };

    constexpr std::array WEAPON_FIELDS{
        MappedField{"WeaponBPClass", "WeaponClass"},
        MappedField{"GripModule", "GripModule"},
        MappedField{"HeadModule", "HeadModule"},
        MappedField{"GuardModule", "GuardModule"},
        MappedField{"PommelModule", "PommelModule"},
        MappedField{"HeadSubModule1", "HeadSubModule1"},
        MappedField{"HeadSubModule2", "HeadSubModule2"},
        MappedField{"HeadSize", "HeadSize"},
        MappedField{"GuardSize", "GuardSize"},
        MappedField{"PommelPommelSize", "PommelSize"},
    };
    constexpr std::array MATERIAL_FIELDS{
        IndexedField{"MaterialMetalSteel", 0},
        IndexedField{"MaterialMetalColored", 1},
        IndexedField{"MaterialWeood", 2},
        IndexedField{"MaterialLeather", 3},
    };
    constexpr std::array COLOR_FIELDS{
        IndexedField{"ColorWood", 2},
        IndexedField{"ColorLeather", 3},
    };

    std::array<char, 64> SaveChangesLabel(std::size_t count) {
        std::array<char, 64> label{};
        (void)std::snprintf(label.data(), label.size(), "Save %zu %s", count, count == 1 ? "change" : "changes");
        return label;
    }

    constexpr std::size_t FindRetentionIndex(int value) noexcept {
        for (std::size_t index = 0; index < RETENTION_OPTIONS.size(); ++index)
            if (RETENTION_OPTIONS[index].count == value) return index;
        return 0;
    }

    std::string BackupTimestampStem() {
        const auto now = std::chrono::system_clock::now();
        const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm local{};
        localtime_s(&local, &nowTime);
        char timestamp[40]{};
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", &local);
        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
        char result[48]{};
        (void)std::snprintf(result, sizeof(result), "%s-%03lld", timestamp, static_cast<long long>(milliseconds));
        return result;
    }

    std::string FriendlyBackupTimestamp(std::string value) {
        if (value.size() >= 19 && value[10] == '_' && value[13] == '-' && value[16] == '-') {
            value[10] = ' ';
            value[13] = ':';
            value[16] = ':';
            value.resize(19);
        }
        return value;
    }

    void AppendSentence(std::string& text, std::string_view sentence) {
        if (!text.empty() && text.back() != '.' && text.back() != '!' && text.back() != '?') text.push_back('.');
        if (!text.empty()) text.push_back(' ');
        text.append(sentence);
    }

    constexpr std::string_view ReadOnlyExplanation(SaveEditorModel::ReadOnlyReason reason) noexcept {
        using SaveEditorModel::ReadOnlyReason;
        switch (reason) {
            case ReadOnlyReason::None: return "This value is read-only.";
            case ReadOnlyReason::NotSaved: return "This value is not stored in the save file.";
            case ReadOnlyReason::Deprecated: return "The game no longer uses this value.";
            case ReadOnlyReason::UnsafeContainer: return "Changing this entry could damage the save.";
            case ReadOnlyReason::MissingMetadata:
                return "The game did not provide enough information to edit this value.";
            case ReadOnlyReason::UnsupportedType: return "This value type is not supported.";
            case ReadOnlyReason::Unverified: return "HSE cannot verify this value, so it is read-only.";
        }
        return "This value is read-only.";
    }

    std::filesystem::path Utf8Path(std::string_view value) {
        std::wstring wide;
        if (!PresetUtils::TryUtf8ToWide(value, wide)) return {};
        return std::filesystem::path(wide);
    }

    std::filesystem::path SaveFilename(std::string_view slotName) {
        auto filename = Utf8Path(slotName);
        if (!filename.empty()) filename += L".sav";
        return filename;
    }

    std::filesystem::path BackupRootFor(const std::filesystem::path& saveDirectory) {
        return saveDirectory.empty() ? std::filesystem::path{} : saveDirectory.parent_path() / BACKUP_ROOT_NAME;
    }

    std::filesystem::path BackupDirectoryFor(
        const std::filesystem::path& root, std::string_view slotName
    ) {
        const auto slot = Utf8Path(slotName);
        return root.empty() || slot.empty() ? std::filesystem::path{} : root / slot;
    }

    bool IsDirectChildPath(const std::filesystem::path& parent, const std::filesystem::path& candidate) {
        std::error_code error;
        const auto resolvedParent = PresetUtils::CanonicalAbsolute(parent, error);
        if (error) return false;
        const auto resolvedCandidate = PresetUtils::CanonicalAbsolute(candidate, error);
        return !error && PresetUtils::PresetPathsEqual(resolvedCandidate.parent_path(), resolvedParent) &&
               PresetUtils::PathComponentEquals(resolvedCandidate.filename(), candidate.filename());
    }

    bool IsBackupRootPath(
        const std::filesystem::path& saveDirectory, const std::filesystem::path& backupRoot
    ) {
        return !saveDirectory.empty() && !backupRoot.empty() &&
               IsDirectChildPath(saveDirectory.parent_path(), backupRoot);
    }

    bool IsBackupTimestamp(std::wstring_view stem) {
        constexpr std::wstring_view PATTERN = L"0000-00-00_00-00-00-000";
        if (stem.size() != PATTERN.size() && stem.size() != PATTERN.size() + 4) return false;
        for (std::size_t index = 0; index < PATTERN.size(); ++index) {
            if (PATTERN[index] == L'0') {
                if (stem[index] < L'0' || stem[index] > L'9') return false;
            } else if (stem[index] != PATTERN[index]) {
                return false;
            }
        }
        if (stem.size() == PATTERN.size()) return true;
        if (stem[PATTERN.size()] != L'-') return false;
        return std::ranges::all_of(stem.substr(PATTERN.size() + 1), [](wchar_t character) {
            return character >= L'0' && character <= L'9';
        });
    }

    bool IsDirectBackupFile(const std::filesystem::path& directory, const std::filesystem::path& candidate) {
        return !directory.empty() && PresetUtils::PathComponentEquals(candidate.extension(), L".sav") &&
               IsBackupTimestamp(candidate.stem().native()) && IsDirectChildPath(directory, candidate);
    }

    bool FilesMatch(const std::filesystem::path& left, const std::filesystem::path& right) {
        std::error_code error;
        const auto leftSize = std::filesystem::file_size(left, error);
        if (error) return false;
        const auto rightSize = std::filesystem::file_size(right, error);
        if (error || leftSize != rightSize) return false;

        std::ifstream leftStream(left, std::ios::binary);
        std::ifstream rightStream(right, std::ios::binary);
        if (!leftStream || !rightStream) return false;
        constexpr std::size_t BUFFER_SIZE = std::size_t{16} * 1024;
        std::array<char, BUFFER_SIZE> leftBuffer{};
        std::array<char, BUFFER_SIZE> rightBuffer{};
        std::uintmax_t remaining = leftSize;
        while (remaining > 0) {
            const auto chunk =
                static_cast<std::streamsize>((std::min)(remaining, static_cast<std::uintmax_t>(leftBuffer.size())));
            leftStream.read(leftBuffer.data(), chunk);
            rightStream.read(rightBuffer.data(), chunk);
            if (leftStream.gcount() != chunk || rightStream.gcount() != chunk ||
                std::memcmp(leftBuffer.data(), rightBuffer.data(), static_cast<std::size_t>(chunk)) != 0) {
                return false;
            }
            remaining -= static_cast<std::uintmax_t>(chunk);
        }
        return true;
    }

    bool InputTextString(const char* label, const char* hint, std::string& value) {
        constexpr ImGuiInputTextFlags FLAGS = ImGuiInputTextFlags_CallbackResize;
        const auto callback = [](ImGuiInputTextCallbackData* data) {
            if (data->EventFlag != ImGuiInputTextFlags_CallbackResize) return 0;
            auto* string = static_cast<std::string*>(data->UserData);
            string->resize(static_cast<std::size_t>(data->BufTextLen));
            data->Buf = string->data();
            return 0;
        };
        const bool changed =
            ImGui::InputTextWithHint(label, hint, value.data(), value.capacity() + 1, FLAGS, callback, &value);
        if (changed) value.resize(std::strlen(value.c_str()));
        return changed;
    }

    template <typename Node> auto* FindChildStartingWith(Node& parent, std::string_view prefix) {
        const auto found = std::ranges::find_if(parent.children, [prefix](const auto& child) {
            return child.rawName.starts_with(prefix);
        });
        return found == parent.children.end() ? nullptr : &*found;
    }

    bool CopyCompatibleValues(SaveEditorModel::ValueNode& target, const SaveEditorModel::ValueNode& source) {
        if (target.kind != source.kind) return false;
        if (target.children.empty() && source.children.empty()) {
            if (!target.editable) return false;
            target.value = source.value;
            return true;
        }

        bool copied = false;
        for (auto& targetChild : target.children) {
            auto sourceIt = std::ranges::find_if(source.children, [&targetChild](const auto& candidate) {
                return candidate.rawName == targetChild.rawName;
            });
            if (sourceIt != source.children.end()) copied |= CopyCompatibleValues(targetChild, *sourceIt);
        }
        return copied;
    }

    bool CopyMappedChild(
        SaveEditorModel::ValueNode& target, std::string_view targetPrefix, const SaveEditorModel::ValueNode& source,
        std::string_view sourcePrefix
    ) {
        auto* destination = FindChildStartingWith(target, targetPrefix);
        const auto* value = FindChildStartingWith(source, sourcePrefix);
        return destination && value && CopyCompatibleValues(*destination, *value);
    }

    std::optional<std::int64_t> IntegerValue(const SaveEditorModel::ValueNode& node) {
        if (const auto* value = std::get_if<std::int64_t>(&node.value)) return *value;
        if (const auto* value = std::get_if<std::uint64_t>(&node.value)) return static_cast<std::int64_t>(*value);
        return std::nullopt;
    }

    bool ApplyMapValue(SaveEditorModel::ValueNode& map, std::int64_t key, const SaveEditorModel::ValueNode& source) {
        for (auto& entry : map.children) {
            if (entry.children.size() < 2) continue;
            const auto entryKey = IntegerValue(entry.children.front());
            if (!entryKey || *entryKey != key) continue;
            return CopyCompatibleValues(entry.children.back(), source);
        }
        return false;
    }

    bool OverlayWeaponPassportOnParts(
        SaveEditorModel::ValueNode& target, const SaveEditorModel::ValueNode& passport, std::string_view gripMeshPath,
        int coa
    ) {
        auto candidate = target;
        for (const auto& [destination, source] : WEAPON_FIELDS)
            if (!CopyMappedChild(candidate, destination, passport, source)) return false;

        if (auto* grip = FindChildStartingWith(candidate, "GripMesh")) {
            if (!grip->editable) return false;
            grip->value = std::string(gripMeshPath);
        } else {
            return false;
        }
        if (auto* coaNode = FindChildStartingWith(candidate, "COAInt")) {
            if (!coaNode->editable) return false;
            coaNode->value = static_cast<std::int64_t>(coa);
        } else {
            return false;
        }

        auto* materialMap = FindChildStartingWith(candidate, "MemberVar_40_43");
        if (!materialMap) return false;
        for (const auto& [sourceName, index] : MATERIAL_FIELDS) {
            const auto* source = FindChildStartingWith(passport, sourceName);
            if (!source || !ApplyMapValue(*materialMap, index, *source)) return false;
        }

        auto* colorMap = FindChildStartingWith(candidate, "MemberVar_44_45");
        if (!colorMap) return false;
        for (const auto& [sourceName, index] : COLOR_FIELDS) {
            const auto* source = FindChildStartingWith(passport, sourceName);
            if (!source || !ApplyMapValue(*colorMap, index, *source)) return false;
        }
        target = std::move(candidate);
        return true;
    }

} // namespace

void SaveEditorSection::AsyncState::Publish(OperationResult result) {
    std::scoped_lock lock(mutex);
    pendingResult = std::move(result);
}

std::optional<SaveEditorSection::OperationResult> SaveEditorSection::AsyncState::TakeResult() {
    std::scoped_lock lock(mutex);
    return std::exchange(pendingResult, std::nullopt);
}

SaveEditorSection::SaveEditorSection(ModContext& ctx) : Section(ctx, SECTION) {
    auto& config = ConfigManager::Get();
    backupsEnabled = config.GetBool(BACKUP_CONFIG_SECTION, "backups_enabled", true);
    backupRetentionIndex = static_cast<std::uint8_t>(FindRetentionIndex(
        config.GetInt(BACKUP_CONFIG_SECTION, "backup_retention", RETENTION_OPTIONS.front().count)
    ));
}

void SaveEditorSection::OnOpen() {
    ResetPanel();
    slotsNeedRefresh = true;
}

const std::filesystem::path& SaveEditorSection::SaveGameDirectory() {
    static const std::filesystem::path DIRECTORY = [] {
        PWSTR localAppData = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)) || !localAppData) {
            CoTaskMemFree(localAppData);
            return std::filesystem::path{};
        }
        auto result = std::filesystem::path(localAppData) / "HalfSwordUE5" / "Saved" / "SaveGames";
        CoTaskMemFree(localAppData);
        return result;
    }();
    return DIRECTORY;
}

const std::filesystem::path& SaveEditorSection::BackupRootDirectory() {
    static const std::filesystem::path DIRECTORY = BackupRootFor(SaveGameDirectory());
    return DIRECTORY;
}

std::string SaveEditorSection::FriendlyClassName(std::string_view classPath) {
    const auto dot = classPath.rfind('.');
    if (dot != std::string_view::npos) classPath.remove_prefix(dot + 1);
    const auto space = classPath.rfind(' ');
    if (space != std::string_view::npos) classPath.remove_prefix(space + 1);
    if (classPath.starts_with("SG_")) classPath.remove_prefix(3);
    if (classPath.ends_with("_C")) classPath.remove_suffix(2);
    std::string result;
    result.reserve(classPath.size());
    for (std::size_t index = 0; index < classPath.size(); ++index) {
        const auto current = static_cast<unsigned char>(classPath[index]);
        if (current == '_') {
            result.push_back(' ');
            continue;
        }
        const bool hasPrevious = index > 0 && classPath[index - 1] != '_';
        const bool startsWord =
            std::isupper(current) && hasPrevious &&
            (std::islower(static_cast<unsigned char>(classPath[index - 1])) ||
             (index + 1 < classPath.size() && std::islower(static_cast<unsigned char>(classPath[index + 1]))));
        if (startsWord) result.push_back(' ');
        result.push_back(static_cast<char>(current));
    }
    return result.empty() ? "Unknown save type" : result;
}

bool SaveEditorSection::NodeMatchesFilterText(const SaveEditorModel::ValueNode& node, std::string_view filter) {
    return (!node.displayName.empty() && GuiUtils::MatchesFilter(
                                             node.displayName.data(), node.displayName.size(), filter.data(), filter.size()
                                         )) ||
           (!node.rawName.empty() &&
            GuiUtils::MatchesFilter(node.rawName.data(), node.rawName.size(), filter.data(), filter.size()));
}

void SaveEditorSection::RefreshSlots() {
    slotsNeedRefresh = false;
    const std::string selected = document
                                     ? document->sourceSlot
                                     : (selectedSlotIndex >= 0 && selectedSlotIndex < static_cast<int>(saveSlots.size())
                                            ? saveSlots[static_cast<std::size_t>(selectedSlotIndex)]
                                            : std::string{});
    saveSlots.clear();

    const auto& directory = SaveGameDirectory();
    std::error_code error;
    if (directory.empty() || !std::filesystem::exists(directory, error) || error) {
        selectedSlotIndex = -1;
        return;
    }

    for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
        const auto& entry = *it;
        std::error_code fileError;
        if (!entry.is_regular_file(fileError) || fileError) continue;
        if (!PresetUtils::PathComponentEquals(entry.path().extension(), L".sav")) continue;

        saveSlots.push_back(PresetUtils::PathToUtf8(entry.path().stem()));
    }

    std::ranges::sort(saveSlots);
    const auto selectedSlot = std::ranges::find(saveSlots, selected);
    selectedSlotIndex = selectedSlot == saveSlots.end()
                            ? -1
                            : static_cast<int>(std::distance(saveSlots.begin(), selectedSlot));
}

void SaveEditorSection::QueueLoad(std::string slotName) {
    if (operationPending || slotName.empty()) return;
    operationPending = true;
    operationStatusToken = status.SetInfo("Opening '" + slotName + "'...");
    auto state = asyncState;
    const bool queued = GameHook::QueueAction([state, slotName = std::move(slotName)](const RuntimeContextSnapshot&) {
        OperationResult result;
        try {
            auto loaded = SaveEditorModel::LoadDocument(slotName);
            result.success = loaded.success;
            if (loaded.success) {
                result.payload = std::move(loaded.document);
            } else {
                result.message = std::move(loaded.error);
            }
        } catch (...) {
            result.message = "Could not open the save because an unexpected error occurred.";
        }
        state->Publish(std::move(result));
    });
    if (!queued) {
        operationPending = false;
        operationStatusToken = 0;
        status.SetError("Could not start opening the save. Try again.");
    }
}

void SaveEditorSection::QueueSave(std::string targetSlot, std::optional<bool> expectedTargetExists) {
    if (operationPending || !document) return;
    operationPending = true;
    operationStatusToken = status.SetInfo("Saving '" + targetSlot + "'...");
    auto state = asyncState;
    std::shared_ptr<const SaveEditorModel::Document> draft = document;
    const bool createBackups = backupsEnabled;
    const int retention = RETENTION_OPTIONS[backupRetentionIndex].count;
    const bool queued =
        GameHook::QueueAction([state, draft = std::move(draft), targetSlot = std::move(targetSlot),
                               expectedTargetExists, createBackups, retention](const RuntimeContextSnapshot& runtime) {
            OperationResult result;
            try {
                if (expectedTargetExists) {
                    bool targetExists = false;
                    std::string targetError;
                    if (!SaveEditorSection::TrySaveFileExists(targetSlot, targetExists, targetError)) {
                        result.message = std::move(targetError);
                        state->Publish(std::move(result));
                        return;
                    }
                    if (targetExists != *expectedTargetExists) {
                        result.message = "The save file changed before saving. Your edits are still open; review the "
                                         "copy name and try again.";
                        state->Publish(std::move(result));
                        return;
                    }
                }
                auto liveFlush = SaveEditorModel::FlushLiveState(*draft, targetSlot, runtime.world);
                if (!liveFlush.success) {
                    result.message = std::move(liveFlush.error);
                    AppendSentence(result.message, "Your edits were not applied; they are still open.");
                    state->Publish(std::move(result));
                    return;
                }
                if (createBackups) {
                    std::string backupError;
                    if (!SaveEditorSection::CreateBackup(targetSlot, backupError)) {
                        result.message = std::move(backupError);
                        state->Publish(std::move(result));
                        return;
                    }
                    result.refreshBackups = true;
                }
                auto saved = SaveEditorModel::SaveDocumentAndSynchronize(*draft, targetSlot, runtime.world);
                result.success = saved.success;
                result.message = std::move(saved.error);
                if (createBackups && saved.success) {
                    std::string pruneError;
                    if (!SaveEditorSection::PruneBackups(targetSlot, retention, pruneError))
                        AppendSentence(result.message, "Old backups could not be removed.");
                }
                if (saved.success) {
                    result.refreshSlots = expectedTargetExists.has_value() && !*expectedTargetExists;
                    if (saved.liveStateSynchronized)
                        result.info = "Your open Progression game was updated too.";
                    else if (SaveEditorModel::IsProgressSlot(targetSlot))
                        result.info = "Changes will load next time you enter Progression.";
                    result.payload = std::move(saved.document);
                }
            } catch (...) {
                result.message = "Saving stopped unexpectedly. Your edits are still open; try again.";
            }
            state->Publish(std::move(result));
        });
    if (!queued) {
        operationPending = false;
        operationStatusToken = 0;
        status.SetError("Could not start saving. Try again.");
    }
}

void SaveEditorSection::QueueRestore(std::filesystem::path backupPath) {
    if (operationPending || !document || backupPath.empty()) return;
    operationPending = true;
    operationStatusToken = status.SetInfo("Restoring the latest backup...");
    auto state = asyncState;
    std::shared_ptr<const SaveEditorModel::Document> current = document;
    const std::string_view targetSlot = document->sourceSlot;
    const bool createBackups = backupsEnabled;
    const int retention = RETENTION_OPTIONS[backupRetentionIndex].count;
    const bool queued =
        GameHook::QueueAction([state, current = std::move(current), backupPath = std::move(backupPath), targetSlot,
                               createBackups, retention](const RuntimeContextSnapshot& runtime) {
            OperationResult result;
            std::string temporarySlot;
            try {
                SaveEditorModel::LiveSyncResult liveFlush;
                if (SaveEditorModel::IsProgressSlot(targetSlot) &&
                    SaveEditorModel::ClassPathMatches(
                        current->classPath, SaveEditorModel::ExpectedClassForSlot(targetSlot)
                    )) {
                    liveFlush = SaveEditorModel::FlushLiveState(*current, targetSlot, runtime.world);
                }
                if (!liveFlush.success) {
                    result.message = std::move(liveFlush.error);
                    AppendSentence(result.message, "The backup was not restored.");
                    state->Publish(std::move(result));
                    return;
                }

                if (createBackups) {
                    std::string backupError;
                    if (!SaveEditorSection::CreateBackup(targetSlot, backupError)) {
                        result.message = std::move(backupError);
                        state->Publish(std::move(result));
                        return;
                    }
                    result.refreshBackups = true;
                }

                auto backup = SaveEditorSection::LoadBackupDocument(backupPath, targetSlot, current->userIndex);
                if (!backup.success) {
                    result.message = std::move(backup.error);
                    state->Publish(std::move(result));
                    return;
                }
                temporarySlot = backup.document.sourceSlot;

                auto restored =
                    SaveEditorModel::RestoreDocumentAndSynchronize(backup.document, targetSlot, runtime.world);
                const bool temporaryDeleted = SaveEditorSection::DeleteTemporarySave(temporarySlot, current->userIndex);
                temporarySlot.clear();
                result.success = restored.success;
                result.message = std::move(restored.error);
                if (!temporaryDeleted) {
                    result.refreshSlots = true;
                    AppendSentence(result.message, "A temporary save could not be removed.");
                }
                if (restored.success) {
                    if (createBackups) {
                        std::string pruneError;
                        if (!SaveEditorSection::PruneBackups(targetSlot, retention, pruneError))
                            AppendSentence(result.message, "Old backups could not be removed.");
                    }
                    if (restored.liveStateSynchronized)
                        result.info = "Your open Progression game was updated too.";
                    result.payload = std::move(restored.document);
                }
            } catch (...) {
                if (!temporarySlot.empty())
                    (void)SaveEditorSection::DeleteTemporarySave(temporarySlot, current->userIndex);
                result.message = "Restoring stopped unexpectedly. Reopen the save to check it.";
            }
            state->Publish(std::move(result));
        });
    if (!queued) {
        operationPending = false;
        operationStatusToken = 0;
        status.SetError("Could not start restoring the backup. Try again.");
    }
}

void SaveEditorSection::DrainResults() {
    auto pendingResult = asyncState->TakeResult();
    if (!pendingResult) return;
    auto& result = *pendingResult;
    operationPending = false;
    if (result.success) {
        if (auto* loaded = std::get_if<SaveEditorModel::Document>(&result.payload))
            AcceptDocument(std::move(*loaded));
        else if (std::holds_alternative<SaveEditorModel::ValueNode>(result.payload))
            ApplyPresetResult(result);
    }
    if (!result.success) {
        status.SetError(std::move(result.message));
    } else if (!result.message.empty()) {
        if (!result.info.empty()) AppendSentence(result.message, result.info);
        status.SetError(std::move(result.message));
    } else if (!result.info.empty()) {
        status.SetInfo(std::move(result.info));
    } else {
        status.ClearText(operationStatusToken);
    }
    operationStatusToken = 0;
    if (result.refreshSlots) slotsNeedRefresh = true;
    if (result.refreshBackups) backupsNeedRefresh = true;
}

void SaveEditorSection::AcceptDocument(SaveEditorModel::Document loaded) {
    ResetPanel();
    document = std::make_shared<SaveEditorModel::Document>(std::move(loaded));
    (void)std::snprintf(copySlotName, sizeof(copySlotName), "%s - Copy", document->sourceSlot.c_str());
    propertyFilter[0] = '\0';
    backups.clear();
    latestBackupTimestamp.clear();
    backupsNeedRefresh = true;
    backupCatalogError.clear();
    classMismatchWarning.clear();
    const std::string_view expected = SaveEditorModel::ExpectedClassForSlot(document->sourceSlot);
    if (!expected.empty() && !SaveEditorModel::ClassPathMatches(document->classPath, expected)) {
        const std::string documentClassLabel = FriendlyClassName(document->classPath);
        const std::string expectedName = FriendlyClassName(expected);
        classMismatchWarning = "'" + document->sourceSlot + "' contains " + documentClassLabel +
                               " data, but the game expects " + expectedName +
                               " data here. HSE will not overwrite it. Save a copy instead, or restore the correct "
                               "file before the game loads this save.";
    }
    const auto selectedSlot = std::ranges::find(saveSlots, document->sourceSlot);
    selectedSlotIndex = selectedSlot == saveSlots.end()
                            ? -1
                            : static_cast<int>(std::distance(saveSlots.begin(), selectedSlot));
    dirtyValueCount = 0;
    RebuildFilterMatches();
}

void SaveEditorSection::RebuildFilterMatches() {
    filterHasMatches = false;
    if (!document || (propertyFilter[0] == '\0' && !showChangedOnly)) return;
    const std::string_view filter(propertyFilter);
    for (auto& child : document->root.children)
        filterHasMatches = CollectFilterMatches(child, filter, false) || filterHasMatches;
}

bool SaveEditorSection::CollectFilterMatches(
    SaveEditorModel::ValueNode& node, std::string_view filter, bool ancestorMatches
) {
    const bool textMatches = filter.empty() || ancestorMatches || NodeMatchesFilterText(node, filter);
    bool matches = textMatches && (!showChangedOnly || (node.children.empty() && SaveEditorModel::IsDirty(node)));
    for (auto& child : node.children)
        matches = CollectFilterMatches(child, filter, textMatches) || matches;
    node.matchesFilter = matches;
    return matches;
}

void SaveEditorSection::RefreshBackups() {
    backupsNeedRefresh = false;
    if (!ListBackups(document->sourceSlot, backups, backupCatalogError)) backups.clear();
    latestBackupTimestamp = backups.empty()
                                ? std::string{}
                                : FriendlyBackupTimestamp(PresetUtils::PathToUtf8(backups.back().path.stem()));
    backupsToDelete = backups.empty() ? 1 : (std::min)(backupsToDelete, static_cast<int>(backups.size()));
}

void SaveEditorSection::ApplyPresetResult(OperationResult& result) {
    auto* presetValue = std::get_if<SaveEditorModel::ValueNode>(&result.payload);
    if (!document || !presetValue) return;
    auto* target = SaveEditorModel::FindNode(*document, result.targetNodeId);
    if (!target) {
        result.success = false;
        result.message = "That preset target is no longer available.";
        return;
    }

    std::string error;
    bool applied = false;
    if (result.targetKind == SaveEditorModel::PresetTargetKind::WeaponParts) {
        applied = OverlayWeaponPassportOnParts(*target, *presetValue, result.gripMeshPath, result.weaponCoa);
        if (!applied) error = "This weapon value is not compatible with the selected preset.";
    } else {
        applied = SaveEditorModel::OverlayValues(*target, *presetValue, error);
    }
    result.success = applied;
    if (applied) {
        dirtyValueCount = SaveEditorModel::CountDirty(document->root);
        RebuildFilterMatches();
    }
    if (!applied)
        result.message = error.empty() ? "This preset is not compatible with the selected value." : std::move(error);
}

void SaveEditorSection::OpenPanel(Panel nextPanel) {
    const ImVec2 itemMinimum = ImGui::GetItemRectMin();
    const ImVec2 itemMaximum = ImGui::GetItemRectMax();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float itemMiddleY = (itemMinimum.y + itemMaximum.y) * 0.5f;
    const float viewportMiddleY = viewport->WorkPos.y + viewport->WorkSize.y * 0.5f;
    const bool openBelow = itemMiddleY < viewportMiddleY;
    const float spacing = ImGui::GetStyle().ItemSpacing.y;

    panelAnchor =
        ImVec2((itemMinimum.x + itemMaximum.x) * 0.5f, openBelow ? itemMaximum.y + spacing : itemMinimum.y - spacing);
    panelPivot = ImVec2(0.5f, openBelow ? 0.0f : 1.0f);
    panel = nextPanel;
    panelOpenRequested = true;
    panelError.clear();
}

void SaveEditorSection::ClosePanel() {
    ImGui::CloseCurrentPopup();
    ResetPanel();
}

void SaveEditorSection::ResetPanel() {
    panel = Panel::None;
    panelOpenRequested = false;
    pendingSaveCopy = false;
    pendingCopyTargetExists = false;
    pendingTargetChanged = false;
    pendingDeleteCount = 0;
    pendingTargetSlot.clear();
    pendingOpenSlot.clear();
    backupCatalogError.clear();
    panelError.clear();
    pendingRestorePath.clear();
    pendingPresetNode = nullptr;
}

void SaveEditorSection::RequestPreset(const SaveEditorModel::ValueNode& node) {
    pendingPresetNode = &node;
    OpenPanel(Panel::Preset);
}

void SaveEditorSection::QueueWeaponPreset(SaveEditorModel::NodeId nodeId, SaveEditorModel::PresetTargetKind kind) {
    if (operationPending || !weaponPresetPicker.HasSelection()) return;
    auto loaded = WeaponPresetSerializer::LoadFromFileResult(weaponPresetPicker.SelectedPath());
    if (!loaded.success) {
        status.SetError("Could not load the weapon preset: " + loaded.error);
        return;
    }

    operationPending = true;
    operationStatusToken = status.SetInfo("Applying weapon preset...");
    auto state = asyncState;
    auto task = std::make_shared<PresetTask<WeaponPresetData>>(std::move(loaded.value), nodeId, kind);
    const bool queued = GameHook::QueueAction([state, task = std::move(task)](const RuntimeContextSnapshot&) {
        OperationResult result;
        try {
            result.targetNodeId = task->targetNodeId;
            result.targetKind = task->targetKind;
            result.gripMeshPath = task->preset.gripMeshPath;
            result.weaponCoa = task->preset.coaInt;
            std::string error;
            result.success = PresetApplication::MaterializeWeaponPreset(task->preset, &error);
            if (result.success) {
                auto value = SaveEditorModel::SnapshotStruct(task->preset.passport, error);
                result.success = error.empty();
                if (result.success) result.payload = std::move(value);
            }
            if (!result.success)
                result.message = error.empty() ? "The weapon preset is invalid." : std::move(error);
        } catch (...) {
            result.message = "Could not apply the weapon preset because an unexpected error occurred.";
        }
        state->Publish(std::move(result));
    });
    if (!queued) {
        operationPending = false;
        operationStatusToken = 0;
        status.SetError("Could not start applying the weapon preset. Try again.");
    }
}

void SaveEditorSection::QueueArmorPreset(SaveEditorModel::NodeId nodeId, SaveEditorModel::PresetTargetKind kind) {
    if (operationPending || !armorPresetPicker.HasSelection()) return;
    auto loaded = ArmorPresetSerializer::LoadFromFileResult(armorPresetPicker.SelectedPath());
    if (!loaded.success) {
        status.SetError("Could not load the armor preset: " + loaded.error);
        return;
    }

    operationPending = true;
    operationStatusToken = status.SetInfo("Applying armor preset...");
    auto state = asyncState;
    auto task = std::make_shared<PresetTask<ArmorPresetData>>(std::move(loaded.value), nodeId, kind);
    const bool queued = GameHook::QueueAction([state, task = std::move(task)](const RuntimeContextSnapshot&) {
        OperationResult result;
        try {
            result.targetNodeId = task->targetNodeId;
            result.targetKind = task->targetKind;
            std::string error;
            result.success = PresetApplication::MaterializeArmorPreset(task->preset, &error);
            if (result.success) {
                auto value = SaveEditorModel::SnapshotStruct(task->preset.passport, error);
                result.success = error.empty();
                if (result.success) result.payload = std::move(value);
            }
            if (!result.success)
                result.message = error.empty() ? "The armor preset is invalid." : std::move(error);
        } catch (...) {
            result.message = "Could not apply the armor preset because an unexpected error occurred.";
        }
        state->Publish(std::move(result));
    });
    if (!queued) {
        operationPending = false;
        operationStatusToken = 0;
        status.SetError("Could not start applying the armor preset. Try again.");
    }
}

bool SaveEditorSection::TrySaveFileExists(std::string_view slotName, bool& exists, std::string& error) {
    exists = false;
    const auto& directory = SaveGameDirectory();
    const auto filename = SaveFilename(slotName);
    if (directory.empty() || filename.empty()) {
        error = "Could not check this save name. Choose another name and try again.";
        return false;
    }
    std::error_code fileError;
    exists = std::filesystem::exists(directory / filename, fileError);
    if (fileError) {
        error = "Could not check whether this save file already exists. Try again.";
        return false;
    }
    return true;
}

bool SaveEditorSection::CreateBackup(std::string_view slotName, std::string& error) {
    error.clear();
    if (!SaveEditorModel::ValidateSlotName(slotName, error)) return false;
    const auto& directory = SaveGameDirectory();
    const auto filename = SaveFilename(slotName);
    if (directory.empty() || filename.empty()) {
        error = "Could not find the save file for the backup.";
        return false;
    }
    const auto source = directory / filename;
    std::error_code fileError;
    const bool sourceExists = std::filesystem::exists(source, fileError);
    if (fileError) {
        error = "Could not access the existing save, so no changes were made.";
        return false;
    }
    if (!sourceExists) {
        return true;
    }

    const auto& backupRoot = BackupRootDirectory();
    if (!IsBackupRootPath(directory, backupRoot)) {
        error = "HSE could not access the backup folder, so no changes were made.";
        return false;
    }
    const auto backupDirectory = BackupDirectoryFor(backupRoot, slotName);
    if (backupDirectory.empty()) {
        error = "HSE could not access the backup folder, so no changes were made.";
        return false;
    }
    std::filesystem::create_directories(backupDirectory, fileError);
    if (fileError || !IsBackupRootPath(directory, backupRoot) || !IsDirectChildPath(backupRoot, backupDirectory)) {
        error = "Could not create the backup folder, so no changes were made.";
        return false;
    }

    const std::string timestamp = BackupTimestampStem();
    std::filesystem::path candidate;
    for (unsigned int suffix = 0; suffix < 100; ++suffix) {
        char suffixText[8]{};
        (void)std::snprintf(suffixText, sizeof(suffixText), "-%03u", suffix);
        const std::string name = suffix == 0 ? timestamp : timestamp + suffixText;
        auto namePath = Utf8Path(name);
        if (namePath.empty()) continue;
        namePath += L".sav";
        auto fileCandidate = backupDirectory / namePath;
        if (CopyFileW(source.c_str(), fileCandidate.c_str(), TRUE)) {
            candidate = std::move(fileCandidate);
            break;
        }
        const DWORD copyError = GetLastError();
        if (copyError == ERROR_FILE_EXISTS || copyError == ERROR_ALREADY_EXISTS) continue;
        error = "Could not create a backup. No changes were made.";
        return false;
    }
    if (candidate.empty()) {
        error = "Could not choose a name for the backup. No changes were made.";
        return false;
    }
    fileError.clear();
    std::filesystem::last_write_time(candidate, std::filesystem::file_time_type::clock::now(), fileError);
    if (fileError) {
        std::error_code cleanupError;
        (void)std::filesystem::remove(candidate, cleanupError);
        error = "Could not finish creating the backup. No changes were made.";
        return false;
    }
    if (!FilesMatch(source, candidate)) {
        std::error_code cleanupError;
        (void)std::filesystem::remove(candidate, cleanupError);
        error = "Could not verify the backup. No changes were made.";
        return false;
    }
    return true;
}

bool SaveEditorSection::ListBackups(std::string_view slotName, std::vector<BackupEntry>& entries, std::string& error) {
    entries.clear();
    error.clear();
    if (!SaveEditorModel::ValidateSlotName(slotName, error)) return false;
    const auto& backupRoot = BackupRootDirectory();
    if (!IsBackupRootPath(SaveGameDirectory(), backupRoot)) {
        error = "The backup folder could not be opened.";
        return false;
    }
    const auto directory = BackupDirectoryFor(backupRoot, slotName);
    if (directory.empty()) {
        error = "HSE could not access the backup folder.";
        return false;
    }

    std::error_code fileError;
    const bool exists = std::filesystem::exists(directory, fileError);
    if (fileError) {
        error = "Could not read the backups.";
        return false;
    }
    if (!exists) return true;
    if (!std::filesystem::is_directory(directory, fileError) || fileError ||
        !IsDirectChildPath(backupRoot, directory)) {
        error = "The backup folder could not be opened.";
        return false;
    }

    for (std::filesystem::directory_iterator it(directory, fileError), end; !fileError && it != end;
         it.increment(fileError)) {
        const auto& entry = *it;
        std::error_code entryError;
        const auto& candidate = entry.path();
        if (!entry.is_regular_file(entryError) || entryError || !IsDirectBackupFile(directory, candidate)) {
            continue;
        }
        const auto writeTime = entry.last_write_time(entryError);
        if (entryError) {
            entries.clear();
            error = "Could not read all backups.";
            return false;
        }
        entries.push_back({candidate, writeTime});
    }
    if (fileError) {
        entries.clear();
        error = "Could not read all backups.";
        return false;
    }
    std::ranges::sort(entries, [](const BackupEntry& left, const BackupEntry& right) {
        if (left.writeTime != right.writeTime) return left.writeTime < right.writeTime;
        return PresetUtils::PathLess(left.path.stem(), right.path.stem());
    });
    return true;
}

bool SaveEditorSection::DeleteBackups(
    std::string_view slotName, const std::vector<BackupEntry>& entries, std::size_t count, std::string& error
) {
    error.clear();
    if (!SaveEditorModel::ValidateSlotName(slotName, error)) return false;
    const auto& backupRoot = BackupRootDirectory();
    if (!IsBackupRootPath(SaveGameDirectory(), backupRoot)) {
        error = "A backup was outside the expected folder and was not deleted.";
        return false;
    }
    const auto directory = BackupDirectoryFor(backupRoot, slotName);
    if (directory.empty()) {
        error = "HSE could not access the backup folder.";
        return false;
    }
    if (!IsDirectChildPath(backupRoot, directory)) {
        error = "A backup was outside the expected folder and was not deleted.";
        return false;
    }
    const std::size_t deleteCount = (std::min)(count, entries.size());
    for (std::size_t index = 0; index < deleteCount; ++index) {
        if (!IsDirectBackupFile(directory, entries[index].path)) {
            error = "A backup was outside the expected folder and was not deleted.";
            return false;
        }
        std::error_code removeError;
        if (!std::filesystem::remove(entries[index].path, removeError) || removeError) {
            error = index == 0 ? "Could not delete the selected backups."
                               : "Only some of the selected backups could be deleted.";
            return false;
        }
    }
    std::error_code cleanupError;
    if (std::filesystem::is_empty(directory, cleanupError) && !cleanupError)
        (void)std::filesystem::remove(directory, cleanupError);
    return true;
}

bool SaveEditorSection::PruneBackups(std::string_view slotName, int retention, std::string& error) {
    if (retention <= 0) return true;
    std::vector<BackupEntry> entries;
    if (!ListBackups(slotName, entries, error)) return false;
    if (entries.size() <= static_cast<std::size_t>(retention)) return true;
    return DeleteBackups(slotName, entries, entries.size() - static_cast<std::size_t>(retention), error);
}

SaveEditorModel::LoadResult SaveEditorSection::LoadBackupDocument(
    const std::filesystem::path& backupPath, std::string_view targetSlot, int userIndex
) {
    SaveEditorModel::LoadResult result;
    std::string validationError;
    if (!SaveEditorModel::ValidateSlotName(targetSlot, validationError)) {
        result.error = std::move(validationError);
        return result;
    }
    const auto& saveDirectory = SaveGameDirectory();
    const auto& backupRoot = BackupRootDirectory();
    const auto backupDirectory = BackupDirectoryFor(backupRoot, targetSlot);
    std::error_code fileError;
    if (!IsBackupRootPath(saveDirectory, backupRoot) || !IsDirectChildPath(backupRoot, backupDirectory) ||
        !IsDirectBackupFile(backupDirectory, backupPath) ||
        !std::filesystem::is_regular_file(backupPath, fileError) || fileError) {
        result.error = "The selected backup is no longer available.";
        return result;
    }

    static std::atomic_uint64_t sequence{0};
    std::string temporarySlot;
    std::filesystem::path temporaryPath;
    for (unsigned int attempt = 0; attempt < 32; ++attempt) {
        const auto unique = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()) +
                            sequence.fetch_add(1, std::memory_order_relaxed);
        temporarySlot = "HSE_BackupRestore_" + std::to_string(unique);
        temporaryPath = saveDirectory / SaveFilename(temporarySlot);
        if (CopyFileW(backupPath.c_str(), temporaryPath.c_str(), TRUE)) break;
        const DWORD copyError = GetLastError();
        if (copyError != ERROR_FILE_EXISTS && copyError != ERROR_ALREADY_EXISTS) {
            temporarySlot.clear();
            temporaryPath.clear();
            break;
        }
        temporarySlot.clear();
        temporaryPath.clear();
    }
    if (temporarySlot.empty() || temporaryPath.empty()) {
        result.error = "Could not prepare the backup for restoring.";
        return result;
    }

    try {
        if (!FilesMatch(backupPath, temporaryPath)) {
            (void)DeleteTemporarySave(temporarySlot, userIndex);
            result.error = "Could not verify the backup before restoring it.";
            return result;
        }
        result = SaveEditorModel::LoadDocument(temporarySlot, userIndex);
    } catch (...) {
        (void)DeleteTemporarySave(temporarySlot, userIndex);
        result.error = "The backup could not be opened safely.";
        return result;
    }
    if (!result.success) {
        (void)DeleteTemporarySave(temporarySlot, userIndex);
        result.error = "The backup could not be opened safely.";
    }
    return result;
}

bool SaveEditorSection::DeleteTemporarySave(std::string_view slotName, int userIndex) {
    std::wstring wideSlot;
    if (!PresetUtils::TryUtf8ToWide(slotName, wideSlot)) return false;
    if (SDK::UGameplayStatics::DeleteGameInSlot(SDK::FString(wideSlot.c_str()), userIndex)) return true;

    const auto& directory = SaveGameDirectory();
    const auto filename = SaveFilename(slotName);
    if (directory.empty() || filename.empty()) return false;
    const auto path = directory / filename;
    std::error_code error;
    return std::filesystem::remove(path, error) && !error;
}

bool SaveEditorSection::ValidateSaveTarget(std::string_view targetSlot, std::string& error) const {
    if (!document) {
        error = "Open a save file first.";
        return false;
    }
    if (!SaveEditorModel::ValidateSlotName(targetSlot, error)) return false;
    const std::string_view expected = SaveEditorModel::ExpectedClassForSlot(targetSlot);
    if (!expected.empty() && !SaveEditorModel::ClassPathMatches(document->classPath, expected)) {
        error = "'" + std::string(targetSlot) + "' is reserved for " + FriendlyClassName(expected) +
                " saves, but this file contains " + FriendlyClassName(document->classPath) +
                " data. Choose another name.";
        return false;
    }
    return true;
}

void SaveEditorSection::RenderSaveSelector() {
    const char* preview = selectedSlotIndex >= 0 && selectedSlotIndex < static_cast<int>(saveSlots.size())
                              ? saveSlots[static_cast<std::size_t>(selectedSlotIndex)].c_str()
                              : (saveSlots.empty() ? "No save files found" : "Choose a save file");
    ImGui::BeginDisabled(operationPending);
    ImGui::TextUnformatted("Save file");
    bool loadSelected = false;
    if (GuiUtils::BeginSizedCombo("##SavedGame", preview, SAVE_COMBO_WIDTH)) {
        for (int index = 0; index < static_cast<int>(saveSlots.size()); ++index) {
            const auto& slot = saveSlots[static_cast<std::size_t>(index)];
            const bool selected = index == selectedSlotIndex;
            if (ImGui::Selectable(slot.c_str(), selected)) {
                pendingOpenSlot = slot;
                if (document && dirtyValueCount > 0)
                    OpenPanel(Panel::DiscardOpen);
                else
                    loadSelected = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    (void)GuiUtils::SameLineIfFitsButton("Refresh");
    if (GuiUtils::Button("Refresh")) RefreshSlots();
    ImGui::EndDisabled();

    if (loadSelected) QueueLoad(std::exchange(pendingOpenSlot, {}));
}

void SaveEditorSection::RenderValues(std::size_t footerDirtyValueCount, const char* saveLabel) {
    if (!document) return;
    auto& activeDocument = *document;
    const float availableWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float actionGroupWidth = GuiUtils::CheckboxNaturalWidth("Changed only") +
                                   GuiUtils::ButtonNaturalWidth("Expand") + GuiUtils::ButtonNaturalWidth("Collapse") +
                                   spacing * 2.0f;
    const bool actionsFitBesideSearch = availableWidth >= SAVE_COMBO_WIDTH.min + actionGroupWidth + spacing;
    const float searchAvailable =
        actionsFitBesideSearch ? availableWidth - actionGroupWidth - spacing : availableWidth;
    GuiUtils::SetNextInputWidth((std::min)(searchAvailable, SAVE_SEARCH_WIDTH));
    if (ImGui::InputTextWithHint("##SaveValueFilter", "Search values...", propertyFilter, sizeof(propertyFilter)))
        RebuildFilterMatches();
    if (actionsFitBesideSearch) ImGui::SameLine();
    if (GuiUtils::CheckboxWithTooltip("Changed only", &showChangedOnly, "Show only values you have changed."))
        RebuildFilterMatches();
    (void)GuiUtils::SameLineIfFitsButton("Expand");
    if (GuiUtils::Button("Expand", GuiUtils::ButtonTone::Quiet)) expandState = 1;
    (void)GuiUtils::SameLineIfFitsButton("Collapse");
    if (GuiUtils::Button("Collapse", GuiUtils::ButtonTone::Quiet)) expandState = -1;

    const bool filtering = propertyFilter[0] != '\0' || showChangedOnly;
    const bool hasChanges = footerDirtyValueCount > 0;
    const std::array<const char*, 4> footerActions{
        saveLabel, hasChanges ? "Discard Changes" : nullptr, "Save a copy...", "Backups..."
    };
    int footerRows = 1;
    float rowWidth = 0.0f;
    const float footerWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
    for (const char* action : footerActions) {
        if (!action) continue;
        const float width = (std::min)(GuiUtils::ButtonNaturalWidth(action), footerWidth);
        const float required = rowWidth > 0.0f ? ImGui::GetStyle().ItemSpacing.x + width : width;
        if (rowWidth > 0.0f && rowWidth + required > footerWidth) {
            ++footerRows;
            rowWidth = width;
        } else {
            rowWidth += required;
        }
    }
    GuiUtils::BeginScrollWithFooter("##SaveValueTree", footerRows);
    std::ptrdiff_t dirtyDelta = 0;
    for (auto& child : activeDocument.root.children)
        dirtyDelta += RenderNode(child);
    if (filtering && !filterHasMatches)
        ImGui::TextDisabled(
            "%s", showChangedOnly && propertyFilter[0] == '\0' ? "No changed values." : "No matching values."
        );
    ImGui::EndChild();
    if (dirtyDelta > 0) {
        dirtyValueCount += static_cast<std::size_t>(dirtyDelta);
    } else if (dirtyDelta < 0) {
        const auto removed = static_cast<std::size_t>(-dirtyDelta);
        dirtyValueCount = removed > dirtyValueCount ? 0 : dirtyValueCount - removed;
    }
    if (showChangedOnly && dirtyDelta != 0) RebuildFilterMatches();
    expandState = 0;
}

std::ptrdiff_t SaveEditorSection::RenderNode(SaveEditorModel::ValueNode& node) {
    const bool filtering = propertyFilter[0] != '\0' || showChangedOnly;
    if (filtering && !node.matchesFilter) return 0;
    ImGui::PushID(&node);

    const bool container =
        !node.children.empty() || node.kind == SaveEditorModel::ValueKind::Struct ||
        node.kind == SaveEditorModel::ValueKind::Array || node.kind == SaveEditorModel::ValueKind::Map ||
        node.kind == SaveEditorModel::ValueKind::MapEntry || node.kind == SaveEditorModel::ValueKind::Set;
    std::ptrdiff_t dirtyDelta = 0;
    if (container) {
        const char* label = node.displayName.empty() ? node.rawName.c_str() : node.displayName.c_str();
        if (expandState != 0) ImGui::SetNextItemOpen(expandState > 0);
        const bool hasPresetAction = node.presetTarget != SaveEditorModel::PresetTargetKind::None;
        ImGuiTreeNodeFlags flags = filtering ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
        if (hasPresetAction) flags |= ImGuiTreeNodeFlags_SpanLabelWidth;
        const bool countedContainer = node.kind == SaveEditorModel::ValueKind::Array ||
                                      node.kind == SaveEditorModel::ValueKind::Map ||
                                      node.kind == SaveEditorModel::ValueKind::Set;
        const bool open = countedContainer ? ImGui::TreeNodeEx("##Node", flags, "%s (%zu)", label, node.children.size())
                                           : ImGui::TreeNodeEx("##Node", flags, "%s", label);
        const bool showContainerTooltip = ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip);
        if (hasPresetAction) {
            (void)GuiUtils::SameLineIfFitsButton("Apply preset...");
            if (GuiUtils::Button("Apply preset...", GuiUtils::ButtonTone::Quiet)) RequestPreset(node);
            GuiUtils::HelpTooltip("Choose a saved preset for this item.");
        }
        if (showContainerTooltip) {
            GuiUtils::BeginStyledTooltip();
            ImGui::TextUnformatted(
                node.children.empty()
                    ? "This group is empty."
                    : "Open this group to view its values. Compatible values can be edited; entries cannot be added "
                      "or removed."
            );
            GuiUtils::EndStyledTooltip();
        }
        if (open) {
            if (node.children.empty()) ImGui::TextDisabled("No editable values");
            for (auto& child : node.children)
                dirtyDelta += RenderNode(child);
            ImGui::TreePop();
        }
    } else {
        dirtyDelta = RenderLeaf(node);
    }

    ImGui::PopID();
    return dirtyDelta;
}

std::ptrdiff_t SaveEditorSection::RenderLeaf(SaveEditorModel::ValueNode& node) {
    if (!node.editable) {
        const char* label = node.displayName.empty() ? node.rawName.c_str() : node.displayName.c_str();
        ImGui::PushTextWrapPos(0.0f);
        if (node.kind == SaveEditorModel::ValueKind::Enum) {
            const auto* value = std::get_if<std::int64_t>(&node.value);
            const auto option = value ? std::ranges::find(node.enumOptions, *value, &SaveEditorModel::EnumOption::value)
                                      : node.enumOptions.end();
            if (option != node.enumOptions.end())
                ImGui::TextDisabled("%s: %s", label, option->displayName.c_str());
            else if (value)
                ImGui::TextDisabled("%s: Unknown (%lld)", label, static_cast<long long>(*value));
            else
                ImGui::TextDisabled("%s: Not available", label);
        } else {
            std::visit(
                [label](const auto& value) {
                    using Type = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Type, std::monostate>)
                        ImGui::TextDisabled("%s: Not available", label);
                    else if constexpr (std::is_same_v<Type, bool>)
                        ImGui::TextDisabled("%s: %s", label, value ? "True" : "False");
                    else if constexpr (std::is_same_v<Type, std::string>)
                        ImGui::TextDisabled("%s: %s", label, value.empty() ? "None" : value.c_str());
                    else if constexpr (std::is_same_v<Type, std::int64_t>)
                        ImGui::TextDisabled("%s: %lld", label, static_cast<long long>(value));
                    else if constexpr (std::is_same_v<Type, std::uint64_t>)
                        ImGui::TextDisabled("%s: %llu", label, static_cast<unsigned long long>(value));
                    else
                        ImGui::TextDisabled("%s: %.17g", label, value);
                },
                node.value
            );
        }
        ImGui::PopTextWrapPos();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
            const std::string_view explanation = ReadOnlyExplanation(node.readOnlyReason);
            ImGui::SetItemTooltip("%.*s", static_cast<int>(explanation.size()), explanation.data());
        }
        return 0;
    }

    const bool wasDirty = SaveEditorModel::IsDirty(node);
    switch (node.kind) {
        case SaveEditorModel::ValueKind::Boolean:
        case SaveEditorModel::ValueKind::SignedInteger:
        case SaveEditorModel::ValueKind::UnsignedInteger:
        case SaveEditorModel::ValueKind::Float:
        case SaveEditorModel::ValueKind::Double: RenderScalar(node); break;
        case SaveEditorModel::ValueKind::Enum: RenderEnumValue(node); break;
        case SaveEditorModel::ValueKind::Name: RenderTextValue(node, "Enter a name"); break;
        case SaveEditorModel::ValueKind::String: RenderTextValue(node, "Enter text"); break;
        case SaveEditorModel::ValueKind::Text: RenderTextValue(node, "Enter the text shown in game"); break;
        case SaveEditorModel::ValueKind::ObjectReference:
        case SaveEditorModel::ValueKind::ClassReference:
        case SaveEditorModel::ValueKind::SoftObjectReference:
        case SaveEditorModel::ValueKind::SoftClassReference:
            RenderTextValue(node, "Enter a /Game/... path, or leave blank");
            break;
        default: break;
    }

    bool isDirty = SaveEditorModel::IsDirty(node);
    if (isDirty) {
        (void)GuiUtils::SameLineIfFitsButton("Reset");
        if (ImGui::SmallButton("Reset")) {
            node.value = node.originalValue;
            isDirty = false;
        }
    }
    return static_cast<std::ptrdiff_t>(isDirty) - static_cast<std::ptrdiff_t>(wasDirty);
}

void SaveEditorSection::RenderScalar(SaveEditorModel::ValueNode& node) {
    ImGui::TextWrapped("%s", node.displayName.c_str());
    if (node.kind == SaveEditorModel::ValueKind::Boolean) {
        auto& value = std::get<bool>(node.value);
        (void)ImGui::Checkbox(value ? "True###Value" : "False###Value", &value);
        return;
    }

    GuiUtils::SetNextFieldWidth(SAVE_SCALAR_WIDTH);
    if (node.kind == SaveEditorModel::ValueKind::SignedInteger) {
        auto& integer = std::get<std::int64_t>(node.value);
        const bool changed = ImGui::InputScalar("##Value", ImGuiDataType_S64, &integer, nullptr, nullptr, "%lld");
        if (changed) {
            if (node.numericBits == 8)
                integer = std::clamp<std::int64_t>(
                    integer, (std::numeric_limits<std::int8_t>::min)(), (std::numeric_limits<std::int8_t>::max)()
                );
            else if (node.numericBits == 16)
                integer = std::clamp<std::int64_t>(
                    integer, (std::numeric_limits<std::int16_t>::min)(), (std::numeric_limits<std::int16_t>::max)()
                );
            else if (node.numericBits == 32)
                integer = std::clamp<std::int64_t>(
                    integer, (std::numeric_limits<std::int32_t>::min)(), (std::numeric_limits<std::int32_t>::max)()
                );
        }
        return;
    }
    if (node.kind == SaveEditorModel::ValueKind::UnsignedInteger) {
        auto& integer = std::get<std::uint64_t>(node.value);
        const bool changed = ImGui::InputScalar("##Value", ImGuiDataType_U64, &integer, nullptr, nullptr, "%llu");
        if (changed) {
            if (node.numericBits == 8)
                integer = (std::min)(integer, static_cast<std::uint64_t>((std::numeric_limits<std::uint8_t>::max)()));
            else if (node.numericBits == 16)
                integer = (std::min)(integer, static_cast<std::uint64_t>((std::numeric_limits<std::uint16_t>::max)()));
            else if (node.numericBits == 32)
                integer = (std::min)(integer, static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)()));
        }
        return;
    }
    auto& value = std::get<double>(node.value);
    if (node.kind == SaveEditorModel::ValueKind::Float) {
        auto floatValue = static_cast<float>(value);
        if (!ImGui::InputFloat("##Value", &floatValue, 0.0f, 0.0f, "%.9g")) return;
        if (!std::isfinite(floatValue)) {
            value = std::get<double>(node.originalValue);
            status.SetError("Enter a valid number. This field was reset to its saved value.");
            return;
        }
        value = static_cast<double>(floatValue);
        return;
    }
    const bool changed = ImGui::InputDouble("##Value", &value, 0.0, 0.0, "%.17g");
    if (changed && !std::isfinite(value)) {
        value = std::get<double>(node.originalValue);
        status.SetError("Enter a valid number. This field was reset to its saved value.");
    }
}

void SaveEditorSection::RenderTextValue(SaveEditorModel::ValueNode& node, const char* hint) {
    ImGui::TextWrapped("%s", node.displayName.c_str());
    GuiUtils::SetNextFieldWidth(SAVE_TEXT_WIDTH);
    (void)InputTextString("##Value", hint, std::get<std::string>(node.value));
}

void SaveEditorSection::RenderEnumValue(SaveEditorModel::ValueNode& node) {
    auto& value = std::get<std::int64_t>(node.value);
    const auto selected = std::ranges::find(node.enumOptions, value, &SaveEditorModel::EnumOption::value);
    std::array<char, 64> unknownValue{};
    const char* preview = nullptr;
    if (selected != node.enumOptions.end()) {
        preview = selected->displayName.c_str();
    } else {
        (void)std::snprintf(unknownValue.data(), unknownValue.size(), "Unknown (%lld)", static_cast<long long>(value));
        preview = unknownValue.data();
    }
    ImGui::TextWrapped("%s", node.displayName.c_str());
    if (!GuiUtils::BeginSizedCombo("##Value", preview, SAVE_ENUM_WIDTH)) return;
    for (const auto& option : node.enumOptions) {
        const bool current = option.value == value;
        if (ImGui::Selectable(option.displayName.c_str(), current)) value = option.value;
        if (current) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
}

void SaveEditorSection::RenderSaveActions(std::size_t visibleDirtyValueCount, const char* saveLabel) {
    if (!document) return;

    const bool saveDisabled = visibleDirtyValueCount == 0 || !classMismatchWarning.empty();
    ImGui::BeginDisabled(saveDisabled);
    if (GuiUtils::Button(saveLabel, GuiUtils::ButtonTone::Primary)) {
        pendingSaveCopy = false;
        pendingTargetChanged = false;
        pendingTargetSlot = document->sourceSlot;
        OpenPanel(Panel::ConfirmSave);
    }
    ImGui::EndDisabled();
    if (!classMismatchWarning.empty())
        GuiUtils::HelpTooltip("This file contains the wrong save type. Save a copy instead.");

    if (visibleDirtyValueCount > 0) {
        (void)GuiUtils::SameLineIfFitsButton("Discard Changes");
        if (GuiUtils::Button("Discard Changes", GuiUtils::ButtonTone::Quiet)) OpenPanel(Panel::DiscardAll);
    }

    (void)GuiUtils::SameLineIfFitsButton("Save a copy...");
    if (GuiUtils::Button("Save a copy...", GuiUtils::ButtonTone::Quiet)) OpenPanel(Panel::SaveCopy);
    (void)GuiUtils::SameLineIfFitsButton("Backups...");
    if (GuiUtils::Button("Backups...", GuiUtils::ButtonTone::Quiet)) {
        backupsNeedRefresh = true;
        OpenPanel(Panel::Backups);
    }
}

void SaveEditorSection::RenderPresetPanel() {
    if (!document || !pendingPresetNode ||
        pendingPresetNode->presetTarget == SaveEditorModel::PresetTargetKind::None) {
        ImGui::TextWrapped("This item is no longer available.");
        return;
    }

    const char* targetLabel = pendingPresetNode->displayName.empty() ? pendingPresetNode->rawName.c_str()
                                                                    : pendingPresetNode->displayName.c_str();
    ImGui::TextWrapped("Apply a preset to %s", targetLabel);
    const bool armor = pendingPresetNode->presetTarget == SaveEditorModel::PresetTargetKind::ArmorPassport;
    if (armor)
        armorPresetPicker.Render("Armor preset", "Choose a preset");
    else
        weaponPresetPicker.Render("Weapon preset", "Choose a preset");

    const bool canApply = armor ? armorPresetPicker.HasSelection() : weaponPresetPicker.HasSelection();
    ImGui::BeginDisabled(!canApply || operationPending);
    if (GuiUtils::Button("Apply preset", GuiUtils::ButtonTone::Primary)) {
        const auto nodeId = pendingPresetNode->id;
        const auto kind = pendingPresetNode->presetTarget;
        if (armor)
            QueueArmorPreset(nodeId, kind);
        else
            QueueWeaponPreset(nodeId, kind);
        ClosePanel();
    }
    ImGui::EndDisabled();
    (void)GuiUtils::SameLineIfFitsButton("Cancel");
    if (GuiUtils::Button("Cancel", GuiUtils::ButtonTone::Quiet)) ClosePanel();
}

void SaveEditorSection::RenderBackupPanel() {
    if (!document) {
        ImGui::TextWrapped("Open a save file first.");
        return;
    }
    if (backupsNeedRefresh) RefreshBackups();

    if (GuiUtils::CheckboxWithTooltip(
            "Create backups", &backupsEnabled, "Keeps the current version before an existing save is replaced."
        )) {
        auto& config = ConfigManager::Get();
        config.SetBool(BACKUP_CONFIG_SECTION, "backups_enabled", backupsEnabled);
        config.SaveConfig();
    }

    ImGui::BeginDisabled(!backupsEnabled);
    ImGui::TextUnformatted("Keep backups");
    const auto& selectedRetention = RETENTION_OPTIONS[backupRetentionIndex];
    if (GuiUtils::BeginSizedCombo("##BackupRetention", selectedRetention.label, SAVE_COMBO_WIDTH)) {
        for (std::size_t index = 0; index < RETENTION_OPTIONS.size(); ++index) {
            const auto& option = RETENTION_OPTIONS[index];
            const bool selected = index == backupRetentionIndex;
            if (ImGui::Selectable(option.label, selected)) {
                backupRetentionIndex = static_cast<std::uint8_t>(index);
                auto& config = ConfigManager::Get();
                config.SetInt(BACKUP_CONFIG_SECTION, "backup_retention", option.count);
                config.SaveConfig();
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Text("%zu %s", backups.size(), backups.size() == 1 ? "backup" : "backups");
    if (!latestBackupTimestamp.empty()) ImGui::TextDisabled("Latest: %s", latestBackupTimestamp.c_str());
    if (!backupCatalogError.empty())
        GuiUtils::RenderCallout("save-editor-backup-error", backupCatalogError, GuiUtils::CalloutTone::Error);

    ImGui::BeginDisabled(backups.empty() || operationPending);
    if (GuiUtils::Button("Restore latest", GuiUtils::ButtonTone::Primary)) {
        pendingRestorePath = backups.back().path;
        panel = Panel::Restore;
    }
    ImGui::EndDisabled();
    (void)GuiUtils::SameLineIfFitsButton("Open folder");
    if (GuiUtils::Button("Open folder")) {
        const auto& backupRoot = BackupRootDirectory();
        if (!IsBackupRootPath(SaveGameDirectory(), backupRoot) || !PresetUtils::OpenInExplorer(backupRoot))
            backupCatalogError = "Windows could not open the backup folder. Try again.";
    }
    ImGui::Spacing();
    ImGui::TextUnformatted("Delete oldest");
    GuiUtils::SetNextFieldWidth(BACKUP_COUNT_WIDTH);
    if (ImGui::InputInt("##BackupsToDelete", &backupsToDelete)) {
        const int maximumCount = (std::max)(1, static_cast<int>(backups.size()));
        backupsToDelete = std::clamp(backupsToDelete, 1, maximumCount);
    }
    (void)GuiUtils::SameLineIfFitsButton("Delete");
    ImGui::BeginDisabled(backups.empty() || operationPending);
    if (GuiUtils::Button("Delete", GuiUtils::ButtonTone::Danger)) {
        pendingDeleteCount = (std::min)(static_cast<std::size_t>(backupsToDelete), backups.size());
        panel = Panel::DeleteOldest;
    }
    (void)GuiUtils::SameLineIfFitsButton("Delete all");
    if (GuiUtils::Button("Delete all", GuiUtils::ButtonTone::Danger)) {
        pendingDeleteCount = backups.size();
        panel = Panel::DeleteAll;
    }
    ImGui::EndDisabled();
}

void SaveEditorSection::RenderSaveCopyPanel() {
    ImGui::TextUnformatted("Copy name");
    GuiUtils::SetNextFieldWidth(SAVE_TEXT_WIDTH);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::InputText("##SaveCopyName", copySlotName, sizeof(copySlotName));
    if (!panelError.empty())
        GuiUtils::RenderCallout("save-editor-copy-error", panelError, GuiUtils::CalloutTone::Error);

    if (GuiUtils::Button("Continue", GuiUtils::ButtonTone::Primary)) {
        std::string validationError;
        if (!ValidateSaveTarget(copySlotName, validationError)) {
            panelError = std::move(validationError);
        } else {
            bool targetExists = false;
            if (!TrySaveFileExists(copySlotName, targetExists, validationError)) {
                panelError = std::move(validationError);
            } else {
                pendingSaveCopy = true;
                pendingCopyTargetExists = targetExists;
                pendingTargetChanged = false;
                pendingTargetSlot = copySlotName;
                panelError.clear();
                panel = Panel::ConfirmSave;
            }
        }
    }
    (void)GuiUtils::SameLineIfFitsButton("Cancel");
    if (GuiUtils::Button("Cancel", GuiUtils::ButtonTone::Quiet)) ClosePanel();
}

void SaveEditorSection::RenderPanel() {
    if (panel == Panel::None) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float availableWidth = (std::max)(1.0f, viewport->WorkSize.x - SAVE_EDITOR_PANEL_VIEWPORT_MARGIN);
    const float maximumHeight = (std::max)(1.0f, viewport->WorkSize.y - SAVE_EDITOR_PANEL_VIEWPORT_MARGIN);
    if (panelOpenRequested) {
        ImGui::OpenPopup(SAVE_EDITOR_PANEL);
        panelOpenRequested = false;
    }

    ImGui::SetNextWindowPos(panelAnchor, ImGuiCond_Always, panelPivot);
    const float minimumWidth =
        (std::min)(availableWidth, SAVE_TEXT_WIDTH.preferred + GuiUtils::K_POPUP_PADDING.x * 2.0f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(minimumWidth, 0.0f), ImVec2(availableWidth, maximumHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, GuiUtils::K_POPUP_PADDING);
    if (!ImGui::BeginPopup(SAVE_EDITOR_PANEL, PANEL_FLAGS)) {
        ImGui::PopStyleVar();
        ResetPanel();
        return;
    }

    switch (panel) {
        case Panel::None: break;
        case Panel::Preset: RenderPresetPanel(); break;
        case Panel::Backups: RenderBackupPanel(); break;
        case Panel::SaveCopy: RenderSaveCopyPanel(); break;
        case Panel::DiscardOpen: {
            ImGui::TextWrapped("Discard your unsaved changes and open '%s'?", pendingOpenSlot.c_str());
            ImGui::BeginDisabled(pendingOpenSlot.empty());
            if (GuiUtils::Button("Discard and Open", GuiUtils::ButtonTone::Danger)) {
                QueueLoad(std::exchange(pendingOpenSlot, {}));
                ClosePanel();
            }
            ImGui::EndDisabled();
            (void)GuiUtils::SameLineIfFitsButton("Cancel");
            if (GuiUtils::Button("Cancel", GuiUtils::ButtonTone::Quiet)) ClosePanel();
            break;
        }
        case Panel::DiscardAll: {
            ImGui::TextWrapped("Discard %zu unsaved %s?", dirtyValueCount, dirtyValueCount == 1 ? "change" : "changes");
            if (GuiUtils::Button("Discard Changes", GuiUtils::ButtonTone::Danger)) {
                if (document) {
                    SaveEditorModel::ResetChanges(document->root);
                    dirtyValueCount = 0;
                    RebuildFilterMatches();
                }
                ClosePanel();
            }
            (void)GuiUtils::SameLineIfFitsButton("Cancel");
            if (GuiUtils::Button("Cancel", GuiUtils::ButtonTone::Quiet)) ClosePanel();
            break;
        }
        case Panel::ConfirmSave: {
            if (pendingSaveCopy && pendingCopyTargetExists) {
                ImGui::TextWrapped("'%s' already exists. Replace it?", pendingTargetSlot.c_str());
                ImGui::TextDisabled(
                    "%s", backupsEnabled ? "Its current version will be kept in Backups."
                                         : "This cannot be undone after saving finishes."
                );
            } else if (pendingSaveCopy) {
                ImGui::TextWrapped("Create a copy named '%s'?", pendingTargetSlot.c_str());
            } else {
                ImGui::TextWrapped("Save %zu %s?", dirtyValueCount, dirtyValueCount == 1 ? "change" : "changes");
            }
            if (pendingTargetChanged) {
                GuiUtils::RenderCallout(
                    "save-editor-target-changed",
                    "This save file changed. Review the updated action before confirming again.",
                    GuiUtils::CalloutTone::Warning
                );
            }
            if (!panelError.empty())
                GuiUtils::RenderCallout("save-editor-panel-error", panelError, GuiUtils::CalloutTone::Error);
            ImGui::Spacing();
            const char* saveLabel =
                pendingSaveCopy ? (pendingCopyTargetExists ? "Replace" : "Create Copy") : "Save Changes";
            const auto saveTone = pendingSaveCopy && pendingCopyTargetExists ? GuiUtils::ButtonTone::Danger
                                                                             : GuiUtils::ButtonTone::Primary;
            if (GuiUtils::Button(saveLabel, saveTone)) {
                std::string validationError;
                panelError.clear();
                if (!ValidateSaveTarget(pendingTargetSlot, validationError)) {
                    panelError = std::move(validationError);
                } else if (pendingSaveCopy) {
                    bool targetExists = false;
                    if (!TrySaveFileExists(pendingTargetSlot, targetExists, validationError)) {
                        panelError = std::move(validationError);
                    } else if (targetExists != pendingCopyTargetExists) {
                        pendingCopyTargetExists = targetExists;
                        pendingTargetChanged = true;
                    } else {
                        QueueSave(pendingTargetSlot, pendingCopyTargetExists);
                        ClosePanel();
                    }
                } else {
                    QueueSave(pendingTargetSlot);
                    ClosePanel();
                }
            }
            (void)GuiUtils::SameLineIfFitsButton("Cancel");
            if (GuiUtils::Button("Cancel", GuiUtils::ButtonTone::Quiet)) ClosePanel();
            break;
        }
        case Panel::Restore: {
            ImGui::TextWrapped("Replace the current save with its latest backup?");
            if (dirtyValueCount > 0) ImGui::TextDisabled("Your unsaved changes will be discarded.");
            ImGui::TextDisabled(
                "%s", backupsEnabled ? "The current version will be kept as a new backup."
                                     : "This cannot be undone after restoring finishes."
            );
            ImGui::Spacing();
            ImGui::BeginDisabled(pendingRestorePath.empty() || operationPending);
            if (GuiUtils::Button("Restore backup", GuiUtils::ButtonTone::Danger)) {
                QueueRestore(std::exchange(pendingRestorePath, {}));
                ClosePanel();
            }
            ImGui::EndDisabled();
            (void)GuiUtils::SameLineIfFitsButton("Cancel");
            if (GuiUtils::Button("Cancel", GuiUtils::ButtonTone::Quiet)) {
                pendingRestorePath.clear();
                panel = Panel::Backups;
            }
            break;
        }
        case Panel::DeleteOldest:
        case Panel::DeleteAll: {
            const bool deletingAll = panel == Panel::DeleteAll;
            if (deletingAll) {
                ImGui::TextWrapped(
                    "Delete all %zu %s?", pendingDeleteCount, pendingDeleteCount == 1 ? "backup" : "backups"
                );
            } else {
                ImGui::TextWrapped(
                    "Delete the %zu oldest %s?", pendingDeleteCount, pendingDeleteCount == 1 ? "backup" : "backups"
                );
            }
            ImGui::TextDisabled("Deleted backups cannot be recovered.");
            ImGui::Spacing();
            ImGui::BeginDisabled(operationPending || pendingDeleteCount == 0);
            if (GuiUtils::Button("Delete", GuiUtils::ButtonTone::Danger)) {
                std::string deleteError;
                const bool success = document &&
                                     DeleteBackups(document->sourceSlot, backups, pendingDeleteCount, deleteError);
                if (!document) deleteError = "Open a save file first.";
                backupsNeedRefresh = true;
                RefreshBackups();
                if (!success && backupCatalogError.empty()) backupCatalogError = deleteError;
                pendingDeleteCount = 0;
                panel = Panel::Backups;
            }
            ImGui::EndDisabled();
            (void)GuiUtils::SameLineIfFitsButton("Cancel");
            if (GuiUtils::Button("Cancel", GuiUtils::ButtonTone::Quiet)) {
                pendingDeleteCount = 0;
                panel = Panel::Backups;
            }
            break;
        }
    }
    ImGui::EndPopup();
    ImGui::PopStyleVar();
}

void SaveEditorSection::Render() {
    DrainResults();
    if (slotsNeedRefresh) RefreshSlots();

    RenderSaveSelector();
    if (!classMismatchWarning.empty())
        GuiUtils::RenderCallout(
            "save-editor-class-mismatch", classMismatchWarning.c_str(), GuiUtils::CalloutTone::Error
        );
    status.Render();
    ImGui::BeginDisabled(operationPending);
    const std::size_t footerDirtyValueCount = dirtyValueCount;
    const auto saveLabel = SaveChangesLabel(footerDirtyValueCount);
    RenderValues(footerDirtyValueCount, saveLabel.data());
    RenderSaveActions(footerDirtyValueCount, saveLabel.data());
    ImGui::EndDisabled();
    RenderPanel();
}
