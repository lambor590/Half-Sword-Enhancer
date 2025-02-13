#pragma once

#include <Windows.h>
#include <string>
#include <unordered_map>
#include <functional>

#include "Logger.h"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Basic.hpp"
#include "MemoryUtils.h"

typedef void* (__stdcall* ProcessEvent)(SDK::UObject*, SDK::UFunction*, void*);

class GameHook
{
public:
    uintptr_t oProcessEvent = NULL;

    // Mapa de hooks registrados: nombre de función -> callback
    std::unordered_map<std::string, std::function<void(SDK::UObject*, SDK::UFunction*, void*)>> registeredHooks;

    bool Hook();
    void Unhook() const;

    // Registrar un nuevo hook
    void RegisterHook(const std::string& functionName, std::function<void()> callback) {
        registeredHooks[functionName] = [callback](SDK::UObject* obj, SDK::UFunction* func, void* params) {
            callback();
        };
    }

    // Desregistrar un hook
    void UnregisterHook(const std::string& functionName) {
        registeredHooks.erase(functionName);
    }

    // Obtener hooks registrados
    const auto& GetRegisteredHooks() const { return registeredHooks; }

    friend void* __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* Parms);

private:
    Logger logger{ "GameHook" };
};