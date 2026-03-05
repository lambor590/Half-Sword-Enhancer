#pragma once

#include <functional>
#include "imgui/imgui.h"
#include "Hooks/GameHook.h"
#include "Menu/SectionConfig.h"
#include "SDK/Engine_classes.hpp"

class LivePreviewManager {
public:
    using CleanupFn = std::function<void()>;

    explicit LivePreviewManager(SectionConfig::PreviewConfig& cfg) : cfg(cfg) {}
    ~LivePreviewManager() { Destroy(); }

    void SetPreviewActor(SDK::AActor* actor) { previewActor = actor; }
    [[nodiscard]] SDK::AActor* GetPreviewActor() const { return previewActor; }
    [[nodiscard]] double GetYaw() const { return yaw; }

    void SetCleanupCallback(CleanupFn fn) { onCleanup = std::move(fn); }

    void Destroy() {
        if (onCleanup) onCleanup();
        if (!previewActor) return;
        SDK::AActor* actor = previewActor;
        previewActor = nullptr;
        GameHook::QueueAction([actor]() {
            if (actor) actor->K2_DestroyActor();
        });
    }

    void Rotate() {
        if (!previewActor || !cfg.autoRotate) return;
        yaw += cfg.rotationSpeed * static_cast<double>(ImGui::GetIO().DeltaTime);
        if (yaw >= 360.0) yaw -= 360.0;
        if (yaw < 0.0) yaw += 360.0;
        double y = yaw;
        SDK::AActor* actor = previewActor;
        GameHook::QueueAction([actor, y]() {
            if (actor) actor->K2_SetActorRotation(SDK::FRotator{0.0, y, 0.0}, true);
        });
    }

    template<typename SpawnFn>
    void Update(bool needsRefresh, SpawnFn&& spawnFn) {
        if (!needsRefresh) return;
        if (previewActor && (ImGui::GetTime() - lastChangeTime < REFRESH_COOLDOWN)) return;
        lastChangeTime = ImGui::GetTime();
        spawnFn();
    }

    void InvalidateIfDead(const SDK::AWillie_BP_C* player, const SDK::UWorld* world) {
        if (previewActor && (!player || !world)) {
            if (onCleanup) onCleanup();
            previewActor = nullptr;
        }
    }

private:
    static constexpr double REFRESH_COOLDOWN = 0.2;

    SDK::AActor* previewActor = nullptr;
    double lastChangeTime = 0.0;
    double yaw = 0.0;

    SectionConfig::PreviewConfig& cfg;
    CleanupFn onCleanup;
};
