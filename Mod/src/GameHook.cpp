#include "GameHook.h"

static GameHook* hookInstance = new GameHook();

inline static void* __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* Parms)
{
    const std::string& funcName = pFunc->GetFullName();
    size_t hash = std::hash<std::string>{}(funcName);

    if (auto it = hookInstance->hookMap.find(hash); it != hookInstance->hookMap.end()) {
        if (it->second.name == funcName) {
            it->second.callback();
        }
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