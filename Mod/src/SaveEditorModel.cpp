#include "Utils/SaveEditorModel.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <Windows.h>

#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/GI_Settings_classes.hpp"
#include "SDK/SG_GameProgress_classes.hpp"
#include "SDK/Str_Passport_Armor1_structs.hpp"
#include "SDK/Str_Passport_Weapon1_structs.hpp"

namespace SaveEditorModel {
    namespace {

        constexpr std::size_t K_MAX_CONTAINER_ELEMENTS = 100'000;
        constexpr std::size_t K_MAX_SNAPSHOT_NODES = 250'000;
        constexpr std::size_t K_MAX_REFLECTION_DEPTH = 64;
        constexpr std::string_view K_PROGRESS_SLOT = "GameProgress";
        constexpr std::string_view K_PROGRESS_CLASS = "SG_GameProgress_C";
        constexpr wchar_t K_PROGRESS_SLOT_WIDE[] = L"GameProgress";
        constexpr NodeId K_FNV_OFFSET = 14695981039346656037ULL;
        constexpr NodeId K_FNV_PRIME = 1099511628211ULL;
        constexpr NodeId K_ROOT_NODE_ID = K_FNV_OFFSET;

        enum class NodeIdSegment : std::uint8_t {
            Property,
            ArrayElement,
            MapEntry,
            MapKey,
            MapValue,
            SetElement,
            RebasedChild,
        };

        [[nodiscard]] NodeId HashBytes(NodeId hash, const void* bytes, std::size_t size) noexcept {
            const auto* characters = static_cast<const unsigned char*>(bytes);
            for (std::size_t index = 0; index < size; ++index) {
                hash ^= characters[index];
                hash *= K_FNV_PRIME;
            }
            return hash;
        }

        template <typename ValueType> [[nodiscard]] NodeId HashValue(NodeId hash, const ValueType& value) noexcept {
            static_assert(std::is_trivially_copyable_v<ValueType>);
            return HashBytes(hash, &value, sizeof(value));
        }

        [[nodiscard]] NodeId MakeNodeId(NodeId parent, NodeIdSegment segment, std::uint64_t discriminator) noexcept {
            auto hash = HashValue(K_FNV_OFFSET, parent);
            hash = HashValue(hash, segment);
            return HashValue(hash, discriminator);
        }

        struct RawArrayHeader {
            void* data = nullptr;
            std::int32_t num = 0;
            std::int32_t max = 0;
        };

        struct RawSparseHeader {
            void* data = nullptr;
            std::int32_t numAllocated = 0;
            std::int32_t maxAllocated = 0;
            std::uint32_t inlineAllocationFlags[4]{};
            const std::uint32_t* secondaryAllocationFlags = nullptr;
            std::int32_t numBits = 0;
            std::int32_t maxBits = 0;
            std::int32_t firstFreeIndex = -1;
            std::int32_t numFreeIndices = 0;
        };

        struct SoftClassPropertyLayout : SDK::FObjectPropertyBase {
            SDK::UClass* metaClass = nullptr;
        };

        static_assert(sizeof(RawArrayHeader) == 0x10);
        static_assert(sizeof(RawSparseHeader) == 0x38);
        static_assert(sizeof(SDK::TArray<std::int32_t>) == 0x10);
        static_assert(sizeof(SDK::TSet<std::int32_t>) == 0x50);
        static_assert(sizeof(SDK::TMap<std::int32_t, std::int32_t>) == 0x50);
        static_assert(sizeof(SDK::FObjectPropertyBase) == 0x78);
        static_assert(sizeof(SoftClassPropertyLayout) == 0x80);
        static_assert(sizeof(SDK::FText) == 0x10);
        static_assert(sizeof(SDK::FStr_Passport_Weapon1) == 0x100);
        static_assert(sizeof(SDK::FStr_Passport_Armor1) == 0xD8);
        static_assert(alignof(SDK::FStr_Passport_Weapon1) == 0x8);
        static_assert(alignof(SDK::FStr_Passport_Armor1) == 0x8);
        static_assert(std::is_trivially_move_assignable_v<SDK::FText>);
        static_assert(std::is_trivially_destructible_v<SDK::FText>);

        struct ValueLocation {
            SDK::FProperty* property = nullptr;
            void* address = nullptr;
            bool directOwnerProperty = false;
        };

        using LocationMap = std::unordered_map<NodeId, ValueLocation>;

        struct SnapshotBudget {
            std::size_t remainingNodes = K_MAX_SNAPSHOT_NODES;
        };

        struct SnapshotContext {
            SDK::UObject* owner = nullptr;
            LocationMap* locations = nullptr;
            SnapshotBudget* budget = nullptr;
            std::string error;
        };

        [[nodiscard]] bool HasCastFlag(const SDK::FField* field, SDK::EClassCastFlags flag) {
            if (!field || !field->ClassPrivate) return false;
            const auto mask = static_cast<std::uint64_t>(flag);
            return (field->ClassPrivate->CastFlags & mask) == mask;
        }

        [[nodiscard]] bool ScalarEqual(const ScalarValue& left, const ScalarValue& right) {
            if (left.index() != right.index()) return false;
            if (const auto* leftDouble = std::get_if<double>(&left)) {
                const auto rightDouble = std::get<double>(right);
                return (*leftDouble == rightDouble) || (std::isnan(*leftDouble) && std::isnan(rightDouble));
            }
            return left == right;
        }

        [[nodiscard]] constexpr bool IsContainerKind(ValueKind kind) noexcept {
            return kind == ValueKind::Array || kind == ValueKind::Map || kind == ValueKind::Set;
        }

        [[nodiscard]] constexpr bool IsUnorderedContainerKind(ValueKind kind) noexcept {
            return kind == ValueKind::Map || kind == ValueKind::Set;
        }

        [[nodiscard]] std::string DisplayName(std::string_view rawName) {
            const auto hashSeparator = rawName.rfind('_');
            if (hashSeparator != std::string_view::npos && rawName.size() - hashSeparator - 1 == 32) {
                const auto hash = rawName.substr(hashSeparator + 1);
                if (std::ranges::all_of(hash, [](unsigned char value) { return std::isxdigit(value) != 0; })) {
                    const auto numberSeparator = rawName.rfind('_', hashSeparator - 1);
                    if (numberSeparator != std::string_view::npos && numberSeparator + 1 != hashSeparator) {
                        const auto number =
                            rawName.substr(numberSeparator + 1, hashSeparator - numberSeparator - 1);
                        if (std::ranges::all_of(number, [](unsigned char value) { return std::isdigit(value) != 0; }))
                            rawName = rawName.substr(0, numberSeparator);
                    }
                }
            }
            std::string result;
            result.reserve(rawName.size() + 8);
            char previous = 0;
            for (const char current : rawName) {
                if (current == '_') {
                    if (!result.empty() && result.back() != ' ') result.push_back(' ');
                    previous = current;
                    continue;
                }
                const bool camelBoundary = previous != 0 && previous != '_' &&
                                           std::islower(static_cast<unsigned char>(previous)) != 0 &&
                                           std::isupper(static_cast<unsigned char>(current)) != 0;
                if (camelBoundary && !result.empty() && result.back() != ' ') result.push_back(' ');
                result.push_back(current);
                previous = current;
            }
            while (!result.empty() && result.back() == ' ')
                result.pop_back();
            return result.empty() ? std::string(rawName) : result;
        }

        [[nodiscard]] std::string HexValue(std::uint64_t value) {
            constexpr std::string_view HEX = "0123456789ABCDEF";
            std::array<char, 16> buffer{};
            auto current = buffer.end();
            do {
                *--current = HEX[value & 0x0F];
                value >>= 4;
            } while (value != 0);
            return {current, buffer.end()};
        }

        [[nodiscard]] std::string ObjectPath(const SDK::UObject* object) {
            if (!object) return {};
            std::vector<std::string> components;
            components.push_back(object->GetName());
            for (auto* outer = object->Outer; outer; outer = outer->Outer) {
                components.push_back(!outer->Outer ? outer->Name.GetRawString() : outer->GetName());
            }

            std::size_t size = components.size() - 1;
            for (const auto& component : components)
                size += component.size();
            std::string result;
            result.reserve(size);
            for (auto component = components.rbegin(); component != components.rend(); ++component) {
                if (!result.empty()) result.push_back('.');
                result.append(*component);
            }
            return result;
        }

        [[nodiscard]] bool TryUtf8ToWide(std::string_view value, std::wstring& wide, std::string& error) {
            if (value.find('\0') != std::string_view::npos) {
                error = "Text contains an embedded null character";
                return false;
            }
            if (value.empty()) {
                wide.clear();
                return true;
            }
            if (value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
                error = "Text is not valid UTF-8";
                return false;
            }
            const auto wideLength = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0
            );
            if (wideLength <= 0) {
                error = "Text is not valid UTF-8";
                return false;
            }
            wide.resize(static_cast<std::size_t>(wideLength));
            if (MultiByteToWideChar(
                    CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), wide.data(), wideLength
                ) != wideLength) {
                wide.clear();
                error = "Text is not valid UTF-8";
                return false;
            }
            return true;
        }

        [[nodiscard]] bool AsciiIEquals(std::string_view left, std::string_view right) {
            return left.size() == right.size() &&
                   std::ranges::equal(left, right, [](unsigned char first, unsigned char second) {
                       return std::tolower(first) == std::tolower(second);
                   });
        }

        [[nodiscard]] bool IsValidSlot(std::string_view slot, std::wstring& wide, std::string& error) {
            if (slot.empty() || slot.size() > 220 || slot == "." || slot == "..") {
                error = "Enter a name between 1 and 220 characters";
                return false;
            }
            if (slot.front() == ' ' || slot.back() == ' ' || slot.back() == '.') {
                error = "The name cannot start or end with a space, or end with a period";
                return false;
            }
            constexpr std::string_view FORBIDDEN = "<>:\"/\\|?*";
            for (const unsigned char value : slot) {
                if (value < 32 || FORBIDDEN.find(static_cast<char>(value)) != std::string_view::npos) {
                    error = "The name cannot contain any of these characters: < > : \" / \\ | ? *";
                    return false;
                }
            }
            const auto deviceBase = slot.substr(0, slot.find('.'));
            const bool numberedDevice = deviceBase.size() == 4 &&
                                        (AsciiIEquals(deviceBase.substr(0, 3), "COM") ||
                                         AsciiIEquals(deviceBase.substr(0, 3), "LPT")) &&
                                        deviceBase.back() >= '1' && deviceBase.back() <= '9';
            if (AsciiIEquals(deviceBase, "CON") || AsciiIEquals(deviceBase, "PRN") ||
                AsciiIEquals(deviceBase, "AUX") || AsciiIEquals(deviceBase, "NUL") ||
                numberedDevice) {
                error = "Choose a different name; Windows reserves this one";
                return false;
            }
            return TryUtf8ToWide(slot, wide, error);
        }

        [[nodiscard]] std::string_view ObjectBaseName(std::string_view objectPath) {
            const auto separator = objectPath.rfind('.');
            return separator == std::string_view::npos ? objectPath : objectPath.substr(separator + 1);
        }

        [[nodiscard]] PresetTargetKind ClassifyPresetTarget(const SDK::UStruct* structure) {
            if (!structure) return PresetTargetKind::None;
            const auto name = structure->GetName();
            if (ClassPathMatches(name, "Str_Passport_Weapon1") || ClassPathMatches(name, "Str_Passport_Weapon1_C")) {
                return PresetTargetKind::WeaponPassport;
            }
            if (ClassPathMatches(name, "Str_WeaponParts") || ClassPathMatches(name, "Str_WeaponParts_C"))
                return PresetTargetKind::WeaponParts;
            if (ClassPathMatches(name, "Str_Passport_Armor1") || ClassPathMatches(name, "Str_Passport_Armor1_C")) {
                return PresetTargetKind::ArmorPassport;
            }
            return PresetTargetKind::None;
        }

        [[nodiscard]] bool IsReadableRange(const void* address, std::size_t size) {
            if (size == 0) return true;
            if (!address) return false;
            auto* current = static_cast<const std::byte*>(address);
            auto remaining = size;
            while (remaining > 0) {
                MEMORY_BASIC_INFORMATION information{};
                if (VirtualQuery(current, &information, sizeof(information)) == 0 || information.State != MEM_COMMIT ||
                    (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
                    return false;
                }
                const auto currentAddress = reinterpret_cast<std::uintptr_t>(current);
                const auto regionAddress = reinterpret_cast<std::uintptr_t>(information.BaseAddress);
                if (currentAddress < regionAddress || currentAddress - regionAddress >= information.RegionSize)
                    return false;
                const auto available = information.RegionSize - (currentAddress - regionAddress);
                const auto advance = (std::min)(remaining, available);
                if (advance == 0) return false;
                remaining -= advance;
                if (remaining > 0) current += advance;
            }
            return true;
        }

        [[nodiscard]] bool IsKnownClass(const SDK::UClass* candidate) {
            if (!IsReadableRange(candidate, sizeof(SDK::UObject))) return false;
            auto* objects = SDK::UObject::GObjects.GetTypedPtr();
            if (!objects) return false;
            const auto index = candidate->Index;
            return objects->GetByIndex(index) == candidate && candidate->HasTypeFlag(SDK::EClassCastFlags::Class);
        }

        [[nodiscard]] bool TrySoftClassMetaClass(
            SDK::FProperty* property, SDK::UClass*& metaClass, std::string& error
        ) {
            metaClass = nullptr;
            if (!property || !HasCastFlag(property, SDK::EClassCastFlags::SoftClassProperty) ||
                property->ClassPrivate->Name.GetRawString() != "SoftClassProperty" ||
                !IsReadableRange(property, sizeof(SoftClassPropertyLayout))) {
                error = "Soft-class property metadata layout cannot be verified";
                return false;
            }
            metaClass = reinterpret_cast<SoftClassPropertyLayout*>(property)->metaClass;
            if (!IsKnownClass(metaClass)) {
                error = "Soft-class constraint metadata is unavailable";
                metaClass = nullptr;
                return false;
            }
            return true;
        }

        [[nodiscard]] std::size_t AlignUp(std::size_t value, std::size_t alignment) {
            if (alignment <= 1) return value;
            return (value + alignment - 1) & ~(alignment - 1);
        }

        [[nodiscard]] std::size_t PropertyAlignment(const SDK::FProperty* property) {
            if (!property) return 1;
            if (HasCastFlag(property, SDK::EClassCastFlags::StructProperty)) {
                const auto* structProperty = static_cast<const SDK::FStructProperty*>(property);
                return structProperty->Struct && structProperty->Struct->MinAlignment > 0
                           ? static_cast<std::size_t>(structProperty->Struct->MinAlignment)
                           : 0;
            }
            if (HasCastFlag(property, SDK::EClassCastFlags::EnumProperty)) {
                const auto* enumProperty = static_cast<const SDK::FEnumProperty*>(property);
                return PropertyAlignment(enumProperty->UnderlayingProperty);
            }
            if (HasCastFlag(property, SDK::EClassCastFlags::Int8Property) ||
                HasCastFlag(property, SDK::EClassCastFlags::ByteProperty) ||
                HasCastFlag(property, SDK::EClassCastFlags::BoolProperty))
                return 1;
            if (HasCastFlag(property, SDK::EClassCastFlags::Int16Property) ||
                HasCastFlag(property, SDK::EClassCastFlags::UInt16Property))
                return 2;
            if (HasCastFlag(property, SDK::EClassCastFlags::IntProperty) ||
                HasCastFlag(property, SDK::EClassCastFlags::UInt32Property) ||
                HasCastFlag(property, SDK::EClassCastFlags::FloatProperty))
                return 4;
            if (HasCastFlag(property, SDK::EClassCastFlags::NameProperty)) return alignof(SDK::FName);
            if (HasCastFlag(property, SDK::EClassCastFlags::DoubleProperty) ||
                HasCastFlag(property, SDK::EClassCastFlags::Int64Property) ||
                HasCastFlag(property, SDK::EClassCastFlags::UInt64Property) ||
                HasCastFlag(property, SDK::EClassCastFlags::StrProperty) ||
                HasCastFlag(property, SDK::EClassCastFlags::TextProperty) ||
                HasCastFlag(property, SDK::EClassCastFlags::ObjectProperty) ||
                HasCastFlag(property, SDK::EClassCastFlags::ClassProperty) ||
                HasCastFlag(property, SDK::EClassCastFlags::ObjectPropertyBase) ||
                HasCastFlag(property, SDK::EClassCastFlags::SoftObjectProperty) ||
                HasCastFlag(property, SDK::EClassCastFlags::SoftClassProperty) ||
                HasCastFlag(property, SDK::EClassCastFlags::ArrayProperty) ||
                HasCastFlag(property, SDK::EClassCastFlags::MapProperty) ||
                HasCastFlag(property, SDK::EClassCastFlags::SetProperty))
                return 8;
            return 0;
        }

        [[nodiscard]] bool TryPropertyElementLayout(
            const SDK::FProperty* property, std::size_t& size, std::size_t& alignment
        ) {
            if (!property || property->ElementSize <= 0 || property->ArrayDim != 1) return false;
            size = static_cast<std::size_t>(property->ElementSize);
            alignment = PropertyAlignment(property);
            return alignment != 0 && alignment <= 64 && (alignment & (alignment - 1)) == 0 &&
                   size % alignment == 0;
        }

        template <typename ValueType> [[nodiscard]] ValueType ReadUnaligned(const void* address) {
            ValueType value{};
            std::memcpy(&value, address, sizeof(value));
            return value;
        }

        template <typename ValueType> void WriteUnaligned(void* address, ValueType value) {
            std::memcpy(address, &value, sizeof(value));
        }

        [[nodiscard]] bool ReadIntegral(
            const SDK::FProperty* property, const void* address, bool& isSigned, std::int64_t& signedValue,
            std::uint64_t& unsignedValue
        ) {
            if (HasCastFlag(property, SDK::EClassCastFlags::Int8Property)) {
                isSigned = true;
                const auto rawValue = ReadUnaligned<std::uint8_t>(address);
                signedValue =
                    rawValue < 0x80 ? static_cast<std::int64_t>(rawValue) : static_cast<std::int64_t>(rawValue) - 0x100;
            } else if (HasCastFlag(property, SDK::EClassCastFlags::Int16Property)) {
                isSigned = true;
                signedValue = ReadUnaligned<std::int16_t>(address);
            } else if (HasCastFlag(property, SDK::EClassCastFlags::IntProperty)) {
                isSigned = true;
                signedValue = ReadUnaligned<std::int32_t>(address);
            } else if (HasCastFlag(property, SDK::EClassCastFlags::Int64Property)) {
                isSigned = true;
                signedValue = ReadUnaligned<std::int64_t>(address);
            } else if (HasCastFlag(property, SDK::EClassCastFlags::ByteProperty)) {
                isSigned = false;
                unsignedValue = ReadUnaligned<std::uint8_t>(address);
            } else if (HasCastFlag(property, SDK::EClassCastFlags::UInt16Property)) {
                isSigned = false;
                unsignedValue = ReadUnaligned<std::uint16_t>(address);
            } else if (HasCastFlag(property, SDK::EClassCastFlags::UInt32Property)) {
                isSigned = false;
                unsignedValue = ReadUnaligned<std::uint32_t>(address);
            } else if (HasCastFlag(property, SDK::EClassCastFlags::UInt64Property)) {
                isSigned = false;
                unsignedValue = ReadUnaligned<std::uint64_t>(address);
            } else {
                return false;
            }
            return true;
        }

        [[nodiscard]] std::string UserDefinedEnumDisplay(
            SDK::UUserDefinedEnum* userDefinedEnum, const SDK::FName& valueName, std::string_view fullName,
            std::string_view shortName
        ) {
            if (!userDefinedEnum) return {};
            auto& displayMap = userDefinedEnum->DisplayNameMap;
            for (SDK::int32 index = 0; index < displayMap.NumAllocated(); ++index) {
                if (!displayMap.IsValidIndex(index)) continue;
                const auto& pair = displayMap[index];
                if (pair.Key() != valueName) {
                    const auto key = pair.Key().GetRawString();
                    if (key != fullName && key != shortName) continue;
                }
                const auto& text = pair.Value();
                if (!text.TextData) continue;
                auto display = text.ToString();
                if (!display.empty()) return display;
            }
            return {};
        }

        void AddEnumOptions(ValueNode& node, SDK::UEnum* enumObject) {
            if (!enumObject) return;
            node.enumOptions.reserve(static_cast<std::size_t>((std::max)(enumObject->Names.Num(), 0)));
            SDK::UUserDefinedEnum* userDefinedEnum = nullptr;
            if (enumObject->IsA(SDK::UUserDefinedEnum::StaticClass()))
                userDefinedEnum = static_cast<SDK::UUserDefinedEnum*>(enumObject);
            for (const auto& item : enumObject->Names) {
                const std::string name = item.Key().GetRawString();
                std::string_view shortName = name;
                if (const auto separator = shortName.rfind("::"); separator != std::string_view::npos)
                    shortName.remove_prefix(separator + 2);
                if (shortName == "MAX" || shortName.ends_with("_MAX")) continue;
                std::string displayName;
                if (item.Value() >= 0 && item.Value() <= 255) {
                    displayName = SDK::UKismetNodeHelperLibrary::GetEnumeratorUserFriendlyName(
                                      enumObject, static_cast<SDK::uint8>(item.Value())
                    ).ToString();
                }
                if (displayName.empty())
                    displayName = UserDefinedEnumDisplay(userDefinedEnum, item.Key(), name, shortName);
                if (displayName.empty()) {
                    constexpr std::string_view GENERATED_PREFIX = "NewEnumerator";
                    displayName = shortName.starts_with(GENERATED_PREFIX)
                                      ? "Option " + std::string(shortName.substr(GENERATED_PREFIX.size()))
                                      : DisplayName(shortName);
                }
                node.enumOptions.push_back(
                    EnumOption{
                        .value = item.Value(),
                        .displayName = std::move(displayName),
                    }
                );
            }
        }

        [[nodiscard]] bool HashNode(NodeId& hash, const ValueNode& node) {
            if (node.kind == ValueKind::Unsupported) return false;
            const auto kind = static_cast<std::uint8_t>(node.kind);
            hash = HashValue(hash, kind);
            const auto variantIndex = static_cast<std::uint8_t>(node.value.index());
            hash = HashValue(hash, variantIndex);
            std::visit(
                [&hash](const auto& value) {
                    using Type = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Type, std::string>) {
                        hash = HashValue(hash, static_cast<std::uint64_t>(value.size()));
                        hash = HashBytes(hash, value.data(), value.size());
                    } else if constexpr (std::is_same_v<Type, double>) {
                        const double canonical = std::isnan(value)
                                                     ? (std::numeric_limits<double>::quiet_NaN)()
                                                     : (value == 0.0 ? 0.0 : value);
                        hash = HashValue(hash, canonical);
                    } else if constexpr (!std::is_same_v<Type, std::monostate>) {
                        hash = HashValue(hash, value);
                    }
                },
                node.value
            );
            hash = HashValue(hash, static_cast<std::uint64_t>(node.children.size()));
            for (const auto& child : node.children) {
                hash = HashValue(hash, static_cast<std::uint64_t>(child.rawName.size()));
                hash = HashBytes(hash, child.rawName.data(), child.rawName.size());
                if (!HashNode(hash, child)) return false;
            }
            return true;
        }

        [[nodiscard]] bool TryNodeFingerprint(const ValueNode& node, NodeId& fingerprint) {
            fingerprint = K_FNV_OFFSET;
            return HashNode(fingerprint, node);
        }

        void RebaseNodeIds(ValueNode& node, NodeId id) {
            node.id = id;
            for (auto& child : node.children) {
                const auto relativeId = child.id;
                RebaseNodeIds(child, MakeNodeId(id, NodeIdSegment::RebasedChild, relativeId));
            }
        }

        void RegisterLocation(
            ValueNode& node, SDK::FProperty* property, void* address, SnapshotContext& context, std::size_t depth
        ) {
            if (!node.editable || !context.locations || !context.error.empty()) return;
            const bool directOwnerProperty =
                context.owner && depth == 1 && property->ArrayDim == 1 && property->Offset >= 0 &&
                address == static_cast<void*>(reinterpret_cast<std::byte*>(context.owner) + property->Offset);
            const auto [iterator, inserted] = context.locations->emplace(
                node.id, ValueLocation{
                             .property = property,
                             .address = address,
                             .directOwnerProperty = directOwnerProperty,
                         }
            );
            if (!inserted)
                context.error = "Reflection produced duplicate value id '" + HexValue(iterator->first) + "'";
        }

        [[nodiscard]] ValueNode SnapshotValue(
            SDK::FProperty* property, void* address, NodeId id, std::string rawName, std::string displayName,
            bool editableAllowed, ReadOnlyReason inheritedReason, bool persisted, SnapshotContext& context,
            std::size_t depth
        );

        void AppendStructProperties(
            ValueNode& parent, SDK::UStruct* structure, void* baseAddress, bool editableAllowed,
            ReadOnlyReason inheritedReason, bool inheritedPersisted, SnapshotContext& context, std::size_t depth
        );

        [[nodiscard]] bool ReadArrayHeader(const void* address, RawArrayHeader& header, std::string& error) {
            if (!IsReadableRange(address, sizeof(header))) {
                error = "An array header is not readable";
                return false;
            }
            std::memcpy(&header, address, sizeof(header));
            if (header.num < 0 || header.max < 0 || header.num > header.max) {
                error = "An array has invalid bounds";
                return false;
            }
            if (static_cast<std::size_t>(header.num) > K_MAX_CONTAINER_ELEMENTS) {
                error = "An array is too large to edit safely";
                return false;
            }
            if (header.num > 0 && !header.data) {
                error = "An array has no data";
                return false;
            }
            return true;
        }

        [[nodiscard]] bool ReadSparseHeader(const void* address, RawSparseHeader& header) {
            if (!IsReadableRange(address, sizeof(header))) return false;
            std::memcpy(&header, address, sizeof(header));
            if (header.numAllocated < 0 || header.maxAllocated < 0 || header.numAllocated > header.maxAllocated ||
                header.numFreeIndices < 0 || header.numFreeIndices > header.numAllocated ||
                header.numBits != header.numAllocated || header.maxBits < header.numBits ||
                (header.numBits > 128 && !header.secondaryAllocationFlags)) {
                return false;
            }
            if (static_cast<std::size_t>(header.numAllocated) > K_MAX_CONTAINER_ELEMENTS) return false;
            if (header.numAllocated > 0 && !header.data) return false;
            if (header.secondaryAllocationFlags && header.numBits > 0) {
                const auto flagWords = (static_cast<std::size_t>(header.numBits) + 31) / 32;
                if (!IsReadableRange(header.secondaryAllocationFlags, flagWords * sizeof(std::uint32_t))) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool SparseIndexAllocated(const RawSparseHeader& header, std::int32_t index) {
            const auto* flags =
                header.secondaryAllocationFlags ? header.secondaryAllocationFlags : header.inlineAllocationFlags;
            return (flags[index / 32] & (std::uint32_t{1} << (index & 31))) != 0;
        }

        [[nodiscard]] std::string ScalarDescription(const ValueNode& node) {
            if (node.kind == ValueKind::Enum) {
                if (const auto* value = std::get_if<std::int64_t>(&node.value)) {
                    const auto option = std::ranges::find_if(node.enumOptions, [value](const EnumOption& candidate) {
                        return candidate.value == *value;
                    });
                    if (option != node.enumOptions.end()) return option->displayName;
                }
            }
            if (const auto* value = std::get_if<std::string>(&node.value)) return *value;
            if (const auto* value = std::get_if<std::int64_t>(&node.value)) return std::to_string(*value);
            if (const auto* value = std::get_if<std::uint64_t>(&node.value)) return std::to_string(*value);
            if (const auto* value = std::get_if<double>(&node.value)) return std::to_string(*value);
            if (const auto* value = std::get_if<bool>(&node.value)) return *value ? "true" : "false";
            if (!node.children.empty()) {
                for (const auto& child : node.children) {
                    const auto description = ScalarDescription(child);
                    if (!description.empty()) return description;
                }
            }
            return node.displayName;
        }

        struct TreeMetadata {
            bool hasEditable = false;
            bool hasPersistentUnsupported = false;
        };

        [[nodiscard]] TreeMetadata FinalizeTreeMetadata(ValueNode& node) {
            TreeMetadata metadata{
                .hasEditable = node.editable,
                .hasPersistentUnsupported = node.persisted && node.kind == ValueKind::Unsupported,
            };
            for (auto& child : node.children) {
                const auto childMetadata = FinalizeTreeMetadata(child);
                metadata.hasEditable = metadata.hasEditable || childMetadata.hasEditable;
                metadata.hasPersistentUnsupported =
                    metadata.hasPersistentUnsupported || childMetadata.hasPersistentUnsupported;
            }
            node.hasPersistentUnsupported = metadata.hasPersistentUnsupported;
            if (!metadata.hasEditable) node.presetTarget = PresetTargetKind::None;
            return metadata;
        }

        [[nodiscard]] bool ValidateUniqueNodeIds(
            const ValueNode& node, std::unordered_set<NodeId>& ids, std::string& error
        ) {
            if (!ids.insert(node.id).second) {
                error = "Reflection produced duplicate or colliding value id '" + HexValue(node.id) + "'";
                return false;
            }
            for (const auto& child : node.children) {
                if (!ValidateUniqueNodeIds(child, ids, error)) return false;
            }
            return true;
        }

        void EraseLocations(const ValueNode& node, LocationMap* locations) {
            if (!locations) return;
            locations->erase(node.id);
            for (const auto& child : node.children)
                EraseLocations(child, locations);
        }

        void DegradeContainer(ValueNode& node, SnapshotContext& context, ReadOnlyReason reason) {
            EraseLocations(node, context.locations);
            std::vector<ValueNode>{}.swap(node.children);
            node.kind = ValueKind::Unsupported;
            node.editable = false;
            node.readOnlyReason = reason;
        }

        [[nodiscard]] ValueNode UnsupportedNode(
            NodeId id, std::string rawName, std::string displayName, ReadOnlyReason reason, bool persisted
        ) {
            ValueNode node;
            node.id = id;
            node.rawName = std::move(rawName);
            node.displayName = std::move(displayName);
            node.kind = ValueKind::Unsupported;
            node.readOnlyReason = reason;
            node.persisted = persisted;
            return node;
        }

        [[nodiscard]] ValueNode SnapshotDynamicArray(
            SDK::FArrayProperty* arrayProperty, void* address, ValueNode node, bool editableAllowed,
            ReadOnlyReason inheritedReason, SnapshotContext& context, std::size_t depth
        ) {
            node.kind = ValueKind::Array;
            node.editable = false;
            if (arrayProperty->ElementSize != static_cast<SDK::int32>(sizeof(SDK::TArray<std::byte>))) {
                node.kind = ValueKind::Unsupported;
                node.readOnlyReason = ReadOnlyReason::UnsafeContainer;
                return node;
            }
            if (!arrayProperty->InnerProperty || arrayProperty->InnerProperty->ElementSize <= 0) {
                node.kind = ValueKind::Unsupported;
                node.readOnlyReason = ReadOnlyReason::MissingMetadata;
                return node;
            }

            RawArrayHeader header;
            if (!ReadArrayHeader(address, header, context.error)) {
                node.kind = ValueKind::Unsupported;
                node.readOnlyReason = ReadOnlyReason::Unverified;
                return node;
            }
            const auto elementCount = static_cast<std::size_t>(header.num);
            const auto elementSize = static_cast<std::size_t>(arrayProperty->InnerProperty->ElementSize);
            const auto totalSize = elementCount * elementSize;
            if (!IsReadableRange(header.data, totalSize)) {
                context.error = "An array allocation is not readable";
                node.kind = ValueKind::Unsupported;
                node.readOnlyReason = ReadOnlyReason::Unverified;
                return node;
            }
            node.children.reserve(elementCount);

            for (std::int32_t index = 0; index < header.num && context.error.empty(); ++index) {
                auto itemName = "[" + std::to_string(index) + "]";
                auto* itemAddress =
                    static_cast<std::byte*>(header.data) + static_cast<std::size_t>(index) * elementSize;
                node.children.push_back(SnapshotValue(
                    arrayProperty->InnerProperty, itemAddress,
                    MakeNodeId(node.id, NodeIdSegment::ArrayElement, static_cast<std::uint64_t>(index)),
                    std::move(itemName),
                    "Item " + std::to_string(index + 1), editableAllowed, inheritedReason, node.persisted, context,
                    depth + 1
                ));
            }
            return node;
        }

        [[nodiscard]] ValueNode SnapshotMap(
            SDK::FMapProperty* mapProperty, void* address, ValueNode node, bool editableAllowed,
            ReadOnlyReason inheritedReason, SnapshotContext& context, std::size_t depth
        ) {
            node.kind = ValueKind::Map;
            node.editable = false;
            if (mapProperty->ElementSize != static_cast<SDK::int32>(sizeof(SDK::TMap<std::int32_t, std::int32_t>))) {
                node.kind = ValueKind::Unsupported;
                node.readOnlyReason = ReadOnlyReason::UnsafeContainer;
                return node;
            }
            if (!mapProperty->KeyProperty || !mapProperty->ValueProperty ||
                mapProperty->KeyProperty->ElementSize <= 0 || mapProperty->ValueProperty->ElementSize <= 0) {
                node.kind = ValueKind::Unsupported;
                node.readOnlyReason = ReadOnlyReason::MissingMetadata;
                return node;
            }

            RawSparseHeader header;
            if (!ReadSparseHeader(address, header)) {
                DegradeContainer(node, context, ReadOnlyReason::UnsafeContainer);
                return node;
            }

            std::size_t keyAlignment = 0;
            std::size_t valueAlignment = 0;
            std::size_t keySize = 0;
            std::size_t valueSize = 0;
            if (!TryPropertyElementLayout(mapProperty->KeyProperty, keySize, keyAlignment) ||
                !TryPropertyElementLayout(mapProperty->ValueProperty, valueSize, valueAlignment)) {
                node.kind = ValueKind::Unsupported;
                node.readOnlyReason = ReadOnlyReason::UnsafeContainer;
                return node;
            }
            const auto valueOffset = AlignUp(keySize, valueAlignment);
            const auto pairAlignment = (std::max)(keyAlignment, valueAlignment);
            const auto pairSize = AlignUp(valueOffset + valueSize, pairAlignment);
            const auto setElementAlignment = (std::max)(pairAlignment, alignof(std::int32_t));
            const auto sparseStride =
                AlignUp((std::max)(pairSize + 2 * sizeof(std::int32_t), 2 * sizeof(std::int32_t)), setElementAlignment);

            const auto allocationBytes = static_cast<std::size_t>(header.numAllocated) * sparseStride;
            if (!IsReadableRange(header.data, allocationBytes)) {
                DegradeContainer(node, context, ReadOnlyReason::UnsafeContainer);
                return node;
            }

            const auto activeCount = static_cast<std::size_t>(header.numAllocated - header.numFreeIndices);
            node.children.reserve(activeCount);
            std::size_t observedActiveCount = 0;

            for (std::int32_t index = 0; index < header.numAllocated; ++index) {
                if (!SparseIndexAllocated(header, index)) continue;
                ++observedActiveCount;
                auto* pairAddress =
                    static_cast<std::byte*>(header.data) + static_cast<std::size_t>(index) * sparseStride;

                SnapshotContext keyProbeContext{.owner = context.owner, .budget = context.budget};
                auto keyProbe = SnapshotValue(
                    mapProperty->KeyProperty, pairAddress, 0, "Key", "Key", false, ReadOnlyReason::UnsafeContainer,
                    node.persisted, keyProbeContext, depth + 1
                );
                if (!keyProbeContext.error.empty()) {
                    DegradeContainer(node, context, ReadOnlyReason::Unverified);
                    return node;
                }
                NodeId fingerprint = 0;
                if (!TryNodeFingerprint(keyProbe, fingerprint)) {
                    DegradeContainer(node, context, ReadOnlyReason::UnsupportedType);
                    return node;
                }
                ValueNode entry;
                entry.id = MakeNodeId(node.id, NodeIdSegment::MapEntry, fingerprint);
                entry.rawName = HexValue(fingerprint);
                entry.displayName = ScalarDescription(keyProbe);
                if (entry.displayName.empty()) entry.displayName = "Map entry";
                entry.kind = ValueKind::MapEntry;
                entry.persisted = node.persisted;
                entry.children.reserve(2);

                RebaseNodeIds(keyProbe, MakeNodeId(entry.id, NodeIdSegment::MapKey, 0));
                auto value = SnapshotValue(
                    mapProperty->ValueProperty, pairAddress + valueOffset,
                    MakeNodeId(entry.id, NodeIdSegment::MapValue, 0), "Value", "Value", editableAllowed,
                    inheritedReason, node.persisted, context, depth + 1
                );
                if (!context.error.empty()) return node;
                entry.children.push_back(std::move(keyProbe));
                entry.children.push_back(std::move(value));
                node.children.push_back(std::move(entry));
            }

            if (observedActiveCount != activeCount) DegradeContainer(node, context, ReadOnlyReason::UnsafeContainer);
            return node;
        }

        [[nodiscard]] ValueNode SnapshotSet(
            SDK::FSetProperty* setProperty, void* address, ValueNode node, SnapshotContext& context, std::size_t depth
        ) {
            node.kind = ValueKind::Set;
            node.editable = false;
            node.readOnlyReason = ReadOnlyReason::UnsafeContainer;
            if (setProperty->ElementSize != static_cast<SDK::int32>(sizeof(SDK::TSet<std::int32_t>))) {
                node.kind = ValueKind::Unsupported;
                node.readOnlyReason = ReadOnlyReason::UnsafeContainer;
                return node;
            }
            if (!setProperty->ElementProperty || setProperty->ElementProperty->ElementSize <= 0) {
                node.kind = ValueKind::Unsupported;
                node.readOnlyReason = ReadOnlyReason::MissingMetadata;
                return node;
            }

            RawSparseHeader header;
            if (!ReadSparseHeader(address, header)) {
                DegradeContainer(node, context, ReadOnlyReason::UnsafeContainer);
                return node;
            }

            std::size_t propertyAlignment = 0;
            std::size_t elementSize = 0;
            if (!TryPropertyElementLayout(setProperty->ElementProperty, elementSize, propertyAlignment)) {
                node.kind = ValueKind::Unsupported;
                node.readOnlyReason = ReadOnlyReason::UnsafeContainer;
                return node;
            }
            const auto elementAlignment = (std::max)(propertyAlignment, alignof(std::int32_t));
            const auto sparseStride =
                AlignUp((std::max)(elementSize + 2 * sizeof(std::int32_t), 2 * sizeof(std::int32_t)), elementAlignment);
            const auto allocationBytes = static_cast<std::size_t>(header.numAllocated) * sparseStride;
            if (!IsReadableRange(header.data, allocationBytes)) {
                DegradeContainer(node, context, ReadOnlyReason::UnsafeContainer);
                return node;
            }

            const auto activeCount = static_cast<std::size_t>(header.numAllocated - header.numFreeIndices);
            node.children.reserve(activeCount);
            std::size_t observedActiveCount = 0;
            for (std::int32_t index = 0; index < header.numAllocated; ++index) {
                if (!SparseIndexAllocated(header, index)) continue;
                ++observedActiveCount;
                auto* elementAddress =
                    static_cast<std::byte*>(header.data) + static_cast<std::size_t>(index) * sparseStride;
                SnapshotContext probeContext{.owner = context.owner, .budget = context.budget};
                auto probe = SnapshotValue(
                    setProperty->ElementProperty, elementAddress, 0, "Element", "Element", false,
                    ReadOnlyReason::UnsafeContainer, node.persisted, probeContext, depth + 1
                );
                if (!probeContext.error.empty()) {
                    DegradeContainer(node, context, ReadOnlyReason::Unverified);
                    return node;
                }
                NodeId fingerprint = 0;
                if (!TryNodeFingerprint(probe, fingerprint)) {
                    DegradeContainer(node, context, ReadOnlyReason::UnsupportedType);
                    return node;
                }
                auto displayName = ScalarDescription(probe);
                RebaseNodeIds(probe, MakeNodeId(node.id, NodeIdSegment::SetElement, fingerprint));
                probe.rawName = HexValue(fingerprint);
                probe.displayName = std::move(displayName);
                node.children.push_back(std::move(probe));
            }
            if (observedActiveCount != activeCount) DegradeContainer(node, context, ReadOnlyReason::UnsafeContainer);
            return node;
        }

        [[nodiscard]] ValueNode SnapshotValue(
            SDK::FProperty* property, void* address, NodeId id, std::string rawName, std::string displayName,
            bool editableAllowed, ReadOnlyReason inheritedReason, bool persisted, SnapshotContext& context,
            std::size_t depth
        ) {
            if (!property)
                return UnsupportedNode(
                    id, std::move(rawName), std::move(displayName), ReadOnlyReason::MissingMetadata, persisted
                );
            if (!address) {
                context.error = "Property '" + property->Name.GetRawString() + "' has no value address";
                return UnsupportedNode(
                    id, std::move(rawName), std::move(displayName), ReadOnlyReason::Unverified, persisted
                );
            }
            if (depth > K_MAX_REFLECTION_DEPTH) {
                context.error = "The reflected value tree is too deeply nested";
                return UnsupportedNode(
                    id, std::move(rawName), std::move(displayName), ReadOnlyReason::Unverified, persisted
                );
            }
            if (!context.budget || context.budget->remainingNodes == 0) {
                context.error = "The reflected value tree exceeds the safe snapshot budget";
                return UnsupportedNode(
                    id, std::move(rawName), std::move(displayName), ReadOnlyReason::Unverified, persisted
                );
            }
            --context.budget->remainingNodes;

            ValueNode node;
            node.id = id;
            node.rawName = std::move(rawName);
            node.displayName = std::move(displayName);
            node.readOnlyReason = inheritedReason;
            node.persisted = persisted;

            if (HasCastFlag(property, SDK::EClassCastFlags::BoolProperty)) {
                const auto* boolProperty = static_cast<const SDK::FBoolProperty*>(property);
                const auto mask = boolProperty->FieldMask != 0 ? boolProperty->FieldMask : boolProperty->ByteMask;
                const auto byte =
                    ReadUnaligned<std::uint8_t>(static_cast<std::byte*>(address) + boolProperty->ByteOffset);
                node.kind = ValueKind::Boolean;
                node.value = mask != 0 ? (byte & mask) != 0 : byte != 0;
                node.editable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
            } else if (HasCastFlag(property, SDK::EClassCastFlags::EnumProperty)) {
                const auto* enumProperty = static_cast<const SDK::FEnumProperty*>(property);
                bool isSigned = false;
                std::int64_t signedValue = 0;
                std::uint64_t unsignedValue = 0;
                if (!enumProperty->UnderlayingProperty ||
                    !ReadIntegral(enumProperty->UnderlayingProperty, address, isSigned, signedValue, unsignedValue) ||
                    (!isSigned &&
                     unsignedValue > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()))) {
                    return UnsupportedNode(
                        node.id, std::move(node.rawName), std::move(node.displayName), ReadOnlyReason::UnsupportedType,
                        persisted
                    );
                }
                node.kind = ValueKind::Enum;
                node.value = isSigned ? signedValue : static_cast<std::int64_t>(unsignedValue);
                AddEnumOptions(node, enumProperty->Enum);
                if (!enumProperty->Enum && node.readOnlyReason == ReadOnlyReason::None)
                    node.readOnlyReason = ReadOnlyReason::MissingMetadata;
                node.editable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
            } else if (
                HasCastFlag(property, SDK::EClassCastFlags::ByteProperty) &&
                static_cast<const SDK::FByteProperty*>(property)->Enum
            ) {
                node.kind = ValueKind::Enum;
                node.value = static_cast<std::int64_t>(ReadUnaligned<std::uint8_t>(address));
                AddEnumOptions(node, static_cast<const SDK::FByteProperty*>(property)->Enum);
                node.editable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
            } else {
                bool isSigned = false;
                std::int64_t signedValue = 0;
                std::uint64_t unsignedValue = 0;
                if (ReadIntegral(property, address, isSigned, signedValue, unsignedValue)) {
                    node.kind = isSigned ? ValueKind::SignedInteger : ValueKind::UnsignedInteger;
                    node.value = isSigned ? ScalarValue{signedValue} : ScalarValue{unsignedValue};
                    if (HasCastFlag(property, SDK::EClassCastFlags::Int8Property) ||
                        HasCastFlag(property, SDK::EClassCastFlags::ByteProperty)) {
                        node.numericBits = 8;
                    } else if (
                        HasCastFlag(property, SDK::EClassCastFlags::Int16Property) ||
                        HasCastFlag(property, SDK::EClassCastFlags::UInt16Property)
                    ) {
                        node.numericBits = 16;
                    } else if (
                        HasCastFlag(property, SDK::EClassCastFlags::IntProperty) ||
                        HasCastFlag(property, SDK::EClassCastFlags::UInt32Property)
                    ) {
                        node.numericBits = 32;
                    } else {
                        node.numericBits = 64;
                    }
                    node.editable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
                } else if (HasCastFlag(property, SDK::EClassCastFlags::FloatProperty)) {
                    node.kind = ValueKind::Float;
                    node.value = static_cast<double>(ReadUnaligned<float>(address));
                    node.editable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
                } else if (HasCastFlag(property, SDK::EClassCastFlags::DoubleProperty)) {
                    node.kind = ValueKind::Double;
                    node.value = ReadUnaligned<double>(address);
                    node.editable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
                } else if (HasCastFlag(property, SDK::EClassCastFlags::NameProperty)) {
                    node.kind = ValueKind::Name;
                    node.value = static_cast<const SDK::FName*>(address)->GetRawString();
                    node.editable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
                } else if (HasCastFlag(property, SDK::EClassCastFlags::StrProperty)) {
                    node.kind = ValueKind::String;
                    node.value = static_cast<const SDK::FString*>(address)->ToString();
                    node.editable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
                } else if (HasCastFlag(property, SDK::EClassCastFlags::TextProperty)) {
                    const auto* text = static_cast<const SDK::FText*>(address);
                    node.kind = ValueKind::Text;
                    node.value = text->TextData ? text->ToString() : std::string{};
                    if (!context.owner && node.readOnlyReason == ReadOnlyReason::None)
                        node.readOnlyReason = ReadOnlyReason::MissingMetadata;
                    node.editable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
                } else if (HasCastFlag(property, SDK::EClassCastFlags::SoftClassProperty)) {
                    const auto* reference = reinterpret_cast<const SDK::TSoftClassPtr<SDK::UClass>*>(address);
                    node.kind = ValueKind::SoftClassReference;
                    node.value = SDK::UKismetSystemLibrary::Conv_SoftClassReferenceToString(*reference).ToString();
                    SDK::UClass* metaClass = nullptr;
                    std::string metadataError;
                    if (TrySoftClassMetaClass(property, metaClass, metadataError)) {
                        node.editable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
                    } else {
                        node.editable = false;
                        if (node.readOnlyReason == ReadOnlyReason::None)
                            node.readOnlyReason = ReadOnlyReason::MissingMetadata;
                    }
                } else if (HasCastFlag(property, SDK::EClassCastFlags::SoftObjectProperty)) {
                    const auto* reference = reinterpret_cast<const SDK::TSoftObjectPtr<SDK::UObject>*>(address);
                    node.kind = ValueKind::SoftObjectReference;
                    node.value = SDK::UKismetSystemLibrary::Conv_SoftObjectReferenceToString(*reference).ToString();
                    node.editable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
                } else if (HasCastFlag(property, SDK::EClassCastFlags::ClassProperty)) {
                    node.kind = ValueKind::ClassReference;
                    node.value = ObjectPath(*static_cast<SDK::UClass* const*>(address));
                    node.editable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
                } else if (HasCastFlag(property, SDK::EClassCastFlags::ObjectProperty)) {
                    node.kind = ValueKind::ObjectReference;
                    node.value = ObjectPath(*static_cast<SDK::UObject* const*>(address));
                    node.editable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
                } else if (HasCastFlag(property, SDK::EClassCastFlags::StructProperty)) {
                    const auto* structProperty = static_cast<const SDK::FStructProperty*>(property);
                    node.kind = ValueKind::Struct;
                    node.editable = false;
                    if (!structProperty->Struct) {
                        node.kind = ValueKind::Unsupported;
                        if (node.readOnlyReason == ReadOnlyReason::None)
                            node.readOnlyReason = ReadOnlyReason::MissingMetadata;
                    } else {
                        node.presetTarget = ClassifyPresetTarget(structProperty->Struct);
                        AppendStructProperties(
                            node, structProperty->Struct, address,
                            editableAllowed && node.readOnlyReason == ReadOnlyReason::None, node.readOnlyReason,
                            node.persisted, context, depth + 1
                        );
                    }
                } else if (HasCastFlag(property, SDK::EClassCastFlags::ArrayProperty)) {
                    const auto childEditable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
                    const auto childReason = node.readOnlyReason;
                    return SnapshotDynamicArray(
                        static_cast<SDK::FArrayProperty*>(property), address, std::move(node), childEditable,
                        childReason, context, depth
                    );
                } else if (HasCastFlag(property, SDK::EClassCastFlags::MapProperty)) {
                    const auto childEditable = editableAllowed && node.readOnlyReason == ReadOnlyReason::None;
                    const auto childReason = node.readOnlyReason;
                    return SnapshotMap(
                        static_cast<SDK::FMapProperty*>(property), address, std::move(node), childEditable, childReason,
                        context, depth
                    );
                } else if (HasCastFlag(property, SDK::EClassCastFlags::SetProperty)) {
                    return SnapshotSet(
                        static_cast<SDK::FSetProperty*>(property), address, std::move(node), context, depth
                    );
                } else {
                    if (node.readOnlyReason == ReadOnlyReason::None)
                        node.readOnlyReason = ReadOnlyReason::UnsupportedType;
                    node.kind = ValueKind::Unsupported;
                    node.editable = false;
                }
            }

            if (node.editable) node.originalValue = node.value;
            if (node.children.empty()) RegisterLocation(node, property, address, context, depth);
            return node;
        }

        void AppendStructProperties(
            ValueNode& parent, SDK::UStruct* structure, void* baseAddress, bool editableAllowed,
            ReadOnlyReason inheritedReason, bool inheritedPersisted, SnapshotContext& context, std::size_t depth
        ) {
            if (!structure || !baseAddress || !context.error.empty()) return;
            if (depth > K_MAX_REFLECTION_DEPTH) {
                context.error = "The reflected value tree is too deeply nested";
                return;
            }
            if (structure->SuperStruct)
                AppendStructProperties(
                    parent, structure->SuperStruct, baseAddress, editableAllowed, inheritedReason, inheritedPersisted,
                    context, depth
                );

            for (auto* field = structure->ChildProperties; field && context.error.empty(); field = field->Next) {
                if (!HasCastFlag(field, SDK::EClassCastFlags::Property)) continue;
                auto* property = static_cast<SDK::FProperty*>(field);
                auto rawName = property->Name.GetRawString();

                if (property->Offset < 0 || property->ElementSize <= 0 || property->ArrayDim <= 0) {
                    context.error = "Property '" + rawName + "' has invalid reflection metadata";
                    break;
                }
                const auto structureSize = static_cast<std::size_t>((std::max)(structure->Size, 0));
                const auto propertyBytes =
                    static_cast<std::size_t>(property->ElementSize) * static_cast<std::size_t>(property->ArrayDim);
                if (static_cast<std::size_t>(property->Offset) > structureSize ||
                    propertyBytes > structureSize - static_cast<std::size_t>(property->Offset)) {
                    context.error = "Property '" + rawName + "' lies outside its reflected structure";
                    break;
                }

                auto displayName = DisplayName(rawName);
                auto propertyId = HashValue(K_FNV_OFFSET, parent.id);
                propertyId = HashValue(propertyId, NodeIdSegment::Property);
                propertyId = HashValue(propertyId, static_cast<std::uint64_t>(rawName.size()));
                propertyId = HashBytes(propertyId, rawName.data(), rawName.size());
                propertyId = HashValue(propertyId, property->Offset);
                auto* propertyAddress = static_cast<std::byte*>(baseAddress) + property->Offset;
                constexpr auto UNSAVED_FLAGS = static_cast<std::uint64_t>(SDK::EPropertyFlags::Transient) |
                                               static_cast<std::uint64_t>(SDK::EPropertyFlags::DuplicateTransient) |
                                               static_cast<std::uint64_t>(SDK::EPropertyFlags::NonPIEDuplicateTransient) |
                                               static_cast<std::uint64_t>(SDK::EPropertyFlags::SkipSerialization);
                const auto flags = property->PropertyFlags;
                auto ownReason = ReadOnlyReason::None;
                if ((flags & static_cast<std::uint64_t>(SDK::EPropertyFlags::Deprecated)) != 0)
                    ownReason = ReadOnlyReason::Deprecated;
                else if ((flags & UNSAVED_FLAGS) != 0)
                    ownReason = ReadOnlyReason::NotSaved;
                const auto reason = inheritedReason == ReadOnlyReason::None ? ownReason : inheritedReason;
                const bool propertyEditable = editableAllowed && reason == ReadOnlyReason::None;
                const bool propertyPersisted = inheritedPersisted && ownReason == ReadOnlyReason::None;
                if (property->ArrayDim == 1) {
                    parent.children.push_back(SnapshotValue(
                        property, propertyAddress, propertyId, std::move(rawName), std::move(displayName),
                        propertyEditable, reason, propertyPersisted, context, depth + 1
                    ));
                    continue;
                }

                ValueNode fixedArray;
                fixedArray.id = propertyId;
                fixedArray.rawName = std::move(rawName);
                fixedArray.displayName = std::move(displayName);
                fixedArray.kind = ValueKind::Array;
                fixedArray.readOnlyReason = reason;
                fixedArray.persisted = propertyPersisted;
                fixedArray.children.reserve(static_cast<std::size_t>(property->ArrayDim));
                for (std::int32_t index = 0; index < property->ArrayDim && context.error.empty(); ++index) {
                    auto itemName = "[" + std::to_string(index) + "]";
                    fixedArray.children.push_back(SnapshotValue(
                        property,
                        propertyAddress +
                            static_cast<std::size_t>(index) * static_cast<std::size_t>(property->ElementSize),
                        MakeNodeId(
                            fixedArray.id, NodeIdSegment::ArrayElement, static_cast<std::uint64_t>(index)
                        ),
                        std::move(itemName), "Item " + std::to_string(index + 1), propertyEditable, reason,
                        propertyPersisted, context, depth + 1
                    ));
                }
                parent.children.push_back(std::move(fixedArray));
            }
        }

        [[nodiscard]] ValueNode SnapshotRoot(
            SDK::UStruct* structure, void* bytes, SDK::UObject* owner, LocationMap* locations, std::string& error
        ) {
            ValueNode root;
            root.id = K_ROOT_NODE_ID;
            root.rawName = structure ? structure->GetName() : "Unknown";
            root.displayName = DisplayName(root.rawName);
            root.kind = ValueKind::Struct;
            root.presetTarget = ClassifyPresetTarget(structure);
            if (!structure || !bytes) {
                root.kind = ValueKind::Unsupported;
                root.readOnlyReason = ReadOnlyReason::MissingMetadata;
                error = "Root reflection metadata or data is unavailable";
                (void)FinalizeTreeMetadata(root);
                return root;
            }
            if (structure->Size <= 0 || !IsReadableRange(bytes, static_cast<std::size_t>(structure->Size))) {
                root.kind = ValueKind::Unsupported;
                root.readOnlyReason = ReadOnlyReason::Unverified;
                error = "Root data does not cover the reflected structure";
                (void)FinalizeTreeMetadata(root);
                return root;
            }

            SnapshotBudget budget;
            SnapshotContext context{.owner = owner, .locations = locations, .budget = &budget};
            AppendStructProperties(root, structure, bytes, true, ReadOnlyReason::None, true, context, 0);
            error = std::move(context.error);
            (void)FinalizeTreeMetadata(root);
            if (error.empty()) {
                std::unordered_set<NodeId> ids;
                ids.reserve(K_MAX_SNAPSHOT_NODES - budget.remainingNodes + 1);
                (void)ValidateUniqueNodeIds(root, ids, error);
            }
            return root;
        }

        [[nodiscard]] Document SnapshotSave(
            SDK::USaveGame* save, std::string_view slot, int userIndex, LocationMap* locations, std::string& error
        ) {
            Document document;
            document.sourceSlot = slot;
            document.userIndex = userIndex;
            if (!save || !save->Class) {
                error = "The save object has no class";
                return document;
            }
            document.classPath = ObjectPath(save->Class);
            document.root = SnapshotRoot(save->Class, save, save, locations, error);
            return document;
        }

        template <typename TargetType, typename SourceType>
        [[nodiscard]] bool WriteChecked(void* address, SourceType value) {
            if (!std::in_range<TargetType>(value)) return false;
            WriteUnaligned(address, static_cast<TargetType>(value));
            return true;
        }

        template <typename ValueType>
        [[nodiscard]] bool WriteIntegral(
            SDK::FProperty* property, void* address, ValueType value, std::string& error
        ) {
            static_assert(std::is_same_v<ValueType, std::int64_t> || std::is_same_v<ValueType, std::uint64_t>);
            bool written = false;
            if constexpr (std::is_signed_v<ValueType>) {
                if (HasCastFlag(property, SDK::EClassCastFlags::Int8Property))
                    written = WriteChecked<std::int8_t>(address, value);
                else if (HasCastFlag(property, SDK::EClassCastFlags::Int16Property))
                    written = WriteChecked<std::int16_t>(address, value);
                else if (HasCastFlag(property, SDK::EClassCastFlags::IntProperty))
                    written = WriteChecked<std::int32_t>(address, value);
                else if (HasCastFlag(property, SDK::EClassCastFlags::Int64Property))
                    written = WriteChecked<std::int64_t>(address, value);
            } else {
                if (HasCastFlag(property, SDK::EClassCastFlags::ByteProperty))
                    written = WriteChecked<std::uint8_t>(address, value);
                else if (HasCastFlag(property, SDK::EClassCastFlags::UInt16Property))
                    written = WriteChecked<std::uint16_t>(address, value);
                else if (HasCastFlag(property, SDK::EClassCastFlags::UInt32Property))
                    written = WriteChecked<std::uint32_t>(address, value);
                else if (HasCastFlag(property, SDK::EClassCastFlags::UInt64Property))
                    written = WriteChecked<std::uint64_t>(address, value);
            }
            if (!written)
                error = std::is_signed_v<ValueType> ? "Integer value is outside the property's range"
                                                    : "Unsigned integer value is outside the property's range";
            return written;
        }

        [[nodiscard]] bool WriteEnumIntegral(
            SDK::FProperty* property, void* address, std::int64_t value, std::string& error
        ) {
            SDK::FProperty* underlying = property;
            SDK::UEnum* enumObject = nullptr;
            if (HasCastFlag(property, SDK::EClassCastFlags::EnumProperty)) {
                const auto* enumProperty = static_cast<SDK::FEnumProperty*>(property);
                underlying = enumProperty->UnderlayingProperty;
                enumObject = enumProperty->Enum;
            } else if (HasCastFlag(property, SDK::EClassCastFlags::ByteProperty)) {
                enumObject = static_cast<SDK::FByteProperty*>(property)->Enum;
            }
            if (!underlying || !enumObject) {
                error = "Enum metadata is unavailable";
                return false;
            }

            bool declaredValue = false;
            for (const auto& item : enumObject->Names) {
                if (item.Value() != value) continue;
                const auto name = item.Key().GetRawString();
                if (name.ends_with("_MAX") || name.ends_with("::MAX")) continue;
                declaredValue = true;
                break;
            }
            if (!declaredValue) {
                error = "Enum value is not declared by the game";
                return false;
            }

            if (HasCastFlag(underlying, SDK::EClassCastFlags::Int8Property) ||
                HasCastFlag(underlying, SDK::EClassCastFlags::Int16Property) ||
                HasCastFlag(underlying, SDK::EClassCastFlags::IntProperty) ||
                HasCastFlag(underlying, SDK::EClassCastFlags::Int64Property))
                return WriteIntegral(underlying, address, value, error);
            if (value < 0) {
                error = "A negative enum value cannot be stored in this property";
                return false;
            }
            return WriteIntegral(underlying, address, static_cast<std::uint64_t>(value), error);
        }

        [[nodiscard]] bool ReplaceStringValue(SDK::FString& destination, std::string_view value, std::string& error) {
            std::wstring replacement;
            if (!TryUtf8ToWide(value, replacement, error)) return false;
            const auto currentUtf8 = destination.ToString();
            if (currentUtf8 == value) return true;

            std::wstring current;
            if (!TryUtf8ToWide(currentUtf8, current, error)) return false;
            if (current.empty()) {
                constexpr auto SENTINEL = L"__HSE_SAVE_EDITOR_EMPTY_STRING__";
                destination = SDK::UKismetStringLibrary::Concat_StrStr(destination, SDK::FString(SENTINEL));
                SDK::UKismetStringLibrary::ReplaceInline(
                    destination, SDK::FString(SENTINEL), SDK::FString(replacement.c_str()),
                    SDK::ESearchCase::CaseSensitive
                );
            } else {
                SDK::UKismetStringLibrary::ReplaceInline(
                    destination, SDK::FString(current.c_str()), SDK::FString(replacement.c_str()),
                    SDK::ESearchCase::CaseSensitive
                );
            }
            if (destination.ToString() != value) {
                error = "The game rejected the string value";
                return false;
            }
            return true;
        }

        [[nodiscard]] bool ApplyHardObjectReference(
            SDK::FObjectPropertyBase* property, void* address, std::string_view path, std::string& error
        ) {
            if (path.empty()) {
                *static_cast<SDK::UObject**>(address) = nullptr;
                return true;
            }
            std::wstring widePath;
            if (!TryUtf8ToWide(path, widePath, error)) return false;
            const auto softPath = SDK::UKismetSystemLibrary::MakeSoftObjectPath(SDK::FString(widePath.c_str()));
            const auto reference = SDK::UKismetSystemLibrary::Conv_SoftObjPathToSoftObjRef(softPath);
            auto* object = SDK::UKismetSystemLibrary::LoadAsset_Blocking(reference);
            if (!object) {
                error = "Object reference could not be loaded";
                return false;
            }
            if (property->PropertyClass && !object->IsA(property->PropertyClass)) {
                error = "Object reference has an incompatible class";
                return false;
            }
            *static_cast<SDK::UObject**>(address) = object;
            return true;
        }

        [[nodiscard]] bool ApplyHardClassReference(
            SDK::FClassProperty* property, void* address, std::string_view path, std::string& error
        ) {
            if (path.empty()) {
                *static_cast<SDK::UClass**>(address) = nullptr;
                return true;
            }
            std::wstring widePath;
            if (!TryUtf8ToWide(path, widePath, error)) return false;
            const auto softPath = SDK::UKismetSystemLibrary::MakeSoftClassPath(SDK::FString(widePath.c_str()));
            const auto reference = SDK::UKismetSystemLibrary::Conv_SoftClassPathToSoftClassRef(softPath);
            auto* loadedClass = SDK::UKismetSystemLibrary::LoadClassAsset_Blocking(reference);
            if (!loadedClass) {
                error = "Class reference could not be loaded";
                return false;
            }
            if (property->MetaClass && !loadedClass->IsSubclassOf(property->MetaClass)) {
                error = "Class reference does not derive from the required class";
                return false;
            }
            *static_cast<SDK::UClass**>(address) = loadedClass;
            return true;
        }

        [[nodiscard]] bool ApplySoftObjectReference(
            SDK::FObjectPropertyBase* property, void* address, std::string_view path, std::string& error
        ) {
            std::wstring widePath;
            if (!TryUtf8ToWide(path, widePath, error)) return false;
            const auto softPath = SDK::UKismetSystemLibrary::MakeSoftObjectPath(SDK::FString(widePath.c_str()));
            const auto reference = SDK::UKismetSystemLibrary::Conv_SoftObjPathToSoftObjRef(softPath);
            if (!path.empty()) {
                auto* object = SDK::UKismetSystemLibrary::LoadAsset_Blocking(reference);
                if (!object) {
                    error = "Soft object reference could not be loaded";
                    return false;
                }
                if (property->PropertyClass && !object->IsA(property->PropertyClass)) {
                    error = "Soft object reference has an incompatible class";
                    return false;
                }
            }
            *reinterpret_cast<SDK::FSoftObjectPtr*>(address) = reference;
            return true;
        }

        [[nodiscard]] bool ApplySoftClassReference(
            SDK::FProperty* property, void* address, std::string_view path, std::string& error
        ) {
            std::wstring widePath;
            if (!TryUtf8ToWide(path, widePath, error)) return false;
            const auto softPath = SDK::UKismetSystemLibrary::MakeSoftClassPath(SDK::FString(widePath.c_str()));
            const auto reference = SDK::UKismetSystemLibrary::Conv_SoftClassPathToSoftClassRef(softPath);
            if (!path.empty()) {
                auto* loadedClass = SDK::UKismetSystemLibrary::LoadClassAsset_Blocking(reference);
                if (!loadedClass) {
                    error = "Soft class reference could not be loaded";
                    return false;
                }
                SDK::UClass* metaClass = nullptr;
                if (!TrySoftClassMetaClass(property, metaClass, error)) return false;
                if (!loadedClass->IsSubclassOf(metaClass)) {
                    error = "Soft class reference does not derive from the required class";
                    return false;
                }
            }
            *reinterpret_cast<SDK::FSoftObjectPtr*>(address) = reference;
            return true;
        }

        [[nodiscard]] bool ApplyScalar(
            const ValueLocation& location, const ValueNode& node, SDK::UObject* owner, std::string& error
        ) {
            if (!location.property || !location.address) {
                error = "Value location no longer matches reflected metadata";
                return false;
            }
            switch (node.kind) {
                case ValueKind::Boolean: {
                    const auto* value = std::get_if<bool>(&node.value);
                    if (!value) break;
                    const auto* boolProperty = static_cast<SDK::FBoolProperty*>(location.property);
                    const auto mask = boolProperty->FieldMask != 0 ? boolProperty->FieldMask : boolProperty->ByteMask;
                    auto* byteAddress = static_cast<std::byte*>(location.address) + boolProperty->ByteOffset;
                    auto byte = ReadUnaligned<std::uint8_t>(byteAddress);
                    if (mask == 0)
                        byte = *value ? 1 : 0;
                    else if (*value)
                        byte |= mask;
                    else
                        byte &= static_cast<std::uint8_t>(~mask);
                    WriteUnaligned(byteAddress, byte);
                    return true;
                }
                case ValueKind::SignedInteger:
                    if (const auto* value = std::get_if<std::int64_t>(&node.value))
                        return WriteIntegral(location.property, location.address, *value, error);
                    break;
                case ValueKind::UnsignedInteger:
                    if (const auto* value = std::get_if<std::uint64_t>(&node.value))
                        return WriteIntegral(location.property, location.address, *value, error);
                    break;
                case ValueKind::Float:
                    if (const auto* value = std::get_if<double>(&node.value)) {
                        if (!std::isfinite(*value) || *value < -(std::numeric_limits<float>::max)() ||
                            *value > (std::numeric_limits<float>::max)()) {
                            error = "Float value must be finite and in range";
                            return false;
                        }
                        WriteUnaligned(location.address, static_cast<float>(*value));
                        return true;
                    }
                    break;
                case ValueKind::Double:
                    if (const auto* value = std::get_if<double>(&node.value)) {
                        if (!std::isfinite(*value)) {
                            error = "Double value must be finite";
                            return false;
                        }
                        WriteUnaligned(location.address, *value);
                        return true;
                    }
                    break;
                case ValueKind::Enum:
                    if (const auto* value = std::get_if<std::int64_t>(&node.value))
                        return WriteEnumIntegral(location.property, location.address, *value, error);
                    break;
                case ValueKind::Name:
                    if (const auto* value = std::get_if<std::string>(&node.value)) {
                        std::wstring wide;
                        if (!TryUtf8ToWide(*value, wide, error)) return false;
                        *static_cast<SDK::FName*>(location.address) =
                            SDK::UKismetStringLibrary::Conv_StringToName(SDK::FString(wide.c_str()));
                        return true;
                    }
                    break;
                case ValueKind::String:
                    if (const auto* value = std::get_if<std::string>(&node.value))
                        return ReplaceStringValue(*static_cast<SDK::FString*>(location.address), *value, error);
                    break;
                case ValueKind::Text:
                    if (const auto* value = std::get_if<std::string>(&node.value)) {
                        if (!owner) {
                            error = "Text property has no owning object";
                            return false;
                        }
                        std::wstring wide;
                        if (!TryUtf8ToWide(*value, wide, error)) return false;
                        auto* destination = static_cast<SDK::FText*>(location.address);
                        auto replacement = SDK::UKismetTextLibrary::MakeInvariantText(SDK::FString(wide.c_str()));
                        if (!replacement.TextData || replacement.ToString() != *value) {
                            error = "The game could not construct the text value";
                            return false;
                        }
                        if (location.directOwnerProperty) {
                            SDK::UKismetSystemLibrary::SetTextPropertyByName(
                                owner, location.property->Name, replacement
                            );
                        } else {
                            // Dumper-7 exposes FText only as its 16-byte ownership shell. Transferring the
                            // engine-created invariant text into this temporary save is safer than mutating a
                            // potentially shared localization history. The overwritten history reference is
                            // intentionally left alive; this bounded leak is preferable to an ABI-dependent destructor
                            // call that could crash.
                            *destination = replacement;
                        }
                        if (!destination->TextData || destination->ToString() != *value) {
                            error = "The game rejected the text value";
                            return false;
                        }
                        return true;
                    }
                    break;
                case ValueKind::ObjectReference:
                    if (const auto* value = std::get_if<std::string>(&node.value))
                        return ApplyHardObjectReference(
                            static_cast<SDK::FObjectPropertyBase*>(location.property), location.address, *value, error
                        );
                    break;
                case ValueKind::ClassReference:
                    if (const auto* value = std::get_if<std::string>(&node.value))
                        return ApplyHardClassReference(
                            static_cast<SDK::FClassProperty*>(location.property), location.address, *value, error
                        );
                    break;
                case ValueKind::SoftObjectReference:
                    if (const auto* value = std::get_if<std::string>(&node.value))
                        return ApplySoftObjectReference(
                            static_cast<SDK::FObjectPropertyBase*>(location.property), location.address, *value, error
                        );
                    break;
                case ValueKind::SoftClassReference:
                    if (const auto* value = std::get_if<std::string>(&node.value))
                        return ApplySoftClassReference(location.property, location.address, *value, error);
                    break;
                default: error = "Only editable leaf values can be applied"; return false;
            }
            error = "Edited value has the wrong scalar type";
            return false;
        }

        using ConstNodeMap = std::unordered_map<NodeId, const ValueNode*>;

        [[nodiscard]] bool BuildNodeMap(const ValueNode& node, ConstNodeMap& nodes, std::string& error) {
            if (!nodes.emplace(node.id, &node).second) {
                error = "The document contains duplicate or colliding value id '" + HexValue(node.id) + "'";
                return false;
            }
            for (const auto& child : node.children) {
                if (!BuildNodeMap(child, nodes, error)) return false;
            }
            return true;
        }

        [[nodiscard]] bool SameContainerIdentities(const ValueNode& edited, const ValueNode& current) {
            if (edited.children.size() != current.children.size()) return false;
            std::unordered_set<NodeId> currentIds;
            currentIds.reserve(current.children.size());
            for (const auto& child : current.children)
                currentIds.emplace(child.id);
            for (const auto& child : edited.children) {
                if (!currentIds.contains(child.id)) return false;
            }
            return true;
        }

        [[nodiscard]] bool OriginalTreeMatchesCurrent(
            const ValueNode& edited, const ValueNode& current, const ConstNodeMap& currentNodes
        ) {
            if (edited.id != current.id || edited.kind != current.kind || edited.persisted != current.persisted)
                return false;
            if (!edited.persisted) return true;

            if (IsContainerKind(edited.kind) && edited.children.size() != current.children.size()) return false;
            if (!IsContainerKind(edited.kind)) {
                const auto editedChildCount =
                    std::ranges::count_if(edited.children, [](const ValueNode& child) { return child.persisted; });
                const auto currentChildCount =
                    std::ranges::count_if(current.children, [](const ValueNode& child) { return child.persisted; });
                if (editedChildCount != currentChildCount) return false;
            }

            for (const auto& editedChild : edited.children) {
                if (!editedChild.persisted) continue;
                const auto currentChild = currentNodes.find(editedChild.id);
                if (currentChild == currentNodes.end() || !currentChild->second->persisted ||
                    !OriginalTreeMatchesCurrent(editedChild, *currentChild->second, currentNodes))
                    return false;
            }
            const auto& baseline = edited.editable ? edited.originalValue : edited.value;
            return !edited.children.empty() || ScalarEqual(baseline, current.value);
        }

        [[nodiscard]] bool ValidateEditedNode(
            const ValueNode& edited, const ConstNodeMap& currentNodes, const LocationMap& locations,
            std::vector<const ValueNode*>& dirtyLeaves, std::string& error
        ) {
            const bool scalarChanged = !ScalarEqual(edited.value, edited.originalValue);
            if (!edited.children.empty() && scalarChanged) {
                error = "Container value '" + edited.displayName + "' was marked dirty directly";
                return false;
            }

            if (!edited.children.empty()) {
                const auto dirtyLeafCount = dirtyLeaves.size();
                for (const auto& child : edited.children) {
                    if (!ValidateEditedNode(child, currentNodes, locations, dirtyLeaves, error)) return false;
                }
                if (dirtyLeaves.size() == dirtyLeafCount) return true;
            } else if (!edited.editable || !scalarChanged)
                return true;

            const auto currentIterator = currentNodes.find(edited.id);
            if (currentIterator == currentNodes.end()) {
                error = "The source save structure changed at '" + edited.displayName + "'";
                return false;
            }
            const auto& current = *currentIterator->second;
            if (edited.kind != current.kind) {
                error = "The source save type changed at '" + edited.displayName + "'";
                return false;
            }

            if (!edited.children.empty()) {
                if (edited.kind == ValueKind::Array && !OriginalTreeMatchesCurrent(edited, current, currentNodes)) {
                    error = "The source save array changed at '" + edited.displayName + "'; reload before saving";
                    return false;
                }
                if (IsUnorderedContainerKind(edited.kind) && !SameContainerIdentities(edited, current)) {
                    error = "The source save container changed at '" + edited.displayName + "'; reload before saving";
                    return false;
                }
                return true;
            }

            if (!edited.editable || !current.editable || edited.kind == ValueKind::Unsupported) {
                error = "Value '" + edited.displayName + "' is read-only";
                return false;
            }
            if (!ScalarEqual(current.value, edited.originalValue)) {
                error = "Value '" + edited.displayName + "' changed in the game; reload before saving";
                return false;
            }
            if (!locations.contains(edited.id)) {
                error = "Value '" + edited.displayName + "' no longer has a writable location";
                return false;
            }
            dirtyLeaves.push_back(&edited);
            return true;
        }

        [[nodiscard]] bool PersistedScalarEqual(const ValueNode& actual, const ValueNode& expected) {
            if (expected.kind != ValueKind::Float) return ScalarEqual(actual.value, expected.value);
            const auto* actualValue = std::get_if<double>(&actual.value);
            const auto* expectedValue = std::get_if<double>(&expected.value);
            if (!actualValue || !expectedValue) return false;
            const auto normalizedExpected = static_cast<double>(static_cast<float>(*expectedValue));
            return (*actualValue == normalizedExpected) || (std::isnan(*actualValue) && std::isnan(normalizedExpected));
        }

        [[nodiscard]] bool ValidatePersistedDocument(
            const Document& persisted, const Document& edited, const std::vector<const ValueNode*>& dirtyLeaves,
            std::string& error
        ) {
            if (persisted.classPath != edited.classPath) {
                error = "The reloaded save has a different class";
                return false;
            }
            ConstNodeMap persistedNodes;
            if (!BuildNodeMap(persisted.root, persistedNodes, error)) return false;
            for (const auto* editedNode : dirtyLeaves) {
                const auto iterator = persistedNodes.find(editedNode->id);
                if (iterator == persistedNodes.end() || iterator->second->kind != editedNode->kind) {
                    error = "Saved value '" + editedNode->displayName + "' could not be found after reloading";
                    return false;
                }
                if (!PersistedScalarEqual(*iterator->second, *editedNode)) {
                    error = "Saved value '" + editedNode->displayName + "' did not survive reloading";
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool ValidateEquivalentNode(
            const ValueNode& expected, const ConstNodeMap& actualNodes, std::string& error
        ) {
            if (!expected.persisted) return true;
            const auto actualIterator = actualNodes.find(expected.id);
            if (actualIterator == actualNodes.end()) {
                error = "Persisted value '" + expected.displayName + "' is missing after reloading";
                return false;
            }
            const auto& actual = *actualIterator->second;
            if (!actual.persisted || actual.kind != expected.kind) {
                error = "Persisted structure changed at '" + expected.displayName + "'";
                return false;
            }
            if (expected.kind == ValueKind::Unsupported || actual.kind == ValueKind::Unsupported) {
                error = "Persisted value '" + expected.displayName + "' cannot be verified safely";
                return false;
            }

            if (IsContainerKind(expected.kind) && actual.children.size() != expected.children.size()) {
                error = "Persisted container size changed at '" + expected.displayName + "'";
                return false;
            }
            if (!IsContainerKind(expected.kind)) {
                const auto expectedChildCount =
                    std::ranges::count_if(expected.children, [](const ValueNode& child) { return child.persisted; });
                const auto actualChildCount =
                    std::ranges::count_if(actual.children, [](const ValueNode& child) { return child.persisted; });
                if (actualChildCount != expectedChildCount) {
                    error = "Persisted child structure changed at '" + expected.displayName + "'";
                    return false;
                }
            }

            for (const auto& expectedChild : expected.children) {
                if (!expectedChild.persisted) continue;
                if (!ValidateEquivalentNode(expectedChild, actualNodes, error)) return false;
            }

            if (expected.children.empty() && !PersistedScalarEqual(actual, expected)) {
                error = "Persisted value '" + expected.displayName + "' changed after reloading";
                return false;
            }
            return true;
        }

        [[nodiscard]] bool ValidateEquivalentDocument(
            const Document& actual, const Document& expected, std::string& error
        ) {
            if (actual.classPath != expected.classPath) {
                error = "The reloaded save has a different class";
                return false;
            }
            ConstNodeMap actualNodes;
            if (!BuildNodeMap(actual.root, actualNodes, error)) return false;
            return ValidateEquivalentNode(expected.root, actualNodes, error);
        }

        using OverlayChildMap = std::unordered_map<std::string_view, const ValueNode*>;

        [[nodiscard]] bool BuildOverlayChildMap(
            const ValueNode& parent, OverlayChildMap& children, std::string& error
        ) {
            children.reserve(parent.children.size());
            for (const auto& child : parent.children) {
                if (!children.emplace(child.rawName, &child).second) {
                    error = "Preset contains duplicate field '" + child.displayName + "'";
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool ValidateOverlayNode(
            const ValueNode& target, const ValueNode& source, bool isRoot, std::string& error
        ) {
            if (target.kind != source.kind) {
                error = "Preset type does not match target type at '" + target.displayName + "'";
                return false;
            }
            if (!isRoot && target.rawName != source.rawName) {
                error = "Preset field does not match target field at '" + target.displayName + "'";
                return false;
            }
            if (source.children.size() != target.children.size()) {
                error = "Preset structure differs at '" + target.displayName + "'";
                return false;
            }
            if (source.children.empty()) return true;

            if (IsUnorderedContainerKind(source.kind)) {
                OverlayChildMap sourceChildren;
                if (!BuildOverlayChildMap(source, sourceChildren, error)) return false;
                for (const auto& targetChild : target.children) {
                    const auto sourceChild = sourceChildren.find(targetChild.rawName);
                    if (sourceChild == sourceChildren.end() ||
                        !ValidateOverlayNode(targetChild, *sourceChild->second, false, error)) {
                        if (error.empty())
                            error = "Preset field is missing at '" + targetChild.displayName + "'";
                        return false;
                    }
                }
                return true;
            }

            for (std::size_t index = 0; index < source.children.size(); ++index) {
                if (!ValidateOverlayNode(target.children[index], source.children[index], false, error)) return false;
            }
            return true;
        }

        void ApplyOverlayNode(ValueNode& target, const ValueNode& source) {
            if (source.children.empty()) {
                if (source.kind != ValueKind::Unsupported && target.editable) target.value = source.value;
                return;
            }
            if (IsUnorderedContainerKind(source.kind)) {
                OverlayChildMap sourceChildren;
                std::string ignored;
                (void)BuildOverlayChildMap(source, sourceChildren, ignored);
                for (auto& targetChild : target.children) {
                    const auto sourceChild = sourceChildren.find(targetChild.rawName);
                    ApplyOverlayNode(targetChild, *sourceChild->second);
                }
                return;
            }
            for (std::size_t index = 0; index < source.children.size(); ++index)
                ApplyOverlayNode(target.children[index], source.children[index]);
        }

        [[nodiscard]] ValueNode* FindNodeImpl(ValueNode& root, NodeId id) {
            if (root.id == id) return &root;
            for (auto& child : root.children) {
                if (auto* found = FindNodeImpl(child, id)) return found;
            }
            return nullptr;
        }

    } // namespace

    std::string_view ExpectedClassForSlot(std::string_view slot) {
        struct ProtectedSlot {
            std::string_view slot;
            std::string_view className;
        };
        constexpr std::array<ProtectedSlot, 5> PROTECTED_SLOTS{{
            {"GameProgress", "SG_GameProgress_C"},
            {"PhotosData", "SG_PhotosDataSaveGame_C"},
            {"Settings", "SG_Settings_C"},
            {"SG Gauntlet Progress", "SG_PlayerProgression_C"},
            {"SG Player Equipment", "SG_Equipment_C"},
        }};
        const auto match = std::ranges::find_if(PROTECTED_SLOTS, [slot](const ProtectedSlot& candidate) {
            return AsciiIEquals(slot, candidate.slot);
        });
        return match == PROTECTED_SLOTS.end() ? std::string_view{} : match->className;
    }

    bool ClassPathMatches(std::string_view classPath, std::string_view expectedClass) {
        if (AsciiIEquals(classPath, expectedClass)) return true;
        const auto dot = classPath.rfind('.');
        if (dot != std::string_view::npos && AsciiIEquals(classPath.substr(dot + 1), expectedClass)) return true;
        const auto space = classPath.rfind(' ');
        return space != std::string_view::npos && AsciiIEquals(classPath.substr(space + 1), expectedClass);
    }

    bool IsProgressSlot(std::string_view slot) {
        return AsciiIEquals(slot, K_PROGRESS_SLOT);
    }

    ValueNode* FindNode(Document& document, NodeId id) {
        return FindNodeImpl(document.root, id);
    }

    bool IsDirty(const ValueNode& node) {
        if (node.editable && !ScalarEqual(node.value, node.originalValue)) return true;
        return std::ranges::any_of(node.children, [](const ValueNode& child) { return IsDirty(child); });
    }

    std::size_t CountDirty(const ValueNode& node) {
        if (node.children.empty()) return node.editable && !ScalarEqual(node.value, node.originalValue) ? 1 : 0;
        std::size_t count = 0;
        for (const auto& child : node.children)
            count += CountDirty(child);
        return count;
    }

    void ResetChanges(ValueNode& node) {
        if (node.editable) node.value = node.originalValue;
        for (auto& child : node.children)
            ResetChanges(child);
    }

    bool OverlayValues(ValueNode& target, const ValueNode& source, std::string& error) {
        error.clear();
        if (!ValidateOverlayNode(target, source, true, error)) return false;
        ApplyOverlayNode(target, source);
        return true;
    }

    bool ValidateSlotName(std::string_view slot, std::string& error) {
        std::wstring wide;
        error.clear();
        return IsValidSlot(slot, wide, error);
    }

    LoadResult LoadDocument(std::string_view slot, int userIndex) {
        LoadResult result;
        std::wstring wideSlot;
        if (!IsValidSlot(slot, wideSlot, result.error)) return result;

        const SDK::FString unrealSlot(wideSlot.c_str());
        auto* save = SDK::UGameplayStatics::LoadGameFromSlot(unrealSlot, userIndex);
        if (!save) {
            result.error = "The game could not open '" + std::string(slot) + "'";
            return result;
        }
        result.document = SnapshotSave(save, slot, userIndex, nullptr, result.error);
        if (!result.error.empty()) return result;
        result.success = true;
        return result;
    }

    namespace {
        template <typename ValueType> struct ApprovedStructSpec;

        template <> struct ApprovedStructSpec<SDK::FStr_Passport_Weapon1> {
            static constexpr std::string_view NAME = "Str_Passport_Weapon1";
            static constexpr std::string_view FULL_NAME =
                "UserDefinedStruct Str_Passport_Weapon1.Str_Passport_Weapon1";
            static constexpr std::size_t REFLECTED_SIZE = 0xF9;
        };

        template <> struct ApprovedStructSpec<SDK::FStr_Passport_Armor1> {
            static constexpr std::string_view NAME = "Str_Passport_Armor1";
            static constexpr std::string_view FULL_NAME =
                "UserDefinedStruct Str_Passport_Armor1.Str_Passport_Armor1";
            static constexpr std::size_t REFLECTED_SIZE = 0xD2;
        };

        template <typename ValueType>
        [[nodiscard]] ValueNode SnapshotApprovedStruct(const ValueType& value, std::string& error) {
            using Spec = ApprovedStructSpec<ValueType>;
            auto* structure = SDK::UObject::FindObject<SDK::UScriptStruct>(
                std::string(Spec::FULL_NAME), SDK::EClassCastFlags::ScriptStruct
            );
            const bool valid =
                structure && structure->GetFullName() == Spec::FULL_NAME && structure->Size > 0 &&
                static_cast<std::size_t>(structure->Size) == Spec::REFLECTED_SIZE &&
                structure->MinAlignment > 0 &&
                static_cast<std::size_t>(structure->MinAlignment) == alignof(ValueType);
            if (!valid) {
                error = "Approved reflected struct '" + std::string(Spec::NAME) + "' is unavailable or incompatible";
                return {};
            }
            return SnapshotRoot(structure, const_cast<ValueType*>(&value), nullptr, nullptr, error);
        }
    } // namespace

    ValueNode SnapshotStruct(const SDK::FStr_Passport_Weapon1& value, std::string& error) {
        return SnapshotApprovedStruct(value, error);
    }

    ValueNode SnapshotStruct(const SDK::FStr_Passport_Armor1& value, std::string& error) {
        return SnapshotApprovedStruct(value, error);
    }

    namespace {

        enum class CommitMode : std::uint8_t {
            EditedValues,
            CompleteDocument,
        };

        class TempSlotGuard {
        public:
            TempSlotGuard(std::wstring slot, int userIndex) : slot(std::move(slot)), userIndex(userIndex) {}

            TempSlotGuard(const TempSlotGuard&) = delete;
            TempSlotGuard& operator=(const TempSlotGuard&) = delete;

            ~TempSlotGuard() {
                if (active) (void)Delete();
            }

            [[nodiscard]] bool Delete() {
                const SDK::FString unrealSlot(slot.c_str());
                if (!SDK::UGameplayStatics::DoesSaveGameExist(unrealSlot, userIndex)) {
                    active = false;
                    return true;
                }
                const bool deleted = SDK::UGameplayStatics::DeleteGameInSlot(unrealSlot, userIndex);
                if (deleted && !SDK::UGameplayStatics::DoesSaveGameExist(unrealSlot, userIndex)) active = false;
                return !active;
            }

        private:
            std::wstring slot;
            int userIndex = 0;
            bool active = true;
        };

        [[nodiscard]] std::wstring MakeTempSlot(int userIndex, std::wstring_view targetSlot) {
            static std::uint64_t sequence = 0;
            for (std::uint32_t attempt = 0; attempt < 128; ++attempt) {
                auto candidate = std::wstring(L"__HSE_SaveEditor_") + std::to_wstring(++sequence);
                if (candidate.size() == targetSlot.size() &&
                    CompareStringOrdinal(
                        candidate.data(), static_cast<int>(candidate.size()), targetSlot.data(),
                        static_cast<int>(targetSlot.size()), TRUE
                    ) == CSTR_EQUAL)
                    continue;
                if (!SDK::UGameplayStatics::DoesSaveGameExist(SDK::FString(candidate.c_str()), userIndex))
                    return candidate;
            }
            return {};
        }

        [[nodiscard]] SDK::USaveGame* LoadSave(const std::wstring& slot, int userIndex) {
            return SDK::UGameplayStatics::LoadGameFromSlot(SDK::FString(slot.c_str()), userIndex);
        }

        [[nodiscard]] bool RestoreTarget(
            const std::wstring& targetSlot, int userIndex, bool targetExisted, SDK::USaveGame* targetBackup,
            const Document* targetBackupDocument, std::string& rollbackError
        ) {
            const SDK::FString slot(targetSlot.c_str());
            if (!targetExisted) {
                if (SDK::UGameplayStatics::DoesSaveGameExist(slot, userIndex) &&
                    !SDK::UGameplayStatics::DeleteGameInSlot(slot, userIndex)) {
                    rollbackError = "could not delete the newly created target slot";
                    return false;
                }
                if (SDK::UGameplayStatics::DoesSaveGameExist(slot, userIndex)) {
                    rollbackError = "the newly created target slot still exists";
                    return false;
                }
                return true;
            }

            if (!targetBackup || !targetBackupDocument ||
                !SDK::UGameplayStatics::SaveGameToSlot(targetBackup, slot, userIndex)) {
                rollbackError = "could not rewrite the previous target save";
                return false;
            }
            auto* restored = LoadSave(targetSlot, userIndex);
            if (!restored || !restored->Class || ObjectPath(restored->Class) != targetBackupDocument->classPath) {
                rollbackError = "the previous target save could not be verified after restoration";
                return false;
            }
            std::string validationError;
            auto restoredDocument =
                SnapshotSave(restored, targetBackupDocument->sourceSlot, userIndex, nullptr, validationError);
            if (!validationError.empty() ||
                !ValidateEquivalentDocument(restoredDocument, *targetBackupDocument, validationError)) {
                rollbackError = "the previous target save changed during restoration";
                if (!validationError.empty()) rollbackError += ": " + validationError;
                return false;
            }
            return true;
        }

        class TargetCommitGuard final {
        public:
            TargetCommitGuard(
                const std::wstring& targetSlot, int userIndex, bool targetExisted, SDK::USaveGame* targetBackup,
                const Document* targetBackupDocument
            )
                : targetSlot(targetSlot),
                  userIndex(userIndex),
                  targetExisted(targetExisted),
                  targetBackup(targetBackup),
                  targetBackupDocument(targetBackupDocument) {}

            TargetCommitGuard(const TargetCommitGuard&) = delete;
            TargetCommitGuard& operator=(const TargetCommitGuard&) = delete;

            ~TargetCommitGuard() noexcept {
                if (!active) return;
                try {
                    std::string ignored;
                    (void)Rollback(ignored);
                } catch (...) {
                    active = false;
                }
            }

            void Disarm() noexcept { active = false; }

            [[nodiscard]] bool Rollback(std::string& rollbackError) {
                if (!active) return true;
                try {
                    if (!RestoreTarget(
                            targetSlot, userIndex, targetExisted, targetBackup, targetBackupDocument, rollbackError
                        )) {
                        return false;
                    }
                    active = false;
                    return true;
                } catch (const std::exception& exception) {
                    rollbackError = std::string("rollback raised an exception: ") + exception.what();
                } catch (...) {
                    rollbackError = "rollback raised an unexpected exception";
                }
                return false;
            }

        private:
            const std::wstring& targetSlot;
            int userIndex = 0;
            bool targetExisted = false;
            SDK::USaveGame* targetBackup = nullptr;
            const Document* targetBackupDocument = nullptr;
            bool active = true;
        };

        [[nodiscard]] SaveResult SaveFailure(std::string error) {
            SaveResult result;
            result.error = std::move(error);
            return result;
        }

    } // namespace

    namespace {
        [[nodiscard]] LiveSyncResult ReloadLiveState(
            const Document& restoredDocument, SDK::UWorld* world
        ) {
            LiveSyncResult result;
            if (!world || restoredDocument.userIndex != 0 || !IsProgressSlot(restoredDocument.sourceSlot) ||
                !AsciiIEquals(ObjectBaseName(restoredDocument.classPath), K_PROGRESS_CLASS)) {
                result.success = false;
                result.error = "The restored progression session could not be identified safely";
                return result;
            }

            auto* gameInstance = SDK::UGameplayStatics::GetGameInstance(world);
            if (!gameInstance || !gameInstance->IsA(SDK::UGI_Settings_C::StaticClass())) {
                result.success = false;
                result.error = "The active progression game instance is unavailable";
                return result;
            }

            auto* settings = static_cast<SDK::UGI_Settings_C*>(gameInstance);
            const std::wstring wideSlot = K_PROGRESS_SLOT_WIDE;
            auto* diskBackup = LoadSave(wideSlot, restoredDocument.userIndex);
            if (!diskBackup || !diskBackup->IsA(SDK::USG_GameProgress_C::StaticClass())) {
                result.success = false;
                result.error = "The restored progression save could not be protected during live validation";
                return result;
            }

            TargetCommitGuard validationGuard(
                wideSlot, restoredDocument.userIndex, true, diskBackup, &restoredDocument
            );
            const auto failAndRestore = [&](std::string error) -> LiveSyncResult {
                std::string rollbackError;
                const bool rolledBack = validationGuard.Rollback(rollbackError);
                if (rolledBack) settings->Load_Game();
                if (!rolledBack) {
                    error += "; rollback validation failed";
                    if (!rollbackError.empty()) error += ": " + rollbackError;
                } else if (!settings->Save_File || !settings->Save_File->IsA(SDK::USG_GameProgress_C::StaticClass())) {
                    error += "; the restored disk state could not be reloaded into the game instance";
                }
                result.success = false;
                result.error = std::move(error);
                return result;
            };

            settings->Load_Game();
            if (!settings->Save_File || !settings->Save_File->IsA(SDK::USG_GameProgress_C::StaticClass()))
                return failAndRestore("The restored progression state could not be reloaded in memory");

            std::string validationError;
            {
                auto liveDocument = SnapshotSave(
                    settings->Save_File, K_PROGRESS_SLOT, restoredDocument.userIndex, nullptr, validationError
                );
                if (!validationError.empty() ||
                    !ValidateEquivalentDocument(liveDocument, restoredDocument, validationError)) {
                    return failAndRestore(
                        validationError.empty() ? "The restored progression state did not match the restored save"
                                                : "Restored progression validation failed: " + validationError
                    );
                }
            }

            const auto* liveProgress = static_cast<SDK::USG_GameProgress_C*>(settings->Save_File);
            if (settings->Player_Funds != liveProgress->Player_Funds)
                return failAndRestore("The restored progression funds cache did not match the restored save");

            settings->Save_Game();
            auto roundTrip = LoadDocument(K_PROGRESS_SLOT, restoredDocument.userIndex);
            validationError.clear();
            if (!roundTrip.success ||
                !ValidateEquivalentDocument(roundTrip.document, restoredDocument, validationError)) {
                return failAndRestore(
                    roundTrip.success ? "Restored progression round-trip validation failed: " + validationError
                                      : "The restored progression round trip could not be reloaded: " + roundTrip.error
                );
            }
            validationGuard.Disarm();
            return result;
        }
    } // namespace

    static LoadResult SynchronizeLiveStateImpl(const Document& savedDocument, SDK::UWorld* world);

    static SaveResult SaveDocumentImpl(
        const Document& document, std::string_view targetSlot, CommitMode mode, SDK::UWorld* liveWorld
    ) {
        // Loaded save objects are consumed synchronously in this single game-thread action and never cross a frame.
        // The generated SDK does not expose UE5's internal AddToRoot/RemoveFromRoot API, so do not yield from here.
        std::wstring wideSourceSlot;
        std::string error;
        if (!IsValidSlot(document.sourceSlot, wideSourceSlot, error)) return SaveFailure(std::move(error));
        std::wstring wideTargetSlot;
        if (!IsValidSlot(targetSlot, wideTargetSlot, error)) return SaveFailure(std::move(error));
        const auto userIndex = document.userIndex;
        const bool progressTarget = IsProgressSlot(targetSlot);
        if (progressTarget && !liveWorld) return SaveFailure("The active game world is unavailable");
        if (document.classPath.empty()) return SaveFailure("The document has no source class identity");
        if (document.root.id != K_ROOT_NODE_ID) return SaveFailure("The document has no reflected root");
        const auto protectedClass = ExpectedClassForSlot(targetSlot);
        if (!protectedClass.empty() && !ClassPathMatches(document.classPath, protectedClass)) {
            return SaveFailure(
                "Protected save slot '" + std::string(targetSlot) + "' requires class '" + std::string(protectedClass) +
                "'"
            );
        }

        const SDK::FString sourceSlot(wideSourceSlot.c_str());
        auto* freshSource = SDK::UGameplayStatics::LoadGameFromSlot(sourceSlot, document.userIndex);
        if (!freshSource || !freshSource->Class) return SaveFailure("The source save could not be reloaded");
        if (ObjectPath(freshSource->Class) != document.classPath)
            return SaveFailure("The source save class changed; reload it before editing");

        Document editedDocument;
        const Document* expectedDocument = &document;
        if (mode == CommitMode::CompleteDocument) {
            auto backupOnDisk = SnapshotSave(freshSource, document.sourceSlot, document.userIndex, nullptr, error);
            if (!error.empty()) return SaveFailure("Could not inspect the backup save: " + error);
            if (backupOnDisk.root.hasPersistentUnsupported)
                return SaveFailure("The backup contains fields that cannot be restored safely");
            if (!ValidateEquivalentDocument(backupOnDisk, document, error))
                return SaveFailure("The backup changed before it could be restored: " + error);
        } else {
            LocationMap locations;
            std::vector<const ValueNode*> dirtyLeaves;
            {
                auto currentDocument =
                    SnapshotSave(freshSource, document.sourceSlot, document.userIndex, &locations, error);
                if (!error.empty()) return SaveFailure("Could not inspect the current source save: " + error);
                if (currentDocument.root.hasPersistentUnsupported)
                    return SaveFailure("The source save contains persisted fields that cannot be validated safely");

                ConstNodeMap currentNodes;
                if (!BuildNodeMap(currentDocument.root, currentNodes, error)) return SaveFailure(std::move(error));
                if (!ValidateEditedNode(document.root, currentNodes, locations, dirtyLeaves, error))
                    return SaveFailure(std::move(error));
            }

            for (const auto* editedNode : dirtyLeaves) {
                auto location = locations.extract(editedNode->id);
                if (location.empty())
                    return SaveFailure("Edited value '" + editedNode->displayName + "' has no writable location");
                if (!ApplyScalar(location.mapped(), *editedNode, freshSource, error)) {
                    return SaveFailure("Could not apply '" + editedNode->displayName + "': " + error);
                }
            }
            LocationMap{}.swap(locations);
            editedDocument = SnapshotSave(freshSource, document.sourceSlot, document.userIndex, nullptr, error);
            if (!error.empty()) return SaveFailure("Could not inspect the edited save in memory: " + error);
            if (!ValidatePersistedDocument(editedDocument, document, dirtyLeaves, error))
                return SaveFailure("Edited save validation failed: " + error);
            if (editedDocument.root.hasPersistentUnsupported)
                return SaveFailure("The edited save contains persisted fields that cannot be validated safely");
            expectedDocument = &editedDocument;
        }

        const SDK::FString targetSlotValue(wideTargetSlot.c_str());
        const bool targetExisted = SDK::UGameplayStatics::DoesSaveGameExist(targetSlotValue, userIndex);
        if (progressTarget && !targetExisted)
            return SaveFailure("Start Progression once before replacing its save");

        SDK::USaveGame* targetBackup = nullptr;
        Document targetBackupDocument;
        if (targetExisted) {
            targetBackup = SDK::UGameplayStatics::LoadGameFromSlot(targetSlotValue, userIndex);
            if (!targetBackup || !targetBackup->Class)
                return SaveFailure("The existing target save could not be loaded safely");
            if (ObjectPath(targetBackup->Class) != document.classPath && protectedClass.empty())
                return SaveFailure("The target slot belongs to a different save class");
            targetBackupDocument = SnapshotSave(targetBackup, targetSlot, userIndex, nullptr, error);
            if (!error.empty()) return SaveFailure("Could not inspect the existing target save: " + error);
            if (targetBackupDocument.root.hasPersistentUnsupported)
                return SaveFailure("The existing target contains persisted fields that cannot be restored safely");
        }

        auto tempSlot = MakeTempSlot(userIndex, wideTargetSlot);
        if (tempSlot.empty()) return SaveFailure("Could not reserve a temporary save slot");
        TempSlotGuard tempGuard(tempSlot, userIndex);
        if (!SDK::UGameplayStatics::SaveGameToSlot(freshSource, SDK::FString(tempSlot.c_str()), userIndex))
            return SaveFailure("The game rejected the temporary save");

        auto* stagedSave = LoadSave(tempSlot, userIndex);
        if (!stagedSave || !stagedSave->Class || ObjectPath(stagedSave->Class) != document.classPath)
            return SaveFailure("The temporary save failed class validation after reloading");
        {
            auto stagedDocument = SnapshotSave(stagedSave, document.sourceSlot, document.userIndex, nullptr, error);
            if (!error.empty()) return SaveFailure("Could not validate the temporary save: " + error);
            if (!ValidateEquivalentDocument(stagedDocument, *expectedDocument, error))
                return SaveFailure("Temporary save validation failed: " + error);
        }

        TargetCommitGuard commitGuard(
            wideTargetSlot, userIndex, targetExisted, targetBackup, targetExisted ? &targetBackupDocument : nullptr
        );
        if (!SDK::UGameplayStatics::SaveGameToSlot(stagedSave, targetSlotValue, userIndex)) {
            std::string rollbackError;
            (void)commitGuard.Rollback(rollbackError);
            error = "The game rejected the target save";
            if (!rollbackError.empty()) error += "; rollback failed: " + rollbackError;
            return SaveFailure(std::move(error));
        }

        auto* finalSave = SDK::UGameplayStatics::LoadGameFromSlot(targetSlotValue, userIndex);
        Document finalDocument;
        if (finalSave && finalSave->Class && ObjectPath(finalSave->Class) == document.classPath)
            finalDocument = SnapshotSave(finalSave, targetSlot, userIndex, nullptr, error);
        else
            error = "The target save failed class validation after reloading";

        if (error.empty() && !ValidateEquivalentDocument(finalDocument, *expectedDocument, error))
            error = "Target save validation failed: " + error;

        if (!error.empty()) {
            std::string rollbackError;
            (void)commitGuard.Rollback(rollbackError);
            if (!rollbackError.empty()) error += "; rollback failed: " + rollbackError;
            return SaveFailure(std::move(error));
        }
        editedDocument = {};

        bool liveStateSynchronized = false;
        if (progressTarget) {
            const auto rollbackLiveState = [&](std::string syncError) -> SaveResult {
                std::string rollbackError;
                const bool rolledBack = commitGuard.Rollback(rollbackError);
                std::string liveRestoreError;
                if (rolledBack) {
                    try {
                        const auto liveRestore = ReloadLiveState(targetBackupDocument, liveWorld);
                        if (!liveRestore.success) liveRestoreError = liveRestore.error;
                    } catch (...) {
                        liveRestoreError = "the active state restore stopped unexpectedly";
                    }
                }
                if (!rolledBack) {
                    syncError += "; rollback failed";
                    if (!rollbackError.empty()) syncError += ": " + rollbackError;
                } else if (!liveRestoreError.empty()) {
                    syncError += "; the save was restored, but the active state could not be restored: " +
                                 liveRestoreError;
                }
                return SaveFailure(std::move(syncError));
            };

            try {
                auto synchronized = SynchronizeLiveStateImpl(finalDocument, liveWorld);
                if (!synchronized.success) {
                    return rollbackLiveState(
                        synchronized.error.empty() ? "The active game state could not be synchronized"
                                                   : std::move(synchronized.error)
                    );
                }
                liveStateSynchronized = true;
                finalDocument = std::move(synchronized.document);
            } catch (...) {
                return rollbackLiveState("The active game state synchronization stopped unexpectedly");
            }
        }

        commitGuard.Disarm();

        SaveResult result;
        result.success = true;
        result.liveStateSynchronized = liveStateSynchronized;
        result.document = std::move(finalDocument);
        if (!tempGuard.Delete()) result.error = "Save succeeded, but the temporary slot could not be deleted";
        return result;
    }

    SaveResult SaveDocumentAndSynchronize(
        const Document& document, std::string_view targetSlot, SDK::UWorld* world
    ) {
        return SaveDocumentImpl(document, targetSlot, CommitMode::EditedValues, world);
    }

    SaveResult RestoreDocumentAndSynchronize(
        const Document& backupDocument, std::string_view targetSlot, SDK::UWorld* world
    ) {
        return SaveDocumentImpl(backupDocument, targetSlot, CommitMode::CompleteDocument, world);
    }

    LiveSyncResult FlushLiveState(const Document& document, std::string_view targetSlot, SDK::UWorld* world) {
        LiveSyncResult result;
        if (!IsProgressSlot(targetSlot)) return result;

        if (document.userIndex != 0 || !AsciiIEquals(ObjectBaseName(document.classPath), K_PROGRESS_CLASS)) {
            result.success = false;
            result.error = "The active progression session could not be identified safely";
            return result;
        }

        auto* gameInstance = world ? SDK::UGameplayStatics::GetGameInstance(world) : nullptr;
        if (!gameInstance || !gameInstance->IsA(SDK::UGI_Settings_C::StaticClass())) {
            result.success = false;
            result.error = "The active progression game instance is unavailable";
            return result;
        }

        const std::wstring wideSlot = K_PROGRESS_SLOT_WIDE;
        auto* diskBackup = LoadSave(wideSlot, document.userIndex);
        if (!diskBackup || !diskBackup->IsA(SDK::USG_GameProgress_C::StaticClass())) {
            result.success = false;
            result.error = "The active progression save could not be backed up in memory";
            return result;
        }
        std::string validationError;
        auto backupDocument = SnapshotSave(diskBackup, "GameProgress", document.userIndex, nullptr, validationError);
        if (!validationError.empty() || backupDocument.root.hasPersistentUnsupported) {
            result.success = false;
            result.error = validationError.empty()
                               ? "The active progression save cannot be rolled back safely"
                               : "Could not inspect the active progression backup: " + validationError;
            return result;
        }

        TargetCommitGuard preFlushGuard(wideSlot, document.userIndex, true, diskBackup, &backupDocument);
        const auto failPreFlush = [&](std::string error) -> LiveSyncResult {
            std::string rollbackError;
            const bool rolledBack = preFlushGuard.Rollback(rollbackError);
            if (!rolledBack) {
                error += "; rollback failed";
                if (!rollbackError.empty()) error += ": " + rollbackError;
            }
            result.success = false;
            result.error = std::move(error);
            return result;
        };

        // GameProgress is mirrored in GI_Settings for the lifetime of the process. Flush that mirror first so
        // SaveDocument can apply only the user's dirty leaves on top of the newest in-session progression state.
        auto* settings = static_cast<SDK::UGI_Settings_C*>(gameInstance);
        settings->Save_Game();

        auto flushed = LoadDocument(K_PROGRESS_SLOT, document.userIndex);
        if (!flushed.success || !AsciiIEquals(ObjectBaseName(flushed.document.classPath), K_PROGRESS_CLASS)) {
            return failPreFlush(
                flushed.error.empty() ? "The active progression state could not be flushed safely"
                                      : "Active progression flush failed: " + flushed.error
            );
        }
        preFlushGuard.Disarm();
        backupDocument = {};

        // The native flush is now the newest authoritative progression state. Never roll back past it: doing so
        // could discard progress that existed only in GI_Settings before this transaction began.
        if (flushed.document.root.hasPersistentUnsupported) {
            result.success = false;
            result.error = "The active progression state was saved, but it cannot be validated completely";
            return result;
        }
        if (!settings->Save_File || !settings->Save_File->IsA(SDK::USG_GameProgress_C::StaticClass())) {
            result.success = false;
            result.error = "The active progression flush did not retain its save object";
            return result;
        }
        auto* flushedBackup = LoadSave(wideSlot, document.userIndex);
        if (!flushedBackup || !flushedBackup->IsA(SDK::USG_GameProgress_C::StaticClass())) {
            result.success = false;
            result.error = "The flushed progression state could not be protected during validation";
            return result;
        }

        TargetCommitGuard stabilizationGuard(wideSlot, document.userIndex, true, flushedBackup, &flushed.document);
        const auto failStabilization = [&](std::string error) -> LiveSyncResult {
            std::string rollbackError;
            const bool rolledBack = stabilizationGuard.Rollback(rollbackError);
            std::string liveRestoreError;
            if (rolledBack) {
                const auto liveRestore = ReloadLiveState(flushed.document, world);
                if (!liveRestore.success) liveRestoreError = liveRestore.error;
            }
            if (!rolledBack) {
                error += "; rollback failed";
                if (!rollbackError.empty()) error += ": " + rollbackError;
            } else if (!liveRestoreError.empty()) {
                error +=
                    "; the flushed save was restored, but its live state could not be verified: " + liveRestoreError;
            }
            result.success = false;
            result.error = std::move(error);
            return result;
        };

        // Stabilize the game's own live-to-disk mapping before overlaying the draft. A full document comparison
        // catches omissions in nested inventories and fight state without comparing a runtime-only cached object.
        settings->Load_Game();
        settings->Save_Game();
        auto stabilized = LoadDocument(K_PROGRESS_SLOT, document.userIndex);
        validationError.clear();
        if (!stabilized.success ||
            !ValidateEquivalentDocument(stabilized.document, flushed.document, validationError)) {
            return failStabilization(
                stabilized.success ? "Active progression flush round-trip failed: " + validationError
                                   : "Active progression flush could not be reloaded: " + stabilized.error
            );
        }
        stabilized.document = {};

        auto* diskSave = LoadSave(K_PROGRESS_SLOT_WIDE, document.userIndex);
        if (!diskSave || !diskSave->IsA(SDK::USG_GameProgress_C::StaticClass())) {
            return failStabilization("The active progression flush could not reload the expected save class");
        }
        const auto* diskProgress = static_cast<SDK::USG_GameProgress_C*>(diskSave);
        if (!settings->Save_File || !settings->Save_File->IsA(SDK::USG_GameProgress_C::StaticClass())) {
            return failStabilization("The active progression flush lost its live save object");
        }
        const auto* liveProgress = static_cast<SDK::USG_GameProgress_C*>(settings->Save_File);
        if (settings->Player_Funds != diskProgress->Player_Funds ||
            liveProgress->Player_Funds != diskProgress->Player_Funds) {
            return failStabilization("The active progression flush did not persist the live funds value");
        }
        stabilizationGuard.Disarm();
        return result;
    }

    static LoadResult SynchronizeLiveStateImpl(const Document& savedDocument, SDK::UWorld* world) {
        LoadResult result;
        if (!world || savedDocument.userIndex != 0 || !IsProgressSlot(savedDocument.sourceSlot) ||
            !AsciiIEquals(ObjectBaseName(savedDocument.classPath), K_PROGRESS_CLASS)) {
            result.success = false;
            result.error = "The active progression session could not be identified safely";
            return result;
        }

        auto* gameInstance = SDK::UGameplayStatics::GetGameInstance(world);
        if (!gameInstance || !gameInstance->IsA(SDK::UGI_Settings_C::StaticClass())) {
            result.success = false;
            result.error = "The active progression game instance is unavailable";
            return result;
        }

        auto* settings = static_cast<SDK::UGI_Settings_C*>(gameInstance);
        settings->Load_Game();
        if (!settings->Save_File || !settings->Save_File->Class) {
            result.success = false;
            result.error = "The active progression state did not reload the saved file";
            return result;
        }

        std::string validationError;
        {
            auto liveDocument =
                SnapshotSave(settings->Save_File, K_PROGRESS_SLOT, savedDocument.userIndex, nullptr, validationError);
            if (!validationError.empty() ||
                !ValidateEquivalentDocument(liveDocument, savedDocument, validationError)) {
                result.success = false;
                result.error = validationError.empty() ? "The active progression state did not match the saved file"
                                                       : "Active progression validation failed: " + validationError;
                return result;
            }
        }

        // Prove the GI mirror write used by later menu transitions cannot overwrite any part of this commit.
        // Keeping the caller's commit guard armed around this round trip makes a mismatch recoverable.
        settings->Save_Game();
        auto roundTrip = LoadDocument(K_PROGRESS_SLOT, savedDocument.userIndex);
        if (!roundTrip.success) {
            result.error = roundTrip.error.empty() ? "The active progression round trip could not be reloaded"
                                                   : "Active progression round trip failed: " + roundTrip.error;
            return result;
        }
        validationError.clear();
        if (!ValidateEquivalentDocument(roundTrip.document, savedDocument, validationError)) {
            result.error = validationError.empty()
                               ? "The active progression state would overwrite the saved file"
                               : "Active progression round-trip validation failed: " + validationError;
            return result;
        }
        return roundTrip;
    }

} // namespace SaveEditorModel
