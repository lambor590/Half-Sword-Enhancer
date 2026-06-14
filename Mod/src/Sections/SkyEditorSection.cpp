#include "Menu/Sections/World/SkyEditorSection.h"
#include "Menu/SectionStyle.h"
#include "Hooks/GameHook.h"

SkyEditorSection::SkyEditorSection(ModContext& ctx) : Section(ctx, SECTION) {}

void SkyEditorSection::ResetState() {
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

void SkyEditorSection::ReadInitialValues() {
    if (sunComp) {
        auto rot = sunActor->RootComponent->RelativeRotation;
        sunPitch = static_cast<float>(rot.Pitch);
        sunYaw = static_cast<float>(rot.Yaw);
        auto* lightComp = static_cast<SDK::ULightComponent*>(sunComp);
        sunIntensity = static_cast<SDK::ULightComponentBase*>(sunComp)->Intensity;
        auto lc = static_cast<SDK::ULightComponentBase*>(sunComp)->LightColor;
        sunColor[0] = lc.R / 255.f;
        sunColor[1] = lc.G / 255.f;
        sunColor[2] = lc.B / 255.f;
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
        rayleighColor[0] = rs.R;
        rayleighColor[1] = rs.G;
        rayleighColor[2] = rs.B;
        mieScale = atmoComp->MieScatteringScale;
        mieAnisotropy = atmoComp->MieAnisotropy;
        multiScatter = atmoComp->MultiScatteringFactor;
        auto& sl = atmoComp->SkyLuminanceFactor;
        skyLuminance[0] = sl.R;
        skyLuminance[1] = sl.G;
        skyLuminance[2] = sl.B;
        skyLuminance[3] = sl.A;
        atmoHeight = atmoComp->AtmosphereHeight;
    }
    if (skyLightComp) {
        skyLightIntensity = static_cast<SDK::ULightComponentBase*>(skyLightComp)->Intensity;
        auto lc = static_cast<SDK::ULightComponentBase*>(skyLightComp)->LightColor;
        skyLightColor[0] = lc.R / 255.f;
        skyLightColor[1] = lc.G / 255.f;
        skyLightColor[2] = lc.B / 255.f;
        auto& lh = skyLightComp->LowerHemisphereColor;
        lowerHemiColor[0] = lh.R;
        lowerHemiColor[1] = lh.G;
        lowerHemiColor[2] = lh.B;
        lowerHemiColor[3] = lh.A;
    }
    if (fogComp) {
        fogDensity = fogComp->FogDensity;
        fogFalloff = fogComp->FogHeightFalloff;
        fogMaxOpacity = fogComp->FogMaxOpacity;
        fogStartDist = fogComp->StartDistance;
        auto& fc = fogComp->FogInscatteringColor;
        fogColor[0] = fc.R;
        fogColor[1] = fc.G;
        fogColor[2] = fc.B;
    }
    if (cloudComp) {
        cloudBottomAlt = cloudComp->LayerBottomAltitude;
        cloudHeight = cloudComp->LayerHeight;
        cloudViewSamples = cloudComp->ViewSampleCountScale;
        cloudShadowSamples = cloudComp->ShadowViewSampleCountScale;
        cloudShadowDist = cloudComp->ShadowTracingDistance;
    }
}

void SkyEditorSection::FindComponents() {
    if (searchPending) return;
    auto* world = RenderWorld();
    if (!world) {
        status.Set("World not available", true);
        return;
    }
    searchPending = true;
    cachedWorld = world;
    status.Set("Searching...");

    GameHook::QueueAction([this](const RuntimeContextSnapshot&) {
        auto* dlClass = SDK::ADirectionalLight::StaticClass();
        auto* dl = SDK::UGameplayStatics::GetActorOfClass(cachedWorld, dlClass);
        if (dl) {
            sunActor = dl;
            sunComp = static_cast<SDK::UDirectionalLightComponent*>(static_cast<SDK::ALight*>(dl)->LightComponent);
        }

        auto* saClass = SDK::ASkyAtmosphere::StaticClass();
        auto* sa = SDK::UGameplayStatics::GetActorOfClass(cachedWorld, saClass);
        if (sa) atmoComp = static_cast<SDK::ASkyAtmosphere*>(sa)->SkyAtmosphereComponent;

        auto* slClass = SDK::ASkyLight::StaticClass();
        auto* sl = SDK::UGameplayStatics::GetActorOfClass(cachedWorld, slClass);
        if (sl) skyLightComp = static_cast<SDK::ASkyLight*>(sl)->LightComponent;

        auto* fgClass = SDK::AExponentialHeightFog::StaticClass();
        auto* fg = SDK::UGameplayStatics::GetActorOfClass(cachedWorld, fgClass);
        if (fg) fogComp = static_cast<SDK::AExponentialHeightFog*>(fg)->Component;

        auto* vcClass = SDK::AVolumetricCloud::StaticClass();
        auto* vc = SDK::UGameplayStatics::GetActorOfClass(cachedWorld, vcClass);
        if (vc) cloudComp = static_cast<SDK::AVolumetricCloud*>(vc)->VolumetricCloudComponent;

        componentsReady.store(true, std::memory_order_release);
    });
}

void SkyEditorSection::ApplySunRotation(bool recapture) {
    auto* comp = static_cast<SDK::USceneComponent*>(sunComp);
    auto* sl = recapture ? skyLightComp : nullptr;
    float p = sunPitch, y = sunYaw;
    GameHook::QueueAction([comp, sl, p, y](const RuntimeContextSnapshot&) {
        comp->K2_SetWorldRotation(SDK::FRotator{p, y, 0.0}, false, nullptr, false);
        if (sl) sl->RecaptureSky();
    });
}

void SkyEditorSection::ApplySunLight() {
    auto* comp = sunComp;
    auto* sl = skyLightComp;
    float intensity = sunIntensity;
    SDK::FLinearColor color{sunColor[0], sunColor[1], sunColor[2], 1.f};
    GameHook::QueueAction([comp, sl, intensity, color](const RuntimeContextSnapshot&) {
        static_cast<SDK::ULightComponent*>(comp)->SetIntensity(intensity);
        static_cast<SDK::ULightComponent*>(comp)->SetLightColor(color, true);
        if (sl) sl->RecaptureSky();
    });
}

void SkyEditorSection::ApplyAtmosphere() {
    auto* comp = atmoComp;
    auto* sl2 = skyLightComp;
    float rs = rayleighScale, ms = mieScale, ma = mieAnisotropy, msc = multiScatter, ah = atmoHeight;
    SDK::FLinearColor rc{rayleighColor[0], rayleighColor[1], rayleighColor[2], 1.f};
    SDK::FLinearColor sl{skyLuminance[0], skyLuminance[1], skyLuminance[2], skyLuminance[3]};
    GameHook::QueueAction([comp, sl2, rs, rc, ms, ma, msc, sl, ah](const RuntimeContextSnapshot&) {
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

void SkyEditorSection::ApplySkyLight() {
    auto* comp = skyLightComp;
    float intensity = skyLightIntensity;
    SDK::FLinearColor color{skyLightColor[0], skyLightColor[1], skyLightColor[2], 1.f};
    SDK::FLinearColor lh{lowerHemiColor[0], lowerHemiColor[1], lowerHemiColor[2], lowerHemiColor[3]};
    GameHook::QueueAction([comp, intensity, color, lh](const RuntimeContextSnapshot&) {
        comp->SetIntensity(intensity);
        comp->SetLightColor(color);
        comp->SetLowerHemisphereColor(lh);
        comp->RecaptureSky();
    });
}

void SkyEditorSection::ApplyFog() {
    auto* comp = fogComp;
    float d = fogDensity, f = fogFalloff, s = fogStartDist, m = fogMaxOpacity;
    SDK::FLinearColor c{fogColor[0], fogColor[1], fogColor[2], 1.f};
    GameHook::QueueAction([comp, d, f, c, s, m](const RuntimeContextSnapshot&) {
        comp->SetFogDensity(d);
        comp->SetFogHeightFalloff(f);
        comp->SetFogInscatteringColor(c);
        comp->SetStartDistance(s);
        comp->SetFogMaxOpacity(m);
    });
}

void SkyEditorSection::ApplySunExtended() {
    auto* comp = sunComp;
    float sa = sunSourceAngle, soft = sunSoftAngle, bs = sunBloomScale;
    float bt = sunBloomThreshold, sha = sunShadowAmount;
    float vs = sunVolumetricScatter, ii = sunIndirectIntensity;
    GameHook::QueueAction([comp, sa, soft, bs, bt, sha, vs, ii](const RuntimeContextSnapshot&) {
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

void SkyEditorSection::ApplyClouds() {
    auto* comp = cloudComp;
    float ba = cloudBottomAlt, h = cloudHeight, vs = cloudViewSamples;
    float ss = cloudShadowSamples, sd = cloudShadowDist;
    GameHook::QueueAction([comp, ba, h, vs, ss, sd](const RuntimeContextSnapshot&) {
        comp->SetLayerBottomAltitude(ba);
        comp->SetLayerHeight(h);
        comp->SetViewSampleCountScale(vs);
        comp->SetShadowViewSampleCountScale(ss);
        comp->SetShadowTracingDistance(sd);
    });
}

void SkyEditorSection::ApplyPreset(float pitch) {
    sunPitch = pitch;
    if (sunComp) ApplySunRotation();
}

void SkyEditorSection::RenderSunTab() {
    if (!sunComp) {
        ImGui::TextDisabled("DirectionalLight not found");
        return;
    }
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    if (GuiUtils::DebouncedDragFloat("Intensity", &sunIntensity, 0.1f, 0.0f, 0.0f, "%.1f")) ApplySunLight();
    float col[3] = {sunColor[0], sunColor[1], sunColor[2]};
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
    if (ImGui::ColorEdit3("Color", col)) {
        sunColor[0] = col[0];
        sunColor[1] = col[1];
        sunColor[2] = col[2];
        ApplySunLight();
    }
    ImGui::PopItemWidth();
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    if (GuiUtils::DebouncedDragFloat("Temperature", &sunTemperature, 50.f, 1000.f, 15000.f, "%.0f K")) {
        auto* comp = sunComp;
        float t = sunTemperature;
        GameHook::QueueAction([comp, t](const RuntimeContextSnapshot&) {
            static_cast<SDK::ULightComponent*>(comp)->SetUseTemperature(true);
            static_cast<SDK::ULightComponent*>(comp)->SetTemperature(t);
        });
    }
    ImGui::Separator();
    bool extChanged = false;
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    extChanged |= GuiUtils::DebouncedDragFloat("Sun Disk Size", &sunSourceAngle, 0.05f, 0.0f, 20.0f, "%.2f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    extChanged |= GuiUtils::DebouncedDragFloat("Soft Angle", &sunSoftAngle, 0.05f, 0.0f, 20.0f, "%.2f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    extChanged |= GuiUtils::DebouncedDragFloat("Bloom Scale", &sunBloomScale, 0.01f, 0.0f, 0.0f, "%.2f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    extChanged |= GuiUtils::DebouncedDragFloat("Bloom Threshold", &sunBloomThreshold, 0.1f, 0.0f, 0.0f, "%.1f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    extChanged |= GuiUtils::DebouncedDragFloat("Shadow Amount", &sunShadowAmount, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    extChanged |= GuiUtils::DebouncedDragFloat("Volumetric", &sunVolumetricScatter, 0.01f, 0.0f, 0.0f, "%.2f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    extChanged |= GuiUtils::DebouncedDragFloat("Indirect", &sunIndirectIntensity, 0.01f, 0.0f, 0.0f, "%.2f");
    if (extChanged) ApplySunExtended();
}

void SkyEditorSection::RenderAtmoTab() {
    if (!atmoComp) {
        ImGui::TextDisabled("SkyAtmosphere not found");
        return;
    }
    bool changed = false;
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("Rayleigh Scale", &rayleighScale, 0.01f, 0.0f, 0.0f, "%.3f");
    float rc[3] = {rayleighColor[0], rayleighColor[1], rayleighColor[2]};
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
    if (ImGui::ColorEdit3("Rayleigh Color", rc)) {
        rayleighColor[0] = rc[0];
        rayleighColor[1] = rc[1];
        rayleighColor[2] = rc[2];
        changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("Mie Scale", &mieScale, 0.01f, 0.0f, 0.0f, "%.3f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("Mie Anisotropy", &mieAnisotropy, 0.005f, 0.0f, 1.0f, "%.3f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("Multi Scattering", &multiScatter, 0.01f, 0.0f, 0.0f, "%.3f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("Atmo Height", &atmoHeight, 0.5f, 0.0f, 0.0f, "%.1f km");
    float sl[4] = {skyLuminance[0], skyLuminance[1], skyLuminance[2], skyLuminance[3]};
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
    if (ImGui::ColorEdit4("Sky Luminance", sl)) {
        skyLuminance[0] = sl[0];
        skyLuminance[1] = sl[1];
        skyLuminance[2] = sl[2];
        skyLuminance[3] = sl[3];
        changed = true;
    }
    ImGui::PopItemWidth();
    if (changed) ApplyAtmosphere();
}

void SkyEditorSection::RenderSkyLightTab() {
    if (!skyLightComp) {
        ImGui::TextDisabled("SkyLight not found");
        return;
    }
    bool changed = false;
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("Intensity", &skyLightIntensity, 0.01f, 0.0f, 0.0f, "%.3f");
    float col[3] = {skyLightColor[0], skyLightColor[1], skyLightColor[2]};
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
    if (ImGui::ColorEdit3("Color", col)) {
        skyLightColor[0] = col[0];
        skyLightColor[1] = col[1];
        skyLightColor[2] = col[2];
        changed = true;
    }
    ImGui::PopItemWidth();
    float lh[4] = {lowerHemiColor[0], lowerHemiColor[1], lowerHemiColor[2], lowerHemiColor[3]};
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
    if (ImGui::ColorEdit4("Lower Hemisphere", lh)) {
        lowerHemiColor[0] = lh[0];
        lowerHemiColor[1] = lh[1];
        lowerHemiColor[2] = lh[2];
        lowerHemiColor[3] = lh[3];
        changed = true;
    }
    ImGui::PopItemWidth();
    if (changed) ApplySkyLight();
}

void SkyEditorSection::RenderFogTab() {
    if (!fogComp) {
        ImGui::TextDisabled("Fog not found");
        return;
    }
    bool changed = false;
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("Density", &fogDensity, 0.001f, 0.0f, 0.0f, "%.4f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("Falloff", &fogFalloff, 0.01f, 0.0f, 0.0f, "%.3f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("Start Distance", &fogStartDist, 10.f, 0.0f, 0.0f, "%.0f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("Max Opacity", &fogMaxOpacity, 0.01f, 0.0f, 1.0f, "%.2f");
    float col[3] = {fogColor[0], fogColor[1], fogColor[2]};
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
    if (ImGui::ColorEdit3("Inscattering Color", col)) {
        fogColor[0] = col[0];
        fogColor[1] = col[1];
        fogColor[2] = col[2];
        changed = true;
    }
    ImGui::PopItemWidth();
    if (changed) ApplyFog();
}

void SkyEditorSection::RenderCloudsTab() {
    if (!cloudComp) {
        ImGui::TextDisabled("VolumetricCloud not found");
        return;
    }
    bool changed = false;
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("Base Altitude", &cloudBottomAlt, 0.1f, 0.0f, 50.0f, "%.1f km");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("Layer Height", &cloudHeight, 0.1f, 0.1f, 100.0f, "%.1f km");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("View Samples", &cloudViewSamples, 0.05f, 0.1f, 4.0f, "%.2f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("Shadow Samples", &cloudShadowSamples, 0.05f, 0.1f, 4.0f, "%.2f");
    ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
    changed |= GuiUtils::DebouncedDragFloat("Shadow Distance", &cloudShadowDist, 1.0f, 1.0f, 200.0f, "%.0f km");
    if (changed) ApplyClouds();
}

bool SkyEditorSection::RenderComponentStatus() {
    auto* world = RenderWorld();
    if (world != cachedWorld) ResetState();

    bool disabled = searchPending;
    if (disabled) ImGui::BeginDisabled();
    if (ImGui::Button("Find Sky Components")) FindComponents();
    if (disabled) ImGui::EndDisabled();

    if (!initialized && componentsReady.exchange(false, std::memory_order_acquire)) {
        int found =
            (sunComp ? 1 : 0) + (atmoComp ? 1 : 0) + (skyLightComp ? 1 : 0) + (fogComp ? 1 : 0) + (cloudComp ? 1 : 0);
        if (found == 0) {
            status.Set("No sky components found", true);
        } else {
            ReadInitialValues();
            infoText = std::string(sunComp ? "Sun " : "") + (atmoComp ? "Atmo " : "") +
                       (skyLightComp ? "SkyLight " : "") + (fogComp ? "Fog " : "") + (cloudComp ? "Clouds" : "");
            initialized = true;
            status.Set("Found " + std::to_string(found) + " components!");
        }
        searchPending = false;
    }

    status.Render();
    return initialized;
}

void SkyEditorSection::Render() {
    const SectionStyle::StyleRAII style;
    ImGui::PushID("SkyEdit");

    if (!RenderComponentStatus()) {
        ImGui::PopID();
        return;
    }

    ImGui::TextDisabled("%s", infoText.c_str());
    ImGui::Spacing();

    if (sunActor) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
        if (GuiUtils::DebouncedSliderFloat("Sun Pitch", &sunPitch, -90.f, 90.f, "%.1f")) ApplySunRotation();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
        if (GuiUtils::DebouncedSliderFloat("Sun Yaw", &sunYaw, -180.f, 180.f, "%.1f")) ApplySunRotation();
    }

    ImGui::Spacing();
    if (ImGui::Button("Day")) ApplyPreset(45.f);
    ImGui::SameLine();
    if (ImGui::Button("Sunset")) ApplyPreset(2.f);
    ImGui::SameLine();
    if (ImGui::Button("Night")) ApplyPreset(-30.f);

    ImGui::Spacing();
    GuiUtils::RenderUnderlineTabs("##SkyTabs", activeTab, TAB_LABELS, TAB_COUNT);

    ImGui::BeginChild("##SkyParams", ImVec2(0, 0), ImGuiChildFlags_None);
    switch (activeTab) {
        case 0: RenderSunTab(); break;
        case 1: RenderAtmoTab(); break;
        case 2: RenderSkyLightTab(); break;
        case 3: RenderFogTab(); break;
        case 4: RenderCloudsTab(); break;
        default: break;
    }
    ImGui::EndChild();

    ImGui::PopID();
}
