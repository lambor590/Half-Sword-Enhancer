#pragma once

#include <functional>
#include "imgui/imgui.h"
#include "Hooks/GameHook.h"
#include "Menu/SectionConfig.h"
#include "SDK/Engine_classes.hpp"

class LivePreviewManager {
public:
    using CleanupFn = std::function<void()>;

    explicit LivePreviewManager(PreviewConfig& cfg) : cfg(cfg) {}
    ~LivePreviewManager() { Destroy(); }

    void SetPreviewActor(SDK::AActor* actor, SDK::UWorld* world) {
        previewActor = actor;
        previewWorld = world;
    }
    [[nodiscard]] SDK::AActor* GetPreviewActor() const { return previewActor; }
    [[nodiscard]] double GetYaw() const { return yaw; }

    void SetCleanupCallback(const CleanupFn& fn) { onCleanup = fn; }

    void Destroy() {
        if (!previewActor) return;
        if (onCleanup) onCleanup();
        SDK::AActor* actor = previewActor;
        SDK::UWorld* world = previewWorld;
        previewActor = nullptr;
        previewWorld = nullptr;
        GameHook::QueueAction([actor, world](const RuntimeContextSnapshot& runtime) {
            if (actor && runtime.world == world) actor->K2_DestroyActor();
        });
    }

    void Rotate() {
        if (!previewActor || !cfg.autoRotate) return;
        yaw += cfg.rotationSpeed * static_cast<double>(ImGui::GetIO().DeltaTime);
        if (yaw >= 360.0) yaw -= 360.0;
        if (yaw < 0.0) yaw += 360.0;
        double y = yaw;
        SDK::AActor* actor = previewActor;
        SDK::UWorld* world = previewWorld;
        GameHook::QueueAction([actor, world, y](const RuntimeContextSnapshot& runtime) {
            if (actor && runtime.world == world) actor->K2_SetActorRotation(SDK::FRotator{0.0, y, 0.0}, true);
        });
    }

    void SyncToggleState() {
        bool enabled = cfg.livePreview;
        if (enabled && !prevEnabled) forceRefresh = true;
        if (!enabled && prevEnabled && previewActor) Destroy();
        prevEnabled = enabled;
    }

    template <typename SpawnFn> void Update(bool needsRefresh, SpawnFn&& spawnFn) {
        if (!needsRefresh && !forceRefresh) return;
        forceRefresh = false;
        if (previewActor && (ImGui::GetTime() - lastChangeTime < REFRESH_COOLDOWN)) return;
        lastChangeTime = ImGui::GetTime();
        spawnFn();
    }

    void InvalidateIfDead(const SDK::AWillie_BP_C* player, const SDK::UWorld* world) {
        if (previewActor && (!player || !world || world != previewWorld)) {
            Destroy();
        }
    }

private:
    static constexpr double REFRESH_COOLDOWN = 0.2;

    SDK::AActor* previewActor = nullptr;
    SDK::UWorld* previewWorld = nullptr;
    double lastChangeTime = 0.0;
    double yaw = 0.0;
    bool forceRefresh = false;
    bool prevEnabled = false;

    PreviewConfig& cfg;
    CleanupFn onCleanup;
};
