#include "Hooks/GameHook.h"
#include "ConfigManager.h"
#include "Menu/Sections/Settings/GraphicsSection.h"
#include "Utils/CompileTimeHash.h"

static GameHook* hookInstance = &GameHook::Get();

std::queue<std::function<void()>> GameHook::gameThreadQueue;
std::mutex GameHook::queueMutex;

namespace {
    struct alignas(64) FunctionHashCache {
        std::unordered_map<void*, uint64_t> cache;

        inline uint64_t GetOrComputeHash(SDK::UFunction* pFunc) noexcept {
            void* funcPtr = static_cast<void*>(pFunc);

            if (auto it = cache.find(funcPtr); it != cache.end()) [[likely]] {
                return it->second;
            }

            const std::string& funcName = pFunc->GetName();
            uint64_t hash = HS::Hash::FNV1A(funcName.c_str());
                cache.emplace(funcPtr, hash);

            return hash;
        }
    };

    static FunctionHashCache g_hashCache;
}

__forceinline static void* __stdcall OnProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunc, void* Parms) noexcept
{
    uint64_t funcHash = g_hashCache.GetOrComputeHash(pFunc);

    if (auto it = hookInstance->hookMap.find(funcHash); it != hookInstance->hookMap.end()) [[unlikely]] {
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
        GameHook::ProcessGameThreadQueue();
    });

    if (ConfigManager::Get().GetBool("UE", "console_enabled", false)) {
        UnlockUEConsole();
    }

    GraphicsSection::ApplyOnStartup();

    logger.Log("ProcessEvent hooked successfully!");
}

void GameHook::Unhook() const
{
    MemoryUtils::Unhook(oProcessEvent);
    logger.Log("ProcessEvent unhooked successfully!");
}

void GameHook::RegisterHook(const std::string& functionName, std::function<void()> callback) {
    uint64_t hash = HS::Hash::FNV1A(functionName.c_str());
    hookMap.emplace(hash, std::move(callback));
}

void GameHook::UnregisterHook(const std::string& functionName) {
    uint64_t hash = HS::Hash::FNV1A(functionName.c_str());
    hookMap.erase(hash);
}

void GameHook::RegisterHook(uint64_t hash, std::function<void()> callback) {
    hookMap.emplace(hash, std::move(callback));
}

void GameHook::UnregisterHook(uint64_t hash) {
    hookMap.erase(hash);
}

void GameHook::UnlockUEConsole() {
    SDK::UEngine* engine = SDK::UEngine::GetEngine();
    while (!engine) {
        logger.Log("Waiting for UEngine instance...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        engine = SDK::UEngine::GetEngine();
    }

    SDK::UInputSettings* inputSettings = SDK::UInputSettings::GetDefaultObj();
    if (inputSettings && inputSettings->ConsoleKeys.Num() > 0) {
        inputSettings->ConsoleKeys[0].KeyName = SDK::UKismetStringLibrary::Conv_StringToName(SDK::FString(L"F2"));
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

void GameHook::QueueAction(std::function<void()> action) {
    std::lock_guard<std::mutex> lock(queueMutex);
    gameThreadQueue.push(std::move(action));
}

void GameHook::ProcessGameThreadQueue() {
    std::lock_guard<std::mutex> lock(queueMutex);
    
    while (!gameThreadQueue.empty()) {
        const auto& action = gameThreadQueue.front();
        action();
        gameThreadQueue.pop();
    }
}

void GameHook::SetInputEnabled(bool enabled) {    
    SDK::APlayerController* playerController = nullptr;
    if (!ComponentValidator::Validate(playerController)) return;

    playerController->SetIgnoreLookInput(!enabled);
}