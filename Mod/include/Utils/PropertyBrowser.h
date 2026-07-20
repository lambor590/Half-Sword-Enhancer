#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Hooks/GameHook.h"
#include "imgui/imgui.h"
#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "Utils/BlueprintRegistry.h"
#include "Utils/GuiUtils.h"

namespace PropertyBrowser {

    inline constexpr float K_SCALAR_WIDTH = 120.0f;
    inline constexpr float K_ENUM_WIDTH = 160.0f;
    inline constexpr float K_VEC2_WIDTH = 180.0f;
    inline constexpr float K_VEC3_WIDTH = 240.0f;

    enum class PropType : uint8_t {
        Float,
        Double,
        Int,
        Bool,
        Byte,
        Enum,
        LinearColor,
        Color,
        Vector2D,
        Vector,
        Rotator,
        Struct,
        Object,
        Name,
        String,
        Text,
        Array,
        Map,
        Set,
        Unsupported
    };

    struct EnumInfo;

    struct PropertyInfo {
        std::string displayName;
        std::string rawName;
        std::string category;
        EnumInfo* enumInfo = nullptr;
        int32_t offset = 0;
        int32_t elementSize = 0;
        PropType type = PropType::Unsupported;
        SDK::UStruct* structType = nullptr;
        std::string typeName;
        uint8_t fieldMask = 0;
        uint8_t byteOffset = 0;
    };

    struct PropertyCategory {
        const std::string* name = nullptr;
        std::vector<const PropertyInfo*> properties;
    };

    struct PropertySchema {
        std::vector<PropertyInfo> properties;
        std::vector<PropertyCategory> categories;
        int editableCount = 0;
    };

    struct EnumInfo {
        std::vector<std::string> names;
        std::vector<std::string> pendingNames;
        std::mutex pendingNamesMutex;
        std::atomic_bool hasPendingNames{false};
        float maxTextWidthEm = 0.0f;
    };

    struct WorldActor {
        SDK::AActor* actor = nullptr;
        std::string className;
        std::string instanceName;
        std::string displayLabel;
        float distanceToPlayer = -1.0f;
    };

    [[nodiscard]] inline std::string CleanPropertyName(const std::string& raw);

    [[nodiscard]] inline std::string BuildActorDisplayLabel(
        std::string_view className, std::string_view instanceName, float distanceToPlayer
    ) {
        std::string label;
        label.reserve(
            className.size() + (instanceName.empty() || instanceName == className ? 0 : instanceName.size() + 3) +
            (distanceToPlayer >= 0.0f ? 35 : 0)
        );
        if (distanceToPlayer >= 0.0f) {
            char distance[32];
            std::snprintf(distance, sizeof(distance), "%.1fm", distanceToPlayer);
            label += distance;
            label += " | ";
        }

        const std::string visibleClass = CleanPropertyName(std::string(className));
        const std::string visibleInstance = CleanPropertyName(std::string(instanceName));
        label += visibleClass;
        if (!visibleInstance.empty() && visibleInstance != visibleClass) {
            label += " | ";
            label += visibleInstance;
        }
        return label;
    }

    [[nodiscard]] inline WorldActor BuildWorldActor(SDK::AActor* actor, const SDK::FVector* playerLocation = nullptr) {
        WorldActor result;
        if (!actor || !actor->Class) return result;

        result.actor = actor;
        result.className = actor->Class->GetName();
        result.instanceName = actor->GetName();
        if (playerLocation) {
            const auto location = actor->K2_GetActorLocation();
            result.distanceToPlayer = static_cast<float>(location.GetDistanceTo(*playerLocation) * 0.01);
        }
        result.displayLabel = BuildActorDisplayLabel(result.className, result.instanceName, result.distanceToPlayer);
        return result;
    }

    [[nodiscard]] inline std::vector<WorldActor> FindWorldActors(SDK::UWorld* world, SDK::AActor* player = nullptr) {
        std::vector<WorldActor> result;
        if (!world) return result;

        SDK::FVector playerLocation{};
        const SDK::FVector* playerLocationPtr = nullptr;
        if (player) {
            playerLocation = player->K2_GetActorLocation();
            playerLocationPtr = &playerLocation;
        }

        auto& levels = world->Levels;
        size_t actorCapacity = 0;
        for (int32_t index = 0; index < levels.Num(); ++index) {
            if (const auto* level = levels[index]) actorCapacity += static_cast<size_t>(level->Actors.Num());
        }
        result.reserve(actorCapacity);
        for (int32_t li = 0; li < levels.Num(); ++li) {
            auto* level = levels[li];
            if (!level) continue;
            auto& actors = level->Actors;
            for (int32_t ai = 0; ai < actors.Num(); ++ai) {
                auto* actor = actors[ai];
                if (!actor || !actor->Class) continue;
                result.push_back(BuildWorldActor(actor, playerLocationPtr));
            }
        }

        std::ranges::sort(result, [](const WorldActor& a, const WorldActor& b) {
            if (a.distanceToPlayer >= 0.0f && b.distanceToPlayer >= 0.0f && a.distanceToPlayer != b.distanceToPlayer)
                return a.distanceToPlayer < b.distanceToPlayer;
            if (a.className != b.className) return a.className < b.className;
            return a.instanceName < b.instanceName;
        });

        return result;
    }

    [[nodiscard]] inline std::string CleanPropertyName(const std::string& raw) {
        std::string cleaned = raw;

        // Strip trailing numeric suffix like "_21"
        if (cleaned.size() > 1) {
            size_t i = cleaned.size();
            while (i > 0 && std::isdigit(static_cast<unsigned char>(cleaned[i - 1])))
                --i;
            if (i > 0 && i < cleaned.size() && cleaned[i - 1] == '_') cleaned.erase(i - 1);
        }

        const auto readable = BlueprintRegistry::CleanDisplayName(cleaned);
        return readable.empty() ? "Setting" : readable;
    }

    [[nodiscard]] inline std::string ExtractCategory(const std::string& raw) {
        auto pos = raw.find('_');
        if (pos == std::string::npos || pos == 0) return "General";
        return CleanPropertyName(raw.substr(0, pos));
    }

    [[nodiscard]] inline std::string ShortEnumValueName(const std::string& fullName) {
        auto colonPos = fullName.rfind(':');
        std::string shortName = colonPos != std::string::npos ? fullName.substr(colonPos + 1) : fullName;
        constexpr std::string_view GENERATED_PREFIX = "NewEnumerator";
        if (shortName.starts_with(GENERATED_PREFIX)) {
            shortName = "Option " + shortName.substr(GENERATED_PREFIX.size());
        }
        return CleanPropertyName(shortName);
    }

    [[nodiscard]] inline bool IsEnumSentinel(std::string_view fullName) {
        if (const auto separator = fullName.rfind("::"); separator != std::string_view::npos)
            fullName.remove_prefix(separator + 2);
        return fullName == "MAX" || fullName.ends_with("_MAX");
    }

    [[nodiscard]] inline std::string FindUserDefinedEnumDisplayName(
        SDK::UUserDefinedEnum* udEnum, const SDK::FName& valueName, const std::string& fullName,
        const std::string& shortName
    ) {
        if (!udEnum) return {};

        auto& displayMap = udEnum->DisplayNameMap;
        for (int32_t i = 0; i < displayMap.NumAllocated(); ++i) {
            if (!displayMap.IsValidIndex(i)) continue;

            auto& pair = displayMap[i];
            const std::string key = pair.Key().ToString();
            if (pair.Key() != valueName && key != fullName && key != shortName) continue;

            std::string display = pair.Value().ToString();
            if (!display.empty()) return display;
        }

        return {};
    }

    [[nodiscard]] inline bool IsEditable(PropType type) noexcept {
        switch (type) {
            case PropType::Float:
            case PropType::Double:
            case PropType::Int:
            case PropType::Bool:
            case PropType::Byte:
            case PropType::Enum:
            case PropType::LinearColor:
            case PropType::Color:
            case PropType::Vector2D:
            case PropType::Vector:
            case PropType::Rotator:
            case PropType::Struct: return true;
            default: return false;
        }
    }

    [[nodiscard]] inline bool IsVisible(PropType type) noexcept {
        return type != PropType::Unsupported;
    }

    [[nodiscard]] inline bool IsLiveObject(const SDK::UObject* object) {
        if (!object || !object->Class || object->Index < 0) return false;
        if (SDK::UObject::GObjects->GetByIndex(object->Index) != object) return false;
        return !(object->Flags & SDK::EObjectFlags::BeginDestroyed) &&
               !(object->Flags & SDK::EObjectFlags::FinishDestroyed);
    }

    [[nodiscard]] inline std::string FieldClassName(const SDK::FProperty* prop) {
        return prop && prop->ClassPrivate ? prop->ClassPrivate->Name.ToString() : "Unsupported";
    }

    [[nodiscard]] inline PropType ClassifyProperty(
        SDK::FProperty* prop, SDK::UEnum*& outEnum, SDK::UStruct*& outStruct, SDK::UClass*& outObjectClass
    ) {
        outEnum = nullptr;
        outStruct = nullptr;
        outObjectClass = nullptr;
        if (!prop || !prop->ClassPrivate) return PropType::Unsupported;
        auto castFlags = static_cast<SDK::EClassCastFlags>(prop->ClassPrivate->CastFlags);

        if (castFlags & SDK::EClassCastFlags::BoolProperty) return PropType::Bool;
        if (castFlags & SDK::EClassCastFlags::FloatProperty) return PropType::Float;
        if (castFlags & SDK::EClassCastFlags::DoubleProperty) return PropType::Double;
        if (castFlags & SDK::EClassCastFlags::IntProperty) return PropType::Int;

        if (castFlags & SDK::EClassCastFlags::ByteProperty) {
            auto* bp = static_cast<SDK::FByteProperty*>(prop);
            if (bp->Enum) {
                outEnum = bp->Enum;
                return PropType::Enum;
            }
            return PropType::Byte;
        }

        if (castFlags & SDK::EClassCastFlags::EnumProperty) {
            auto* ep = static_cast<SDK::FEnumProperty*>(prop);
            outEnum = ep->Enum;
            return PropType::Enum;
        }

        if (castFlags & SDK::EClassCastFlags::NameProperty) return PropType::Name;
        if (castFlags & SDK::EClassCastFlags::StrProperty) return PropType::String;
        if (castFlags & SDK::EClassCastFlags::TextProperty) return PropType::Text;

        if (castFlags & SDK::EClassCastFlags::ObjectPropertyBase) {
            auto* op = static_cast<SDK::FObjectPropertyBase*>(prop);
            outObjectClass = op->PropertyClass;
            return PropType::Object;
        }

        if (castFlags & SDK::EClassCastFlags::ArrayProperty) return PropType::Array;
        if (castFlags & SDK::EClassCastFlags::MapProperty) return PropType::Map;
        if (castFlags & SDK::EClassCastFlags::SetProperty) return PropType::Set;

        if (castFlags & SDK::EClassCastFlags::StructProperty) {
            auto* sp = static_cast<SDK::FStructProperty*>(prop);
            if (sp->Struct) {
                std::string structName = sp->Struct->GetName();
                if (structName == "LinearColor") return PropType::LinearColor;
                if (structName == "Color") return PropType::Color;
                if (structName == "Vector2D") return PropType::Vector2D;
                if (structName == "Vector") return PropType::Vector;
                if (structName == "Rotator") return PropType::Rotator;
                if (sp->Struct->ChildProperties) {
                    outStruct = sp->Struct;
                    return PropType::Struct;
                }
            }
        }

        return PropType::Unsupported;
    }

    inline void UpdateEnumTextWidth(EnumInfo& info) {
        float maxWidth = ImGui::CalcTextSize("Unknown").x;
        for (const auto& name : info.names)
            maxWidth = (std::max)(maxWidth, ImGui::CalcTextSize(name.c_str()).x);
        info.maxTextWidthEm = maxWidth / (std::max)(1.0f, ImGui::GetFontSize());
    }

    [[nodiscard]] inline std::vector<std::string> BuildEnumNames(SDK::UEnum* enumPtr, bool queryNative) {
        std::vector<std::string> names;
        if (!enumPtr) return names;

        names.reserve(static_cast<size_t>(enumPtr->Names.Num()));
        auto* userDefined = enumPtr->IsA(SDK::UUserDefinedEnum::StaticClass())
                                ? static_cast<SDK::UUserDefinedEnum*>(enumPtr)
                                : nullptr;
        for (const auto& enumValue : enumPtr->Names) {
            const std::string fullName = enumValue.Key().GetRawString();
            if (IsEnumSentinel(fullName)) continue;

            std::string displayName;
            const auto numericValue = enumValue.Value();
            if (queryNative && numericValue >= 0 && numericValue <= 255) {
                displayName = SDK::UKismetNodeHelperLibrary::GetEnumeratorUserFriendlyName(
                                  enumPtr, static_cast<SDK::uint8>(numericValue)
                ).ToString();
            }

            if (displayName.empty()) {
                const std::string shortName = ShortEnumValueName(fullName);
                displayName = FindUserDefinedEnumDisplayName(userDefined, enumValue.Key(), fullName, shortName);
                if (displayName.empty()) displayName = shortName;
            }
            names.push_back(std::move(displayName));
        }
        return names;
    }

    [[nodiscard]] inline std::vector<std::string> ResolveEnumNamesOnGameThread(SDK::UEnum* enumPtr) {
        return BuildEnumNames(enumPtr, true);
    }

    inline void ApplyPendingEnumNames(EnumInfo& info) {
        if (!info.hasPendingNames.load(std::memory_order_acquire)) return;
        {
            std::lock_guard lock(info.pendingNamesMutex);
            info.names = std::move(info.pendingNames);
            info.hasPendingNames.store(false, std::memory_order_release);
        }
        UpdateEnumTextWidth(info);
    }

    [[nodiscard]] inline EnumInfo& GetEnumInfo(SDK::UEnum* enumPtr) {
        static std::unordered_map<SDK::UEnum*, EnumInfo> cache;
        static EnumInfo empty;
        if (!enumPtr) return empty;
        auto [cacheIt, inserted] = cache.try_emplace(enumPtr);
        if (!inserted) return cacheIt->second;
        auto& info = cacheIt->second;
        info.names = BuildEnumNames(enumPtr, false);
        UpdateEnumTextWidth(info);
        (void)GameHook::QueueAction([enumPtr, info = &info](const RuntimeContextSnapshot&) {
            if (!IsLiveObject(enumPtr)) return;
            auto names = ResolveEnumNamesOnGameThread(enumPtr);
            if (names.empty()) return;

            std::lock_guard lock(info->pendingNamesMutex);
            info->pendingNames = std::move(names);
            info->hasPendingNames.store(true, std::memory_order_release);
        });
        return info;
    }

    [[nodiscard]] inline const PropertySchema& GetPropertySchema(SDK::UStruct* ustruct) {
        static std::unordered_map<SDK::UStruct*, PropertySchema> cache;
        static const PropertySchema empty;
        if (!ustruct) return empty;
        if (auto it = cache.find(ustruct); it != cache.end()) return it->second;

        PropertySchema schema;

        for (auto* s = ustruct; s; s = s->SuperStruct) {
            std::string sName = s->GetName();
            if (sName == "Actor" || sName == "Object") break;

            for (auto* field = s->ChildProperties; field; field = field->Next) {
                auto* prop = static_cast<SDK::FProperty*>(field);

                SDK::UEnum* enumPtr = nullptr;
                SDK::UStruct* structPtr = nullptr;
                SDK::UClass* objectClass = nullptr;
                PropType type = ClassifyProperty(prop, enumPtr, structPtr, objectClass);

                std::string rawName = prop->Name.ToString();

                PropertyInfo info;
                info.displayName = CleanPropertyName(rawName);
                info.category = ExtractCategory(rawName);
                info.rawName = std::move(rawName);
                info.offset = prop->Offset;
                info.elementSize = prop->ElementSize;
                info.type = type;
                info.structType = structPtr;
                info.typeName = FieldClassName(prop);
                if (structPtr) info.typeName = structPtr->GetName();
                if (objectClass) info.typeName = objectClass->GetName();

                if (type == PropType::Bool) {
                    auto* bp = static_cast<SDK::FBoolProperty*>(prop);
                    info.fieldMask = bp->FieldMask;
                    info.byteOffset = bp->ByteOffset;
                }

                if (type == PropType::Enum) {
                    auto& enumInfo = GetEnumInfo(enumPtr);
                    info.enumInfo = &enumInfo;
                }

                schema.properties.push_back(std::move(info));
            }
        }

        std::ranges::sort(schema.properties, [](const PropertyInfo& a, const PropertyInfo& b) {
            if (a.category != b.category) return a.category < b.category;
            return a.rawName < b.rawName;
        });

        auto cacheIt = cache.emplace(ustruct, std::move(schema)).first;
        auto& cached = cacheIt->second;
        for (const auto& property : cached.properties) {
            if (IsEditable(property.type)) ++cached.editableCount;
            if (cached.categories.empty() || *cached.categories.back().name != property.category)
                cached.categories.push_back({.name = &property.category});
            cached.categories.back().properties.push_back(&property);
        }
        return cached;
    }

    [[nodiscard]] inline bool PropertyMatchesFilter(const PropertyInfo& prop, const char* filter, size_t filterLen);

    [[nodiscard]] inline bool StructMatchesFilter(
        SDK::UStruct* structType, const char* filter, size_t filterLen, int depth = 0
    ) {
        if (!structType || filterLen == 0 || depth > 6) return false;

        const auto& schema = GetPropertySchema(structType);
        for (const auto& prop : schema.properties) {
            if (PropertyMatchesFilter(prop, filter, filterLen)) return true;
            if (prop.type == PropType::Struct && StructMatchesFilter(prop.structType, filter, filterLen, depth + 1))
                return true;
        }
        return false;
    }

    [[nodiscard]] inline bool PropertyMatchesFilter(const PropertyInfo& prop, const char* filter, size_t filterLen) {
        if (filterLen == 0) return true;
        if (GuiUtils::MatchesFilter(prop.displayName.c_str(), prop.displayName.size(), filter, filterLen)) return true;
        if (GuiUtils::MatchesFilter(prop.rawName.c_str(), prop.rawName.size(), filter, filterLen)) return true;
        if (GuiUtils::MatchesFilter(prop.typeName.c_str(), prop.typeName.size(), filter, filterLen)) return true;
        return prop.type == PropType::Struct && StructMatchesFilter(prop.structType, filter, filterLen);
    }

    inline bool DragDouble3(const char* label, double* d, float speed, const char* fmt) {
        float tmp[3] = {static_cast<float>(d[0]), static_cast<float>(d[1]), static_cast<float>(d[2])};
        ImGui::SetNextItemWidth(K_VEC3_WIDTH);
        bool committed = GuiUtils::DebouncedDragFloat3(label, tmp, speed, 0.0f, 0.0f, fmt);
        if (ImGui::IsItemEdited()) std::copy_n(tmp, 3, d);
        return committed;
    }

    inline bool DragDouble2(const char* label, double* d, float speed, const char* fmt) {
        float tmp[2] = {static_cast<float>(d[0]), static_cast<float>(d[1])};
        ImGui::SetNextItemWidth(K_VEC2_WIDTH);
        ImGui::DragFloat2(label, tmp, speed, 0.0f, 0.0f, fmt);
        bool committed = ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::IsItemEdited()) std::copy_n(tmp, 2, d);
        return committed;
    }

    inline bool RenderPropertyWidget(const PropertyInfo& prop, std::byte* objectBytes);

    inline bool RenderStructWidget(const PropertyInfo& prop, std::byte* structBytes) {
        if (!prop.structType) {
            ImGui::TextDisabled("%s: can't be edited", prop.displayName.c_str());
            return false;
        }

        const auto& schema = GetPropertySchema(prop.structType);

        char label[192];
        std::snprintf(label, sizeof(label), "%s (%d)", prop.displayName.c_str(), schema.editableCount);
        const bool open = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_None);
        if (!open) return false;

        bool changed = false;
        for (const auto& category : schema.categories) {
            char categoryLabel[160];
            std::snprintf(
                categoryLabel, sizeof(categoryLabel), "%s (%zu)", category.name->c_str(), category.properties.size()
            );
            if (!ImGui::TreeNodeEx(categoryLabel, ImGuiTreeNodeFlags_DefaultOpen)) continue;

            for (const auto* nested : category.properties) {
                if (!IsVisible(nested->type)) continue;
                changed |= RenderPropertyWidget(*nested, structBytes);
            }

            ImGui::TreePop();
        }

        ImGui::TreePop();
        return changed;
    }

    inline void RenderReadOnlyTextValue(const PropertyInfo& prop, const std::string& value) {
        ImGui::TextDisabled("%s: %s", prop.displayName.c_str(), value.empty() ? "Empty" : value.c_str());
    }

    inline bool RenderPropertyWidget(const PropertyInfo& prop, std::byte* objectBytes) {
        auto* valuePtr = reinterpret_cast<uint8_t*>(objectBytes + prop.offset);
        bool changed = false;

        ImGui::PushID(prop.rawName.c_str());

        switch (prop.type) {
            case PropType::Float: {
                ImGui::SetNextItemWidth(K_SCALAR_WIDTH);
                changed = GuiUtils::DebouncedDragFloat(
                    prop.displayName.c_str(), reinterpret_cast<float*>(valuePtr), 0.01f, 0.0f, 0.0f, "%.4f"
                );
                break;
            }
            case PropType::Double: {
                ImGui::SetNextItemWidth(K_SCALAR_WIDTH);
                changed = GuiUtils::DebouncedDragScalar(
                    prop.displayName.c_str(), ImGuiDataType_Double, reinterpret_cast<double*>(valuePtr), 0.01f, nullptr,
                    nullptr, "%.4f"
                );
                break;
            }
            case PropType::Int: {
                ImGui::SetNextItemWidth(K_SCALAR_WIDTH);
                changed =
                    GuiUtils::DebouncedDragInt(prop.displayName.c_str(), reinterpret_cast<int32_t*>(valuePtr), 1.0f);
                break;
            }
            case PropType::Bool: {
                uint8_t* byte = valuePtr + prop.byteOffset;
                bool val = (*byte & prop.fieldMask) != 0;
                if (ImGui::Checkbox(prop.displayName.c_str(), &val)) {
                    if (val)
                        *byte |= prop.fieldMask;
                    else
                        *byte &= ~prop.fieldMask;
                    changed = true;
                }
                break;
            }
            case PropType::Byte: {
                int intVal = *valuePtr;
                ImGui::SetNextItemWidth(K_SCALAR_WIDTH);
                changed = GuiUtils::DebouncedDragInt(prop.displayName.c_str(), &intVal, 1.0f, 0, 255);
                if (ImGui::IsItemEdited()) *valuePtr = static_cast<uint8_t>(intVal);
                break;
            }
            case PropType::Enum: {
                int intVal =
                    (prop.elementSize <= 1) ? static_cast<int>(*valuePtr) : *reinterpret_cast<int32_t*>(valuePtr);

                if (prop.enumInfo) ApplyPendingEnumNames(*prop.enumInfo);
                if (prop.enumInfo && !prop.enumInfo->names.empty()) {
                    const auto& names = prop.enumInfo->names;
                    const char* preview = (intVal >= 0 && intVal < static_cast<int>(names.size()))
                                              ? names[static_cast<size_t>(intVal)].c_str()
                                              : "Unknown";
                    const float comboWidth =
                        (std::max)(K_ENUM_WIDTH,
                                   GuiUtils::ComboWidthFromText(prop.enumInfo->maxTextWidthEm * ImGui::GetFontSize()));
                    if (GuiUtils::BeginSizedCombo(prop.displayName.c_str(), preview, comboWidth)) {
                        for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                            if (ImGui::Selectable(names[static_cast<size_t>(i)].c_str(), i == intVal)) {
                                if (prop.elementSize <= 1)
                                    *valuePtr = static_cast<uint8_t>(i);
                                else
                                    *reinterpret_cast<int32_t*>(valuePtr) = i;
                                changed = true;
                            }
                            if (i == intVal) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                } else {
                    ImGui::SetNextItemWidth(K_SCALAR_WIDTH);
                    changed = GuiUtils::DebouncedDragInt(prop.displayName.c_str(), &intVal, 1.0f, 0, 255);
                    if (ImGui::IsItemEdited()) {
                        if (prop.elementSize <= 1)
                            *valuePtr = static_cast<uint8_t>(intVal);
                        else
                            *reinterpret_cast<int32_t*>(valuePtr) = intVal;
                    }
                }
                break;
            }
            case PropType::LinearColor: {
                GuiUtils::SetNextColorFieldWidth(prop.displayName.c_str());
                changed = ImGui::ColorEdit4(prop.displayName.c_str(), reinterpret_cast<float*>(valuePtr));
                break;
            }
            case PropType::Color: {
                uint8_t* bytes = valuePtr;
                float col[4] = {bytes[2] / 255.0f, bytes[1] / 255.0f, bytes[0] / 255.0f, bytes[3] / 255.0f};
                GuiUtils::SetNextColorFieldWidth(prop.displayName.c_str());
                if (ImGui::ColorEdit4(prop.displayName.c_str(), col)) {
                    bytes[2] = static_cast<uint8_t>(col[0] * 255.0f);
                    bytes[1] = static_cast<uint8_t>(col[1] * 255.0f);
                    bytes[0] = static_cast<uint8_t>(col[2] * 255.0f);
                    bytes[3] = static_cast<uint8_t>(col[3] * 255.0f);
                    changed = true;
                }
                break;
            }
            case PropType::Vector2D:
                changed = DragDouble2(prop.displayName.c_str(), reinterpret_cast<double*>(valuePtr), 0.1f, "%.2f");
                break;
            case PropType::Vector:
                changed = DragDouble3(prop.displayName.c_str(), reinterpret_cast<double*>(valuePtr), 0.1f, "%.2f");
                break;
            case PropType::Rotator:
                changed = DragDouble3(prop.displayName.c_str(), reinterpret_cast<double*>(valuePtr), 0.5f, "%.1f");
                break;
            case PropType::Struct: changed = RenderStructWidget(prop, reinterpret_cast<std::byte*>(valuePtr)); break;
            case PropType::Object: {
                auto* object = *reinterpret_cast<SDK::UObject**>(valuePtr);
                const bool live = IsLiveObject(object);
                std::string value = live ? CleanPropertyName(object->GetName()) : "None";
                RenderReadOnlyTextValue(prop, value);
                break;
            }
            case PropType::Name:
                RenderReadOnlyTextValue(prop, reinterpret_cast<SDK::FName*>(valuePtr)->ToString());
                break;
            case PropType::String:
                RenderReadOnlyTextValue(prop, reinterpret_cast<SDK::FString*>(valuePtr)->ToString());
                break;
            case PropType::Text: {
                auto* text = reinterpret_cast<SDK::FText*>(valuePtr);
                RenderReadOnlyTextValue(prop, text && text->TextData ? text->ToString() : "");
                break;
            }
            case PropType::Array:
            case PropType::Map:
            case PropType::Set: ImGui::TextDisabled("%s: view only", prop.displayName.c_str()); break;
            case PropType::Unsupported: break;
        }

        ImGui::PopID();
        return changed;
    }

    struct PanelState {
        const PropertySchema* schema = nullptr;
        std::vector<PropertyCategory> visibleCategories;
        std::string visibleFilter;
        char filterBuffer[128] = "";
        bool visiblePropertiesReady = false;
        int expandState = 0;

        void Clear() {
            schema = nullptr;
            InvalidateVisibleProperties();
        }

        void SetType(SDK::UStruct* type) {
            schema = &GetPropertySchema(type);
            InvalidateVisibleProperties();
        }

        [[nodiscard]] int EditableCount() const { return schema ? schema->editableCount : 0; }

        void InvalidateVisibleProperties() {
            visibleCategories.clear();
            visibleFilter.clear();
            visiblePropertiesReady = false;
        }

        [[nodiscard]] size_t PrepareVisibleProperties() {
            const size_t filterLength = std::strlen(filterBuffer);
            if (visiblePropertiesReady && visibleFilter.size() == filterLength &&
                std::memcmp(visibleFilter.data(), filterBuffer, filterLength) == 0)
                return filterLength;

            visibleCategories.clear();
            visibleFilter.assign(filterBuffer, filterLength);
            visiblePropertiesReady = true;
            if (!schema) return filterLength;
            for (const auto& category : schema->categories) {
                PropertyCategory visible{.name = category.name};
                visible.properties.reserve(category.properties.size());
                for (const auto* property : category.properties) {
                    if (!IsVisible(property->type)) continue;
                    if (filterLength > 0 && !PropertyMatchesFilter(*property, filterBuffer, filterLength)) continue;
                    visible.properties.push_back(property);
                }
                if (!visible.properties.empty()) visibleCategories.push_back(std::move(visible));
            }
            return filterLength;
        }
    };

    template <typename OnPropertyChanged>
    inline void RenderPanel(
        PanelState& state, std::byte* objectBytes, const char* filterId, const char* childId,
        OnPropertyChanged onPropertyChanged, bool showNoMatches = false, bool spaceBeforeList = false
    ) {
        if (!objectBytes) return;

        ImGui::SeparatorText("Detailed Settings");
        const float buttonWidth = ImGui::CalcTextSize("+").x + ImGui::GetStyle().FramePadding.x * 2;
        const float buttonsWidth = buttonWidth * 2 + ImGui::GetStyle().ItemSpacing.x;
        GuiUtils::SetNextInputWidth(
            ImGui::GetContentRegionAvail().x - buttonsWidth - ImGui::GetStyle().ItemSpacing.x
        );
        if (ImGui::InputTextWithHint(filterId, "Search settings...", state.filterBuffer, sizeof(state.filterBuffer)))
            state.visiblePropertiesReady = false;
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(buttonWidth, 0))) state.expandState = 1;
        GuiUtils::HelpTooltip("Expand all groups");
        ImGui::SameLine();
        if (ImGui::Button("-", ImVec2(buttonWidth, 0))) state.expandState = -1;
        GuiUtils::HelpTooltip("Collapse all groups");

        if (spaceBeforeList) ImGui::Spacing();
        const size_t filterLength = state.PrepareVisibleProperties();
        ImGui::BeginChild(childId, ImVec2(0, 0), ImGuiChildFlags_None);
        for (const auto& category : state.visibleCategories) {
            char label[128];
            std::snprintf(label, sizeof(label), "%s (%zu)", category.name->c_str(), category.properties.size());
            if (state.expandState != 0) ImGui::SetNextItemOpen(state.expandState > 0);
            if (!ImGui::TreeNodeEx(label, filterLength > 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0)) continue;
            for (const auto* property : category.properties)
                if (RenderPropertyWidget(*property, objectBytes)) onPropertyChanged();
            ImGui::TreePop();
        }
        state.expandState = 0;
        if (showNoMatches && state.visibleCategories.empty()) ImGui::TextDisabled("No matching settings");
        ImGui::EndChild();
    }

    inline void RenderPanel(
        PanelState& state, std::byte* objectBytes, const char* filterId, const char* childId,
        bool showNoMatches = false, bool spaceBeforeList = false
    ) {
        RenderPanel(state, objectBytes, filterId, childId, [] {}, showNoMatches, spaceBeforeList);
    }

} // namespace PropertyBrowser
