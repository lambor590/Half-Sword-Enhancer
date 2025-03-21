﻿#pragma once

/*
* SDK generated by Dumper-7
*
* https://github.com/Encryqed/Dumper-7
*/

// Package: UI_BeginFight

#include "Basic.hpp"

#include "Engine_structs.hpp"
#include "UMG_classes.hpp"


namespace SDK
{

// WidgetBlueprintGeneratedClass UI_BeginFight.UI_BeginFight_C
// 0x0020 (0x0300 - 0x02E0)
class UUI_BeginFight_C final : public UUserWidget
{
public:
	struct FPointerToUberGraphFrame               UberGraphFrame;                                    // 0x02E0(0x0008)(ZeroConstructor, Transient, DuplicateTransient)
	class UImage*                                 Image_49;                                          // 0x02E8(0x0008)(BlueprintVisible, ExportObject, BlueprintReadOnly, ZeroConstructor, DisableEditOnInstance, InstancedReference, RepSkip, NoDestructor, PersistentInstance, HasGetValueTypeHash)
	class UMaterialInstanceDynamic*               NewVar;                                            // 0x02F0(0x0008)(Edit, BlueprintVisible, ZeroConstructor, DisableEditOnInstance, NoDestructor, HasGetValueTypeHash)
	float                                         In_Delta_Time;                                     // 0x02F8(0x0004)(Edit, BlueprintVisible, ZeroConstructor, DisableEditOnInstance, IsPlainOldData, NoDestructor, HasGetValueTypeHash)

public:
	void Construct();
	void ExecuteUbergraph_UI_BeginFight(int32 EntryPoint);
	void Tick(const struct FGeometry& MyGeometry, float InDeltaTime);

public:
	static class UClass* StaticClass()
	{
		return StaticBPGeneratedClassImpl<"UI_BeginFight_C">();
	}
	static class UUI_BeginFight_C* GetDefaultObj()
	{
		return GetDefaultObjImpl<UUI_BeginFight_C>();
	}
};
static_assert(alignof(UUI_BeginFight_C) == 0x000008, "Wrong alignment on UUI_BeginFight_C");
static_assert(sizeof(UUI_BeginFight_C) == 0x000300, "Wrong size on UUI_BeginFight_C");
static_assert(offsetof(UUI_BeginFight_C, UberGraphFrame) == 0x0002E0, "Member 'UUI_BeginFight_C::UberGraphFrame' has a wrong offset!");
static_assert(offsetof(UUI_BeginFight_C, Image_49) == 0x0002E8, "Member 'UUI_BeginFight_C::Image_49' has a wrong offset!");
static_assert(offsetof(UUI_BeginFight_C, NewVar) == 0x0002F0, "Member 'UUI_BeginFight_C::NewVar' has a wrong offset!");
static_assert(offsetof(UUI_BeginFight_C, In_Delta_Time) == 0x0002F8, "Member 'UUI_BeginFight_C::In_Delta_Time' has a wrong offset!");

}

