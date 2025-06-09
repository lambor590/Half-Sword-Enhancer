#include "Hooks/GameHook.h"
#include "Menu/Utils/Spawner.h"

static GameHook* hookInstance = &GameHook::Get();

static inline uint64_t hash_string(std::string_view str) noexcept {
    constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;
    
    uint64_t hash = FNV_OFFSET_BASIS;
    for (char c : str) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= FNV_PRIME;
    }
    return hash;
}

inline static void* __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* Parms)
{
    const std::string& funcName = pFunc->GetName();
    uint64_t funcHash = hash_string(funcName);
        
    if (auto it = hookInstance->hookMap.find(funcHash); it != hookInstance->hookMap.end()) {
        const auto& hookData = it->second;
        if (hookData.className.empty() ||
            hash_string(pObject->Class->GetName()) == hookData.classHash) {
            hookData.callback();
        }
    }

    return ((ProcessEvent)hookInstance->oProcessEvent)(pObject, pFunc, Parms);
}

void GameHook::Hook()
{
    logger.Log("Hooking ProcessEvent");

    SDK::UObject* pObject = SDK::BasicFilesImpleUtils::GetObjectByIndex(0);

    while (!pObject) {
        logger.Log("Could not get an instance of UObject. Retrying...");
        pObject = SDK::BasicFilesImpleUtils::GetObjectByIndex(0);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(pObject);
    oProcessEvent = vtable[SDK::Offsets::ProcessEventIdx];

    MemoryUtils::PlaceHook(oProcessEvent, (uintptr_t)OnProcessEvent, (uintptr_t*)&hookInstance->oProcessEvent);

    RegisterEvent(GameEvent::OnTick, &hookInstance, []() {
        Spawner::ProcessSpawnQueue();
    });

    logger.Log("ProcessEvent hooked successfully!");
}

void GameHook::Unhook() const
{
    MemoryUtils::Unhook(oProcessEvent);
    logger.Log("ProcessEvent unhooked successfully!");
}

void GameHook::RegisterHook(const std::string& functionName, std::function<void()> callback) {
    auto [hookClass, hookFunc] = ParseFunctionName(functionName);
    uint64_t funcHash = hash_string(hookFunc);
    uint64_t classHash = hookClass.empty() ? 0 : hash_string(hookClass);
    
    hookMap[funcHash] = HookData(std::string(hookClass), std::string(hookFunc), classHash, std::move(callback));
}

void GameHook::UnregisterHook(const std::string& functionName) {
    auto [_, hookFunc] = ParseFunctionName(functionName);
    uint64_t hash = hash_string(hookFunc);
    hookMap.erase(hash);
}