#pragma once

#include <string>
#include <atomic>

#include "Menu/Section.h"
#include "Utils/GuiUtils.h"
#include "SDK/Engine_classes.hpp"

class SkyEditorSection : public Section {
public:
    static constexpr SectionDefinition SECTION{MenuTab::World, "Sky Editor", "Environment"};

private:
    static constexpr int TAB_COUNT = 5;
    static constexpr const char* TAB_LABELS[TAB_COUNT] = {"Sun", "Atmosphere", "Sky Light", "Fog", "Clouds"};

    SDK::AActor* sunActor = nullptr;
    SDK::UDirectionalLightComponent* sunComp = nullptr;
    SDK::USkyAtmosphereComponent* atmoComp = nullptr;
    SDK::USkyLightComponent* skyLightComp = nullptr;
    SDK::UExponentialHeightFogComponent* fogComp = nullptr;
    SDK::UVolumetricCloudComponent* cloudComp = nullptr;
    SDK::UWorld* cachedWorld = nullptr;

    bool initialized = false;
    bool searchPending = false;
    std::atomic<bool> componentsReady{false};
    int activeTab = 0;
    GuiUtils::StatusMessage status;
    std::string infoText;

    float sunPitch = 45.0f;
    float sunYaw = 0.0f;
    float sunIntensity = 10.0f;
    float sunColor[3] = {1.f, 1.f, 1.f};
    float sunTemperature = 6500.f;
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
    void ApplySunRotation(bool recapture = true);
    void ApplySunLight();
    void ApplyAtmosphere();
    void ApplySkyLight();
    void ApplyFog();
    void ApplySunExtended();
    void ApplyClouds();
    void ApplyPreset(float pitch);
    void RenderSunTab();
    void RenderAtmoTab();
    void RenderSkyLightTab();
    void RenderFogTab();
    void RenderCloudsTab();
    bool RenderComponentStatus();

public:
    explicit SkyEditorSection(ModContext& ctx);
    void Render() override;
};
