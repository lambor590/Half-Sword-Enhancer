#pragma once

#include <atomic>
#include <vector>

#include "Menu/Section.h"
#include "SDK/Engine_classes.hpp"

class SkyEditorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::World, "Sky Editor", "Change the time of day, lighting, atmosphere, fog, and clouds."
    };

private:
    static constexpr int TAB_COUNT = 5;
    static constexpr const char* TAB_LABELS[TAB_COUNT] = {"Sun", "Atmosphere", "Ambient Light", "Fog", "Clouds"};

    SDK::UDirectionalLightComponent* sunComp = nullptr;
    SDK::USkyAtmosphereComponent* atmoComp = nullptr;
    SDK::USkyLightComponent* skyLightComp = nullptr;
    SDK::UExponentialHeightFogComponent* fogComp = nullptr;
    SDK::UVolumetricCloudComponent* cloudComp = nullptr;
    SDK::UWorld* cachedWorld = nullptr;
    std::vector<SDK::UDirectionalLightComponent*> sunTargets;

    bool searchPending = false;
    std::atomic<bool> componentsReady{false};
    int activeTab = 0;

    float sunPitch = 45.0f;
    float sunYaw = 0.0f;
    float sunIntensity = 10.0f;
    float sunColor[3] = {1.f, 1.f, 1.f};
    float sunTemperature = 6500.f;
    bool sunUseTemperature = false;
    bool sunOverrideActive = false;
    std::atomic<bool> sunOverrideQueued{false};
    float sunSourceAngle = 0.5357f;
    float sunSoftAngle = 0.0f;
    float sunBloomScale = 1.0f;
    float sunBloomThreshold = -1.0f;
    float sunShadowAmount = 1.0f;
    float sunVolumetricScatter = 1.0f;
    float sunIndirectIntensity = 1.0f;

    float rayleighScale = 1.0f;
    float rayleighColor[3] = {0.175f, 0.409f, 1.0f};
    float mieScale = 1.0f;
    float mieAnisotropy = 0.8f;
    float multiScatter = 1.0f;
    float skyLuminance[4] = {1.f, 1.f, 1.f, 1.f};
    float atmoHeight = 60.0f;

    float skyLightIntensity = 1.0f;
    float skyLightColor[3] = {1.f, 1.f, 1.f};
    float lowerHemiColor[4] = {0.f, 0.f, 0.f, 1.f};

    float fogDensity = 0.02f;
    float fogColor[3] = {0.45f, 0.55f, 0.65f};
    float fogFalloff = 0.2f;
    float fogStartDist = 0.0f;
    float fogMaxOpacity = 1.0f;

    float cloudBottomAlt = 5.0f;
    float cloudHeight = 10.0f;
    float cloudViewSamples = 1.0f;
    float cloudShadowSamples = 1.0f;
    float cloudShadowDist = 50.0f;

    void ResetState();
    void ReadInitialValues();
    void FindComponents();
    void QueueApplySunState();
    void ApplyAtmosphere();
    void ApplySkyLight();
    void ApplyFog();
    void ApplyClouds();
    void ApplyPreset(int presetIndex);
    void RenderSunTab();
    void RenderAtmoTab();
    void RenderSkyLightTab();
    void RenderFogTab();
    void RenderCloudsTab();
    bool UpdateComponentScan();

public:
    explicit SkyEditorSection(ModContext& ctx);
    void OnOpen() override;
    void Render() override;
};
