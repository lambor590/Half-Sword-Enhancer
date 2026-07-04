#pragma once

#include <algorithm>
#include <cstddef>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "imgui/imgui.h"
#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"
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

    struct PropertyInfo {
        std::string displayName;
        std::string rawName;
        std::string category;
        std::vector<std::string> enumNames;
        int32_t offset = 0;
        int32_t elementSize = 0;
        float enumComboWidth = K_ENUM_WIDTH;
        PropType type = PropType::Unsupported;
        SDK::UStruct* structType = nullptr;
        std::string typeName;
        uint8_t fieldMask = 0;
        uint8_t byteOffset = 0;
    };

    struct WorldActor {
        SDK::AActor* actor = nullptr;
        std::string className;
        std::string instanceName;
        std::string displayLabel;
        float distanceToPlayer = -1.0f;
    };

    [[nodiscard]] inline std::string BuildActorDisplayLabel(
        std::string_view className, std::string_view instanceName, float distanceToPlayer
    ) {
        std::string label;
        if (distanceToPlayer >= 0.0f) {
            char distance[32];
            std::snprintf(distance, sizeof(distance), "%.1fm", distanceToPlayer);
            label += distance;
            label += " | ";
        }

        label += className;
        if (!instanceName.empty() && instanceName != className) {
            label += " | ";
            label += instanceName;
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
        result.displayLabel =
            BuildActorDisplayLabel(result.className, result.instanceName, result.distanceToPlayer);
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
            if (a.distanceToPlayer >= 0.0f && b.distanceToPlayer >= 0.0f &&
                a.distanceToPlayer != b.distanceToPlayer)
                return a.distanceToPlayer < b.distanceToPlayer;
            if (a.className != b.className) return a.className < b.className;
            return a.instanceName < b.instanceName;
        });

        return result;
    }

    [[nodiscard]] inline bool IsHexChar(char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    }

    [[nodiscard]] inline std::string CleanPropertyName(const std::string& raw) {
        std::string cleaned = raw;

        // Strip trailing Dumper-7 hash suffix like "_0EB204DF"
        if (cleaned.size() > 9 && cleaned[cleaned.size() - 9] == '_') {
            bool allHex = true;
            for (size_t i = cleaned.size() - 8; i < cleaned.size(); ++i) {
                if (!IsHexChar(cleaned[i])) {
                    allHex = false;
                    break;
                }
            }
            if (allHex) cleaned.erase(cleaned.size() - 9);
        }

        // Strip trailing numeric suffix like "_21"
        if (cleaned.size() > 1) {
            size_t i = cleaned.size();
            while (i > 0 && std::isdigit(static_cast<unsigned char>(cleaned[i - 1])))
                --i;
            if (i > 0 && i < cleaned.size() && cleaned[i - 1] == '_') cleaned.erase(i - 1);
        }

        for (char& c : cleaned) {
            if (c == '_') c = ' ';
        }
        return cleaned;
    }

    [[nodiscard]] inline std::string ExtractCategory(const std::string& raw) {
        auto pos = raw.find('_');
        if (pos == std::string::npos || pos == 0) return "General";
        return raw.substr(0, pos);
    }

    [[nodiscard]] inline std::string ShortEnumValueName(const std::string& fullName) {
        auto colonPos = fullName.rfind(':');
        return colonPos != std::string::npos ? fullName.substr(colonPos + 1) : fullName;
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

    [[nodiscard]] inline std::vector<std::string> BuildEnumNames(SDK::UEnum* enumPtr) {
        static std::unordered_map<SDK::UEnum*, std::vector<std::string>> cache;
        std::vector<std::string> names;
        if (!enumPtr) return names;
        if (auto it = cache.find(enumPtr); it != cache.end()) return it->second;

        SDK::UUserDefinedEnum* udEnum =
            enumPtr->IsA(SDK::UUserDefinedEnum::StaticClass()) ? static_cast<SDK::UUserDefinedEnum*>(enumPtr) : nullptr;

        auto& enumNames = enumPtr->Names;
        for (int32_t i = 0; i < enumNames.Num(); ++i) {
            auto& valueName = enumNames[i].Key();

            std::string fullName = valueName.ToString();
            if (fullName.size() >= 4 && fullName.compare(fullName.size() - 4, 4, "_MAX") == 0) continue;
            std::string shortName = ShortEnumValueName(fullName);

            if (udEnum) {
                std::string displayName = FindUserDefinedEnumDisplayName(udEnum, valueName, fullName, shortName);
                if (!displayName.empty()) {
                    names.push_back(std::move(displayName));
                    continue;
                }
            }

            names.push_back(std::move(shortName));
        }
        auto [it, _] = cache.emplace(enumPtr, std::move(names));
        return it->second;
    }

    [[nodiscard]] inline std::vector<PropertyInfo> EnumerateProperties(SDK::UStruct* ustruct) {
        static std::unordered_map<SDK::UStruct*, std::vector<PropertyInfo>> cache;
        if (!ustruct) return {};
        if (auto it = cache.find(ustruct); it != cache.end()) return it->second;

        std::vector<PropertyInfo> result;

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
                info.rawName = rawName;
                info.displayName = CleanPropertyName(rawName);
                info.category = ExtractCategory(rawName);
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
                    info.enumNames = BuildEnumNames(enumPtr);

                    float maxW = ImGui::CalcTextSize("Unknown").x;
                    for (const auto& name : info.enumNames) {
                        const float width = ImGui::CalcTextSize(name.c_str()).x;
                        if (width > maxW) maxW = width;
                    }
                    info.enumComboWidth = GuiUtils::ComboWidthFromText(maxW);
                    if (info.enumComboWidth < K_ENUM_WIDTH) info.enumComboWidth = K_ENUM_WIDTH;
                }

                result.push_back(std::move(info));
            }
        }

        std::ranges::sort(result, [](const PropertyInfo& a, const PropertyInfo& b) {
            if (a.category != b.category) return a.category < b.category;
            return a.rawName < b.rawName;
        });

        auto [it, _] = cache.emplace(ustruct, std::move(result));
        return it->second;
    }

    using CategoryMap = std::map<std::string, std::vector<const PropertyInfo*>>;

    [[nodiscard]] inline CategoryMap GroupByCategory(const std::vector<PropertyInfo>& props) {
        CategoryMap map;
        for (const auto& p : props)
            map[p.category].push_back(&p);
        return map;
    }

    [[nodiscard]] inline bool PropertyMatchesFilter(const PropertyInfo& prop, const char* filter, size_t filterLen);

    [[nodiscard]] inline bool StructMatchesFilter(
        SDK::UStruct* structType, const char* filter, size_t filterLen, int depth = 0
    ) {
        if (!structType || filterLen == 0 || depth > 6) return false;

        auto props = EnumerateProperties(structType);
        for (const auto& prop : props) {
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
            ImGui::TextDisabled("%s: unsupported struct", prop.displayName.c_str());
            return false;
        }

        auto props = EnumerateProperties(prop.structType);
        int editableCount = 0;
        for (const auto& nested : props)
            if (IsEditable(nested.type)) ++editableCount;

        char label[192];
        std::snprintf(label, sizeof(label), "%s (%d)", prop.displayName.c_str(), editableCount);
        const bool open = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_None);
        if (!open) return false;

        bool changed = false;
        auto categories = GroupByCategory(props);
        for (auto& [category, categoryProps] : categories) {
            char categoryLabel[160];
            std::snprintf(categoryLabel, sizeof(categoryLabel), "%s (%zu)", category.c_str(), categoryProps.size());
            if (!ImGui::TreeNodeEx(categoryLabel, ImGuiTreeNodeFlags_DefaultOpen)) continue;

            for (const auto* nested : categoryProps) {
                if (!IsVisible(nested->type)) continue;
                changed |= RenderPropertyWidget(*nested, structBytes);
            }

            ImGui::TreePop();
        }

        ImGui::TreePop();
        return changed;
    }

    inline void RenderReadOnlyTextValue(const PropertyInfo& prop, const std::string& value) {
        ImGui::TextDisabled("%s: %s", prop.displayName.c_str(), value.empty() ? "(empty)" : value.c_str());
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

                if (!prop.enumNames.empty()) {
                    const char* preview = (intVal >= 0 && intVal < static_cast<int>(prop.enumNames.size()))
                                              ? prop.enumNames[intVal].c_str()
                                              : "Unknown";
                    if (GuiUtils::BeginSizedCombo(prop.displayName.c_str(), preview, prop.enumComboWidth)) {
                        for (int i = 0; i < static_cast<int>(prop.enumNames.size()); ++i) {
                            if (ImGui::Selectable(prop.enumNames[i].c_str(), i == intVal)) {
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
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.75f);
                changed = ImGui::ColorEdit4(prop.displayName.c_str(), reinterpret_cast<float*>(valuePtr));
                ImGui::PopItemWidth();
                break;
            }
            case PropType::Color: {
                uint8_t* bytes = valuePtr;
                float col[4] = {bytes[2] / 255.0f, bytes[1] / 255.0f, bytes[0] / 255.0f, bytes[3] / 255.0f};
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.75f);
                if (ImGui::ColorEdit4(prop.displayName.c_str(), col)) {
                    bytes[2] = static_cast<uint8_t>(col[0] * 255.0f);
                    bytes[1] = static_cast<uint8_t>(col[1] * 255.0f);
                    bytes[0] = static_cast<uint8_t>(col[2] * 255.0f);
                    bytes[3] = static_cast<uint8_t>(col[3] * 255.0f);
                    changed = true;
                }
                ImGui::PopItemWidth();
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
            case PropType::Struct:
                changed = RenderStructWidget(prop, reinterpret_cast<std::byte*>(valuePtr));
                break;
            case PropType::Object: {
                auto* object = *reinterpret_cast<SDK::UObject**>(valuePtr);
                const bool live = IsLiveObject(object);
                std::string value = live ? object->GetName() : "(null)";
                RenderReadOnlyTextValue(prop, value);
                if (live && ImGui::IsItemHovered()) {
                    GuiUtils::BeginStyledTooltip();
                    ImGui::TextUnformatted(object->GetFullName().c_str());
                    GuiUtils::EndStyledTooltip();
                }
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
            case PropType::Set:
                ImGui::TextDisabled("%s: %s container", prop.displayName.c_str(), prop.typeName.c_str());
                break;
            case PropType::Unsupported: break;
        }

        ImGui::PopID();
        return changed;
    }

} // namespace PropertyBrowser
