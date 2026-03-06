#pragma once

#include <string>
#include <atomic>

#include "Menu/ICollapsibleSection.h"
#include "Utils/GuiUtils.h"
#include "ComponentValidator.h"
#include "Hooks/GameHook.h"

class SkyEditorSection : public CollapsibleSection {
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

    void ResetState() {
        sunActor = nullptr;
        sunComp = nullptr;
        atmoComp = nullptr;
        skyLightComp = nullptr;
        fogComp = nullptr;
        cloudComp = nullptr;
        cachedWorld = nullptr;
        initialized = false;
        searchPending = false;
        infoText.clear();
        status = {};
    }

    void ReadInitialValues() {
        if (sunComp) {
            auto rot = sunActor->RootComponent->RelativeRotation;
            sunPitch = static_cast<float>(rot.Pitch);
            sunYaw = static_cast<float>(rot.Yaw);
            auto* lightComp = static_cast<SDK::ULightComponent*>(sunComp);
            sunIntensity = static_cast<SDK::ULightComponentBase*>(sunComp)->Intensity;
            auto lc = static_cast<SDK::ULightComponentBase*>(sunComp)->LightColor;
            sunColor[0] = lc.R / 255.f; sunColor[1] = lc.G / 255.f; sunColor[2] = lc.B / 255.f;
            sunTemperature = lightComp->Temperature;
            sunSourceAngle = sunComp->LightSourceAngle;
            sunSoftAngle = sunComp->LightSourceSoftAngle;
            sunBloomScale = lightComp->BloomScale;
            sunBloomThreshold = lightComp->BloomThreshold;
            sunShadowAmount = sunComp->ShadowAmount;
            sunVolumetricScatter = static_cast<SDK::ULightComponentBase*>(sunComp)->VolumetricScatteringIntensity;
            sunIndirectIntensity = static_cast<SDK::ULightComponentBase*>(sunComp)->IndirectLightingIntensity;
        }
        if (atmoComp) {
            rayleighScale = atmoComp->RayleighScatteringScale;
            auto& rs = atmoComp->RayleighScattering;
            rayleighColor[0] = rs.R; rayleighColor[1] = rs.G; rayleighColor[2] = rs.B;
            mieScale = atmoComp->MieScatteringScale;
            mieAnisotropy = atmoComp->MieAnisotropy;
            multiScatter = atmoComp->MultiScatteringFactor;
            auto& sl = atmoComp->SkyLuminanceFactor;
            skyLuminance[0] = sl.R; skyLuminance[1] = sl.G; skyLuminance[2] = sl.B; skyLuminance[3] = sl.A;
            atmoHeight = atmoComp->AtmosphereHeight;
        }
        if (skyLightComp) {
            skyLightIntensity = static_cast<SDK::ULightComponentBase*>(skyLightComp)->Intensity;
            auto lc = static_cast<SDK::ULightComponentBase*>(skyLightComp)->LightColor;
            skyLightColor[0] = lc.R / 255.f; skyLightColor[1] = lc.G / 255.f; skyLightColor[2] = lc.B / 255.f;
            auto& lh = skyLightComp->LowerHemisphereColor;
            lowerHemiColor[0] = lh.R; lowerHemiColor[1] = lh.G; lowerHemiColor[2] = lh.B; lowerHemiColor[3] = lh.A;
        }
        if (fogComp) {
            fogDensity = fogComp->FogDensity;
            fogFalloff = fogComp->FogHeightFalloff;
            fogMaxOpacity = fogComp->FogMaxOpacity;
            fogStartDist = fogComp->StartDistance;
            auto& fc = fogComp->FogInscatteringColor;
            fogColor[0] = fc.R; fogColor[1] = fc.G; fogColor[2] = fc.B;
        }
        if (cloudComp) {
            cloudBottomAlt = cloudComp->LayerBottomAltitude;
            cloudHeight = cloudComp->LayerHeight;
            cloudViewSamples = cloudComp->ViewSampleCountScale;
            cloudShadowSamples = cloudComp->ShadowViewSampleCountScale;
            cloudShadowDist = cloudComp->ShadowTracingDistance;
        }
    }

    void FindComponents() {
        if (searchPending) return;
        if (!ComponentValidator::Validate(world)) {
            status.Set("World not available", true);
            return;
        }
        searchPending = true;
        cachedWorld = world;
        status.Set("Searching...");

        GameHook::QueueAction([this]() {
            auto* dlClass = SDK::ADirectionalLight::StaticClass();
            auto* dl = SDK::UGameplayStatics::GetActorOfClass(cachedWorld, dlClass);
            if (dl) {
                sunActor = dl;
                sunComp = static_cast<SDK::UDirectionalLightComponent*>(
                    static_cast<SDK::ALight*>(dl)->LightComponent);
            }

            auto* saClass = SDK::ASkyAtmosphere::StaticClass();
            auto* sa = SDK::UGameplayStatics::GetActorOfClass(cachedWorld, saClass);
            if (sa)
                atmoComp = static_cast<SDK::ASkyAtmosphere*>(sa)->SkyAtmosphereComponent;

            auto* slClass = SDK::ASkyLight::StaticClass();
            auto* sl = SDK::UGameplayStatics::GetActorOfClass(cachedWorld, slClass);
            if (sl)
                skyLightComp = static_cast<SDK::ASkyLight*>(sl)->LightComponent;

            auto* fgClass = SDK::AExponentialHeightFog::StaticClass();
            auto* fg = SDK::UGameplayStatics::GetActorOfClass(cachedWorld, fgClass);
            if (fg)
                fogComp = static_cast<SDK::AExponentialHeightFog*>(fg)->Component;

            auto* vcClass = SDK::AVolumetricCloud::StaticClass();
            auto* vc = SDK::UGameplayStatics::GetActorOfClass(cachedWorld, vcClass);
            if (vc)
                cloudComp = static_cast<SDK::AVolumetricCloud*>(vc)->VolumetricCloudComponent;

            componentsReady.store(true, std::memory_order_release);
        });
    }

    void ApplySunRotation() {
        auto* comp = static_cast<SDK::USceneComponent*>(sunComp);
        auto* sl = skyLightComp;
        float p = sunPitch, y = sunYaw;
        GameHook::QueueAction([comp, sl, p, y]() {
            comp->K2_SetWorldRotation(SDK::FRotator{p, y, 0.0}, false, nullptr, false);
            if (sl) sl->RecaptureSky();
        });
    }

    void ApplySunLight() {
        auto* comp = sunComp;
        auto* sl = skyLightComp;
        float intensity = sunIntensity;
        SDK::FLinearColor color{sunColor[0], sunColor[1], sunColor[2], 1.f};
        GameHook::QueueAction([comp, sl, intensity, color]() {
            static_cast<SDK::ULightComponent*>(comp)->SetIntensity(intensity);
            static_cast<SDK::ULightComponent*>(comp)->SetLightColor(color, true);
            if (sl) sl->RecaptureSky();
        });
    }

    void ApplyAtmosphere() {
        auto* comp = atmoComp;
        auto* sl2 = skyLightComp;
        float rs = rayleighScale, ms = mieScale, ma = mieAnisotropy, msc = multiScatter, ah = atmoHeight;
        SDK::FLinearColor rc{rayleighColor[0], rayleighColor[1], rayleighColor[2], 1.f};
        SDK::FLinearColor sl{skyLuminance[0], skyLuminance[1], skyLuminance[2], skyLuminance[3]};
        GameHook::QueueAction([comp, sl2, rs, rc, ms, ma, msc, sl, ah]() {
            comp->SetRayleighScatteringScale(rs);
            comp->SetRayleighScattering(rc);
            comp->SetMieScatteringScale(ms);
            comp->SetMieAnisotropy(ma);
            comp->SetMultiScatteringFactor(msc);
            comp->SetSkyLuminanceFactor(sl);
            comp->SetAtmosphereHeight(ah);
            if (sl2) sl2->RecaptureSky();
        });
    }

    void ApplySkyLight() {
        auto* comp = skyLightComp;
        float intensity = skyLightIntensity;
        SDK::FLinearColor color{skyLightColor[0], skyLightColor[1], skyLightColor[2], 1.f};
        SDK::FLinearColor lh{lowerHemiColor[0], lowerHemiColor[1], lowerHemiColor[2], lowerHemiColor[3]};
        GameHook::QueueAction([comp, intensity, color, lh]() {
            comp->SetIntensity(intensity);
            comp->SetLightColor(color);
            comp->SetLowerHemisphereColor(lh);
            comp->RecaptureSky();
        });
    }

    void ApplyFog() {
        auto* comp = fogComp;
        float d = fogDensity, f = fogFalloff, s = fogStartDist, m = fogMaxOpacity;
        SDK::FLinearColor c{fogColor[0], fogColor[1], fogColor[2], 1.f};
        GameHook::QueueAction([comp, d, f, c, s, m]() {
            comp->SetFogDensity(d);
            comp->SetFogHeightFalloff(f);
            comp->SetFogInscatteringColor(c);
            comp->SetStartDistance(s);
            comp->SetFogMaxOpacity(m);
        });
    }

    void ApplyClouds() {
        auto* comp = cloudComp;
        float ba = cloudBottomAlt, h = cloudHeight, vs = cloudViewSamples;
        float ss = cloudShadowSamples, sd = cloudShadowDist;
        GameHook::QueueAction([comp, ba, h, vs, ss, sd]() {
            comp->SetLayerBottomAltitude(ba);
            comp->SetLayerHeight(h);
            comp->SetViewSampleCountScale(vs);
            comp->SetShadowViewSampleCountScale(ss);
            comp->SetShadowTracingDistance(sd);
        });
    }

    void ApplySunExtended() {
        auto* comp = sunComp;
        float sa = sunSourceAngle, soft = sunSoftAngle, bs = sunBloomScale;
        float bt = sunBloomThreshold, sha = sunShadowAmount;
        float vs = sunVolumetricScatter, ii = sunIndirectIntensity;
        GameHook::QueueAction([comp, sa, soft, bs, bt, sha, vs, ii]() {
            comp->SetLightSourceAngle(sa);
            comp->SetLightSourceSoftAngle(soft);
            comp->SetShadowAmount(sha);
            auto* lc = static_cast<SDK::ULightComponent*>(comp);
            lc->SetBloomScale(bs);
            lc->SetBloomThreshold(bt);
            lc->SetVolumetricScatteringIntensity(vs);
            lc->SetIndirectLightingIntensity(ii);
        });
    }

    void ApplyPreset(float pitch) {
        sunPitch = pitch;
        if (sunComp) ApplySunRotation();
    }

    void RenderSunTab() {
        if (!sunComp) { ImGui::TextDisabled("DirectionalLight not found"); return; }
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        if (ImGui::DragFloat("Intensity", &sunIntensity, 0.1f, 0.0f, 0.0f, "%.1f"))
            ApplySunLight();
        float col[3] = {sunColor[0], sunColor[1], sunColor[2]};
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
        if (ImGui::ColorEdit3("Color", col)) {
            sunColor[0] = col[0]; sunColor[1] = col[1]; sunColor[2] = col[2];
            ApplySunLight();
        }
        ImGui::PopItemWidth();
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        if (ImGui::DragFloat("Temperature", &sunTemperature, 50.f, 1000.f, 15000.f, "%.0f K")) {
            auto* comp = sunComp;
            float t = sunTemperature;
            GameHook::QueueAction([comp, t]() {
                static_cast<SDK::ULightComponent*>(comp)->SetUseTemperature(true);
                static_cast<SDK::ULightComponent*>(comp)->SetTemperature(t);
            });
        }
        ImGui::Separator();
        bool extChanged = false;
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        extChanged |= ImGui::DragFloat("Sun Disk Size", &sunSourceAngle, 0.05f, 0.0f, 20.0f, "%.2f");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        extChanged |= ImGui::DragFloat("Soft Angle", &sunSoftAngle, 0.05f, 0.0f, 20.0f, "%.2f");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        extChanged |= ImGui::DragFloat("Bloom Scale", &sunBloomScale, 0.01f, 0.0f, 0.0f, "%.2f");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        extChanged |= ImGui::DragFloat("Bloom Threshold", &sunBloomThreshold, 0.1f, 0.0f, 0.0f, "%.1f");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        extChanged |= ImGui::DragFloat("Shadow Amount", &sunShadowAmount, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        extChanged |= ImGui::DragFloat("Volumetric", &sunVolumetricScatter, 0.01f, 0.0f, 0.0f, "%.2f");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        extChanged |= ImGui::DragFloat("Indirect", &sunIndirectIntensity, 0.01f, 0.0f, 0.0f, "%.2f");
        if (extChanged) ApplySunExtended();
    }

    void RenderAtmoTab() {
        if (!atmoComp) { ImGui::TextDisabled("SkyAtmosphere not found"); return; }
        bool changed = false;
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("Rayleigh Scale", &rayleighScale, 0.01f, 0.0f, 0.0f, "%.3f");
        float rc[3] = {rayleighColor[0], rayleighColor[1], rayleighColor[2]};
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
        if (ImGui::ColorEdit3("Rayleigh Color", rc)) {
            rayleighColor[0] = rc[0]; rayleighColor[1] = rc[1]; rayleighColor[2] = rc[2];
            changed = true;
        }
        ImGui::PopItemWidth();
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("Mie Scale", &mieScale, 0.01f, 0.0f, 0.0f, "%.3f");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("Mie Anisotropy", &mieAnisotropy, 0.005f, 0.0f, 1.0f, "%.3f");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("Multi Scattering", &multiScatter, 0.01f, 0.0f, 0.0f, "%.3f");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("Atmo Height", &atmoHeight, 0.5f, 0.0f, 0.0f, "%.1f km");
        float sl[4] = {skyLuminance[0], skyLuminance[1], skyLuminance[2], skyLuminance[3]};
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
        if (ImGui::ColorEdit4("Sky Luminance", sl)) {
            skyLuminance[0] = sl[0]; skyLuminance[1] = sl[1]; skyLuminance[2] = sl[2]; skyLuminance[3] = sl[3];
            changed = true;
        }
        ImGui::PopItemWidth();
        if (changed) ApplyAtmosphere();
    }

    void RenderSkyLightTab() {
        if (!skyLightComp) { ImGui::TextDisabled("SkyLight not found"); return; }
        bool changed = false;
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("Intensity", &skyLightIntensity, 0.01f, 0.0f, 0.0f, "%.3f");
        float col[3] = {skyLightColor[0], skyLightColor[1], skyLightColor[2]};
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
        if (ImGui::ColorEdit3("Color", col)) {
            skyLightColor[0] = col[0]; skyLightColor[1] = col[1]; skyLightColor[2] = col[2];
            changed = true;
        }
        ImGui::PopItemWidth();
        float lh[4] = {lowerHemiColor[0], lowerHemiColor[1], lowerHemiColor[2], lowerHemiColor[3]};
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
        if (ImGui::ColorEdit4("Lower Hemisphere", lh)) {
            lowerHemiColor[0] = lh[0]; lowerHemiColor[1] = lh[1]; lowerHemiColor[2] = lh[2]; lowerHemiColor[3] = lh[3];
            changed = true;
        }
        ImGui::PopItemWidth();
        if (changed) ApplySkyLight();
    }

    void RenderFogTab() {
        if (!fogComp) { ImGui::TextDisabled("Fog not found"); return; }
        bool changed = false;
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("Density", &fogDensity, 0.001f, 0.0f, 0.0f, "%.4f");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("Falloff", &fogFalloff, 0.01f, 0.0f, 0.0f, "%.3f");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("Start Distance", &fogStartDist, 10.f, 0.0f, 0.0f, "%.0f");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("Max Opacity", &fogMaxOpacity, 0.01f, 0.0f, 1.0f, "%.2f");
        float col[3] = {fogColor[0], fogColor[1], fogColor[2]};
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
        if (ImGui::ColorEdit3("Inscattering Color", col)) {
            fogColor[0] = col[0]; fogColor[1] = col[1]; fogColor[2] = col[2];
            changed = true;
        }
        ImGui::PopItemWidth();
        if (changed) ApplyFog();
    }

    void RenderCloudsTab() {
        if (!cloudComp) { ImGui::TextDisabled("VolumetricCloud not found"); return; }
        bool changed = false;
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("Base Altitude", &cloudBottomAlt, 0.1f, 0.0f, 50.0f, "%.1f km");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("Layer Height", &cloudHeight, 0.1f, 0.1f, 100.0f, "%.1f km");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("View Samples", &cloudViewSamples, 0.05f, 0.1f, 4.0f, "%.2f");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("Shadow Samples", &cloudShadowSamples, 0.05f, 0.1f, 4.0f, "%.2f");
        ImGui::SetNextItemWidth(GuiUtils::kDragWidth);
        changed |= ImGui::DragFloat("Shadow Distance", &cloudShadowDist, 1.0f, 1.0f, 200.0f, "%.0f km");
        if (changed) ApplyClouds();
    }

public:
    const char* GetGroup() const noexcept override { return "Editors"; }

    SkyEditorSection() : CollapsibleSection("Sky Editor") {}

    void RenderContent() override {
        const SectionStyle::StyleRAII style;
        ImGui::PushID("SkyEdit");

        if (world != cachedWorld)
            ResetState();

        bool disabled = searchPending;
        if (disabled) ImGui::BeginDisabled();
        if (ImGui::Button("Find Sky Components"))
            FindComponents();
        if (disabled) ImGui::EndDisabled();

        if (!initialized && componentsReady.exchange(false, std::memory_order_acquire)) {
            int found = (sunComp ? 1 : 0) + (atmoComp ? 1 : 0) +
                        (skyLightComp ? 1 : 0) + (fogComp ? 1 : 0) +
                        (cloudComp ? 1 : 0);
            if (found == 0) {
                status.Set("No sky components found", true);
            } else {
                ReadInitialValues();
                infoText = std::string(sunComp ? "Sun " : "") +
                           (atmoComp ? "Atmo " : "") +
                           (skyLightComp ? "SkyLight " : "") +
                           (fogComp ? "Fog " : "") +
                           (cloudComp ? "Clouds" : "");
                initialized = true;
                status.Set("Found " + std::to_string(found) + " components!");
            }
            searchPending = false;
        }

        status.Render();
        if (!initialized) { ImGui::PopID(); return; }

        ImGui::TextDisabled("%s", infoText.c_str());
        ImGui::Spacing();

        if (sunActor) {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
            if (ImGui::SliderFloat("Sun Pitch", &sunPitch, -90.f, 90.f, "%.1f"))
                ApplySunRotation();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
            if (ImGui::SliderFloat("Sun Yaw", &sunYaw, -180.f, 180.f, "%.1f"))
                ApplySunRotation();
        }

        ImGui::Spacing();
        if (ImGui::Button("Day"))
            ApplyPreset(45.f);
        ImGui::SameLine();
        if (ImGui::Button("Sunset"))
            ApplyPreset(2.f);
        ImGui::SameLine();
        if (ImGui::Button("Night"))
            ApplyPreset(-30.f);

        ImGui::Spacing();
        GuiUtils::RenderUnderlineTabs("##SkyTabs", activeTab, TAB_LABELS, TAB_COUNT);

        ImGui::BeginChild("##SkyParams", ImVec2(0, 0), ImGuiChildFlags_None);
        switch (activeTab) {
        case 0: RenderSunTab(); break;
        case 1: RenderAtmoTab(); break;
        case 2: RenderSkyLightTab(); break;
        case 3: RenderFogTab(); break;
        case 4: RenderCloudsTab(); break;
        }
        ImGui::EndChild();

        ImGui::PopID();
    }
};
