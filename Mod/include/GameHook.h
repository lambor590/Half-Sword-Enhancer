#pragma once

#include "Logger.h"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Basic.hpp"
#include "MemoryUtils.h"

typedef void*(__stdcall* ProcessEvent)(SDK::UObject*, SDK::UFunction*, void*);

class GameHook
{
public:
    uintptr_t oProcessEvent = NULL;

    bool Hook();
    void Unhook() const;
    void AddHook(SDK::UFunction*, void*); // TODO: Implement this function with a proper system

private:
    Logger logger{ "GameHook" };
};