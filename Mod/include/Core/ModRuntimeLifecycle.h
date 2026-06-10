#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

#include "Logger.h"
#include "Render/Renderer.h"

class ModRuntimeLifecycle {
public:
    enum class State : std::uint8_t { NotStarted, Starting, Running, Failed, Stopping, Stopped };

    static ModRuntimeLifecycle& Get() noexcept;

    void StartAsync() noexcept;
    void Stop() noexcept;

    [[nodiscard]] State GetState() const noexcept { return state.load(std::memory_order_acquire); }

    ModRuntimeLifecycle(const ModRuntimeLifecycle&) = delete;
    ModRuntimeLifecycle& operator=(const ModRuntimeLifecycle&) = delete;

private:
    enum class StartedStep : std::uint8_t { None, Renderer, GameHook };

    ModRuntimeLifecycle() = default;

    void StartWorker() noexcept;
    void StopStartedAdapters() noexcept;
    void MarkStartupFailed(const char* step) noexcept;

    static constexpr auto CLEANUP_TIMEOUT = std::chrono::seconds(2);

    Logger logger{"ModRuntimeLifecycle"};
    Renderer renderer;

    std::atomic<State> state{State::NotStarted};
    std::atomic<bool> stopRequested{false};
    StartedStep startedStep = StartedStep::None;

    std::mutex workerMutex;
    std::thread startupWorker;
};
