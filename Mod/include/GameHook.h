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

private:
    Logger logger{ "GameHook" };
};