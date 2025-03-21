﻿#pragma once

/*
* SDK generated by Dumper-7
*
* https://github.com/Encryqed/Dumper-7
*/

// Package: UI_TabButton

#include "Basic.hpp"

#include "Engine_structs.hpp"
#include "CoreUObject_structs.hpp"
#include "SlateCore_structs.hpp"
#include "UMG_classes.hpp"


namespace SDK
{

// WidgetBlueprintGeneratedClass UI_TabButton.UI_TabButton_C
// 0x0070 (0x0350 - 0x02E0)
class UUI_TabButton_C final : public UUserWidget
{
public:
	struct FPointerToUberGraphFrame               UberGraphFrame;                                    // 0x02E0(0x0008)(ZeroConstructor, Transient, DuplicateTransient)
	class UButton*                                Button;                                            // 0x02E8(0x0008)(BlueprintVisible, ExportObject, BlueprintReadOnly, ZeroConstructor, DisableEditOnInstance, InstancedReference, RepSkip, NoDestructor, PersistentInstance, HasGetValueTypeHash)
	int32                                         TabToOpen;                                         // 0x02F0(0x0004)(Edit, BlueprintVisible, ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash)
	uint8                                         Pad_2F4[0x4];                                      // 0x02F4(0x0004)(Fixing Size After Last Property [ Dumper-7 ])
	class UUI_PhotoMode_C*                        PhotoModeWidgetRef;                                // 0x02F8(0x0008)(Edit, BlueprintVisible, ZeroConstructor, DisableEditOnInstance, InstancedReference, NoDestructor, HasGetValueTypeHash)
	class UObject*                                TabIcon;                                           // 0x0300(0x0008)(Edit, BlueprintVisible, ZeroConstructor, NoDestructor, HasGetValueTypeHash)
	struct FVector2D                              ButtonSize;                                        // 0x0308(0x0010)(Edit, BlueprintVisible, ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash)
	struct FVector2D                              ButtonSizeWhenSelected;                            // 0x0318(0x0010)(Edit, BlueprintVisible, ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash)
	struct FSlateColor                            SelectedTabColor;                                  // 0x0328(0x0014)(Edit, BlueprintVisible)
	struct FSlateColor                            UnselectedTabColor;                                // 0x033C(0x0014)(Edit, BlueprintVisible)

public:
	void BndEvt__Button_99_K2Node_ComponentBoundEvent_0_OnButtonClickedEvent__DelegateSignature();
	void ExecuteUbergraph_UI_TabButton(int32 EntryPoint);
	void Initialize(class UUI_PhotoMode_C* PhotoModeWidgetRef_0);
	void PreConstruct(bool IsDesignTime);
	void SetSelectedTabStyle();
	void SetUnselectedTabStyle();

public:
	static class UClass* StaticClass()
	{
		return StaticBPGeneratedClassImpl<"UI_TabButton_C">();
	}
	static class UUI_TabButton_C* GetDefaultObj()
	{
		return GetDefaultObjImpl<UUI_TabButton_C>();
	}
};
static_assert(alignof(UUI_TabButton_C) == 0x000008, "Wrong alignment on UUI_TabButton_C");
static_assert(sizeof(UUI_TabButton_C) == 0x000350, "Wrong size on UUI_TabButton_C");
static_assert(offsetof(UUI_TabButton_C, UberGraphFrame) == 0x0002E0, "Member 'UUI_TabButton_C::UberGraphFrame' has a wrong offset!");
static_assert(offsetof(UUI_TabButton_C, Button) == 0x0002E8, "Member 'UUI_TabButton_C::Button' has a wrong offset!");
static_assert(offsetof(UUI_TabButton_C, TabToOpen) == 0x0002F0, "Member 'UUI_TabButton_C::TabToOpen' has a wrong offset!");
static_assert(offsetof(UUI_TabButton_C, PhotoModeWidgetRef) == 0x0002F8, "Member 'UUI_TabButton_C::PhotoModeWidgetRef' has a wrong offset!");
static_assert(offsetof(UUI_TabButton_C, TabIcon) == 0x000300, "Member 'UUI_TabButton_C::TabIcon' has a wrong offset!");
static_assert(offsetof(UUI_TabButton_C, ButtonSize) == 0x000308, "Member 'UUI_TabButton_C::ButtonSize' has a wrong offset!");
static_assert(offsetof(UUI_TabButton_C, ButtonSizeWhenSelected) == 0x000318, "Member 'UUI_TabButton_C::ButtonSizeWhenSelected' has a wrong offset!");
static_assert(offsetof(UUI_TabButton_C, SelectedTabColor) == 0x000328, "Member 'UUI_TabButton_C::SelectedTabColor' has a wrong offset!");
static_assert(offsetof(UUI_TabButton_C, UnselectedTabColor) == 0x00033C, "Member 'UUI_TabButton_C::UnselectedTabColor' has a wrong offset!");

}

