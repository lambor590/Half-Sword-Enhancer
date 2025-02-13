#include "GameHook.h"
#include "MemoryUtils.h"

static GameHook* hookInstance = new GameHook();

inline static void* __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* Parms)
{
    auto& hooks = hookInstance->GetRegisteredHooks();

    auto it = hooks.find(pFunc->GetFullName());
    if (it != hooks.end()) {
        it->second(pObject, pFunc, Parms);
    }

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

    logger.Log("ProcessEvent hooked successfully!");
    return true;
}

void GameHook::Unhook() const
{
    MemoryUtils::Unhook(oProcessEvent);
    logger.Log("ProcessEvent unhooked successfully!");
}