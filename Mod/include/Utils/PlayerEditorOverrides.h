#pragma once

#include "Utils/CharacterOverrideSets.h"

struct PlayerEditorOverrides
    : CharacterPhysicalOverrides,
      CharacterCombatOverrides,
      CharacterBodyConditionOverrides {
    RuntimeOverride health;
    RuntimeOverride backHealth;
    RuntimeOverride consciousness;

    RuntimeOverride allBodyTonus;
    RuntimeOverride headTonus;
    RuntimeOverride armRTonus;
    RuntimeOverride armLTonus;
    RuntimeOverride legRTonus;
    RuntimeOverride legLTonus;
    RuntimeOverride musclePower;
    RuntimeOverride orientationStrength;
    RuntimeOverride angularStrength;
    RuntimeOverride hitRigidity;

    RuntimeOverride runningSpeedRate;
    RuntimeOverride walkSpeedRateRun;
    RuntimeOverride jumpRate;
    RuntimeOverride dodgeRate;
    RuntimeOverride crawlRate;
    RuntimeOverride getUpRate;
    RuntimeOverride fallenRate;

    RuntimeOverride stamina;
    RuntimeOverride staminaBurnSwingR;
    RuntimeOverride staminaBurnSwingL;
    RuntimeOverride staminaBurnDodge;
    RuntimeOverride grabForceR;
    RuntimeOverride grabForceL;
    RuntimeOverride handsRigidity;
    RuntimeOverride weaponSkill;

    BoolOverride skillThrust;
    BoolOverride skillParry;
    BoolOverride skillAltGrip;
    BoolOverride skillAltStance;
    BoolOverride skillRotate;
    BoolOverride skillCrouch;
    BoolOverride skillDodge;
    BoolOverride skillKick;
    BoolOverride skillSlomo;

    RuntimeOverride exhaustion;
    RuntimeOverride drunk;
    RuntimeOverride fear;
    BoolOverride invulnerable;
    BoolOverride fearless;
};
