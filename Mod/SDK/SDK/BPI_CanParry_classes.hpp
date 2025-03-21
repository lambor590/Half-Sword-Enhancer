﻿#pragma once

/*
* SDK generated by Dumper-7
*
* https://github.com/Encryqed/Dumper-7
*/

// Package: BPI_CanParry

#include "Basic.hpp"

#include "CoreUObject_classes.hpp"


namespace SDK
{

// BlueprintGeneratedClass BPI_CanParry.BPI_CanParry_C
// 0x0000 (0x0028 - 0x0028)
class IBPI_CanParry_C final : public IInterface
{
public:
	void Send_Threat_Location_R(const struct FVector& Threat_Location, const struct FVector& Offset, bool Immediate, bool* Nul);

public:
	static class UClass* StaticClass()
	{
		return StaticBPGeneratedClassImpl<"BPI_CanParry_C">();
	}
	static class IBPI_CanParry_C* GetDefaultObj()
	{
		return GetDefaultObjImpl<IBPI_CanParry_C>();
	}
};
static_assert(alignof(IBPI_CanParry_C) == 0x000008, "Wrong alignment on IBPI_CanParry_C");
static_assert(sizeof(IBPI_CanParry_C) == 0x000028, "Wrong size on IBPI_CanParry_C");

}

