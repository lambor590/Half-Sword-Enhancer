#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>

#include "Menu/Section.h"
#include "SDK/Ultra_Dynamic_Sky_classes.hpp"

class SkyEditorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::World, "Sky Editor", "Edit the available sun or moon, lighting, atmosphere, fog, and clouds."
    };

private:
    enum LightEdit : unsigned { POSITION = 1, LIGHTING = 2, SIZE = 4, EFFECTS = 8 };

    struct State {
        SDK::UWorld* world = nullptr;
        std::uint64_t generation = 0;
        SDK::UDirectionalLightComponent* lightComp = nullptr;
        SDK::USkyAtmosphereComponent* atmoComp = nullptr;
        SDK::USkyLightComponent* skyLightComp = nullptr;
        SDK::UExponentialHeightFogComponent* fogComp = nullptr;
        SDK::UVolumetricCloudComponent* cloudComp = nullptr;
        SDK::AUltra_Dynamic_Sky_C* skyActor = nullptr;
        std::array<int32_t, 6> indices{-1, -1, -1, -1, -1, -1};
        bool moon = false;

        std::array<SDK::UObject*, 6> Targets() const {
            return {lightComp, atmoComp, skyLightComp, fogComp, cloudComp, skyActor};
        }

        float lightPitch = 45.f, lightYaw = 0.f, lightIntensity = 10.f;
        float lightColor[3] = {1.f, 1.f, 1.f};
        float lightTemperature = 6500.f;
        bool lightUseTemperature = false;
        float lightSize = 0.5357f, lightSoftAngle = 0.f;
        float lightBloomScale = 1.f, lightBloomThreshold = -1.f;
        float lightShadowAmount = 1.f, lightVolumetricScatter = 1.f, lightIndirectIntensity = 1.f;

        float rayleighScale = 1.f;
        float rayleighColor[3] = {0.175f, 0.409f, 1.f};
        float mieScale = 1.f, mieAnisotropy = 0.8f, multiScatter = 1.f;
        float skyLuminance[4] = {1.f, 1.f, 1.f, 1.f};
        float atmoHeight = 60.f;

        float skyLightIntensity = 1.f;
        float skyLightColor[3] = {1.f, 1.f, 1.f};
        float lowerHemiColor[4] = {0.f, 0.f, 0.f, 1.f};

        float fogDensity = 0.02f;
        float fogColor[3] = {0.45f, 0.55f, 0.65f};
        float fogFalloff = 0.2f, fogStartDist = 0.f, fogMaxOpacity = 1.f;

        float cloudBottomAlt = 5.f, cloudHeight = 10.f;
        float cloudViewSamples = 1.f, cloudShadowSamples = 1.f, cloudShadowDist = 50.f;
    };

    State state;
    std::mutex scanMutex;
    std::optional<State> scanResult;
    std::atomic<bool> scanPending{false}, lightEditQueued{false};
    std::atomic<std::uint64_t> generation{0};
    std::chrono::steady_clock::time_point nextScan{};
    unsigned pendingLightEdits = 0;
    int activeTab = 0;
    bool selectTab = false;

    static void ReadInitialValues(State& state);
    static State ScanComponents(SDK::UWorld* world);
    void FindComponents();
    void QueueApplyLightState(unsigned fields = 0);
    void ApplyPreset(int presetIndex);
    void RenderLightTab();
    void RenderAtmoTab();
    void RenderSkyLightTab();
    void RenderFogTab();
    void RenderCloudsTab();
    void UpdateComponentScan();

public:
    explicit SkyEditorSection(ModContext& ctx);
    void OnOpen() override;
    void Render() override;
};
