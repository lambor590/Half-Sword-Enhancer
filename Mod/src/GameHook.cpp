#include "Hooks/GameHook.h"
#include "Utils/Spawner.h"
#include "ConfigManager.h"

static GameHook* hookInstance = &GameHook::Get();

namespace {
    constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;

    constexpr uint64_t hash_string_impl(const char* str, size_t len) noexcept {
        uint64_t hash = FNV_OFFSET_BASIS;
        for (size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(str[i]));
            hash *= FNV_PRIME;
        }
        return hash;
    }

    uint64_t hash_string_fast(const char* str) noexcept {
        uint64_t hash = FNV_OFFSET_BASIS;
        while (*str) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(*str++));
            hash *= FNV_PRIME;
        }
        return hash;
    }
}

inline static void* __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* Parms)
{
    const std::string& funcName = pFunc->GetName();
    uint64_t funcHash = hash_string_fast(funcName.c_str());

    if (auto it = hookInstance->hookMap.find(funcHash); it != hookInstance->hookMap.end()) [[likely]] {
        it->second();
    }

    return ((ProcessEvent)hookInstance->oProcessEvent)(pObject, pFunc, Parms);
}

void GameHook::Hook()
{
    logger.Log("Hooking ProcessEvent");

    hookMap.reserve(HOOK_MAP_RESERVE_SIZE);

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

    if (ConfigManager::Get().GetBool("UE", "console_enabled", false)) {
        UnlockUEConsole();
    }

    logger.Log("ProcessEvent hooked successfully!");
}

void GameHook::Unhook() const
{
    MemoryUtils::Unhook(oProcessEvent);
    logger.Log("ProcessEvent unhooked successfully!");
}

void GameHook::RegisterHook(const std::string& functionName, std::function<void()> callback) {
    uint64_t hash = hash_string_fast(functionName.c_str());
    hookMap.emplace(hash, std::move(callback));
}

void GameHook::UnregisterHook(const std::string& functionName) {
    uint64_t hash = hash_string_fast(functionName.c_str());
    hookMap.erase(hash);
}

void GameHook::UnlockUEConsole() {
    SDK::UEngine* engine = SDK::UEngine::GetEngine();
    if (!engine) {
        logger.Log("Failed to get UEngine instance");
        return;
    }

    SDK::UInputSettings* inputSettings = SDK::UInputSettings::GetDefaultObj();
    if (inputSettings && inputSettings->ConsoleKeys.Num() > 0) {
        inputSettings->ConsoleKeys[0].KeyName = SDK::UKismetStringLibrary::Conv_StringToName(SDK::FString(L"F2"));
        logger.Log("Console key changed to F2");
    }

    SDK::UGameViewportClient* viewport = engine->GameViewport;
    if (viewport) {
        if (!viewport->ViewportConsole) {
            SDK::UObject* newConsole = SDK::UGameplayStatics::SpawnObject(engine->ConsoleClass, engine->GameViewport);
            if (newConsole) {
                viewport->ViewportConsole = static_cast<SDK::UConsole*>(newConsole);
                logger.Log("Console object created successfully");
            }
        }
        logger.Log("Viewport input settings configured");
    }

    logger.Log("UE Console unlocked - Press F2 to open console");
}

void GameHook::LockUEConsole() {
    SDK::UEngine* engine = SDK::UEngine::GetEngine();
    if (!engine) {
        logger.Log("Failed to get UEngine instance");
        return;
    }

    SDK::UGameViewportClient* viewport = engine->GameViewport;
    if (viewport && viewport->ViewportConsole) {
        viewport->ViewportConsole = nullptr;
        logger.Log("Console object destroyed");
    }

    SDK::UInputSettings* inputSettings = SDK::UInputSettings::GetDefaultObj();
    if (inputSettings && inputSettings->ConsoleKeys.Num() > 0) {
        inputSettings->ConsoleKeys[0].KeyName = SDK::UKismetStringLibrary::Conv_StringToName(SDK::FString(L"None"));
        logger.Log("Console key disabled");
    }

    logger.Log("UE Console locked");
}