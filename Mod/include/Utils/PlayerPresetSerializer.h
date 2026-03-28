#pragma once

#include <string>
#include <filesystem>
#include <cstring>

#include "Utils/PresetUtils.h"
#include "Utils/PresetDataBase.h"
#include "Utils/PresetSerializerBase.h"
#include "Utils/PlayerEditorOverrides.h"

struct PlayerPresetData : PresetDataBase {
    PlayerEditorOverrides overrides{};
};

class PlayerPresetSerializer : public PresetSerializerBase<PlayerPresetSerializer, PlayerPresetData> {
public:
    static constexpr const char* kPresetsSubdir = "player_presets";

    static std::string SerializeToIni(const PlayerPresetData& data, bool minimal = false) {
        CSimpleIniA ini;
        ini.SetUnicode(false);

        ini.SetValue("Preset", "name", data.name.c_str());
        ini.SetValue("Preset", "version", "1");

        const auto& o = data.overrides;
        auto set = [&](const char* sec, const char* key, bool en, double val) {
            if (!minimal || en) ini.SetValue(sec, key, PresetUtils::DoubleOverrideToString(en, val).c_str());
        };
        auto setBool = [&](const char* sec, const char* key, bool en, bool val) {
            if (!minimal || en) ini.SetValue(sec, key, PresetUtils::IntOverrideToString(en, val ? 1 : 0).c_str());
        };

        set("Physical", "heightRate", o.heightRate.enabled, o.heightRate.value);
        set("Physical", "muscleRate", o.muscleRate.enabled, o.muscleRate.value);
        set("Physical", "scaleMutationInhibitor", o.scaleMutationInhibitor.enabled, o.scaleMutationInhibitor.value);

        set("Health", "health", o.health.enabled, o.health.value);
        set("Health", "headHealth", o.headHealth.enabled, o.headHealth.value);
        set("Health", "neckHealth", o.neckHealth.enabled, o.neckHealth.value);
        set("Health", "armRHealth", o.armRHealth.enabled, o.armRHealth.value);
        set("Health", "armLHealth", o.armLHealth.enabled, o.armLHealth.value);
        set("Health", "bodyUpperHealth", o.bodyUpperHealth.enabled, o.bodyUpperHealth.value);
        set("Health", "bodyLowerHealth", o.bodyLowerHealth.enabled, o.bodyLowerHealth.value);
        set("Health", "legRHealth", o.legRHealth.enabled, o.legRHealth.value);
        set("Health", "legLHealth", o.legLHealth.enabled, o.legLHealth.value);
        set("Health", "backHealth", o.backHealth.enabled, o.backHealth.value);
        set("Health", "consciousness", o.consciousness.enabled, o.consciousness.value);
        set("Health", "regenRate", o.regenRate.enabled, o.regenRate.value);

        set("Physics", "allBodyTonus", o.allBodyTonus.enabled, o.allBodyTonus.value);
        set("Physics", "headTonus", o.headTonus.enabled, o.headTonus.value);
        set("Physics", "armRTonus", o.armRTonus.enabled, o.armRTonus.value);
        set("Physics", "armLTonus", o.armLTonus.enabled, o.armLTonus.value);
        set("Physics", "legRTonus", o.legRTonus.enabled, o.legRTonus.value);
        set("Physics", "legLTonus", o.legLTonus.enabled, o.legLTonus.value);
        set("Physics", "musclePower", o.musclePower.enabled, o.musclePower.value);
        set("Physics", "orientationStrength", o.orientationStrength.enabled, o.orientationStrength.value);
        set("Physics", "angularStrength", o.angularStrength.enabled, o.angularStrength.value);
        set("Physics", "hitRigidity", o.hitRigidity.enabled, o.hitRigidity.value);

        set("Movement", "runningSpeedRate", o.runningSpeedRate.enabled, o.runningSpeedRate.value);
        set("Movement", "walkSpeedRateRun", o.walkSpeedRateRun.enabled, o.walkSpeedRateRun.value);
        set("Movement", "jumpRate", o.jumpRate.enabled, o.jumpRate.value);
        set("Movement", "dodgeRate", o.dodgeRate.enabled, o.dodgeRate.value);
        set("Movement", "crawlRate", o.crawlRate.enabled, o.crawlRate.value);
        set("Movement", "getUpRate", o.getUpRate.enabled, o.getUpRate.value);
        set("Movement", "fallenRate", o.fallenRate.enabled, o.fallenRate.value);

        set("Combat", "damageRate", o.damageRate.enabled, o.damageRate.value);
        set("Combat", "limbDamageRate", o.limbDamageRate.enabled, o.limbDamageRate.value);
        set("Combat", "dismemberThreshold", o.dismemberThreshold.enabled, o.dismemberThreshold.value);
        set("Combat", "stamina", o.stamina.enabled, o.stamina.value);
        set("Combat", "staminaBurnSwingR", o.staminaBurnSwingR.enabled, o.staminaBurnSwingR.value);
        set("Combat", "staminaBurnSwingL", o.staminaBurnSwingL.enabled, o.staminaBurnSwingL.value);
        set("Combat", "staminaBurnDodge", o.staminaBurnDodge.enabled, o.staminaBurnDodge.value);
        set("Combat", "grabForceR", o.grabForceR.enabled, o.grabForceR.value);
        set("Combat", "grabForceL", o.grabForceL.enabled, o.grabForceL.value);
        set("Combat", "handsRigidity", o.handsRigidity.enabled, o.handsRigidity.value);
        set("Combat", "bodySkill", o.bodySkill.enabled, o.bodySkill.value);
        set("Combat", "weaponSkill", o.weaponSkill.enabled, o.weaponSkill.value);

        setBool("Skills", "thrust", o.skillThrust.enabled, o.skillThrust.value);
        setBool("Skills", "parry", o.skillParry.enabled, o.skillParry.value);
        setBool("Skills", "altGrip", o.skillAltGrip.enabled, o.skillAltGrip.value);
        setBool("Skills", "altStance", o.skillAltStance.enabled, o.skillAltStance.value);
        setBool("Skills", "rotate", o.skillRotate.enabled, o.skillRotate.value);
        setBool("Skills", "crouch", o.skillCrouch.enabled, o.skillCrouch.value);
        setBool("Skills", "dodge", o.skillDodge.enabled, o.skillDodge.value);
        setBool("Skills", "kick", o.skillKick.enabled, o.skillKick.value);
        setBool("Skills", "slomo", o.skillSlomo.enabled, o.skillSlomo.value);

        set("State", "exhaustion", o.exhaustion.enabled, o.exhaustion.value);
        set("State", "drunk", o.drunk.enabled, o.drunk.value);
        set("State", "fear", o.fear.enabled, o.fear.value);
        setBool("State", "invulnerable", o.invulnerable.enabled, o.invulnerable.value);
        setBool("State", "fearless", o.fearless.enabled, o.fearless.value);

        std::string output;
        ini.Save(output);
        return output;
    }

    static PlayerPresetData DeserializeFromIni(const std::string& iniContent) {
        PlayerPresetData result;
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

        auto& o = result.overrides;
        auto get = [&](const char* sec, const char* key, bool& en, double& val) {
            PresetUtils::ParseDoubleOverride(ini.GetValue(sec, key, ""), en, val);
        };
        auto getBool = [&](const char* sec, const char* key, bool& en, bool& val) {
            PresetUtils::ParseBoolOverride(ini.GetValue(sec, key, ""), en, val);
        };

        get("Physical", "heightRate", o.heightRate.enabled, o.heightRate.value);
        get("Physical", "muscleRate", o.muscleRate.enabled, o.muscleRate.value);
        get("Physical", "scaleMutationInhibitor", o.scaleMutationInhibitor.enabled, o.scaleMutationInhibitor.value);

        get("Health", "health", o.health.enabled, o.health.value);
        get("Health", "headHealth", o.headHealth.enabled, o.headHealth.value);
        get("Health", "neckHealth", o.neckHealth.enabled, o.neckHealth.value);
        get("Health", "armRHealth", o.armRHealth.enabled, o.armRHealth.value);
        get("Health", "armLHealth", o.armLHealth.enabled, o.armLHealth.value);
        get("Health", "bodyUpperHealth", o.bodyUpperHealth.enabled, o.bodyUpperHealth.value);
        get("Health", "bodyLowerHealth", o.bodyLowerHealth.enabled, o.bodyLowerHealth.value);
        get("Health", "legRHealth", o.legRHealth.enabled, o.legRHealth.value);
        get("Health", "legLHealth", o.legLHealth.enabled, o.legLHealth.value);
        get("Health", "backHealth", o.backHealth.enabled, o.backHealth.value);
        get("Health", "consciousness", o.consciousness.enabled, o.consciousness.value);
        get("Health", "regenRate", o.regenRate.enabled, o.regenRate.value);

        get("Physics", "allBodyTonus", o.allBodyTonus.enabled, o.allBodyTonus.value);
        get("Physics", "headTonus", o.headTonus.enabled, o.headTonus.value);
        get("Physics", "armRTonus", o.armRTonus.enabled, o.armRTonus.value);
        get("Physics", "armLTonus", o.armLTonus.enabled, o.armLTonus.value);
        get("Physics", "legRTonus", o.legRTonus.enabled, o.legRTonus.value);
        get("Physics", "legLTonus", o.legLTonus.enabled, o.legLTonus.value);
        get("Physics", "musclePower", o.musclePower.enabled, o.musclePower.value);
        get("Physics", "orientationStrength", o.orientationStrength.enabled, o.orientationStrength.value);
        get("Physics", "angularStrength", o.angularStrength.enabled, o.angularStrength.value);
        get("Physics", "hitRigidity", o.hitRigidity.enabled, o.hitRigidity.value);

        get("Movement", "runningSpeedRate", o.runningSpeedRate.enabled, o.runningSpeedRate.value);
        get("Movement", "walkSpeedRateRun", o.walkSpeedRateRun.enabled, o.walkSpeedRateRun.value);
        get("Movement", "jumpRate", o.jumpRate.enabled, o.jumpRate.value);
        get("Movement", "dodgeRate", o.dodgeRate.enabled, o.dodgeRate.value);
        get("Movement", "crawlRate", o.crawlRate.enabled, o.crawlRate.value);
        get("Movement", "getUpRate", o.getUpRate.enabled, o.getUpRate.value);
        get("Movement", "fallenRate", o.fallenRate.enabled, o.fallenRate.value);

        get("Combat", "damageRate", o.damageRate.enabled, o.damageRate.value);
        get("Combat", "limbDamageRate", o.limbDamageRate.enabled, o.limbDamageRate.value);
        get("Combat", "dismemberThreshold", o.dismemberThreshold.enabled, o.dismemberThreshold.value);
        get("Combat", "stamina", o.stamina.enabled, o.stamina.value);
        get("Combat", "staminaBurnSwingR", o.staminaBurnSwingR.enabled, o.staminaBurnSwingR.value);
        get("Combat", "staminaBurnSwingL", o.staminaBurnSwingL.enabled, o.staminaBurnSwingL.value);
        get("Combat", "staminaBurnDodge", o.staminaBurnDodge.enabled, o.staminaBurnDodge.value);
        get("Combat", "grabForceR", o.grabForceR.enabled, o.grabForceR.value);
        get("Combat", "grabForceL", o.grabForceL.enabled, o.grabForceL.value);
        get("Combat", "handsRigidity", o.handsRigidity.enabled, o.handsRigidity.value);
        get("Combat", "bodySkill", o.bodySkill.enabled, o.bodySkill.value);
        get("Combat", "weaponSkill", o.weaponSkill.enabled, o.weaponSkill.value);

        getBool("Skills", "thrust", o.skillThrust.enabled, o.skillThrust.value);
        getBool("Skills", "parry", o.skillParry.enabled, o.skillParry.value);
        getBool("Skills", "altGrip", o.skillAltGrip.enabled, o.skillAltGrip.value);
        getBool("Skills", "altStance", o.skillAltStance.enabled, o.skillAltStance.value);
        getBool("Skills", "rotate", o.skillRotate.enabled, o.skillRotate.value);
        getBool("Skills", "crouch", o.skillCrouch.enabled, o.skillCrouch.value);
        getBool("Skills", "dodge", o.skillDodge.enabled, o.skillDodge.value);
        getBool("Skills", "kick", o.skillKick.enabled, o.skillKick.value);
        getBool("Skills", "slomo", o.skillSlomo.enabled, o.skillSlomo.value);

        get("State", "exhaustion", o.exhaustion.enabled, o.exhaustion.value);
        get("State", "drunk", o.drunk.enabled, o.drunk.value);
        get("State", "fear", o.fear.enabled, o.fear.value);
        getBool("State", "invulnerable", o.invulnerable.enabled, o.invulnerable.value);
        getBool("State", "fearless", o.fearless.enabled, o.fearless.value);

        result.success = true;
        return result;
    }
};
