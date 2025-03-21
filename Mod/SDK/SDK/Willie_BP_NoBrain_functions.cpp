﻿#pragma once

/*
* SDK generated by Dumper-7
*
* https://github.com/Encryqed/Dumper-7
*/

// Package: Willie_BP_NoBrain

#include "Basic.hpp"

#include "Willie_BP_NoBrain_classes.hpp"
#include "Willie_BP_NoBrain_parameters.hpp"


namespace SDK
{

// Function Willie_BP_NoBrain.Willie_BP_NoBrain_C.ExecuteUbergraph_Willie_BP_NoBrain
// (Final, UbergraphFunction)
// Parameters:
// int32                                   EntryPoint                                             (BlueprintVisible, BlueprintReadOnly, Parm, ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash)

void AWillie_BP_NoBrain_C::ExecuteUbergraph_Willie_BP_NoBrain(int32 EntryPoint)
{
	static class UFunction* Func = nullptr;

	if (Func == nullptr)
		Func = Class->GetFunction("Willie_BP_NoBrain_C", "ExecuteUbergraph_Willie_BP_NoBrain");

	Params::Willie_BP_NoBrain_C_ExecuteUbergraph_Willie_BP_NoBrain Parms{};

	Parms.EntryPoint = EntryPoint;

	UObject::ProcessEvent(Func, &Parms);
}


// Function Willie_BP_NoBrain.Willie_BP_NoBrain_C.ReceiveBeginPlay
// (Event, Protected, BlueprintEvent)

void AWillie_BP_NoBrain_C::ReceiveBeginPlay()
{
	static class UFunction* Func = nullptr;

	if (Func == nullptr)
		Func = Class->GetFunction("Willie_BP_NoBrain_C", "ReceiveBeginPlay");

	UObject::ProcessEvent(Func, nullptr);
}


// Function Willie_BP_NoBrain.Willie_BP_NoBrain_C.ReceiveTick
// (Event, Public, BlueprintEvent)
// Parameters:
// float                                   DeltaSeconds                                           (BlueprintVisible, BlueprintReadOnly, Parm, ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash)

void AWillie_BP_NoBrain_C::ReceiveTick(float DeltaSeconds)
{
	static class UFunction* Func = nullptr;

	if (Func == nullptr)
		Func = Class->GetFunction("Willie_BP_NoBrain_C", "ReceiveTick");

	Params::Willie_BP_NoBrain_C_ReceiveTick Parms{};

	Parms.DeltaSeconds = DeltaSeconds;

	UObject::ProcessEvent(Func, &Parms);
}

}

