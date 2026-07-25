#pragma once

#include "Utils/OverrideTypes.h"

struct CharacterPhysicalOverrides {
    RuntimeOverride heightRate;
    RuntimeOverride muscleRate;
    RuntimeOverride scaleMutationInhibitor;
};

struct CharacterCombatOverrides {
    RuntimeOverride damageRate;
    RuntimeOverride limbDamageRate;
    RuntimeOverride dismemberThreshold;
    RuntimeOverride regenRate;
    RuntimeOverride bodySkill;
};

struct CharacterBodyConditionOverrides {
    RuntimeOverride headHealth;
    RuntimeOverride neckHealth;
    RuntimeOverride armRHealth;
    RuntimeOverride armLHealth;
    RuntimeOverride bodyUpperHealth;
    RuntimeOverride bodyLowerHealth;
    RuntimeOverride legRHealth;
    RuntimeOverride legLHealth;
};
