﻿#pragma once

/*
* SDK generated by Dumper-7
*
* https://github.com/Encryqed/Dumper-7
*/

// Package: SG_Weapon_Try

#include "Basic.hpp"

#include "Str_WeaponParts_structs.hpp"
#include "Engine_classes.hpp"


namespace SDK
{

// BlueprintGeneratedClass SG_Weapon_Try.SG_Weapon_Try_C
// 0x0130 (0x0158 - 0x0028)
class USG_Weapon_Try_C final : public USaveGame
{
public:
	struct FStr_WeaponParts                       Weapon_1;                                          // 0x0028(0x0130)(Edit, BlueprintVisible, DisableEditOnInstance, HasGetValueTypeHash)

public:
	static class UClass* StaticClass()
	{
		return StaticBPGeneratedClassImpl<"SG_Weapon_Try_C">();
	}
	static class USG_Weapon_Try_C* GetDefaultObj()
	{
		return GetDefaultObjImpl<USG_Weapon_Try_C>();
	}
};
static_assert(alignof(USG_Weapon_Try_C) == 0x000008, "Wrong alignment on USG_Weapon_Try_C");
static_assert(sizeof(USG_Weapon_Try_C) == 0x000158, "Wrong size on USG_Weapon_Try_C");
static_assert(offsetof(USG_Weapon_Try_C, Weapon_1) == 0x000028, "Member 'USG_Weapon_Try_C::Weapon_1' has a wrong offset!");

}

