﻿#pragma once

/*
* SDK generated by Dumper-7
*
* https://github.com/Encryqed/Dumper-7
*/

// Package: S_InputKeys

#include "Basic.hpp"

#include "InputCore_structs.hpp"


namespace SDK
{

// UserDefinedStruct S_InputKeys.S_InputKeys
// 0x0030 (0x0030 - 0x0000)
struct FS_InputKeys final
{
public:
	struct FKey                                   KeyboardKey_14_025090314E7198E085CC0CBE988DC21F;   // 0x0000(0x0018)(Edit, BlueprintVisible, HasGetValueTypeHash)
	struct FKey                                   GamepadKey_15_5F26C35E4857CEA4E16D529A1424B51B;    // 0x0018(0x0018)(Edit, BlueprintVisible, HasGetValueTypeHash)
};
static_assert(alignof(FS_InputKeys) == 0x000008, "Wrong alignment on FS_InputKeys");
static_assert(sizeof(FS_InputKeys) == 0x000030, "Wrong size on FS_InputKeys");
static_assert(offsetof(FS_InputKeys, KeyboardKey_14_025090314E7198E085CC0CBE988DC21F) == 0x000000, "Member 'FS_InputKeys::KeyboardKey_14_025090314E7198E085CC0CBE988DC21F' has a wrong offset!");
static_assert(offsetof(FS_InputKeys, GamepadKey_15_5F26C35E4857CEA4E16D529A1424B51B) == 0x000018, "Member 'FS_InputKeys::GamepadKey_15_5F26C35E4857CEA4E16D529A1424B51B' has a wrong offset!");

}

