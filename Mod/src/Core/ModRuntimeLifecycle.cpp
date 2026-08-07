#include "Core/ModRuntimeLifecycle.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

#include "ConfigManager.h"
#include "Gui.h"
#include "Hooks/GameHook.h"
#include "Logger.h"
#include "Menu/Keybind.h"
#include "Menu/Sections/Settings/GraphicsSection.h"
#include "Menu/Sections/World/MapLoaderSection.h"
#include "Render/Renderer.h"
#include "Utils/AIDirector.h"
#include "Utils/ActorUtils.h"
#include "Utils/AssetOverrideManager.h"
#include "Utils/EquipmentApplication.h"
#include "Utils/FreeCameraManager.h"

namespace {
    enum class StartedStep : std::uint8_t { None, GameHook, Renderer, AssetOverrides, RuntimeSubsystems };

    Logger logger{"ModRuntimeLifecycle"};
    Renderer renderer;
    std::atomic<bool> active{false};
    std::atomic<bool> stopRequested{false};
    StartedStep startedStep = StartedStep::None;
    std::mutex workerMutex;
    std::thread startupWorker;

    void ShutdownStarted() noexcept {
        if (startedStep >= StartedStep::Renderer) renderer.Cleanup();
        if (!ConfigManager::Get().Flush()) logger.Log("Deferred configuration could not be flushed during shutdown");

        if (startedStep >= StartedStep::RuntimeSubsystems) AIDirector::Get().PrepareForRuntimeShutdown();

        if (startedStep >= StartedStep::AssetOverrides &&
            !GameHook::Get().ExecuteOnGameThreadAndWait([](const RuntimeContextSnapshot& runtime) {
                if (startedStep >= StartedStep::RuntimeSubsystems) {
                    FreeCameraManager::Get().PrepareForRuntimeShutdown(runtime);
                    EquipmentApplication::AbortRuntimeTransactionsForShutdown();
                }
                AssetOverrideManager::Get().PrepareForRuntimeShutdown();
            })) {
            logger.Log("Game-thread runtime cleanup could not be completed");
        }

        GameHook::Get().Quiesce();
        if (startedStep >= StartedStep::Renderer) MapLoaderSection::OnRuntimeShutdown();
        if (startedStep >= StartedStep::RuntimeSubsystems) {
            FreeCameraManager::Get().OnRuntimeShutdown();
            EquipmentApplication::OnRuntimeShutdown();
            AIDirector::Get().OnRuntimeShutdown();
            ActorUtils::OnRuntimeShutdown();
        }
        if (startedStep >= StartedStep::Renderer) KeybindRuntime::OnRuntimeShutdown();
        if (startedStep >= StartedStep::AssetOverrides) AssetOverrideManager::Get().Shutdown();
        GameHook::Get().Unhook();
        startedStep = StartedStep::None;
        active.store(false, std::memory_order_release);
    }

    [[nodiscard]] bool ContinueStartup(StartedStep completedStep) noexcept {
        startedStep = completedStep;
        if (!stopRequested.load(std::memory_order_acquire)) return true;
        ShutdownStarted();
        return false;
    }

    void FailStartup(const char* step) noexcept {
        logger.Log("Startup failed at %s", step);
        ShutdownStarted();
    }

    void StartWorker() noexcept {
        try {
            if (!GameHook::Get().Hook()) {
                FailStartup("game hook");
                return;
            }
            if (!ContinueStartup(StartedStep::GameHook)) return;

            if (!renderer.Hook()) {
                FailStartup("renderer hook");
                return;
            }
            if (!ContinueStartup(StartedStep::Renderer)) return;

            if (!AssetOverrideManager::Get().Initialize()) {
                FailStartup("asset overrides");
                return;
            }
            if (!ContinueStartup(StartedStep::AssetOverrides)) return;

            FreeCameraManager::Get().OnRuntimeStart();
            if (!ContinueStartup(StartedStep::RuntimeSubsystems)) return;
        } catch (...) {
            FailStartup("startup exception");
            return;
        }

        try {
            GraphicsSection::ApplyOnStartup();
        } catch (...) {
            logger.Log("Non-critical graphics startup settings failed");
        }

        if (!GameHook::QueueAction([](const RuntimeContextSnapshot&) {
                if (!renderer.HookSwapChainAfterStartup()) logger.Log("Failed to hook the game swap chain");
            })) {
            FailStartup("deferred renderer hook");
        }
    }
}

void ModRuntimeLifecycle::StartAsync() noexcept {
    std::lock_guard lock(workerMutex);
    if (active.load(std::memory_order_acquire)) return;
    if (startupWorker.joinable()) startupWorker.join();

    stopRequested.store(false, std::memory_order_release);
    active.store(true, std::memory_order_release);
    try {
        startupWorker = std::thread(StartWorker);
    } catch (...) {
        logger.Log("Failed to create startup worker");
        active.store(false, std::memory_order_release);
    }
}

bool ModRuntimeLifecycle::Stop() noexcept {
    if (GameHook::Get().IsGameThread() || renderer.IsInCallback() || Gui::Get().IsInCallback()) {
        logger.Log("Runtime shutdown must be requested outside game/render/window callbacks");
        return false;
    }

    std::lock_guard lock(workerMutex);
    stopRequested.store(true, std::memory_order_release);
    if (startupWorker.joinable()) startupWorker.join();
    if (!active.load(std::memory_order_acquire)) return true;

    ShutdownStarted();
    return true;
}
