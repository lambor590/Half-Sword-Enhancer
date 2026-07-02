#pragma once

#include <algorithm>

#include "Utils/NPCPresetSerializer.h"
#include "SDK/AI_BP_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"
#include "SDK/Str_Passport_Character1_structs.hpp"
#include "SDK/Str_Character_Body_Condition_structs.hpp"
#include "Utils/ActorUtils.h"

namespace NPCSpawnHelpers {
    inline SDK::FLinearColor MelaninToColor(float melanin) {
        float m = std::clamp(melanin, 0.0f, 1.0f);
        float inv = 1.0f - m;
        return {inv * inv, inv * inv * inv, inv * inv * inv * inv, 1.0f};
    }

    inline bool HasAnyBodyConditionOverride(const NPCOverrides& ovr) {
        return ovr.headHealth.enabled || ovr.neckHealth.enabled || ovr.armRHealth.enabled || ovr.armLHealth.enabled ||
               ovr.bodyUpperHealth.enabled || ovr.bodyLowerHealth.enabled || ovr.legRHealth.enabled ||
               ovr.legLHealth.enabled;
    }

    inline void ApplyPassportOverrides(SDK::FStr_Passport_Character1& passport, const NPCOverrides& ovr) {
        if (ovr.heightRate.enabled) passport.Height_21_0EB204DF4978B92AD0ED188FD32EEC7B = ovr.heightRate.value;
        if (ovr.muscleRate.enabled) passport.Weight_23_65E4C6534D14653F96EB739F159E58CD = ovr.muscleRate.value;
        if (ovr.bodySkill.enabled) passport.Skill_43_4CF5DCC248424BFADCCD6AB9F5F39CC9 = ovr.bodySkill.value;
        if (ovr.faceType.enabled) passport.FaceType_34_FB5E4D464B2A5CF6406C3CB19051FCE3 = ovr.faceType.value;
        if (ovr.eyeColor.enabled) passport.EyeColor_46_826504294B0D51C1343D848E8B1AB4C6 = ovr.eyeColor.value;
        if (ovr.hairLength.enabled) passport.HairLength_41_9295B3CF41DF9BED0FEDB9AE02E7FC16 = ovr.hairLength.value;
        if (ovr.hairColor.enabled)
            passport.HairColor_38_CBDC51B043E6816A062799A9A96EB232 =
                MelaninToColor(static_cast<float>(ovr.hairColor.value));
    }

    inline void ApplyPropertyOverrides(SDK::AWillie_BP_C* npc, const NPCOverrides& ovr) {
        if (ovr.heightRate.enabled) npc->Height_Rate = ovr.heightRate.value;
        if (ovr.muscleRate.enabled) npc->Muscle_Rate = ovr.muscleRate.value;
        if (ovr.scaleMutationInhibitor.enabled) npc->Scale_Mutation_Inhibitor = ovr.scaleMutationInhibitor.value;

        if (ovr.damageRate.enabled) npc->Damage_Rate__Additional_ = ovr.damageRate.value;
        if (ovr.limbDamageRate.enabled) npc->Limb_Damage_Rate__Additional_ = ovr.limbDamageRate.value;
        if (ovr.dismemberThreshold.enabled) npc->Health_Threshold_For_Dismemberment = ovr.dismemberThreshold.value;
        if (ovr.regenRate.enabled) npc->Regen_Rate = ovr.regenRate.value;
        if (ovr.aiInvincibility.enabled) npc->AI_Invincibility_Rate = ovr.aiInvincibility.value;
        if (ovr.aiArmorInvincibility.enabled) npc->AI_Armor_Invincibility_Rate = ovr.aiArmorInvincibility.value;
        if (ovr.bodySkill.enabled) npc->Body_Skill__Temp_ = ovr.bodySkill.value;

        if (ovr.startKneeled.enabled) npc->Start_Kneeled = ovr.startKneeled.value;
        if (ovr.spawnInPants.enabled) npc->Spawn_in_Pants = ovr.spawnInPants.value;
        if (ovr.blossfechtenGear.enabled) npc->Blossfechten_Gear = ovr.blossfechtenGear.value;
        if (ovr.clearSpawnArea.enabled) npc->Clear_Spawn_Area = ovr.clearSpawnArea.value;
        if (ovr.drunk.enabled) npc->Drunk = ovr.drunk.value;
        if (ovr.boltsInQuiver.enabled) npc->Bolts_in_Quiver = ovr.boltsInQuiver.value;

        if (HasAnyBodyConditionOverride(ovr)) {
            auto condition = npc->Start_Body_Condition;
            if (ovr.headHealth.enabled) condition.HeadHealth_2_61859BB444171EF8952E0FA5DD8628EE = ovr.headHealth.value;
            if (ovr.neckHealth.enabled) condition.NeckHealth_4_C658DC6A4BD1988C40F1A5B3C4F8F4EE = ovr.neckHealth.value;
            if (ovr.armRHealth.enabled) condition.ArmRHealth_9_A65DD4C14ACBF6030A2B3AAD90FD0CFD = ovr.armRHealth.value;
            if (ovr.armLHealth.enabled) condition.ArmLHealth_11_32345C31454A51B3CDE618918B9574F6 = ovr.armLHealth.value;
            if (ovr.bodyUpperHealth.enabled)
                condition.BodyUpperHealth_16_F71EA0C742135DC3B4F71EA3FEF07C46 = ovr.bodyUpperHealth.value;
            if (ovr.bodyLowerHealth.enabled)
                condition.BodyLowerHealth_18_37C008FF4FA0C0E5F5E09C9F0C174FE3 = ovr.bodyLowerHealth.value;
            if (ovr.legRHealth.enabled) condition.LegRHealth_13_D50D4E174859A541DBEA66963D162E12 = ovr.legRHealth.value;
            if (ovr.legLHealth.enabled) condition.LegLHealth_15_41C766B5460596C0804EA5B4B8F8EB36 = ovr.legLHealth.value;
            npc->Start_Body_Condition = condition;
        }
    }

    inline void ApplyHairColor(SDK::AWillie_BP_C* npc, const NPCOverrides& ovr) {
        if (ovr.hairColor.enabled && npc->Hair_Mat) {
            auto melaninName = SDK::BasicFilesImplUtils::StringToName(L"Melanin");
            npc->Hair_Mat->SetScalarParameterValue(melaninName, static_cast<float>(ovr.hairColor.value));
        }
    }

}
