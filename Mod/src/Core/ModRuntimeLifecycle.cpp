#include "Core/ModRuntimeLifecycle.h"

#include <future>
#include <memory>

#include "Hooks/GameHook.h"
#include "Menu/Sections/Settings/GraphicsSection.h"
#include "Utils/AssetOverrideManager.h"

ModRuntimeLifecycle& ModRuntimeLifecycle::Get() noexcept {
    static auto* lifecycle = new ModRuntimeLifecycle();
    return *lifecycle;
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
        startupWorker = std::thread([this]() noexcept { StartWorker(); });
    } catch (...) {
        logger.Log("Failed to create startup worker");
        state.store(State::Failed, std::memory_order_release);
    }
}

void ModRuntimeLifecycle::StartWorker() noexcept {
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

    auto cleanupDone = std::make_shared<std::promise<void>>();
    auto cleanupFuture = cleanupDone->get_future();

    try {
        std::thread cleanupThread([this, cleanupDone]() noexcept {
            StopStartedAdapters();
            try {
                cleanupDone->set_value();
            } catch (...) {}
        });

        if (cleanupFuture.wait_for(CLEANUP_TIMEOUT) == std::future_status::timeout) {
            logger.Log("Cleanup timed out; releasing cleanup worker");
            cleanupThread.detach();
            return;
        }

        cleanupThread.join();
        state.store(State::Stopped, std::memory_order_release);
    } catch (...) {
        logger.Log("Failed to create cleanup worker; cleaning up synchronously");
        StopStartedAdapters();
        state.store(State::Stopped, std::memory_order_release);
    }
}

void ModRuntimeLifecycle::StopStartedAdapters() noexcept {
    try {
        if (startedStep == StartedStep::GameHook) GameHook::Get().Unhook();
        if (startedStep != StartedStep::None) renderer.Cleanup();
        startedStep = StartedStep::None;
    } catch (...) {
        logger.Log("Exception during Mod runtime lifecycle cleanup");
    }
}

void ModRuntimeLifecycle::MarkStartupFailed(const char* step) noexcept {
    logger.Log("Startup failed at %s", step);
    StopStartedAdapters();
    state.store(State::Failed, std::memory_order_release);
}
