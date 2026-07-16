#include "Menu/Preset.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>

#include "SimpleIni.h"
#include "Utils/PresetUtils.h"

// Preset data types implemented in this translation unit.
#include "Utils/PlayerPresetSerializer.h"
#include "Utils/NPCPresetSerializer.h"
#include "Utils/WeaponPresetSerializer.h"
#include "Utils/ArmorPresetSerializer.h"
#include "Utils/LoadoutPresetSerializer.h"

namespace {
    void AddInvalidValue(std::string& error, std::string_view, std::string_view, const char*) {
        if (!error.empty()) return;
        error = "This preset contains an invalid value";
    }

    int ReadInt(
        std::string& error, const CSimpleIniA& ini, std::string_view section, const char* key, int defaultValue
    ) {
        const char* raw = ini.GetValue(std::string(section).c_str(), key, nullptr);
        int result = defaultValue;
        if (!PresetUtils::TryParseInt(raw ? raw : "", result)) AddInvalidValue(error, section, key, raw);
        return result;
    }

    int ReadRangedInt(
        std::string& error, const CSimpleIniA& ini, std::string_view section, const char* key, int defaultValue,
        int minimum, int maximum
    ) {
        const int result = ReadInt(error, ini, section, key, defaultValue);
        if (result < minimum || result > maximum) {
            if (error.empty()) error = "This preset contains a value outside the allowed range";
            return defaultValue;
        }
        return result;
    }

    double ReadDouble(
        std::string& error, const CSimpleIniA& ini, std::string_view section, const char* key, double defaultValue
    ) {
        const char* raw = ini.GetValue(std::string(section).c_str(), key, nullptr);
        double result = defaultValue;
        if (!PresetUtils::TryParseDouble(raw ? raw : "", result)) AddInvalidValue(error, section, key, raw);
        return result;
    }

    bool ReadBool(
        std::string& error, const CSimpleIniA& ini, std::string_view section, const char* key, bool defaultValue
    ) {
        const char* raw = ini.GetValue(std::string(section).c_str(), key, nullptr);
        bool result = defaultValue;
        if (!PresetUtils::TryParseBool(raw ? raw : "", result)) AddInvalidValue(error, section, key, raw);
        return result;
    }

    SDK::FLinearColor ReadColor(
        std::string& error, const CSimpleIniA& ini, std::string_view section, const char* key,
        SDK::FLinearColor defaultValue
    ) {
        const char* raw = ini.GetValue(std::string(section).c_str(), key, nullptr);
        if (!raw || !raw[0]) {
            AddInvalidValue(error, section, key, raw);
            return defaultValue;
        }
        SDK::FLinearColor result{};
        if (!PresetUtils::TryStringToColor(raw, result)) {
            AddInvalidValue(error, section, key, raw);
            return defaultValue;
        }
        return result;
    }

    bool TrySplitExact(std::string_view value, char delimiter, std::span<std::string_view> fields) {
        if (fields.empty()) return value.empty();

        std::size_t start = 0;
        for (std::size_t index = 0; index + 1 < fields.size(); ++index) {
            const std::size_t end = value.find(delimiter, start);
            if (end == std::string_view::npos) return false;
            fields[index] = value.substr(start, end - start);
            start = end + 1;
        }
        if (value.find(delimiter, start) != std::string_view::npos) return false;
        fields.back() = value.substr(start);
        return true;
    }

    PresetOperationResult PresetValidationFailure(std::string_view kind) {
        return {
            .success = false,
            .error = std::string(kind) + " preset contains an invalid value",
        };
    }

    bool IsFinite(const SDK::FVector& value) {
        return std::isfinite(value.X) && std::isfinite(value.Y) && std::isfinite(value.Z);
    }

    bool IsFinite(const SDK::FRotator& value) {
        return std::isfinite(value.Pitch) && std::isfinite(value.Yaw) && std::isfinite(value.Roll);
    }

    bool IsFinite(const SDK::FLinearColor& value) {
        return std::isfinite(value.R) && std::isfinite(value.G) && std::isfinite(value.B) && std::isfinite(value.A);
    }

    bool ContainsIniControlCharacter(const std::string& value) {
        return !PresetUtils::IsSafeIniValue(value);
    }

    PresetOperationResult ValidateFinitePresetFields(
        std::string_view kind, std::span<const PresetFieldDescriptor> fields
    ) {
        for (const auto& field : fields) {
            bool valid = true;
            switch (field.type) {
                case PresetFieldType::String:
                    valid = !ContainsIniControlCharacter(*static_cast<const std::string*>(field.value));
                    break;
                case PresetFieldType::Double: valid = std::isfinite(*static_cast<const double*>(field.value)); break;
                case PresetFieldType::Vec3: valid = IsFinite(*static_cast<const SDK::FVector*>(field.value)); break;
                case PresetFieldType::Rotator: valid = IsFinite(*static_cast<const SDK::FRotator*>(field.value)); break;
                case PresetFieldType::Color:
                    valid = IsFinite(*static_cast<const SDK::FLinearColor*>(field.value));
                    break;
                case PresetFieldType::Int:
                case PresetFieldType::Bool: break;
            }
            if (!valid) return PresetValidationFailure(kind);
        }
        return {.success = true};
    }

    PresetOperationResult ValidateFinitePresetOverrides(
        std::string_view kind, std::span<const PresetOverrideDescriptor> fields
    ) {
        for (const auto& descriptor : fields) {
            const auto& field = descriptor.field;
            if (field.type == OverrideFieldType::Double && !std::isfinite(*static_cast<const double*>(field.value)))
                return PresetValidationFailure(kind);
        }
        return {.success = true};
    }
}

// Unified field serialize/deserialize

std::string PresetSectionName(std::string_view prefix, std::string_view section) {
    if (prefix.empty()) return std::string(section);
    if (section.empty()) return std::string(prefix);
    return std::string(prefix) + "." + std::string(section);
}

void SerializePresetFields(
    std::span<const PresetFieldDescriptor> fields, CSimpleIniA& ini, std::string_view sectionPrefix
) {
    std::string_view currentSection;
    std::string section;
    for (const auto& f : fields) {
        if (currentSection != f.section) {
            currentSection = f.section;
            section = PresetSectionName(sectionPrefix, currentSection);
        }
        switch (f.type) {
            case PresetFieldType::String: {
                const auto& str = *static_cast<std::string*>(f.value);
                ini.SetValue(section.c_str(), f.key, str.c_str());
                break;
            }
            case PresetFieldType::Int:
                ini.SetValue(section.c_str(), f.key, std::to_string(*static_cast<int*>(f.value)).c_str());
                break;
            case PresetFieldType::Double:
                ini.SetValue(
                    section.c_str(), f.key, PresetUtils::FormatFloating(*static_cast<double*>(f.value)).c_str()
                );
                break;
            case PresetFieldType::Bool:
                ini.SetValue(section.c_str(), f.key, *static_cast<bool*>(f.value) ? "1" : "0");
                break;
            case PresetFieldType::Vec3:
                ini.SetValue(
                    section.c_str(), f.key, PresetUtils::VecToString(*static_cast<SDK::FVector*>(f.value)).c_str()
                );
                break;
            case PresetFieldType::Rotator:
                ini.SetValue(
                    section.c_str(), f.key, PresetUtils::RotToString(*static_cast<SDK::FRotator*>(f.value)).c_str()
                );
                break;
            case PresetFieldType::Color:
                ini.SetValue(
                    section.c_str(), f.key,
                    PresetUtils::ColorToString(*static_cast<SDK::FLinearColor*>(f.value)).c_str()
                );
                break;
        }
    }
}

bool DeserializePresetFields(
    std::span<const PresetFieldDescriptor> fields, const CSimpleIniA& ini, std::string_view sectionPrefix,
    std::string* error
) {
    auto invalid = [&](const PresetFieldDescriptor&, std::string_view, const char*) {
        if (error) *error = "This preset contains an invalid value";
        return false;
    };

    std::string_view currentSection;
    std::string section;
    for (const auto& f : fields) {
        if (currentSection != f.section) {
            currentSection = f.section;
            section = PresetSectionName(sectionPrefix, currentSection);
        }
        const char* raw = ini.GetValue(section.c_str(), f.key, nullptr);

        if (!raw) return invalid(f, section, raw);

        switch (f.type) {
            case PresetFieldType::String:
                if (raw && PresetUtils::IsSafeIniValue(raw))
                    *static_cast<std::string*>(f.value) = raw;
                else
                    return invalid(f, section, raw);
                break;
            case PresetFieldType::Int: {
                int parsed = 0;
                if (PresetUtils::TryParseInt(raw ? raw : "", parsed))
                    *static_cast<int*>(f.value) = parsed;
                else
                    return invalid(f, section, raw);
                break;
            }
            case PresetFieldType::Double: {
                double parsed = 0.0;
                if (PresetUtils::TryParseDouble(raw ? raw : "", parsed))
                    *static_cast<double*>(f.value) = parsed;
                else
                    return invalid(f, section, raw);
                break;
            }
            case PresetFieldType::Bool: {
                bool parsed = false;
                if (PresetUtils::TryParseBool(raw ? raw : "", parsed))
                    *static_cast<bool*>(f.value) = parsed;
                else
                    return invalid(f, section, raw);
                break;
            }
            case PresetFieldType::Vec3: {
                SDK::FVector parsed{1.0, 1.0, 1.0};
                if (PresetUtils::TryStringToVec(raw, parsed))
                    *static_cast<SDK::FVector*>(f.value) = parsed;
                else
                    return invalid(f, section, raw);
                break;
            }
            case PresetFieldType::Rotator: {
                SDK::FRotator parsed{0.0, 0.0, 0.0};
                if (PresetUtils::TryStringToRot(raw, parsed))
                    *static_cast<SDK::FRotator*>(f.value) = parsed;
                else
                    return invalid(f, section, raw);
                break;
            }
            case PresetFieldType::Color: {
                SDK::FLinearColor parsed{0.5f, 0.5f, 0.5f, 1.0f};
                if (PresetUtils::TryStringToColor(raw, parsed))
                    *static_cast<SDK::FLinearColor*>(f.value) = parsed;
                else
                    return invalid(f, section, raw);
                break;
            }
        }
    }
    return true;
}

void SerializePresetOverrides(
    std::span<const PresetOverrideDescriptor> fields, CSimpleIniA& ini, std::string_view sectionPrefix
) {
    std::string_view currentSection;
    std::string section;
    for (const auto& descriptor : fields) {
        if (currentSection != descriptor.section) {
            currentSection = descriptor.section;
            section = PresetSectionName(sectionPrefix, currentSection);
        }
        const auto& field = descriptor.field;
        std::string value = *field.enabled ? "1," : "0,";
        switch (field.type) {
            case OverrideFieldType::Double:
                value += PresetUtils::FormatFloating(*static_cast<double*>(field.value));
                break;
            case OverrideFieldType::Int: value += std::to_string(*static_cast<int*>(field.value)); break;
            case OverrideFieldType::Bool: value += *static_cast<bool*>(field.value) ? "1" : "0"; break;
        }
        ini.SetValue(section.c_str(), field.name, value.c_str());
    }
}

bool DeserializePresetOverrides(
    std::span<const PresetOverrideDescriptor> fields, const CSimpleIniA& ini, std::string_view sectionPrefix,
    std::string* error
) {
    std::string_view currentSection;
    std::string section;
    for (const auto& descriptor : fields) {
        if (currentSection != descriptor.section) {
            currentSection = descriptor.section;
            section = PresetSectionName(sectionPrefix, currentSection);
        }
        const auto& field = descriptor.field;
        const char* raw = ini.GetValue(section.c_str(), field.name, nullptr);
        if (!raw || !raw[0]) {
            *field.enabled = false;
            switch (field.type) {
                case OverrideFieldType::Double: *static_cast<double*>(field.value) = 0.0; break;
                case OverrideFieldType::Int: *static_cast<int*>(field.value) = 0; break;
                case OverrideFieldType::Bool: *static_cast<bool*>(field.value) = false; break;
            }
            if (error) *error = "This preset contains an invalid setting";
            return false;
        }

        const std::string_view text(raw);
        const auto comma = text.find(',');
        bool enabled = false;
        bool valid = comma != std::string_view::npos && comma > 0 && comma + 1 < text.size() &&
                     PresetUtils::TryParseBool(text.substr(0, comma), enabled);
        if (valid) {
            const auto value = text.substr(comma + 1);
            switch (field.type) {
                case OverrideFieldType::Double:
                    valid = PresetUtils::TryParseDouble(value, *static_cast<double*>(field.value));
                    break;
                case OverrideFieldType::Int:
                    valid = PresetUtils::TryParseInt(value, *static_cast<int*>(field.value));
                    break;
                case OverrideFieldType::Bool:
                    valid = PresetUtils::TryParseBool(value, *static_cast<bool*>(field.value));
                    break;
            }
        }
        if (valid) {
            *field.enabled = enabled;
        } else {
            *field.enabled = false;
            if (error) *error = "This preset contains an invalid setting";
            return false;
        }
    }
    return true;
}

PresetOperationResult ValidatePresetOverrideValuesForSave(
    std::span<const PresetOverrideDescriptor> fields, std::string_view presetKind
) {
    return ValidateFinitePresetOverrides(presetKind, fields);
}

// PlayerPresetData descriptors

std::array<PresetOverrideDescriptor, 57> PlayerPresetData::GetPresetOverrides(PlayerPresetData& data) {
    auto& o = data.overrides;
    return {{
        {"Physical", OverrideField("heightRate", o.heightRate)},
        {"Physical", OverrideField("muscleRate", o.muscleRate)},
        {"Physical", OverrideField("scaleMutationInhibitor", o.scaleMutationInhibitor)},
        {"Health", OverrideField("health", o.health)},
        {"Health", OverrideField("headHealth", o.headHealth)},
        {"Health", OverrideField("neckHealth", o.neckHealth)},
        {"Health", OverrideField("armRHealth", o.armRHealth)},
        {"Health", OverrideField("armLHealth", o.armLHealth)},
        {"Health", OverrideField("bodyUpperHealth", o.bodyUpperHealth)},
        {"Health", OverrideField("bodyLowerHealth", o.bodyLowerHealth)},
        {"Health", OverrideField("legRHealth", o.legRHealth)},
        {"Health", OverrideField("legLHealth", o.legLHealth)},
        {"Health", OverrideField("backHealth", o.backHealth)},
        {"Health", OverrideField("consciousness", o.consciousness)},
        {"Health", OverrideField("regenRate", o.regenRate)},
        {"Physics", OverrideField("allBodyTonus", o.allBodyTonus)},
        {"Physics", OverrideField("headTonus", o.headTonus)},
        {"Physics", OverrideField("armRTonus", o.armRTonus)},
        {"Physics", OverrideField("armLTonus", o.armLTonus)},
        {"Physics", OverrideField("legRTonus", o.legRTonus)},
        {"Physics", OverrideField("legLTonus", o.legLTonus)},
        {"Physics", OverrideField("musclePower", o.musclePower)},
        {"Physics", OverrideField("orientationStrength", o.orientationStrength)},
        {"Physics", OverrideField("angularStrength", o.angularStrength)},
        {"Physics", OverrideField("hitRigidity", o.hitRigidity)},
        {"Movement", OverrideField("runningSpeedRate", o.runningSpeedRate)},
        {"Movement", OverrideField("walkSpeedRateRun", o.walkSpeedRateRun)},
        {"Movement", OverrideField("jumpRate", o.jumpRate)},
        {"Movement", OverrideField("dodgeRate", o.dodgeRate)},
        {"Movement", OverrideField("crawlRate", o.crawlRate)},
        {"Movement", OverrideField("getUpRate", o.getUpRate)},
        {"Movement", OverrideField("fallenRate", o.fallenRate)},
        {"Combat", OverrideField("damageRate", o.damageRate)},
        {"Combat", OverrideField("limbDamageRate", o.limbDamageRate)},
        {"Combat", OverrideField("dismemberThreshold", o.dismemberThreshold)},
        {"Combat", OverrideField("stamina", o.stamina)},
        {"Combat", OverrideField("staminaBurnSwingR", o.staminaBurnSwingR)},
        {"Combat", OverrideField("staminaBurnSwingL", o.staminaBurnSwingL)},
        {"Combat", OverrideField("staminaBurnDodge", o.staminaBurnDodge)},
        {"Combat", OverrideField("grabForceR", o.grabForceR)},
        {"Combat", OverrideField("grabForceL", o.grabForceL)},
        {"Combat", OverrideField("handsRigidity", o.handsRigidity)},
        {"Combat", OverrideField("bodySkill", o.bodySkill)},
        {"Combat", OverrideField("weaponSkill", o.weaponSkill)},
        {"Skills", OverrideField("thrust", o.skillThrust)},
        {"Skills", OverrideField("parry", o.skillParry)},
        {"Skills", OverrideField("altGrip", o.skillAltGrip)},
        {"Skills", OverrideField("altStance", o.skillAltStance)},
        {"Skills", OverrideField("rotate", o.skillRotate)},
        {"Skills", OverrideField("crouch", o.skillCrouch)},
        {"Skills", OverrideField("dodge", o.skillDodge)},
        {"Skills", OverrideField("kick", o.skillKick)},
        {"Skills", OverrideField("slomo", o.skillSlomo)},
        {"State", OverrideField("exhaustion", o.exhaustion)},
        {"State", OverrideField("drunk", o.drunk)},
        {"State", OverrideField("fear", o.fear)},
        {"State", OverrideField("invulnerable", o.invulnerable)},
    }};
}

PresetOperationResult PlayerPresetData::ValidateForSave() const {
    return ValidatePresetOverrideValuesForSave(GetPresetOverrides(const_cast<PlayerPresetData&>(*this)), "Player");
}

// NPCPresetData descriptors

std::array<PresetFieldDescriptor, 10> NPCPresetData::GetPresetFields(NPCPresetData& data) {
    return {
        PresetField::Int("Generator", "npcType", &data.npcTypeIndex),
        PresetField::Int("Generator", "nationality", &data.nationality),
        PresetField::Int("Generator", "tier", &data.tier),
        PresetField::Bool("Generator", "mercenary", &data.mercenary),
        PresetField::Bool("Spawn", "bodyguard", &data.bodyguard),
        PresetField::Int("Spawn", "team", &data.team),
        PresetField::Double("Spawn", "distanceForward", &data.spawnDistanceForward),
        PresetField::Double("Spawn", "distanceUp", &data.spawnDistanceUp),
        PresetField::Double("Spawn", "scale", &data.spawnScale),
        PresetField::Bool("Spawn", "snapToGround", &data.snapToGround),
    };
}

std::array<PresetOverrideDescriptor, 28> NPCPresetData::GetPresetOverrides(NPCPresetData& data) {
    auto& o = data.overrides;
    return {{
        {"Physical", OverrideField("heightRate", o.heightRate)},
        {"Physical", OverrideField("muscleRate", o.muscleRate)},
        {"Physical", OverrideField("scaleMutationInhibitor", o.scaleMutationInhibitor)},
        {"Physical", OverrideField("faceType", o.faceType)},
        {"Physical", OverrideField("eyeColor", o.eyeColor)},
        {"Physical", OverrideField("hairLength", o.hairLength)},
        {"Physical", OverrideField("hairColor", o.hairColor)},
        {"Combat", OverrideField("damageRate", o.damageRate)},
        {"Combat", OverrideField("limbDamageRate", o.limbDamageRate)},
        {"Combat", OverrideField("dismemberThreshold", o.dismemberThreshold)},
        {"Combat", OverrideField("regenRate", o.regenRate)},
        {"Combat", OverrideField("aiInvincibility", o.aiInvincibility)},
        {"Combat", OverrideField("aiArmorInvincibility", o.aiArmorInvincibility)},
        {"Combat", OverrideField("bodySkill", o.bodySkill)},
        {"Behavior", OverrideField("startKneeled", o.startKneeled)},
        {"Behavior", OverrideField("spawnInPants", o.spawnInPants)},
        {"Behavior", OverrideField("blossfechtenGear", o.blossfechtenGear)},
        {"Behavior", OverrideField("clearSpawnArea", o.clearSpawnArea)},
        {"Behavior", OverrideField("drunk", o.drunk)},
        {"Behavior", OverrideField("boltsInQuiver", o.boltsInQuiver)},
        {"BodyCondition", OverrideField("headHealth", o.headHealth)},
        {"BodyCondition", OverrideField("neckHealth", o.neckHealth)},
        {"BodyCondition", OverrideField("armRHealth", o.armRHealth)},
        {"BodyCondition", OverrideField("armLHealth", o.armLHealth)},
        {"BodyCondition", OverrideField("bodyUpperHealth", o.bodyUpperHealth)},
        {"BodyCondition", OverrideField("bodyLowerHealth", o.bodyLowerHealth)},
        {"BodyCondition", OverrideField("legRHealth", o.legRHealth)},
        {"BodyCondition", OverrideField("legLHealth", o.legLHealth)},
    }};
}

// WeaponPresetData descriptors

std::array<PresetFieldDescriptor, 18> WeaponPresetData::GetPresetFields(WeaponPresetData& data) {
    auto& p = data.passport;
    return {
        PresetField::String("Passport", "weaponClass", &data.classPaths.weaponClass),
        PresetField::String("Passport", "headModule", &data.classPaths.headModule),
        PresetField::String("Passport", "guardModule", &data.classPaths.guardModule),
        PresetField::String("Passport", "gripModule", &data.classPaths.gripModule),
        PresetField::String("Passport", "pommelModule", &data.classPaths.pommelModule),
        PresetField::String("Passport", "subModule1", &data.classPaths.subModule1),
        PresetField::String("Passport", "subModule2", &data.classPaths.subModule2),
        PresetField::String("Passport", "gripMesh", &data.gripMeshPath),
        PresetField::Int("Passport", "coaInt", &data.coaInt),
        PresetField::Vec3("Passport", "headSize", &p.HeadSize_21_2D425E61473B8F64FBAB51B223459D57),
        PresetField::Vec3("Passport", "guardSize", &p.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704),
        PresetField::Vec3("Passport", "gripSize", &p.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF),
        PresetField::Vec3("Passport", "pommelSize", &p.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E),
        PresetField::Double("Passport", "massHead", &p.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7),
        PresetField::Double("Passport", "massGuard", &p.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927),
        PresetField::Double("Passport", "massGrip", &p.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10),
        PresetField::Double("Passport", "massPommel", &p.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173),
        PresetField::Double("Passport", "price", &p.Price_60_83FE5A624EA188485BBE4E9C8606AEE5),
    };
}

std::array<PresetOverrideDescriptor, 20> WeaponPresetData::GetPresetOverrides(WeaponPresetData& data) {
    auto& rp = data.runtimeProps;
    return {{
        {"Overrides", OverrideField("rigidity", rp.rigidity)},
        {"Overrides", OverrideField("edgeSharpness", rp.edgeSharpness)},
        {"Overrides", OverrideField("rawDamage", rp.rawDamage)},
        {"Overrides", OverrideField("cuttingRate", rp.cuttingRate)},
        {"Overrides", OverrideField("stabRate", rp.stabRate)},
        {"Overrides", OverrideField("defRating", rp.defRating)},
        {"Overrides", OverrideField("gripRate", rp.gripRate)},
        {"Overrides", OverrideField("drawCutRate", rp.drawCutRate)},
        {"Overrides", OverrideField("tipSharpness", rp.tipSharpness)},
        {"Overrides", OverrideField("kickPower", rp.kickPower)},
        {"Overrides", OverrideField("matDensity", rp.matDensity)},
        {"Overrides", OverrideField("dismemberSharp", rp.dismemberSharp)},
        {"Overrides", OverrideField("dismemberBlunt", rp.dismemberBlunt)},
        {"Overrides", OverrideField("doubleEdged", rp.doubleEdged)},
        {"Overrides", OverrideField("piercing", rp.piercing)},
        {"Overrides", OverrideField("noStab", rp.noStab)},
        {"Overrides", OverrideField("staminaBurnR", rp.staminaBurnR)},
        {"Overrides", OverrideField("staminaBurnL", rp.staminaBurnL)},
        {"Overrides", OverrideField("staminaBurn2H", rp.staminaBurn2H)},
        {"Overrides", OverrideField("staminaBurn2HAlt", rp.staminaBurn2HAlt)},
    }};
}

PresetOperationResult WeaponPresetData::ValidateForSave() const {
    const auto& data = *this;
    auto& mutableData = const_cast<WeaponPresetData&>(data);
    if (auto validation = ValidateFinitePresetFields("Weapon", GetPresetFields(mutableData)); !validation)
        return validation;
    if (auto validation = ValidatePresetOverrideValuesForSave(GetPresetOverrides(mutableData), "Weapon"); !validation)
        return validation;
    const std::string* requiredClasses[] = {
        &data.classPaths.weaponClass,
        &data.classPaths.headModule,
        &data.classPaths.gripModule,
    };
    for (const auto* path : requiredClasses)
        if (path->empty()) return {.error = "This weapon preset is incomplete"};

    const auto& passportData = data.passport;
    if (!IsFinite(passportData.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743) ||
        !IsFinite(passportData.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638))
        return PresetValidationFailure("Weapon");

    for (int slot = 0; slot < MODULE_SLOT_COUNT; ++slot) {
        const auto& mesh = data.meshPresets[slot];
        if (mesh.enabled && mesh.meshPath.empty()) return {.error = "This weapon preset is incomplete"};
        if (!IsFinite(mesh.scale) || !IsFinite(mesh.rotation) || !IsFinite(mesh.offset))
            return PresetValidationFailure("Weapon");
    }

    return {.success = true};
}

void WeaponPresetData::SerializeCustom(const WeaponPresetData& data, CSimpleIniA& ini, std::string_view sectionPrefix) {
    // Material enums and colors (passport fields that need int/enum casting)
    const auto& p = data.passport;
    const auto passportSection = PresetSectionName(sectionPrefix, "Passport");
    const auto meshSection = PresetSectionName(sectionPrefix, "MeshOverrides");
    ini.SetValue(passportSection.c_str(), "id", std::to_string(p.ID_70_C02CF656483647A1933EEA96314B78A6).c_str());
    const std::string weaponName = data.deferredWeaponName.empty()
                                       ? p.Name_57_3729B51148E846FE8DD336B9419BCEE1.GetRawString()
                                       : data.deferredWeaponName;
    ini.SetValue(passportSection.c_str(), "name", weaponName.c_str());
    ini.SetValue(
        passportSection.c_str(), "materialSteel",
        std::to_string(static_cast<int>(p.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36)).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "materialColored",
        std::to_string(static_cast<int>(p.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2)).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "materialWood",
        std::to_string(static_cast<int>(p.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A)).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "materialLeather",
        std::to_string(static_cast<int>(p.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB)).c_str()
    );

    ini.SetValue(
        passportSection.c_str(), "colorWood",
        PresetUtils::ColorToString(p.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "colorLeather",
        PresetUtils::ColorToString(p.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638).c_str()
    );

    ini.SetValue(
        passportSection.c_str(), "tier",
        std::to_string(static_cast<int>(p.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461)).c_str()
    );

    // Mesh overrides
    static constexpr const char* MESH_KEYS[] = {"head", "guard", "grip", "pommel"};
    for (int slot = 0; slot < MODULE_SLOT_COUNT; ++slot) {
        const auto& mp = data.meshPresets[slot];
        std::string encoded;
        encoded.reserve(mp.meshPath.size() + 192);
        encoded += mp.enabled ? "1|" : "0|";
        encoded += mp.meshPath;
        encoded += '|';
        encoded += std::to_string(static_cast<int>(mp.meshType));
        encoded += '|';
        encoded += PresetUtils::VecToString(mp.scale);
        encoded += '|';
        encoded += PresetUtils::RotToString(mp.rotation);
        encoded += '|';
        encoded += PresetUtils::VecToString(mp.offset);
        ini.SetValue(meshSection.c_str(), MESH_KEYS[slot], encoded.c_str());
    }
}

PresetOperationResult WeaponPresetData::DeserializeCustom(
    WeaponPresetData& data, const CSimpleIniA& ini, std::string_view sectionPrefix
) {
    std::string error;
    auto& p = data.passport;
    const auto passportSection = PresetSectionName(sectionPrefix, "Passport");
    const auto meshSection = PresetSectionName(sectionPrefix, "MeshOverrides");

    p.ID_70_C02CF656483647A1933EEA96314B78A6 = ReadInt(error, ini, passportSection, "id", 0);
    const char* weaponName = ini.GetValue(passportSection.c_str(), "name", nullptr);
    if (weaponName && PresetUtils::IsSafeIniValue(weaponName))
        data.deferredWeaponName = weaponName;
    else
        AddInvalidValue(error, passportSection, "name", weaponName);

    // Material enums
    p.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36 =
        static_cast<SDK::Enum_MaterialLayer>(ReadRangedInt(error, ini, passportSection, "materialSteel", 3, 0, 15));
    p.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2 =
        static_cast<SDK::Enum_MaterialLayer>(ReadRangedInt(error, ini, passportSection, "materialColored", 0, 0, 15));
    p.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A =
        static_cast<SDK::Enum_MaterialLayer>(ReadRangedInt(error, ini, passportSection, "materialWood", 14, 0, 15));
    p.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB =
        static_cast<SDK::Enum_MaterialLayer>(ReadRangedInt(error, ini, passportSection, "materialLeather", 10, 0, 15));

    // Colors
    static constexpr SDK::FLinearColor DEFAULT_WOOD_COLOR = {0.4f, 0.26f, 0.13f, 1.0f};
    static constexpr SDK::FLinearColor DEFAULT_LEATHER_COLOR = {0.3f, 0.18f, 0.08f, 1.0f};
    p.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 =
        ReadColor(error, ini, passportSection, "colorWood", DEFAULT_WOOD_COLOR);
    p.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 =
        ReadColor(error, ini, passportSection, "colorLeather", DEFAULT_LEATHER_COLOR);

    // Tier
    p.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461 =
        static_cast<SDK::Enum_Ranks>(ReadRangedInt(error, ini, passportSection, "tier", 4, 0, 8));

    // Mesh overrides
    static constexpr const char* MESH_KEYS[] = {"head", "guard", "grip", "pommel"};
    for (int slot = 0; slot < MODULE_SLOT_COUNT; ++slot) {
        const char* raw = ini.GetValue(meshSection.c_str(), MESH_KEYS[slot], nullptr);
        if (!raw || !raw[0]) {
            AddInvalidValue(error, meshSection, MESH_KEYS[slot], raw);
            continue;
        }

        auto& mp = data.meshPresets[slot];
        std::array<std::string_view, 6> fields;
        bool enabled = false;
        int meshType = 0;
        MeshOverridePreset parsed;
        const bool valid = TrySplitExact(raw, '|', fields) && PresetUtils::TryParseBool(fields[0], enabled) &&
                           PresetUtils::TryParseInt(fields[2], meshType) && meshType >= 0 && meshType <= 1 &&
                           PresetUtils::TryStringToVec(std::string(fields[3]).c_str(), parsed.scale) &&
                           PresetUtils::TryStringToRot(std::string(fields[4]).c_str(), parsed.rotation) &&
                           PresetUtils::TryStringToVec(std::string(fields[5]).c_str(), parsed.offset);
        if (!valid) {
            AddInvalidValue(error, meshSection, MESH_KEYS[slot], raw);
            continue;
        }

        parsed.enabled = enabled;
        parsed.meshPath = fields[1];
        parsed.meshType = meshType == 1 ? MeshType::Skeletal : MeshType::Static;
        mp = std::move(parsed);
    }
    return error.empty() ? PresetOperationResult{.success = true} : PresetOperationResult{.error = std::move(error)};
}

// ArmorPresetData descriptors

std::array<PresetFieldDescriptor, 1> ArmorPresetData::GetPresetFields(ArmorPresetData& data) {
    return {
        PresetField::String("Passport", "armorCore", &data.armorCorePath),
    };
}

std::array<PresetOverrideDescriptor, 10> ArmorPresetData::GetPresetOverrides(ArmorPresetData& data) {
    auto& rp = data.runtimeProps;
    return {{
        {"Overrides", OverrideField("protectionBlunt", rp.protectionBlunt)},
        {"Overrides", OverrideField("protectionCut", rp.protectionCut)},
        {"Overrides", OverrideField("protectionStab", rp.protectionStab)},
        {"Overrides", OverrideField("materialDensity", rp.materialDensity)},
        {"Overrides", OverrideField("massScale", rp.massScale)},
        {"Overrides", OverrideField("handsRigidity", rp.handsRigidity)},
        {"Overrides", OverrideField("strapPower", rp.strapPower)},
        {"Overrides", OverrideField("aiInvincibilityRate", rp.aiInvincibilityRate)},
        {"Overrides", OverrideField("price", rp.price)},
        {"Overrides", OverrideField("pickUp", rp.pickUp)},
    }};
}

PresetOperationResult ArmorPresetData::ValidateForSave() const {
    const auto& data = *this;
    auto& mutableData = const_cast<ArmorPresetData&>(data);
    if (auto validation = ValidateFinitePresetFields("Armor", GetPresetFields(mutableData)); !validation)
        return validation;
    if (auto validation = ValidatePresetOverrideValuesForSave(GetPresetOverrides(mutableData), "Armor"); !validation)
        return validation;
    if (data.armorCorePath.empty()) return {.error = "This armor preset is incomplete"};

    const auto& passportData = data.passport;
    if (!std::isfinite(passportData.Price_27_8E3ADD54484EFC4A59FE9381485AC192)) return PresetValidationFailure("Armor");
    if (!IsFinite(passportData.BackgroundColor_58_CD7AE55B4C46E5A79F8448BB9CDB3B82) ||
        !IsFinite(passportData.LeatherColor_67_A8A17E654ED0341E58247C9B39D29597) ||
        !IsFinite(passportData.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393) ||
        !IsFinite(passportData.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C) ||
        !IsFinite(passportData.FabricColor3_89_167D399343950DE18CC2F9AC76D99042))
        return PresetValidationFailure("Armor");

    return {.success = true};
}

void ArmorPresetData::SerializeCustom(const ArmorPresetData& data, CSimpleIniA& ini, std::string_view sectionPrefix) {
    const auto& p = data.passport;
    const auto passportSection = PresetSectionName(sectionPrefix, "Passport");

    ini.SetValue(passportSection.c_str(), "id", std::to_string(p.ID_54_C6BBB1A64A3828B5AB1D8E804EC7C8F7).c_str());
    ini.SetValue(passportSection.c_str(), "coreRemoved", p.CoreRemoved_12_5CFF8F6D4A05C15812594CAF6771C66B ? "1" : "0");
    ini.SetValue(
        passportSection.c_str(), "module1", std::to_string(p.Module1_5_46B7198E4341C93CBF6AE989EF9898E4).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "module2", std::to_string(p.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "module3", std::to_string(p.Module3_9_E282C465414F6D4EF2A8039FBA847AD2).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "backgroundColor",
        PresetUtils::ColorToString(p.BackgroundColor_58_CD7AE55B4C46E5A79F8448BB9CDB3B82).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "leatherColor",
        PresetUtils::ColorToString(p.LeatherColor_67_A8A17E654ED0341E58247C9B39D29597).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "fabricColor1",
        PresetUtils::ColorToString(p.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "fabricColor2",
        PresetUtils::ColorToString(p.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "fabricColor3",
        PresetUtils::ColorToString(p.FabricColor3_89_167D399343950DE18CC2F9AC76D99042).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "steelType",
        std::to_string(static_cast<int>(p.SteelType_84_7BA6626740476C2CD69648847A1E592F)).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "metalPiecesType",
        std::to_string(static_cast<int>(p.MetalPiecesType_81_203BFD454D41FA24B0B5C5838898AA60)).c_str()
    );
    ini.SetValue(passportSection.c_str(), "rust", p.RustToggle_73_E4F1415F4A1E7AD75CAFD68BBA632FEF ? "1" : "0");
    ini.SetValue(passportSection.c_str(), "dirt", p.DirtToggle_74_A166ACBE4A95555CCA980F98495E364A ? "1" : "0");
    ini.SetValue(
        passportSection.c_str(), "price",
        PresetUtils::FormatFloating(p.Price_27_8E3ADD54484EFC4A59FE9381485AC192).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "slot",
        std::to_string(static_cast<int>(p.Slot_30_7561CB484566A4512003EA96ED44F88D)).c_str()
    );
    ini.SetValue(
        passportSection.c_str(), "providesUpperAP", p.ProvidesUpperAP_34_A85C3E3B4E4EF35DA44FFA960797B6C6 ? "1" : "0"
    );
    ini.SetValue(
        passportSection.c_str(), "providesLowerAP", p.ProvidesLowerAP_36_FFA5916240E32AC30239D58BCDD69D62 ? "1" : "0"
    );
    ini.SetValue(
        passportSection.c_str(), "requiresUpperAP", p.RequiresUpperAP_38_079BBCD74D92FB832584E8B776EC8A6E ? "1" : "0"
    );
    ini.SetValue(
        passportSection.c_str(), "requiresLowerAP", p.RequiresLowerAP_40_BF13845C4B210380A7A569A912A6F614 ? "1" : "0"
    );
    ini.SetValue(
        passportSection.c_str(), "requiresModuleHierarchy",
        p.RequiresModuleHirarchy_47_9ED58E2C48514BE5153606977BE68B6A ? "1" : "0"
    );
    ini.SetValue(
        passportSection.c_str(), "tier",
        std::to_string(static_cast<int>(p.Tier_50_E497AE434B01B84C559DEE8A863BB42E)).c_str()
    );
}

PresetOperationResult ArmorPresetData::DeserializeCustom(
    ArmorPresetData& data, const CSimpleIniA& ini, std::string_view sectionPrefix
) {
    std::string error;
    auto& p = data.passport;
    const auto passportSection = PresetSectionName(sectionPrefix, "Passport");

    p.ID_54_C6BBB1A64A3828B5AB1D8E804EC7C8F7 = ReadInt(error, ini, passportSection, "id", 0);
    p.CoreRemoved_12_5CFF8F6D4A05C15812594CAF6771C66B = ReadBool(error, ini, passportSection, "coreRemoved", false);
    p.Module1_5_46B7198E4341C93CBF6AE989EF9898E4 = ReadInt(error, ini, passportSection, "module1", 0);
    p.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0 = ReadInt(error, ini, passportSection, "module2", 0);
    p.Module3_9_E282C465414F6D4EF2A8039FBA847AD2 = ReadInt(error, ini, passportSection, "module3", 0);

    static constexpr SDK::FLinearColor DEFAULT_FABRIC_COLOR = {0.5f, 0.5f, 0.5f, 1.0f};
    p.BackgroundColor_58_CD7AE55B4C46E5A79F8448BB9CDB3B82 =
        ReadColor(error, ini, passportSection, "backgroundColor", DEFAULT_FABRIC_COLOR);
    p.LeatherColor_67_A8A17E654ED0341E58247C9B39D29597 =
        ReadColor(error, ini, passportSection, "leatherColor", DEFAULT_FABRIC_COLOR);
    p.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393 =
        ReadColor(error, ini, passportSection, "fabricColor1", DEFAULT_FABRIC_COLOR);
    p.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C =
        ReadColor(error, ini, passportSection, "fabricColor2", DEFAULT_FABRIC_COLOR);
    p.FabricColor3_89_167D399343950DE18CC2F9AC76D99042 =
        ReadColor(error, ini, passportSection, "fabricColor3", DEFAULT_FABRIC_COLOR);
    p.SteelType_84_7BA6626740476C2CD69648847A1E592F =
        static_cast<SDK::ESteel_Type>(ReadRangedInt(error, ini, passportSection, "steelType", 0, 0, 11));
    p.MetalPiecesType_81_203BFD454D41FA24B0B5C5838898AA60 =
        static_cast<SDK::ESecondaryMetal_Type>(ReadRangedInt(error, ini, passportSection, "metalPiecesType", 0, 0, 7));
    p.RustToggle_73_E4F1415F4A1E7AD75CAFD68BBA632FEF = ReadBool(error, ini, passportSection, "rust", false);
    p.DirtToggle_74_A166ACBE4A95555CCA980F98495E364A = ReadBool(error, ini, passportSection, "dirt", false);

    p.Price_27_8E3ADD54484EFC4A59FE9381485AC192 = ReadDouble(error, ini, passportSection, "price", 50.0);
    p.Slot_30_7561CB484566A4512003EA96ED44F88D =
        static_cast<SDK::EArmorSlots_Enum>(ReadRangedInt(error, ini, passportSection, "slot", 0, 0, 16));
    p.ProvidesUpperAP_34_A85C3E3B4E4EF35DA44FFA960797B6C6 =
        ReadBool(error, ini, passportSection, "providesUpperAP", false);
    p.ProvidesLowerAP_36_FFA5916240E32AC30239D58BCDD69D62 =
        ReadBool(error, ini, passportSection, "providesLowerAP", false);
    p.RequiresUpperAP_38_079BBCD74D92FB832584E8B776EC8A6E =
        ReadBool(error, ini, passportSection, "requiresUpperAP", false);
    p.RequiresLowerAP_40_BF13845C4B210380A7A569A912A6F614 =
        ReadBool(error, ini, passportSection, "requiresLowerAP", false);
    p.RequiresModuleHirarchy_47_9ED58E2C48514BE5153606977BE68B6A =
        ReadBool(error, ini, passportSection, "requiresModuleHierarchy", false);
    p.Tier_50_E497AE434B01B84C559DEE8A863BB42E =
        static_cast<SDK::Enum_Ranks>(ReadRangedInt(error, ini, passportSection, "tier", 4, 0, 8));

    return error.empty() ? PresetOperationResult{.success = true} : PresetOperationResult{.error = std::move(error)};
}

// LoadoutPresetData descriptors + utilities

namespace {
    template <typename TWeapons> decltype(auto) ResolveWeaponSlot(TWeapons& weapons, int index) {
        switch (index) {
            case 0: return (weapons.WeaponHandR_2_64D3388F445655CA2E9E60B639016D17);
            case 1: return (weapons.WeaponHandL_4_4BF5616F480598D39F54058D5181EB86);
            case 2: return (weapons.WeaponSlotR1_6_140F311C4B659EE501761B8D99781B20);
            case 3: return (weapons.WeaponSlotR2_8_8B0CA70A4477398EB3B1E58EBB1AD2DC);
            case 4: return (weapons.WeaponSlotL1_10_908E8A984A1C041B0CC6238D804CEB60);
            case 5: return (weapons.WeaponSlotL2_12_EF7AA9044E150C11545E349E5AD7C2E0);
            case 6: return (weapons.WeaponBack_14_2CBE21CA47095EF150DD5791D72AC8C9);
            default: std::abort();
        }
    }

}

SDK::FStr_WeaponParts& LoadoutPresetData::GetWeaponSlot(SDK::FStr_Loadout_Weapons& weapons, int index) {
    return ResolveWeaponSlot(weapons, index);
}

const SDK::FStr_WeaponParts& LoadoutPresetData::GetWeaponSlot(const SDK::FStr_Loadout_Weapons& weapons, int index) {
    return ResolveWeaponSlot(weapons, index);
}

void LoadoutPresetData::SerializeCustom(
    const LoadoutPresetData& data, CSimpleIniA& ini, std::string_view sectionPrefix
) {
    for (std::size_t index = 0; index < data.weaponSlots.size(); ++index) {
        const auto section = PresetSectionName(sectionPrefix, "Weapon." + std::string(K_WEAPON_SLOT_KEYS[index]));
        SerializePresetLink(data.weaponSlots[index], ini, section);
    }
    for (std::size_t index = 0; index < data.armorSlots.size(); ++index) {
        const auto section = PresetSectionName(sectionPrefix, "Armor." + std::string(K_ARMOR_SLOT_KEYS[index]));
        SerializePresetLink(data.armorSlots[index], ini, section);
    }
}

PresetOperationResult LoadoutPresetData::DeserializeCustom(
    LoadoutPresetData& data, const CSimpleIniA& ini, std::string_view sectionPrefix
) {
    for (std::size_t index = 0; index < data.weaponSlots.size(); ++index) {
        const auto section = PresetSectionName(sectionPrefix, "Weapon." + std::string(K_WEAPON_SLOT_KEYS[index]));
        auto link = DeserializePresetLink<WeaponPresetData>(ini, section);
        if (link.success)
            data.weaponSlots[index] = std::move(link.value);
        else
            return {.error = "Weapon: " + link.error};
    }
    for (std::size_t index = 0; index < data.armorSlots.size(); ++index) {
        const auto section = PresetSectionName(sectionPrefix, "Armor." + std::string(K_ARMOR_SLOT_KEYS[index]));
        auto link = DeserializePresetLink<ArmorPresetData>(ini, section);
        if (link.success)
            data.armorSlots[index] = std::move(link.value);
        else
            return {.error = "Armor: " + link.error};
    }
    return {.success = true};
}
