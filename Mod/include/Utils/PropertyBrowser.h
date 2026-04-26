#pragma once

#include <algorithm>
#include <cstddef>
#include <cctype>
#include <cstdint>
#include <map>
#include <ranges>
#include <string>
#include <vector>

#include "imgui/imgui.h"
#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"

namespace PropertyBrowser {

    inline constexpr float K_SCALAR_WIDTH = 120.0f;
    inline constexpr float K_ENUM_WIDTH = 160.0f;
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
        Vector,
        Rotator,
        Unsupported
    };

    struct PropertyInfo {
        std::string displayName;
        std::string rawName;
        std::string category;
        std::vector<std::string> enumNames;
        int32_t offset = 0;
        int32_t elementSize = 0;
        PropType type = PropType::Unsupported;
        uint8_t fieldMask = 0;
        uint8_t byteOffset = 0;
    };

    struct WorldActor {
        SDK::AActor* actor;
        std::string className;
    };

    [[nodiscard]] inline std::vector<WorldActor> FindWorldActors(SDK::UWorld* world) {
        std::vector<WorldActor> result;
        if (!world) return result;
        auto& levels = world->Levels;
        for (int32_t li = 0; li < levels.Num(); ++li) {
            auto* level = levels[li];
            if (!level) continue;
            auto& actors = level->Actors;
            for (int32_t ai = 0; ai < actors.Num(); ++ai) {
                auto* actor = actors[ai];
                if (!actor || !actor->Class) continue;
                result.push_back({actor, actor->Class->GetName()});
            }
        }
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

    [[nodiscard]] inline PropType ClassifyProperty(SDK::FProperty* prop, SDK::UEnum*& outEnum) {
        outEnum = nullptr;
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

        if (castFlags & SDK::EClassCastFlags::StructProperty) {
            auto* sp = static_cast<SDK::FStructProperty*>(prop);
            if (sp->Struct) {
                std::string structName = sp->Struct->GetName();
                if (structName == "LinearColor") return PropType::LinearColor;
                if (structName == "Color") return PropType::Color;
                if (structName == "Vector") return PropType::Vector;
                if (structName == "Rotator") return PropType::Rotator;
            }
        }

        return PropType::Unsupported;
    }

    [[nodiscard]] inline std::vector<std::string> BuildEnumNames(SDK::UEnum* enumPtr) {
        std::vector<std::string> names;
        if (!enumPtr) return names;

        SDK::UUserDefinedEnum* udEnum =
            enumPtr->IsA(SDK::UUserDefinedEnum::StaticClass()) ? static_cast<SDK::UUserDefinedEnum*>(enumPtr) : nullptr;

        auto& enumNames = enumPtr->Names;
        for (int32_t i = 0; i < enumNames.Num(); ++i) {
            auto& valueName = enumNames[i].Key();

            std::string fullName = valueName.ToString();
            if (fullName.size() >= 4 && fullName.compare(fullName.size() - 4, 4, "_MAX") == 0) continue;

            if (udEnum) {
                auto& displayMap = udEnum->DisplayNameMap;
                bool found = false;
                for (int32_t mi = 0; mi < displayMap.NumAllocated(); ++mi) {
                    if (!displayMap.IsValidIndex(mi)) continue;
                    auto& pair = displayMap[mi];
                    if (pair.Key().ComparisonIndex == valueName.ComparisonIndex) {
                        names.push_back(pair.Value().ToString());
                        found = true;
                        break;
                    }
                }
                if (found) continue;
            }

            auto colonPos = fullName.rfind(':');
            names.push_back(colonPos != std::string::npos ? fullName.substr(colonPos + 1) : fullName);
        }
        return names;
    }

    [[nodiscard]] inline std::vector<PropertyInfo> EnumerateProperties(SDK::UStruct* ustruct) {
        std::vector<PropertyInfo> result;
        if (!ustruct) return result;

        for (auto* s = ustruct; s; s = s->SuperStruct) {
            std::string sName = s->GetName();
            if (sName == "Actor" || sName == "Object") break;

            for (auto* field = s->ChildProperties; field; field = field->Next) {
                auto* prop = static_cast<SDK::FProperty*>(field);

                SDK::UEnum* enumPtr = nullptr;
                PropType type = ClassifyProperty(prop, enumPtr);

                std::string rawName = prop->Name.ToString();

                PropertyInfo info;
                info.rawName = rawName;
                info.displayName = CleanPropertyName(rawName);
                info.category = ExtractCategory(rawName);
                info.offset = prop->Offset;
                info.elementSize = prop->ElementSize;
                info.type = type;

                if (type == PropType::Bool) {
                    auto* bp = static_cast<SDK::FBoolProperty*>(prop);
                    info.fieldMask = bp->FieldMask;
                    info.byteOffset = bp->ByteOffset;
                }

                if (type == PropType::Enum) info.enumNames = BuildEnumNames(enumPtr);

                result.push_back(std::move(info));
            }
        }

        std::ranges::sort(result, [](const PropertyInfo& a, const PropertyInfo& b) {
            if (a.category != b.category) return a.category < b.category;
            return a.rawName < b.rawName;
        });

        return result;
    }

    using CategoryMap = std::map<std::string, std::vector<const PropertyInfo*>>;

    [[nodiscard]] inline CategoryMap GroupByCategory(const std::vector<PropertyInfo>& props) {
        CategoryMap map;
        for (const auto& p : props)
            map[p.category].push_back(&p);
        return map;
    }

    inline bool DragDouble3(const char* label, double* d, float speed, const char* fmt) {
        float tmp[3] = {static_cast<float>(d[0]), static_cast<float>(d[1]), static_cast<float>(d[2])};
        ImGui::SetNextItemWidth(K_VEC3_WIDTH);
        if (ImGui::DragFloat3(label, tmp, speed, 0.0f, 0.0f, fmt)) {
            d[0] = tmp[0];
            d[1] = tmp[1];
            d[2] = tmp[2];
            return true;
        }
        return false;
    }

    inline bool RenderPropertyWidget(const PropertyInfo& prop, std::byte* objectBytes) {
        auto* valuePtr = reinterpret_cast<uint8_t*>(objectBytes + prop.offset);
        bool changed = false;

        ImGui::PushID(prop.rawName.c_str());

        switch (prop.type) {
            case PropType::Float: {
                ImGui::SetNextItemWidth(K_SCALAR_WIDTH);
                changed = ImGui::DragFloat(
                    prop.displayName.c_str(), reinterpret_cast<float*>(valuePtr), 0.01f, 0.0f, 0.0f, "%.4f"
                );
                break;
            }
            case PropType::Double: {
                ImGui::SetNextItemWidth(K_SCALAR_WIDTH);
                changed = ImGui::DragScalar(
                    prop.displayName.c_str(), ImGuiDataType_Double, reinterpret_cast<double*>(valuePtr), 0.01f, nullptr,
                    nullptr, "%.4f"
                );
                break;
            }
            case PropType::Int: {
                ImGui::SetNextItemWidth(K_SCALAR_WIDTH);
                changed = ImGui::DragInt(prop.displayName.c_str(), reinterpret_cast<int32_t*>(valuePtr), 1.0f, 0, 0);
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
                if (ImGui::DragInt(prop.displayName.c_str(), &intVal, 1.0f, 0, 255)) {
                    *valuePtr = static_cast<uint8_t>(intVal);
                    changed = true;
                }
                break;
            }
            case PropType::Enum: {
                int intVal =
                    (prop.elementSize <= 1) ? static_cast<int>(*valuePtr) : *reinterpret_cast<int32_t*>(valuePtr);

                if (!prop.enumNames.empty()) {
                    const char* preview = (intVal >= 0 && intVal < static_cast<int>(prop.enumNames.size()))
                                              ? prop.enumNames[intVal].c_str()
                                              : "Unknown";
                    ImGui::SetNextItemWidth(K_ENUM_WIDTH);
                    if (ImGui::BeginCombo(prop.displayName.c_str(), preview)) {
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
                    if (ImGui::DragInt(prop.displayName.c_str(), &intVal, 1.0f, 0, 255)) {
                        if (prop.elementSize <= 1)
                            *valuePtr = static_cast<uint8_t>(intVal);
                        else
                            *reinterpret_cast<int32_t*>(valuePtr) = intVal;
                        changed = true;
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
            case PropType::Vector:
                changed = DragDouble3(prop.displayName.c_str(), reinterpret_cast<double*>(valuePtr), 0.1f, "%.2f");
                break;
            case PropType::Rotator:
                changed = DragDouble3(prop.displayName.c_str(), reinterpret_cast<double*>(valuePtr), 0.5f, "%.1f");
                break;
            case PropType::Unsupported: break;
        }

        ImGui::PopID();
        return changed;
    }

} // namespace PropertyBrowser
