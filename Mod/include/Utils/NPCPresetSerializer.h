#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstring>

#include "Utils/PresetUtils.h"
#include "Utils/OverrideTypes.h"

struct NPCOverrides {
    RuntimeOverride heightRate;
    RuntimeOverride muscleRate;
    RuntimeOverride scaleMutationInhibitor;

    IntOverride faceType;
    IntOverride eyeColor;
    RuntimeOverride hairLength;
    RuntimeOverride hairColor;

    RuntimeOverride damageRate;
    RuntimeOverride limbDamageRate;
    RuntimeOverride dismemberThreshold;
    RuntimeOverride regenRate;
    RuntimeOverride aiInvincibility;
    RuntimeOverride aiArmorInvincibility;
    RuntimeOverride bodySkill;

    BoolOverride fearless;
    BoolOverride startKneeled;
    BoolOverride spawnInPants;
    BoolOverride clearSpawnArea;
    RuntimeOverride drunk;
    IntOverride boltsInQuiver;

    RuntimeOverride headHealth;
    RuntimeOverride neckHealth;
    RuntimeOverride armRHealth;
    RuntimeOverride armLHealth;
    RuntimeOverride bodyUpperHealth;
    RuntimeOverride bodyLowerHealth;
    RuntimeOverride legRHealth;
    RuntimeOverride legLHealth;
};

struct NPCPresetData {
    std::string name;
    bool success = false;
    std::string error;

    int npcTypeIndex = 0;
    int nationality = 0;
    int tier = 4;
    bool mercenary = false;

    NPCOverrides overrides{};
};

class NPCPresetSerializer {
private:

public:
    static std::string SerializeToIni(const NPCPresetData& data, bool minimalMode = false) {
        CSimpleIniA ini;
        ini.SetUnicode(false);

        ini.SetValue("Preset", "name", data.name.c_str());
        ini.SetValue("Preset", "version", "1");

        if (!minimalMode || data.npcTypeIndex != 0)
            ini.SetValue("Generator", "npcType", std::to_string(data.npcTypeIndex).c_str());
        if (!minimalMode || data.nationality != 0)
            ini.SetValue("Generator", "nationality", std::to_string(data.nationality).c_str());
        if (!minimalMode || data.tier != 4)
            ini.SetValue("Generator", "tier", std::to_string(data.tier).c_str());
        if (!minimalMode || data.mercenary)
            ini.SetValue("Generator", "mercenary", data.mercenary ? "1" : "0");

        const auto& o = data.overrides;
        auto setOvr = [&](const char* section, const char* key, bool enabled, double val) {
            if (!minimalMode || enabled)
                ini.SetValue(section, key, PresetUtils::DoubleOverrideToString(enabled, val).c_str());
        };
        auto setOvrInt = [&](const char* section, const char* key, bool enabled, int val) {
            if (!minimalMode || enabled)
                ini.SetValue(section, key, PresetUtils::IntOverrideToString(enabled, val).c_str());
        };
        auto setOvrBool = [&](const char* section, const char* key, bool enabled, bool val) {
            if (!minimalMode || enabled)
                ini.SetValue(section, key, PresetUtils::IntOverrideToString(enabled, val ? 1 : 0).c_str());
        };

        setOvr("Physical", "heightRate", o.heightRate.enabled, o.heightRate.value);
        setOvr("Physical", "muscleRate", o.muscleRate.enabled, o.muscleRate.value);
        setOvr("Physical", "scaleMutationInhibitor", o.scaleMutationInhibitor.enabled, o.scaleMutationInhibitor.value);
        setOvrInt("Physical", "faceType", o.faceType.enabled, o.faceType.value);
        setOvrInt("Physical", "eyeColor", o.eyeColor.enabled, o.eyeColor.value);
        setOvr("Physical", "hairLength", o.hairLength.enabled, o.hairLength.value);
        setOvr("Physical", "hairColor", o.hairColor.enabled, o.hairColor.value);

        setOvr("Combat", "damageRate", o.damageRate.enabled, o.damageRate.value);
        setOvr("Combat", "limbDamageRate", o.limbDamageRate.enabled, o.limbDamageRate.value);
        setOvr("Combat", "dismemberThreshold", o.dismemberThreshold.enabled, o.dismemberThreshold.value);
        setOvr("Combat", "regenRate", o.regenRate.enabled, o.regenRate.value);
        setOvr("Combat", "aiInvincibility", o.aiInvincibility.enabled, o.aiInvincibility.value);
        setOvr("Combat", "aiArmorInvincibility", o.aiArmorInvincibility.enabled, o.aiArmorInvincibility.value);
        setOvr("Combat", "bodySkill", o.bodySkill.enabled, o.bodySkill.value);

        setOvrBool("Behavior", "fearless", o.fearless.enabled, o.fearless.value);
        setOvrBool("Behavior", "startKneeled", o.startKneeled.enabled, o.startKneeled.value);
        setOvrBool("Behavior", "spawnInPants", o.spawnInPants.enabled, o.spawnInPants.value);
        setOvrBool("Behavior", "clearSpawnArea", o.clearSpawnArea.enabled, o.clearSpawnArea.value);
        setOvr("Behavior", "drunk", o.drunk.enabled, o.drunk.value);
        setOvrInt("Behavior", "boltsInQuiver", o.boltsInQuiver.enabled, o.boltsInQuiver.value);

        setOvr("BodyCondition", "headHealth", o.headHealth.enabled, o.headHealth.value);
        setOvr("BodyCondition", "neckHealth", o.neckHealth.enabled, o.neckHealth.value);
        setOvr("BodyCondition", "armRHealth", o.armRHealth.enabled, o.armRHealth.value);
        setOvr("BodyCondition", "armLHealth", o.armLHealth.enabled, o.armLHealth.value);
        setOvr("BodyCondition", "bodyUpperHealth", o.bodyUpperHealth.enabled, o.bodyUpperHealth.value);
        setOvr("BodyCondition", "bodyLowerHealth", o.bodyLowerHealth.enabled, o.bodyLowerHealth.value);
        setOvr("BodyCondition", "legRHealth", o.legRHealth.enabled, o.legRHealth.value);
        setOvr("BodyCondition", "legLHealth", o.legLHealth.enabled, o.legLHealth.value);

        std::string output;
        ini.Save(output);
        return output;
    }

    static NPCPresetData DeserializeFromIni(const std::string& iniContent) {
        NPCPresetData result;
        CSimpleIniA ini;
        ini.SetUnicode(false);

        if (ini.LoadData(iniContent) < 0) {
            result.error = "Failed to parse INI data";
            return result;
        }

        const char* ver = ini.GetValue("Preset", "version", "0");
        if (std::strcmp(ver, "1") != 0) {
            result.error = "Unsupported preset version: " + std::string(ver);
            return result;
        }

        result.name = ini.GetValue("Preset", "name", "Unnamed");

        result.npcTypeIndex = std::atoi(ini.GetValue("Generator", "npcType", "0"));
        result.nationality = std::atoi(ini.GetValue("Generator", "nationality", "0"));
        result.tier = std::atoi(ini.GetValue("Generator", "tier", "4"));
        result.mercenary = std::atoi(ini.GetValue("Generator", "mercenary", "0")) != 0;

        auto& o = result.overrides;
        PresetUtils::ParseDoubleOverride(ini.GetValue("Physical", "heightRate", ""), o.heightRate.enabled, o.heightRate.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Physical", "muscleRate", ""), o.muscleRate.enabled, o.muscleRate.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Physical", "scaleMutationInhibitor", ""), o.scaleMutationInhibitor.enabled, o.scaleMutationInhibitor.value);
        PresetUtils::ParseIntOverride(ini.GetValue("Physical", "faceType", ""), o.faceType.enabled, o.faceType.value);
        PresetUtils::ParseIntOverride(ini.GetValue("Physical", "eyeColor", ""), o.eyeColor.enabled, o.eyeColor.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Physical", "hairLength", ""), o.hairLength.enabled, o.hairLength.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Physical", "hairColor", ""), o.hairColor.enabled, o.hairColor.value);

        PresetUtils::ParseDoubleOverride(ini.GetValue("Combat", "damageRate", ""), o.damageRate.enabled, o.damageRate.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Combat", "limbDamageRate", ""), o.limbDamageRate.enabled, o.limbDamageRate.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Combat", "dismemberThreshold", ""), o.dismemberThreshold.enabled, o.dismemberThreshold.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Combat", "regenRate", ""), o.regenRate.enabled, o.regenRate.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Combat", "aiInvincibility", ""), o.aiInvincibility.enabled, o.aiInvincibility.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Combat", "aiArmorInvincibility", ""), o.aiArmorInvincibility.enabled, o.aiArmorInvincibility.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Combat", "bodySkill", ""), o.bodySkill.enabled, o.bodySkill.value);

        PresetUtils::ParseBoolOverride(ini.GetValue("Behavior", "fearless", ""), o.fearless.enabled, o.fearless.value);
        PresetUtils::ParseBoolOverride(ini.GetValue("Behavior", "startKneeled", ""), o.startKneeled.enabled, o.startKneeled.value);
        PresetUtils::ParseBoolOverride(ini.GetValue("Behavior", "spawnInPants", ""), o.spawnInPants.enabled, o.spawnInPants.value);
        PresetUtils::ParseBoolOverride(ini.GetValue("Behavior", "clearSpawnArea", ""), o.clearSpawnArea.enabled, o.clearSpawnArea.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("Behavior", "drunk", ""), o.drunk.enabled, o.drunk.value);
        PresetUtils::ParseIntOverride(ini.GetValue("Behavior", "boltsInQuiver", ""), o.boltsInQuiver.enabled, o.boltsInQuiver.value);

        PresetUtils::ParseDoubleOverride(ini.GetValue("BodyCondition", "headHealth", ""), o.headHealth.enabled, o.headHealth.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("BodyCondition", "neckHealth", ""), o.neckHealth.enabled, o.neckHealth.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("BodyCondition", "armRHealth", ""), o.armRHealth.enabled, o.armRHealth.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("BodyCondition", "armLHealth", ""), o.armLHealth.enabled, o.armLHealth.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("BodyCondition", "bodyUpperHealth", ""), o.bodyUpperHealth.enabled, o.bodyUpperHealth.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("BodyCondition", "bodyLowerHealth", ""), o.bodyLowerHealth.enabled, o.bodyLowerHealth.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("BodyCondition", "legRHealth", ""), o.legRHealth.enabled, o.legRHealth.value);
        PresetUtils::ParseDoubleOverride(ini.GetValue("BodyCondition", "legLHealth", ""), o.legLHealth.enabled, o.legLHealth.value);

        result.success = true;
        return result;
    }

    static std::filesystem::path GetPresetsDirectory() {
        return PresetUtils::EnsureDirectory(ConfigManager::GetAppDataPath() / "npc_presets");
    }

    static bool SaveToFile(const std::filesystem::path& path, const NPCPresetData& data) {
        return PresetUtils::SaveStringToFile(path, SerializeToIni(data, false));
    }

    static NPCPresetData LoadFromFile(const std::filesystem::path& path) {
        NPCPresetData result;
        std::string content = PresetUtils::LoadStringFromFile(path);
        if (content.empty()) {
            result.error = "Cannot open file: " + path.string();
            return result;
        }
        return DeserializeFromIni(content);
    }

    static std::vector<PresetListEntry> ListPresets() {
        return PresetUtils::ListPresetsInDir(GetPresetsDirectory());
    }

    static PresetUtils::PresetTreeNode ListPresetsTree() {
        return PresetUtils::ListPresetsRecursive(GetPresetsDirectory());
    }

    static bool DeletePreset(const std::filesystem::path& path) {
        return PresetUtils::DeletePreset(path);
    }

    static bool SavePresetByName(const std::string& name, const NPCPresetData& data) {
        auto [folder, filename] = PresetUtils::SanitizePresetPath(name);
        auto dir = GetPresetsDirectory();
        if (!folder.empty()) dir /= folder;
        PresetUtils::EnsureDirectory(dir);
        return SaveToFile(dir / (filename + ".ini"), data);
    }
};
