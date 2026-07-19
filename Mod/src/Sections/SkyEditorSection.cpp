#include "Menu/Sections/World/SkyEditorSection.h"
#include "Hooks/GameHook.h"
#include "Utils/GuiUtils.h"
#include "SDK/Enum_DayTime_structs.hpp"
#include "SDK/GI_Settings_classes.hpp"

#include <string>
#include <utility>

namespace {
    struct SkyTimePreset {
        const char* label;
        SDK::Enum_DayTime dayTime;
        float sunPitch;
        float sunYaw;
    };

    constexpr SkyTimePreset K_TIME_PRESETS[] = {
        {"Morning", SDK::Enum_DayTime::NewEnumerator0, 24.596f, 125.602f},
        {"Day", SDK::Enum_DayTime::NewEnumerator1, -54.212f, 53.548f},
        {"Evening", SDK::Enum_DayTime::NewEnumerator2, -11.875f, 149.903f},
        {"Night", SDK::Enum_DayTime::NewEnumerator3, -36.412f, 113.579f},
    };

    constexpr int K_TIME_PRESET_COUNT = static_cast<int>(sizeof(K_TIME_PRESETS) / sizeof(K_TIME_PRESETS[0]));

    struct LightingPresetSet {
        const char* mapToken;
        const wchar_t* levels[K_TIME_PRESET_COUNT];
    };

    constexpr const wchar_t* K_DEFAULT_LIGHTING_PRESETS[K_TIME_PRESET_COUNT] = {
        L"/Game/Maps/Lighting/Lighting_Morning",
        L"/Game/Maps/Lighting/Arena_Cutting_Map_Lighting",
        L"/Game/Maps/Lighting/Arena_Cutting_Map_Lighting_Evening",
        L"/Game/Maps/Lighting/Arena_Cutting_Map_Lighting_Night",
    };

    constexpr LightingPresetSet K_MAP_LIGHTING_PRESETS[] = {
        {"Map_Arena_Ambush",
         {
             L"/Game/Maps/Lighting/Arena_Ambush_Map_Morning_Lighting",
             L"/Game/Maps/Lighting/Arena_Ambush_Map_Lighting",
             L"/Game/Maps/Lighting/Arena_Ambush_Map_Evening_Lighting",
             L"/Game/Maps/Lighting/Arena_Ambush_Map_Night_Lighting",
         }},
        {"Map_Arena_EastTower",
         {
             L"/Game/Maps/Lighting/Map_Arena_EastTower_Dawn_Lighting",
             L"/Game/Maps/Lighting/Map_Arena_EastTower_Lighting",
             L"/Game/Maps/Lighting/Map_Arena_EastTower_Dawn_Lighting",
             L"/Game/Maps/Lighting/Map_Arena_EastTower_Night_Lighting",
         }},
        {"Map_Arena_Slums",
         {
             L"/Game/Maps/Lighting/Map_Arena_Slums_Morning_Lighting",
             L"/Game/Maps/Lighting/Map_Arena_Slums_Lighting",
             L"/Game/Maps/Lighting/Map_Arena_Slums_Evening_Lighting_2",
             L"/Game/Maps/Lighting/Map_Arena_Slums_Night_Lighting",
         }},
        {"Map_Arena_Cellar",
         {
             L"/Game/Maps/Lighting/Arena_Cellar_Map_Lighting",
             L"/Game/Maps/Lighting/Arena_Cellar_Map_Lighting",
             L"/Game/Maps/Lighting/Arena_Cellar_Map_Lighting",
             L"/Game/Maps/Lighting/Arena_Cellar_Map_Lighting",
         }},
        {"Map_Arena_LordsHall",
         {
             L"/Game/Maps/Lighting/Arena_LordsHall_Map_Lighting",
             L"/Game/Maps/Lighting/Arena_LordsHall_Map_Lighting",
             L"/Game/Maps/Lighting/Arena_LordsHall_Map_Lighting",
             L"/Game/Maps/Lighting/Arena_LordsHall_Map_Lighting",
         }},
        {"Workshop_Smithery_Map",
         {
             L"/Game/Maps/Lighting/Workshop_Smithery_Map_Lighting",
             L"/Game/Maps/Lighting/Workshop_Smithery_Map_Lighting",
             L"/Game/Maps/Lighting/Workshop_Smithery_Map_Lighting",
             L"/Game/Maps/Lighting/Workshop_Smithery_Map_Lighting",
         }},
    };

    constexpr const wchar_t* K_KNOWN_LIGHTING_LEVELS[] = {
        L"/Game/Maps/Lighting/Arena_Ambush_Map_Evening_Lighting",
        L"/Game/Maps/Lighting/Arena_Ambush_Map_Lighting",
        L"/Game/Maps/Lighting/Arena_Ambush_Map_Morning_Lighting",
        L"/Game/Maps/Lighting/Arena_Ambush_Map_Night_Lighting",
        L"/Game/Maps/Lighting/Arena_Cellar_Map_Lighting",
        L"/Game/Maps/Lighting/Arena_Cutting_Map_Lighting",
        L"/Game/Maps/Lighting/Arena_Cutting_Map_Lighting_Evening",
        L"/Game/Maps/Lighting/Arena_Cutting_Map_Lighting_Night",
        L"/Game/Maps/Lighting/Arena_LordsHall_Map_Lighting",
        L"/Game/Maps/Lighting/Lighting_Morning",
        L"/Game/Maps/Lighting/Lighting_Overcast_001",
        L"/Game/Maps/Lighting/Map_Arena_EastTower_Dawn_Lighting",
        L"/Game/Maps/Lighting/Map_Arena_EastTower_Lighting",
        L"/Game/Maps/Lighting/Map_Arena_EastTower_Night_Lighting",
        L"/Game/Maps/Lighting/Map_Arena_Slums_Evening_Lighting_2",
        L"/Game/Maps/Lighting/Map_Arena_Slums_Lighting",
        L"/Game/Maps/Lighting/Map_Arena_Slums_Morning_Lighting",
        L"/Game/Maps/Lighting/Map_Arena_Slums_Night_Lighting",
        L"/Game/Maps/Lighting/Workshop_Smithery_Map_Lighting",
    };

    [[nodiscard]] const wchar_t* LightingLevelForPreset(const std::string& currentLevel, int presetIndex) {
        for (const auto& presetSet : K_MAP_LIGHTING_PRESETS) {
            if (currentLevel.find(presetSet.mapToken) != std::string::npos) return presetSet.levels[presetIndex];
        }
        return K_DEFAULT_LIGHTING_PRESETS[presetIndex];
    }

    SDK::FLatentActionInfo LatentAction(int32_t uuid) {
        SDK::FLatentActionInfo info{};
        info.UUID = uuid;
        return info;
    }

    SDK::FName PackageName(const wchar_t* path) {
        return SDK::BasicFilesImplUtils::StringToName(path);
    }

    [[nodiscard]] std::string NarrowPath(const wchar_t* path) {
        std::string result;
        if (!path) return result;

        while (*path) {
            result.push_back(static_cast<char>(*path));
            ++path;
        }
        return result;
    }

    [[nodiscard]] bool IsKnownLightingLevel(const std::string& packageName) {
        for (const wchar_t* knownLevel : K_KNOWN_LIGHTING_LEVELS) {
            if (packageName == NarrowPath(knownLevel)) return true;
        }
        return false;
    }

    void SetStreamLevelTarget(SDK::ULevelStreaming* streamingLevel, bool shouldLoad) {
        if (!streamingLevel) return;

        streamingLevel->bShouldBlockOnLoad = true;
        streamingLevel->bShouldBlockOnUnload = true;
        if (shouldLoad) {
            streamingLevel->SetShouldBeLoaded(true);
            streamingLevel->SetShouldBeVisible(true);
            return;
        }

        streamingLevel->SetShouldBeVisible(false);
        streamingLevel->SetShouldBeLoaded(false);
    }

    void ApplyLightingPresetLevel(SDK::UWorld* world, const wchar_t* levelPath) {
        if (!world || !levelPath) return;

        const std::string targetLevel = NarrowPath(levelPath);
        bool foundExistingTarget = false;
        for (int32_t i = 0; i < world->StreamingLevels.Num(); ++i) {
            auto* streamingLevel = world->StreamingLevels[i];
            if (!streamingLevel) continue;

            const std::string packageName = streamingLevel->GetWorldAssetPackageFName().GetRawString();
            if (!IsKnownLightingLevel(packageName)) continue;

            const bool shouldLoad = packageName == targetLevel;
            foundExistingTarget = foundExistingTarget || shouldLoad;
            SetStreamLevelTarget(streamingLevel, shouldLoad);
        }

        if (foundExistingTarget) {
            SDK::UGameplayStatics::FlushLevelStreaming(world);
            return;
        }

        int32_t uuid = 1000;
        for (const wchar_t* loadedLevel : K_KNOWN_LIGHTING_LEVELS) {
            SDK::UGameplayStatics::UnloadStreamLevel(world, PackageName(loadedLevel), LatentAction(uuid++), true);
        }

        SDK::UGameplayStatics::FlushLevelStreaming(world);
        SDK::UGameplayStatics::LoadStreamLevel(world, PackageName(levelPath), true, true, LatentAction(uuid));
        SDK::UGameplayStatics::FlushLevelStreaming(world);
    }

    [[nodiscard]] bool IsLiveObject(const SDK::UObject* object) {
        return object && object->Index >= 0 && SDK::UObject::GObjects->GetByIndex(object->Index) == object;
    }

    [[nodiscard]] bool IsLiveActor(SDK::AActor* actor) {
        return IsLiveObject(actor) && !actor->IsActorBeingDestroyed();
    }

    [[nodiscard]] SDK::AActor* ComponentOwner(SDK::UActorComponent* component) {
        auto* owner = component ? component->GetOwner() : nullptr;
        return IsLiveActor(owner) ? owner : nullptr;
    }

    [[nodiscard]] bool IsVisibleCandidate(SDK::AActor* actor, SDK::USceneComponent* component) {
        return IsLiveActor(actor) && IsLiveObject(component) && !actor->bHidden && component->bVisible &&
               !component->bHiddenInGame;
    }

    [[nodiscard]] bool ShouldUseComponent(
        SDK::USceneComponent* current, SDK::AActor* candidateActor, SDK::USceneComponent* candidate
    ) {
        if (!candidate) return false;
        if (!current || !IsLiveObject(current)) return true;

        return !IsVisibleCandidate(ComponentOwner(current), current) && IsVisibleCandidate(candidateActor, candidate);
    }

    [[nodiscard]] SDK::UActorComponent* FirstComponentOfClass(SDK::AActor* actor, SDK::UClass* componentClass) {
        if (!IsLiveActor(actor)) return nullptr;

        SDK::TArray<SDK::UActorComponent*> components = actor->K2_GetComponentsByClass(componentClass);
        for (auto* component : components) {
            if (IsLiveObject(component) && component->IsA(componentClass)) return component;
        }
        return nullptr;
    }

}

SkyEditorSection::SkyEditorSection(ModContext& ctx) : Section(ctx, SECTION) {}

void SkyEditorSection::ResetState() {
    sunComp = nullptr;
    atmoComp = nullptr;
    skyLightComp = nullptr;
    fogComp = nullptr;
    cloudComp = nullptr;
    cachedWorld = nullptr;
    sunTargets.clear();
    searchPending = false;
    componentsReady.store(false, std::memory_order_release);
    sunOverrideActive = false;
    sunOverrideQueued.store(false, std::memory_order_release);
}

void SkyEditorSection::ReadInitialValues() {
    if (sunComp) {
        auto* base = static_cast<SDK::ULightComponentBase*>(sunComp);
        auto* lightComp = static_cast<SDK::ULightComponent*>(sunComp);
        auto rot = static_cast<SDK::USceneComponent*>(sunComp)->RelativeRotation;
        sunPitch = static_cast<float>(rot.Pitch);
        sunYaw = static_cast<float>(rot.Yaw);
        sunIntensity = base->Intensity;
        auto lc = base->LightColor;
        sunColor[0] = static_cast<float>(lc.R) / 255.0f;
        sunColor[1] = static_cast<float>(lc.G) / 255.0f;
        sunColor[2] = static_cast<float>(lc.B) / 255.0f;
        sunUseTemperature = lightComp->bUseTemperature;
        sunTemperature = lightComp->Temperature;
        sunSourceAngle = sunComp->LightSourceAngle;
        sunSoftAngle = sunComp->LightSourceSoftAngle;
        sunBloomScale = lightComp->BloomScale;
        sunBloomThreshold = lightComp->BloomThreshold;
        sunShadowAmount = sunComp->ShadowAmount;
        sunVolumetricScatter = base->VolumetricScatteringIntensity;
        sunIndirectIntensity = base->IndirectLightingIntensity;
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
        auto* base = static_cast<SDK::ULightComponentBase*>(skyLightComp);
        skyLightIntensity = base->Intensity;
        auto lc = base->LightColor;
        skyLightColor[0] = static_cast<float>(lc.R) / 255.0f;
        skyLightColor[1] = static_cast<float>(lc.G) / 255.0f;
        skyLightColor[2] = static_cast<float>(lc.B) / 255.0f;
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
    auto* world = RenderWorld();
    if (!world) {
        ResetState();
        return;
    }

    if (searchPending && world == cachedWorld) return;

    ResetState();
    searchPending = true;
    cachedWorld = world;

    const bool queued = GameHook::QueueAction([this, world](const RuntimeContextSnapshot&) {
        if (world != cachedWorld) return;

        sunTargets.clear();

        SDK::TArray<SDK::AActor*> actors;
        SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AActor::StaticClass(), &actors);

        for (auto* actor : actors) {
            if (!IsLiveActor(actor)) continue;

            if (auto* component = static_cast<SDK::UDirectionalLightComponent*>(
                    FirstComponentOfClass(actor, SDK::UDirectionalLightComponent::StaticClass())
                )) {
                sunTargets.push_back(component);
                if (ShouldUseComponent(sunComp, actor, component)) sunComp = component;
            }
            if (auto* component = static_cast<SDK::USkyAtmosphereComponent*>(
                    FirstComponentOfClass(actor, SDK::USkyAtmosphereComponent::StaticClass())
                )) {
                if (ShouldUseComponent(atmoComp, actor, component)) atmoComp = component;
            }
            if (auto* component = static_cast<SDK::USkyLightComponent*>(
                    FirstComponentOfClass(actor, SDK::USkyLightComponent::StaticClass())
                )) {
                if (ShouldUseComponent(skyLightComp, actor, component)) skyLightComp = component;
            }
            if (auto* component = static_cast<SDK::UExponentialHeightFogComponent*>(
                    FirstComponentOfClass(actor, SDK::UExponentialHeightFogComponent::StaticClass())
                )) {
                if (ShouldUseComponent(fogComp, actor, component)) fogComp = component;
            }
            if (auto* component = static_cast<SDK::UVolumetricCloudComponent*>(
                    FirstComponentOfClass(actor, SDK::UVolumetricCloudComponent::StaticClass())
                )) {
                if (ShouldUseComponent(cloudComp, actor, component)) cloudComp = component;
            }
        }

        componentsReady.store(true, std::memory_order_release);
    });
    if (!queued) searchPending = false;
}

void SkyEditorSection::OnOpen() {
    FindComponents();
}

void SkyEditorSection::QueueApplySunState() {
    sunOverrideActive = true;
    if (sunOverrideQueued.exchange(true, std::memory_order_acq_rel)) return;

    auto* queued = &sunOverrideQueued;
    auto targets = sunTargets;
    float p = sunPitch, y = sunYaw, intensity = sunIntensity, temperature = sunTemperature;
    bool useTemperature = sunUseTemperature;
    SDK::FLinearColor color{sunColor[0], sunColor[1], sunColor[2], 1.f};
    float sa = sunSourceAngle, soft = sunSoftAngle, bs = sunBloomScale;
    float bt = sunBloomThreshold, sha = sunShadowAmount;
    float vs = sunVolumetricScatter, ii = sunIndirectIntensity;

    GameHook::QueueAction([targets = std::move(targets), p, y, intensity, color, temperature, useTemperature, sa, soft,
                           bs, bt, sha, vs, ii, queued](const RuntimeContextSnapshot&) {
        for (auto* targetComp : targets) {
            if (!IsLiveObject(targetComp)) continue;

            auto* lightBase = static_cast<SDK::ULightComponentBase*>(targetComp);
            lightBase->bAffectsWorld = true;
            lightBase->SetAffectGlobalIllumination(true);
            lightBase->SetAffectReflection(true);
            lightBase->SetCastShadows(true);
            static_cast<SDK::USceneComponent*>(targetComp)
                ->K2_SetWorldRotation(SDK::FRotator{p, y, 0.0}, false, nullptr, false);

            auto* light = static_cast<SDK::ULightComponent*>(targetComp);
            light->SetIntensity(intensity);
            light->SetLightColor(color, true);
            auto* targetActor = ComponentOwner(targetComp);
            if (IsLiveActor(targetActor) && targetActor->IsA(SDK::ALight::StaticClass())) {
                static_cast<SDK::ALight*>(targetActor)->SetLightColor(color);
            }
            light->SetUseTemperature(useTemperature);
            if (useTemperature) light->SetTemperature(temperature);

            targetComp->SetLightSourceAngle(sa);
            targetComp->SetLightSourceSoftAngle(soft);
            targetComp->SetShadowAmount(sha);
            light->SetBloomScale(bs);
            light->SetBloomThreshold(bt);
            light->SetVolumetricScatteringIntensity(vs);
            light->SetIndirectLightingIntensity(ii);
        }
        queued->store(false, std::memory_order_release);
    });
}

void SkyEditorSection::ApplyPreset(int presetIndex) {
    const auto preset = K_TIME_PRESETS[presetIndex];
    sunPitch = preset.sunPitch;
    sunYaw = preset.sunYaw;

    GameHook::QueueAction([presetIndex, dayTime = preset.dayTime](const RuntimeContextSnapshot& runtime) {
        if (!runtime.world) return;
        auto* gameInstance = SDK::UGameplayStatics::GetGameInstance(runtime.world);
        if (gameInstance && gameInstance->IsA(SDK::UGI_Settings_C::StaticClass())) {
            static_cast<SDK::UGI_Settings_C*>(gameInstance)->Day_Time = dayTime;
        }

        const auto currentLevel = SDK::UGameplayStatics::GetCurrentLevelName(runtime.world, true).ToString();
        ApplyLightingPresetLevel(runtime.world, LightingLevelForPreset(currentLevel, presetIndex));
    });

    FindComponents();
}

void SkyEditorSection::RenderSunTab() {
    if (!sunComp) {
        ImGui::TextDisabled("Sun controls are unavailable in this map.");
        return;
    }
    if (ImGui::DragFloat("Brightness", &sunIntensity, 0.1f, 0.0f, 0.0f, "%.1f")) QueueApplySunState();
    float col[3] = {sunColor[0], sunColor[1], sunColor[2]};
    GuiUtils::SetNextColorFieldWidth("Color");
    if (ImGui::ColorEdit3("Color", col)) {
        sunColor[0] = col[0];
        sunColor[1] = col[1];
        sunColor[2] = col[2];
        sunUseTemperature = false;
        QueueApplySunState();
    }
    if (ImGui::DragFloat("Color Temperature", &sunTemperature, 50.f, 1000.f, 15000.f, "%.0f K")) {
        sunUseTemperature = true;
        QueueApplySunState();
    }
    ImGui::Separator();
    bool extChanged = false;
    extChanged |= ImGui::DragFloat("Sun Size", &sunSourceAngle, 0.05f, 0.0f, 20.0f, "%.2f");
    extChanged |= ImGui::DragFloat("Shadow Softness", &sunSoftAngle, 0.05f, 0.0f, 20.0f, "%.2f");
    extChanged |= ImGui::DragFloat("Glow", &sunBloomScale, 0.01f, 0.0f, 0.0f, "%.2f");
    extChanged |= ImGui::DragFloat("Glow Sensitivity", &sunBloomThreshold, 0.1f, 0.0f, 0.0f, "%.1f");
    extChanged |= ImGui::DragFloat("Shadow Strength", &sunShadowAmount, 0.01f, 0.0f, 1.0f, "%.2f");
    extChanged |= ImGui::DragFloat("Atmospheric Light", &sunVolumetricScatter, 0.01f, 0.0f, 0.0f, "%.2f");
    extChanged |= ImGui::DragFloat("Indirect Light", &sunIndirectIntensity, 0.01f, 0.0f, 0.0f, "%.2f");
    if (extChanged) QueueApplySunState();
}

void SkyEditorSection::RenderAtmoTab() {
    if (!atmoComp) {
        ImGui::TextDisabled("Atmosphere controls are unavailable in this map.");
        return;
    }
    bool changed = false;
    changed |= GuiUtils::DebouncedDragFloat("Sky Color Strength", &rayleighScale, 0.01f, 0.0f, 0.0f, "%.3f");
    float rc[3] = {rayleighColor[0], rayleighColor[1], rayleighColor[2]};
    GuiUtils::SetNextColorFieldWidth("Sky Color");
    if (ImGui::ColorEdit3("Sky Color", rc)) {
        rayleighColor[0] = rc[0];
        rayleighColor[1] = rc[1];
        rayleighColor[2] = rc[2];
        changed = true;
    }
    changed |= GuiUtils::DebouncedDragFloat("Haze Strength", &mieScale, 0.01f, 0.0f, 0.0f, "%.3f");
    changed |= GuiUtils::DebouncedDragFloat("Haze Focus", &mieAnisotropy, 0.005f, 0.0f, 1.0f, "%.3f");
    changed |= GuiUtils::DebouncedDragFloat("Light Scattering", &multiScatter, 0.01f, 0.0f, 0.0f, "%.3f");
    changed |= GuiUtils::DebouncedDragFloat("Atmosphere Height", &atmoHeight, 0.5f, 0.0f, 0.0f, "%.1f km");
    float sl[4] = {skyLuminance[0], skyLuminance[1], skyLuminance[2], skyLuminance[3]};
    GuiUtils::SetNextColorFieldWidth("Sky Tint");
    if (ImGui::ColorEdit4("Sky Tint", sl)) {
        skyLuminance[0] = sl[0];
        skyLuminance[1] = sl[1];
        skyLuminance[2] = sl[2];
        skyLuminance[3] = sl[3];
        changed = true;
    }
    if (changed) {
        auto* comp = atmoComp;
        float rs = rayleighScale, ms = mieScale, ma = mieAnisotropy, msc = multiScatter, ah = atmoHeight;
        SDK::FLinearColor rayleigh{rayleighColor[0], rayleighColor[1], rayleighColor[2], 1.f};
        SDK::FLinearColor luminance{skyLuminance[0], skyLuminance[1], skyLuminance[2], skyLuminance[3]};
        GameHook::QueueAction([comp, rs, rayleigh, ms, ma, msc, luminance, ah](const RuntimeContextSnapshot&) {
            comp->SetRayleighScatteringScale(rs);
            comp->SetRayleighScattering(rayleigh);
            comp->SetMieScatteringScale(ms);
            comp->SetMieAnisotropy(ma);
            comp->SetMultiScatteringFactor(msc);
            comp->SetSkyLuminanceFactor(luminance);
            comp->SetAtmosphereHeight(ah);
        });
    }
}

void SkyEditorSection::RenderSkyLightTab() {
    if (!skyLightComp) {
        ImGui::TextDisabled("Ambient light controls are unavailable in this map.");
        return;
    }
    bool changed = false;
    changed |= GuiUtils::DebouncedDragFloat("Brightness", &skyLightIntensity, 0.01f, 0.0f, 0.0f, "%.3f");
    float col[3] = {skyLightColor[0], skyLightColor[1], skyLightColor[2]};
    GuiUtils::SetNextColorFieldWidth("Color");
    if (ImGui::ColorEdit3("Color", col)) {
        skyLightColor[0] = col[0];
        skyLightColor[1] = col[1];
        skyLightColor[2] = col[2];
        changed = true;
    }
    float lh[4] = {lowerHemiColor[0], lowerHemiColor[1], lowerHemiColor[2], lowerHemiColor[3]};
    GuiUtils::SetNextColorFieldWidth("Ground Light");
    if (ImGui::ColorEdit4("Ground Light", lh)) {
        lowerHemiColor[0] = lh[0];
        lowerHemiColor[1] = lh[1];
        lowerHemiColor[2] = lh[2];
        lowerHemiColor[3] = lh[3];
        changed = true;
    }
    if (changed) {
        auto* comp = skyLightComp;
        float intensity = skyLightIntensity;
        SDK::FLinearColor color{skyLightColor[0], skyLightColor[1], skyLightColor[2], 1.f};
        SDK::FLinearColor lowerHemi{lowerHemiColor[0], lowerHemiColor[1], lowerHemiColor[2], lowerHemiColor[3]};
        GameHook::QueueAction([comp, intensity, color, lowerHemi](const RuntimeContextSnapshot&) {
            comp->SetIntensity(intensity);
            comp->SetLightColor(color);
            comp->SetLowerHemisphereColor(lowerHemi);
        });
    }
}

void SkyEditorSection::RenderFogTab() {
    if (!fogComp) {
        ImGui::TextDisabled("Fog controls are unavailable in this map.");
        return;
    }
    bool changed = false;
    changed |= GuiUtils::DebouncedDragFloat("Density", &fogDensity, 0.001f, 0.0f, 0.0f, "%.4f");
    changed |= GuiUtils::DebouncedDragFloat("Vertical Fade", &fogFalloff, 0.01f, 0.0f, 0.0f, "%.3f");
    changed |= GuiUtils::DebouncedDragFloat("Start Distance", &fogStartDist, 10.f, 0.0f, 0.0f, "%.0f");
    changed |= GuiUtils::DebouncedDragFloat("Maximum Thickness", &fogMaxOpacity, 0.01f, 0.0f, 1.0f, "%.2f");
    float col[3] = {fogColor[0], fogColor[1], fogColor[2]};
    GuiUtils::SetNextColorFieldWidth("Fog Color");
    if (ImGui::ColorEdit3("Fog Color", col)) {
        fogColor[0] = col[0];
        fogColor[1] = col[1];
        fogColor[2] = col[2];
        changed = true;
    }
    if (changed) {
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
}

void SkyEditorSection::RenderCloudsTab() {
    if (!cloudComp) {
        ImGui::TextDisabled("Cloud controls are unavailable in this map.");
        return;
    }
    bool changed = false;
    changed |= GuiUtils::DebouncedDragFloat("Base Altitude", &cloudBottomAlt, 0.1f, 0.0f, 50.0f, "%.1f km");
    changed |= GuiUtils::DebouncedDragFloat("Layer Height", &cloudHeight, 0.1f, 0.1f, 100.0f, "%.1f km");
    changed |= GuiUtils::DebouncedDragFloat("Visual Quality", &cloudViewSamples, 0.05f, 0.1f, 4.0f, "%.2f");
    changed |= GuiUtils::DebouncedDragFloat("Shadow Quality", &cloudShadowSamples, 0.05f, 0.1f, 4.0f, "%.2f");
    changed |= GuiUtils::DebouncedDragFloat("Shadow Range", &cloudShadowDist, 1.0f, 1.0f, 200.0f, "%.0f km");
    if (changed) {
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
}

bool SkyEditorSection::UpdateComponentScan() {
    auto* world = RenderWorld();
    if (world != cachedWorld) FindComponents();

    if (componentsReady.exchange(false, std::memory_order_acquire)) {
        ReadInitialValues();
        searchPending = false;
    }

    return !searchPending && (sunComp || atmoComp || skyLightComp || fogComp || cloudComp);
}

void SkyEditorSection::Render() {
    ImGui::PushID("SkyEdit");

    if (!UpdateComponentScan()) {
        ImGui::PopID();
        return;
    }

    ImGui::PushItemWidth(GuiUtils::K_DRAG_WIDTH);

    if (sunComp) {
        if (ImGui::DragFloat("Sun Height", &sunPitch, 0.2f, -90.f, 90.f, "%.1f")) QueueApplySunState();
        if (ImGui::DragFloat("Sun Direction", &sunYaw, 0.2f, -180.f, 180.f, "%.1f")) QueueApplySunState();
    }
    ImGui::PopItemWidth();

    ImGui::Spacing();
    for (int i = 0; i < K_TIME_PRESET_COUNT; ++i) {
        if (i > 0) (void)GuiUtils::SameLineIfFitsButton(K_TIME_PRESETS[i].label);
        if (GuiUtils::Button(K_TIME_PRESETS[i].label)) ApplyPreset(i);
    }

    ImGui::Spacing();
    GuiUtils::RenderUnderlineTabs("##SkyTabs", activeTab, TAB_LABELS, TAB_COUNT);

    ImGui::BeginChild("##SkyParams", ImVec2(0, 0), ImGuiChildFlags_None);
    ImGui::PushItemWidth(GuiUtils::K_DRAG_WIDTH);
    switch (activeTab) {
        case 0: RenderSunTab(); break;
        case 1: RenderAtmoTab(); break;
        case 2: RenderSkyLightTab(); break;
        case 3: RenderFogTab(); break;
        case 4: RenderCloudsTab(); break;
        default: break;
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();

    if (sunOverrideActive && sunComp) QueueApplySunState();
    ImGui::PopID();
}
