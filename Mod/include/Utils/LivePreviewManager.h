#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include "imgui/imgui.h"
#include "Hooks/GameHook.h"
#include "Menu/SectionConfig.h"
#include "SDK/Engine_classes.hpp"

class LivePreviewManager {
    struct PreviewState;

public:
    using CleanupFn = std::function<void()>;

    struct PreviewToken {
    private:
        friend class LivePreviewManager;

        PreviewToken(std::weak_ptr<PreviewState> requestState, std::uint64_t requestGeneration)
            : state(requestState), generation(requestGeneration) {}

        std::weak_ptr<PreviewState> state;
        std::uint64_t generation = 0;
    };

    explicit LivePreviewManager(PreviewConfig& cfg) : cfg(cfg) {}
    ~LivePreviewManager() {
        previewState->alive.store(false, std::memory_order_release);
        previewState->enabled.store(false, std::memory_order_release);
        Destroy();
    }

    [[nodiscard]] SDK::AActor* GetPreviewActor() const {
        auto* state = previewState.get();
        return state->previewActor.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool IsEnabled() const { return cfg.livePreview; }

    void SetCleanupCallback(const CleanupFn& fn) {
        auto* state = previewState.get();
        std::lock_guard<std::mutex> lock(state->cleanupMutex);
        state->onCleanup = fn;
    }

    PreviewToken BeginPreview() {
        auto state = previewState;
        const auto generation = state->generation.fetch_add(1, std::memory_order_acq_rel) + 1;
        state->enabled.store(cfg.livePreview, std::memory_order_release);
        state->autoRotate.store(cfg.autoRotate, std::memory_order_release);
        DestroyPreviewActor();
        return {state, generation};
    }

    [[nodiscard]] static bool IsCurrent(const PreviewToken& token) {
        return IsCurrent(token.state.lock(), token.generation);
    }

    [[nodiscard]] static bool SetPreviewActor(const PreviewToken& token, SDK::AActor* actor, SDK::UWorld* world) {
        auto state = token.state.lock();
        if (!state) return false;

        bool shouldRotate = false;
        double yaw = 0.0;
        {
            std::lock_guard<std::mutex> lock(state->actorMutex);
            if (!IsCurrent(state, token.generation)) return false;

            state->previewWorld.store(world, std::memory_order_release);
            state->previewActor.store(actor, std::memory_order_release);
            shouldRotate = actor && state->autoRotate.load(std::memory_order_acquire);
            yaw = state->yaw.load(std::memory_order_acquire);
        }

        if (shouldRotate) actor->K2_SetActorRotation(SDK::FRotator{0.0, yaw, 0.0}, true);
        return true;
    }

    void Disable() {
        cfg.livePreview = false;
        previewState->enabled.store(false, std::memory_order_release);
        prevEnabled = false;
        Destroy();
    }

    void Destroy() {
        previewState->generation.fetch_add(1, std::memory_order_acq_rel);
        DestroyPreviewActor();
    }

    void Rotate() {
        if (!cfg.autoRotate) return;

        auto* state = previewState.get();
        auto* actor = state->previewActor.load(std::memory_order_acquire);
        if (!actor) return;

        double yaw = state->yaw.load(std::memory_order_acquire);
        yaw += cfg.rotationSpeed * static_cast<double>(ImGui::GetIO().DeltaTime);
        if (yaw >= 360.0) yaw -= 360.0;
        if (yaw < 0.0) yaw += 360.0;
        state->yaw.store(yaw, std::memory_order_release);
        auto* world = state->previewWorld.load(std::memory_order_acquire);

        rotationState->yaw.store(yaw, std::memory_order_release);
        if (rotationState->queued.exchange(true, std::memory_order_acq_rel)) return;

        auto queuedRotation = rotationState;
        if (!GameHook::QueueAction([actor, world, queuedRotation](const RuntimeContextSnapshot& runtime) {
                queuedRotation->queued.store(false, std::memory_order_release);
                const double y = queuedRotation->yaw.load(std::memory_order_acquire);
                if (actor && runtime.world == world) actor->K2_SetActorRotation(SDK::FRotator{0.0, y, 0.0}, true);
            })) {
            queuedRotation->queued.store(false, std::memory_order_release);
        }
    }

    void SyncToggleState() {
        bool enabled = cfg.livePreview;
        previewState->enabled.store(enabled, std::memory_order_release);
        if (enabled && !prevEnabled) forceRefresh = true;
        const bool destroy = !enabled && prevEnabled;
        prevEnabled = enabled;

        if (destroy) Destroy();
    }

    template <typename SpawnFn> void Update(bool needsRefresh, SpawnFn&& spawnFn) {
        if (!needsRefresh && !forceRefresh) return;
        forceRefresh = false;
        auto* state = previewState.get();
        if (state->previewActor.load(std::memory_order_acquire) &&
            (ImGui::GetTime() - lastChangeTime < REFRESH_COOLDOWN))
            return;
        lastChangeTime = ImGui::GetTime();
        spawnFn();
    }

    void InvalidateIfDead(const SDK::AWillie_BP_C* player, const SDK::UWorld* world) {
        auto* state = previewState.get();
        const bool destroy = state->previewActor.load(std::memory_order_acquire) &&
                             (!player || !world || world != state->previewWorld.load(std::memory_order_acquire));

        if (destroy) Destroy();
    }

private:
    static constexpr double REFRESH_COOLDOWN = 0.2;

    struct PreviewState {
        std::mutex actorMutex;
        std::mutex cleanupMutex;
        std::atomic<std::uint64_t> generation{0};
        std::atomic_bool enabled{false};
        std::atomic_bool alive{true};
        std::atomic_bool autoRotate{false};
        std::atomic<SDK::AActor*> previewActor{nullptr};
        std::atomic<SDK::UWorld*> previewWorld{nullptr};
        std::atomic<double> yaw{0.0};
        CleanupFn onCleanup;
    };

    [[nodiscard]] static bool IsCurrent(const std::shared_ptr<PreviewState>& state, std::uint64_t generation) {
        return state && state->alive.load(std::memory_order_acquire) &&
               state->enabled.load(std::memory_order_acquire) &&
               state->generation.load(std::memory_order_acquire) == generation;
    }

    void DestroyPreviewActor() {
        SDK::AActor* actor = nullptr;
        SDK::UWorld* world = nullptr;
        CleanupFn cleanup;
        auto* state = previewState.get();
        {
            std::lock_guard<std::mutex> lock(state->actorMutex);
            actor = state->previewActor.exchange(nullptr, std::memory_order_acq_rel);
            world = state->previewWorld.exchange(nullptr, std::memory_order_acq_rel);
        }

        if (!actor) return;
        {
            std::lock_guard<std::mutex> lock(state->cleanupMutex);
            cleanup = state->onCleanup;
        }
        if (cleanup) cleanup();
        GameHook::QueueAction([actor, world](const RuntimeContextSnapshot& runtime) {
            if (actor && runtime.world == world) actor->K2_DestroyActor();
        });
    }

    double lastChangeTime = 0.0;
    bool forceRefresh = false;
    bool prevEnabled = false;

    std::shared_ptr<PreviewState> previewState = std::make_shared<PreviewState>();

    struct RotationQueueState {
        std::atomic_bool queued{false};
        std::atomic<double> yaw{0.0};
    };
    std::shared_ptr<RotationQueueState> rotationState = std::make_shared<RotationQueueState>();

    PreviewConfig& cfg;
};
