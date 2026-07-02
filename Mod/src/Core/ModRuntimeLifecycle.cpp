#include "Core/ModRuntimeLifecycle.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

#include "Hooks/GameHook.h"
#include "Logger.h"
#include "Menu/Sections/Settings/GraphicsSection.h"
#include "Render/Renderer.h"
#include "Utils/AIDirector.h"
#include "Utils/AssetOverrideManager.h"

namespace {
    enum class State : std::uint8_t { NotStarted, Starting, Running, Failed, Stopping, Stopped };
    enum class StartedStep : std::uint8_t { None, Renderer, GameHook };

    Logger logger{"ModRuntimeLifecycle"};
    Renderer renderer;

    std::atomic<State> state{State::NotStarted};
    std::atomic<bool> stopRequested{false};
    StartedStep startedStep = StartedStep::None;

    std::mutex workerMutex;
    std::thread startupWorker;

    void StopStartedAdapters() noexcept {
        try {
            if (startedStep == StartedStep::GameHook) {
                AIDirector::Get().OnRuntimeShutdown();
                GameHook::Get().Unhook();
            }
            if (startedStep != StartedStep::None) renderer.Cleanup();
            startedStep = StartedStep::None;
        } catch (...) {
            logger.Log("Exception during Mod runtime lifecycle cleanup");
        }
    }

    void MarkStartupFailed(const char* step) noexcept {
        logger.Log("Startup failed at %s", step);
        StopStartedAdapters();
        state.store(State::Failed, std::memory_order_release);
    }

    void StartWorker() noexcept {
        try {
            if (!renderer.Hook()) {
                MarkStartupFailed("renderer hook");
                return;
            }
            startedStep = StartedStep::Renderer;

            if (stopRequested.load(std::memory_order_acquire)) {
                StopStartedAdapters();
                state.store(State::Stopped, std::memory_order_release);
                return;
            }

            if (!GameHook::Get().Hook()) {
                MarkStartupFailed("game hook");
                return;
            }
            startedStep = StartedStep::GameHook;

            if (stopRequested.load(std::memory_order_acquire)) {
                StopStartedAdapters();
                state.store(State::Stopped, std::memory_order_release);
                return;
            }

            if (!AssetOverrideManager::Get().Initialize()) {
                MarkStartupFailed("asset overrides");
                return;
            }
        } catch (...) {
            MarkStartupFailed("startup exception");
            return;
        }

        try {
            GraphicsSection::ApplyOnStartup();
        } catch (...) {
            logger.Log("Non-critical graphics startup settings failed");
        }

        state.store(State::Running, std::memory_order_release);
    }
}

void ModRuntimeLifecycle::StartAsync() noexcept {
    std::lock_guard lock(workerMutex);

    const auto current = state.load(std::memory_order_acquire);
    if (current == State::Starting || current == State::Running || current == State::Stopping) return;

    if (startupWorker.joinable()) startupWorker.join();

    stopRequested.store(false, std::memory_order_release);
    startedStep = StartedStep::None;
    state.store(State::Starting, std::memory_order_release);

    try {
        startupWorker = std::thread([]() noexcept { StartWorker(); });
    } catch (...) {
        logger.Log("Failed to create startup worker");
        state.store(State::Failed, std::memory_order_release);
    }
}

void ModRuntimeLifecycle::Stop() noexcept {
    stopRequested.store(true, std::memory_order_release);

    {
        std::lock_guard lock(workerMutex);
        if (startupWorker.joinable()) startupWorker.join();
    }

    const auto current = state.load(std::memory_order_acquire);
    if (current == State::NotStarted || current == State::Stopped || current == State::Stopping) return;

    if (startedStep == StartedStep::None) return;

    state.store(State::Stopping, std::memory_order_release);
    StopStartedAdapters();
    state.store(State::Stopped, std::memory_order_release);
}
