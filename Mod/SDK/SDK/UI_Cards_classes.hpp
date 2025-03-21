﻿#pragma once

/*
* SDK generated by Dumper-7
*
* https://github.com/Encryqed/Dumper-7
*/

// Package: UI_Cards

#include "Basic.hpp"

#include "Engine_structs.hpp"
#include "UMG_classes.hpp"
#include "Enum_PlayCards_structs.hpp"


namespace SDK
{

// WidgetBlueprintGeneratedClass UI_Cards.UI_Cards_C
// 0x0078 (0x0358 - 0x02E0)
class UUI_Cards_C final : public UUserWidget
{
public:
	struct FPointerToUberGraphFrame               UberGraphFrame;                                    // 0x02E0(0x0008)(ZeroConstructor, Transient, DuplicateTransient)
	class UWidgetAnimation*                       Select_Card_2;                                     // 0x02E8(0x0008)(BlueprintVisible, BlueprintReadOnly, ZeroConstructor, Transient, RepSkip, NoDestructor, HasGetValueTypeHash)
	class UWidgetAnimation*                       Select_Card_1;                                     // 0x02F0(0x0008)(BlueprintVisible, BlueprintReadOnly, ZeroConstructor, Transient, RepSkip, NoDestructor, HasGetValueTypeHash)
	class UWidgetAnimation*                       FadeIn;                                            // 0x02F8(0x0008)(BlueprintVisible, BlueprintReadOnly, ZeroConstructor, Transient, RepSkip, NoDestructor, HasGetValueTypeHash)
	class UButton*                                Card_1;                                            // 0x0300(0x0008)(BlueprintVisible, ExportObject, BlueprintReadOnly, ZeroConstructor, DisableEditOnInstance, InstancedReference, RepSkip, NoDestructor, PersistentInstance, HasGetValueTypeHash)
	class UButton*                                Card_2;                                            // 0x0308(0x0008)(BlueprintVisible, ExportObject, BlueprintReadOnly, ZeroConstructor, DisableEditOnInstance, InstancedReference, RepSkip, NoDestructor, PersistentInstance, HasGetValueTypeHash)
	class UImage*                                 Image_2;                                           // 0x0310(0x0008)(BlueprintVisible, ExportObject, BlueprintReadOnly, ZeroConstructor, DisableEditOnInstance, InstancedReference, RepSkip, NoDestructor, PersistentInstance, HasGetValueTypeHash)
	class UWidgetAnimation*                       WinAnim_0;                                         // 0x0318(0x0008)(Edit, BlueprintVisible, ZeroConstructor, DisableEditOnInstance, NoDestructor, HasGetValueTypeHash)
	double                                        Hold_Meter;                                        // 0x0320(0x0008)(Edit, BlueprintVisible, ZeroConstructor, DisableEditOnInstance, IsPlainOldData, NoDestructor, HasGetValueTypeHash)
	class ABP_HalfSwordGameMode_C*                BP_Half_Sword_Game_Mode;                           // 0x0328(0x0008)(Edit, BlueprintVisible, ZeroConstructor, DisableEditOnTemplate, DisableEditOnInstance, NoDestructor, HasGetValueTypeHash)
	class AWillie_BP_C*                           My_Master__Willie_;                                // 0x0330(0x0008)(Edit, BlueprintVisible, ZeroConstructor, DisableEditOnTemplate, DisableEditOnInstance, NoDestructor, HasGetValueTypeHash)
	TArray<Enum_PlayCards>                        Cards_Array;                                       // 0x0338(0x0010)(Edit, BlueprintVisible, DisableEditOnInstance)
	class ABP_HalfSwordGameMode_C*                As_BP_Half_Sword_Game_Mode;                        // 0x0348(0x0008)(Edit, BlueprintVisible, ZeroConstructor, DisableEditOnTemplate, DisableEditOnInstance, NoDestructor, HasGetValueTypeHash)
	int32                                         Player_Rank;                                       // 0x0350(0x0004)(Edit, BlueprintVisible, ZeroConstructor, DisableEditOnInstance, IsPlainOldData, NoDestructor, HasGetValueTypeHash)
	Enum_PlayCards                                Card_1_Type;                                       // 0x0354(0x0001)(Edit, BlueprintVisible, ZeroConstructor, DisableEditOnInstance, IsPlainOldData, NoDestructor, HasGetValueTypeHash)
	Enum_PlayCards                                Card_2_Type;                                       // 0x0355(0x0001)(Edit, BlueprintVisible, ZeroConstructor, DisableEditOnInstance, IsPlainOldData, NoDestructor, HasGetValueTypeHash)
	bool                                          Destroyed;                                         // 0x0356(0x0001)(Edit, BlueprintVisible, ZeroConstructor, DisableEditOnInstance, IsPlainOldData, NoDestructor, HasGetValueTypeHash)

public:
	void BndEvt__UI_Cards_Card_1_K2Node_ComponentBoundEvent_0_OnButtonClickedEvent__DelegateSignature();
	void BndEvt__UI_Cards_Card_1_K2Node_ComponentBoundEvent_1_OnButtonHoverEvent__DelegateSignature();
	void BndEvt__UI_Cards_Card_1_K2Node_ComponentBoundEvent_2_OnButtonHoverEvent__DelegateSignature();
	void BndEvt__UI_Cards_Card_2_K2Node_ComponentBoundEvent_4_OnButtonHoverEvent__DelegateSignature();
	void BndEvt__UI_Cards_Card_2_K2Node_ComponentBoundEvent_5_OnButtonHoverEvent__DelegateSignature();
	void BndEvt__UI_Cards_Card_2_K2Node_ComponentBoundEvent_6_OnButtonClickedEvent__DelegateSignature();
	void Construct();
	void ExecuteUbergraph_UI_Cards(int32 EntryPoint);
	void PreConstruct(bool IsDesignTime);
	void Tick(const struct FGeometry& MyGeometry, float InDeltaTime);

public:
	static class UClass* StaticClass()
	{
		return StaticBPGeneratedClassImpl<"UI_Cards_C">();
	}
	static class UUI_Cards_C* GetDefaultObj()
	{
		return GetDefaultObjImpl<UUI_Cards_C>();
	}
};
static_assert(alignof(UUI_Cards_C) == 0x000008, "Wrong alignment on UUI_Cards_C");
static_assert(sizeof(UUI_Cards_C) == 0x000358, "Wrong size on UUI_Cards_C");
static_assert(offsetof(UUI_Cards_C, UberGraphFrame) == 0x0002E0, "Member 'UUI_Cards_C::UberGraphFrame' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, Select_Card_2) == 0x0002E8, "Member 'UUI_Cards_C::Select_Card_2' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, Select_Card_1) == 0x0002F0, "Member 'UUI_Cards_C::Select_Card_1' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, FadeIn) == 0x0002F8, "Member 'UUI_Cards_C::FadeIn' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, Card_1) == 0x000300, "Member 'UUI_Cards_C::Card_1' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, Card_2) == 0x000308, "Member 'UUI_Cards_C::Card_2' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, Image_2) == 0x000310, "Member 'UUI_Cards_C::Image_2' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, WinAnim_0) == 0x000318, "Member 'UUI_Cards_C::WinAnim_0' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, Hold_Meter) == 0x000320, "Member 'UUI_Cards_C::Hold_Meter' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, BP_Half_Sword_Game_Mode) == 0x000328, "Member 'UUI_Cards_C::BP_Half_Sword_Game_Mode' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, My_Master__Willie_) == 0x000330, "Member 'UUI_Cards_C::My_Master__Willie_' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, Cards_Array) == 0x000338, "Member 'UUI_Cards_C::Cards_Array' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, As_BP_Half_Sword_Game_Mode) == 0x000348, "Member 'UUI_Cards_C::As_BP_Half_Sword_Game_Mode' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, Player_Rank) == 0x000350, "Member 'UUI_Cards_C::Player_Rank' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, Card_1_Type) == 0x000354, "Member 'UUI_Cards_C::Card_1_Type' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, Card_2_Type) == 0x000355, "Member 'UUI_Cards_C::Card_2_Type' has a wrong offset!");
static_assert(offsetof(UUI_Cards_C, Destroyed) == 0x000356, "Member 'UUI_Cards_C::Destroyed' has a wrong offset!");

}

