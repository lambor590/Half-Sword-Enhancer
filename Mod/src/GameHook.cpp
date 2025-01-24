#include "GameHook.h"

static GameHook* hookInstance = new GameHook();

inline static void* __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* Parms)
{
    // std::cout << pFunc->GetFullName() << std::endl;
    return ((ProcessEvent)hookInstance->oProcessEvent)(pObject, pFunc, Parms);
}

bool GameHook::Hook()
{
    logger.Log("Hooking ProcessEvent");

    SDK::UObject* pObject = SDK::BasicFilesImpleUtils::GetObjectByIndex(0);

    if (!pObject) {
        logger.Log("Could not get an instance of UObject. Retrying...");
        return false;
    }

    uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(pObject);
    oProcessEvent = vtable[SDK::Offsets::ProcessEventIdx];

    MemoryUtils::PlaceHook(oProcessEvent, (uintptr_t)OnProcessEvent, (uintptr_t*)&hookInstance->oProcessEvent);

    logger.Log("Main hook initialized successfully!");
    return true;
}

void GameHook::Unhook() const
{
    logger.Log("Unhooking ProcessEvent");
    MemoryUtils::Unhook(oProcessEvent);
}