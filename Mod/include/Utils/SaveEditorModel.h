#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace SDK {
    struct FStr_Passport_Armor1;
    struct FStr_Passport_Weapon1;
    class UWorld;
}

namespace SaveEditorModel {

    using NodeId = std::uint64_t;

    enum class ValueKind : std::uint8_t {
        Boolean,
        SignedInteger,
        UnsignedInteger,
        Float,
        Double,
        Enum,
        Name,
        String,
        Text,
        ObjectReference,
        ClassReference,
        SoftObjectReference,
        SoftClassReference,
        Struct,
        Array,
        Map,
        MapEntry,
        Set,
        Unsupported,
    };

    enum class ReadOnlyReason : std::uint8_t {
        None,
        NotSaved,
        Deprecated,
        UnsafeContainer,
        MissingMetadata,
        UnsupportedType,
        Unverified,
    };

    enum class PresetTargetKind : std::uint8_t {
        None,
        WeaponPassport,
        WeaponParts,
        ArmorPassport,
    };

    using ScalarValue = std::variant<std::monostate, bool, std::int64_t, std::uint64_t, double, std::string>;

    struct EnumOption {
        std::int64_t value = 0;
        std::string displayName;
    };

    struct ValueNode {
        NodeId id = 0;
        std::string rawName;
        std::string displayName;
        ValueKind kind = ValueKind::Unsupported;
        ReadOnlyReason readOnlyReason = ReadOnlyReason::None;
        PresetTargetKind presetTarget = PresetTargetKind::None;
        std::uint8_t numericBits = 0;
        bool editable = false;
        bool persisted = true;
        bool hasPersistentUnsupported = false;
        bool matchesFilter = false;
        ScalarValue value;
        ScalarValue originalValue;
        std::vector<EnumOption> enumOptions;
        std::vector<ValueNode> children;
    };

    // Keep the per-node inline budget small; reflected documents may contain hundreds of thousands of nodes.
    static_assert(sizeof(ValueNode) <= 240);

    struct Document {
        std::string sourceSlot;
        std::string classPath;
        int userIndex = 0;
        ValueNode root;
    };

    struct LoadResult {
        bool success = false;
        Document document;
        std::string error;
    };

    struct SaveResult {
        bool success = false;
        bool liveStateSynchronized = false;
        Document document;
        std::string error;
    };

    struct LiveSyncResult {
        bool success = true;
        std::string error;
    };

    // Unreal-facing operations. Call these only from the game thread.
    [[nodiscard]] LoadResult LoadDocument(std::string_view slot, int userIndex = 0);
    [[nodiscard]] SaveResult SaveDocumentAndSynchronize(
        const Document& document, std::string_view targetSlot, SDK::UWorld* world
    );
    // The backup document's source slot must remain available for the duration of this game-thread call.
    [[nodiscard]] SaveResult RestoreDocumentAndSynchronize(
        const Document& backupDocument, std::string_view targetSlot, SDK::UWorld* world
    );
    [[nodiscard]] LiveSyncResult FlushLiveState(
        const Document& document, std::string_view targetSlot, SDK::UWorld* world
    );

    // Snapshots an approved preset UScriptStruct into an Unreal-free tree. Call on the game thread.
    [[nodiscard]] ValueNode SnapshotStruct(const SDK::FStr_Passport_Weapon1& value, std::string& error);
    [[nodiscard]] ValueNode SnapshotStruct(const SDK::FStr_Passport_Armor1& value, std::string& error);

    // Pure tree operations; these are safe to use from the render thread.
    [[nodiscard]] bool ValidateSlotName(std::string_view slot, std::string& error);
    [[nodiscard]] bool OverlayValues(ValueNode& target, const ValueNode& source, std::string& error);
    [[nodiscard]] ValueNode* FindNode(Document& document, NodeId id);
    [[nodiscard]] bool IsDirty(const ValueNode& node);
    [[nodiscard]] std::size_t CountDirty(const ValueNode& node);
    void ResetChanges(ValueNode& node);

    [[nodiscard]] std::string_view ExpectedClassForSlot(std::string_view slot);
    [[nodiscard]] bool ClassPathMatches(std::string_view classPath, std::string_view expectedClass);
    [[nodiscard]] bool IsProgressSlot(std::string_view slot);

} // namespace SaveEditorModel
