#include "Utils/EngineArray.h"

#include "Menu/Sections/World/SkyEditorSection.h"
#include "Hooks/GameHook.h"
#include "Utils/GuiUtils.h"
#include "SDK/Enum_DayTime_structs.hpp"
#include "SDK/GI_Settings_classes.hpp"
#include "SDK/Ultra_Dynamic_Sky_classes.hpp"

#include <algorithm>
#include <string>

namespace {
    struct SkyTimePreset {
        const char* label;
        SDK::Enum_DayTime dayTime;
    };

    constexpr SkyTimePreset K_TIME_PRESETS[] = {
        {"Morning", SDK::Enum_DayTime::NewEnumerator0},
        {"Day", SDK::Enum_DayTime::NewEnumerator1},
        {"Evening", SDK::Enum_DayTime::NewEnumerator2},
        {"Night", SDK::Enum_DayTime::NewEnumerator3},
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

    [[nodiscard]] bool IsEditableComponent(SDK::USceneComponent* component, int32_t index, SDK::UWorld* world) {
        if (!component || index < 0 || SDK::UObject::GObjects->GetByIndex(index) != component) return false;
        auto* actor = component->GetOwner();
        if (!IsLiveObject(actor) || actor->IsActorBeingDestroyed() || actor->bHidden) return false;
        auto* level = actor->GetLevel();
        return level && level->OwningWorld == world && level->bIsVisible && component->bVisible &&
               !component->bHiddenInGame;
    }

    bool CallSkyFunction(SDK::AUltra_Dynamic_Sky_C* sky, const char* name, void* parameters = nullptr) {
        auto* function = sky->Class->GetFunction("Ultra_Dynamic_Sky_C", name);
        if (!function) return false;
        sky->ProcessEvent(function, parameters);
        return true;
    }
}

SkyEditorSection::SkyEditorSection(ModContext& ctx) : Section(ctx, SECTION) {}

void SkyEditorSection::ReadInitialValues(State& state) {
    if (state.lightComp) {
        auto* base = static_cast<SDK::ULightComponentBase*>(state.lightComp);
        auto* lightComp = static_cast<SDK::ULightComponent*>(state.lightComp);
        auto rot = static_cast<SDK::USceneComponent*>(state.lightComp)->K2_GetComponentRotation();
        state.lightPitch = static_cast<float>(rot.Pitch);
        state.lightYaw = static_cast<float>(rot.Yaw);
        state.lightIntensity = base->Intensity;
        auto lc = SDK::UKismetMathLibrary::Conv_ColorToLinearColor(base->LightColor);
        state.lightColor[0] = lc.R;
        state.lightColor[1] = lc.G;
        state.lightColor[2] = lc.B;
        state.lightUseTemperature = lightComp->bUseTemperature;
        state.lightTemperature = lightComp->Temperature;
        state.lightSize = state.lightComp->LightSourceAngle;
        state.lightSoftAngle = state.lightComp->LightSourceSoftAngle;
        state.lightBloomScale = lightComp->BloomScale;
        state.lightBloomThreshold = lightComp->BloomThreshold;
        state.lightShadowAmount = state.lightComp->ShadowAmount;
        state.lightVolumetricScatter = base->VolumetricScatteringIntensity;
        state.lightIndirectIntensity = base->IndirectLightingIntensity;
    }
    if (auto* sky = state.skyActor) {
        state.lightIntensity = static_cast<float>(state.moon ? sky->Moon_Light_Intensity : sky->Sun_Light_Intensity);
        const auto color = state.moon ? sky->Moon_Light_Color : sky->Sun_Light_Color;
        state.lightColor[0] = color.R;
        state.lightColor[1] = color.G;
        state.lightColor[2] = color.B;
        state.lightSize = static_cast<float>(state.moon ? sky->Moon_Scale : sky->Sun_Radius);
    }
    if (state.atmoComp) {
        state.rayleighScale = state.atmoComp->RayleighScatteringScale;
        auto& rs = state.atmoComp->RayleighScattering;
        state.rayleighColor[0] = rs.R;
        state.rayleighColor[1] = rs.G;
        state.rayleighColor[2] = rs.B;
        state.mieScale = state.atmoComp->MieScatteringScale;
        state.mieAnisotropy = state.atmoComp->MieAnisotropy;
        state.multiScatter = state.atmoComp->MultiScatteringFactor;
        auto& sl = state.atmoComp->SkyLuminanceFactor;
        state.skyLuminance[0] = sl.R;
        state.skyLuminance[1] = sl.G;
        state.skyLuminance[2] = sl.B;
        state.skyLuminance[3] = sl.A;
        state.atmoHeight = state.atmoComp->AtmosphereHeight;
    }
    if (state.skyLightComp) {
        auto* base = static_cast<SDK::ULightComponentBase*>(state.skyLightComp);
        state.skyLightIntensity = base->Intensity;
        auto lc = SDK::UKismetMathLibrary::Conv_ColorToLinearColor(base->LightColor);
        state.skyLightColor[0] = lc.R;
        state.skyLightColor[1] = lc.G;
        state.skyLightColor[2] = lc.B;
        auto& lh = state.skyLightComp->LowerHemisphereColor;
        state.lowerHemiColor[0] = lh.R;
        state.lowerHemiColor[1] = lh.G;
        state.lowerHemiColor[2] = lh.B;
        state.lowerHemiColor[3] = lh.A;
    }
    if (state.fogComp) {
        state.fogDensity = state.fogComp->FogDensity;
        state.fogFalloff = state.fogComp->FogHeightFalloff;
        state.fogMaxOpacity = state.fogComp->FogMaxOpacity;
        state.fogStartDist = state.fogComp->StartDistance;
        auto& fc = state.fogComp->FogInscatteringColor;
        state.fogColor[0] = fc.R;
        state.fogColor[1] = fc.G;
        state.fogColor[2] = fc.B;
    }
    if (state.cloudComp) {
        state.cloudBottomAlt = state.cloudComp->LayerBottomAltitude;
        state.cloudHeight = state.cloudComp->LayerHeight;
        state.cloudViewSamples = state.cloudComp->ViewSampleCountScale;
        state.cloudShadowSamples = state.cloudComp->ShadowViewSampleCountScale;
        state.cloudShadowDist = state.cloudComp->ShadowTracingDistance;
    }
}


SkyEditorSection::State SkyEditorSection::ScanComponents(SDK::UWorld* world) {
    State result;
    result.world = world;
    EngineArray<SDK::AActor*> actors;
    SDK::UGameplayStatics::GetAllActorsOfClass(world, SDK::AActor::StaticClass(), &actors);
    for (auto* actor : actors) {
        if (!IsLiveObject(actor) || actor->IsActorBeingDestroyed() || actor->bHidden) continue;
        auto* level = actor->GetLevel();
        if (!level || level->OwningWorld != world || !level->bIsVisible) continue;
        const bool dynamicSky = actor->IsA(SDK::AUltra_Dynamic_Sky_C::StaticClass());
        if (dynamicSky) {
            auto* sky = static_cast<SDK::AUltra_Dynamic_Sky_C*>(actor);
            double night = 0.0;
            CallSkyFunction(sky, "Night Filter", &night);
            const bool moon = night >= 0.5;
            auto* light = moon ? sky->Moon_LightComponent : sky->Sun_LightComponent;
            if (light && IsEditableComponent(light, light->Index, world) && light->bAffectsWorld) {
                result.lightComp = light;
                result.skyActor = sky;
                result.moon = moon;
            }
        }
        EngineArray<SDK::UActorComponent*> components{
            actor->K2_GetComponentsByClass(SDK::USceneComponent::StaticClass())
        };
        for (auto* base : components) {
            auto* component = static_cast<SDK::USceneComponent*>(base);
            if (!component || !IsEditableComponent(component, component->Index, world)) continue;
            if (component->IsA(SDK::ULightComponentBase::StaticClass()) &&
                !static_cast<SDK::ULightComponentBase*>(component)->bAffectsWorld)
                continue;
            if (!dynamicSky && !result.lightComp && component->IsA(SDK::UDirectionalLightComponent::StaticClass())) {
                auto* light = static_cast<SDK::UDirectionalLightComponent*>(component);
                if (!light->bAtmosphereSunLight) continue;
                result.lightComp = light;
                // The game's standalone night lighting also uses atmosphere light index zero.
                const auto levelName = actor->GetLevel()->GetFullName();
                result.moon = light->AtmosphereSunLightIndex == 1 || levelName.find("Night") != std::string::npos;
            } else if (!result.atmoComp && component->IsA(SDK::USkyAtmosphereComponent::StaticClass())) {
                result.atmoComp = static_cast<SDK::USkyAtmosphereComponent*>(component);
            } else if (!result.skyLightComp && component->IsA(SDK::USkyLightComponent::StaticClass())) {
                result.skyLightComp = static_cast<SDK::USkyLightComponent*>(component);
            } else if (!result.fogComp && component->IsA(SDK::UExponentialHeightFogComponent::StaticClass())) {
                result.fogComp = static_cast<SDK::UExponentialHeightFogComponent*>(component);
            } else if (!result.cloudComp && component->IsA(SDK::UVolumetricCloudComponent::StaticClass())) {
                result.cloudComp = static_cast<SDK::UVolumetricCloudComponent*>(component);
            }
        }
    }
    const auto targets = result.Targets();
    for (std::size_t i = 0; i < targets.size(); ++i)
        if (targets[i]) result.indices[i] = targets[i]->Index;
    ReadInitialValues(result);
    return result;
}

void SkyEditorSection::FindComponents() {
    generation.fetch_add(1, std::memory_order_acq_rel);
    state = {};
    state.world = RenderWorld();
    pendingLightEdits = 0;
    nextScan = {};
    UpdateComponentScan();
}

void SkyEditorSection::OnOpen() {
    FindComponents();
}

void SkyEditorSection::QueueApplyLightState(unsigned fields) {
    pendingLightEdits |= fields;
    if (!pendingLightEdits || !state.lightComp || lightEditQueued.exchange(true, std::memory_order_acq_rel)) return;
    const auto edits = pendingLightEdits;
    const auto values = state;
    const auto revision = generation.load(std::memory_order_acquire);
    const bool queued = GameHook::QueueAction([this, values, edits, revision](const RuntimeContextSnapshot& runtime) {
        auto* light = values.lightComp;
        if (revision == generation.load(std::memory_order_acquire) && runtime.world == values.world &&
            IsEditableComponent(light, values.indices[0], runtime.world)) {
            auto* sky = values.skyActor;
            if (sky && SDK::UObject::GObjects->GetByIndex(values.indices[5]) != sky) sky = nullptr;
            const SDK::FRotator rotation{values.lightPitch, values.lightYaw, 0.0};
            const SDK::FLinearColor color{values.lightColor[0], values.lightColor[1], values.lightColor[2], 1.f};
            if (sky) {
                if (edits & POSITION) {
                    const auto direction = SDK::UKismetMathLibrary::GetForwardVector(rotation);
                    const SDK::FVector target{-direction.X, -direction.Y, -direction.Z};
                    if (values.moon) {
                        sky->Manually_Position_Moon_Target = true;
                        sky->Moon_Target = target;
                    } else {
                        sky->Manually_Position_Sun_Target = true;
                        sky->Sun_Target = target;
                    }
                }
                if (edits & LIGHTING) {
                    if (values.moon) {
                        sky->Moon_Light_Intensity = values.lightIntensity;
                        sky->Moon_Light_Color = color;
                    } else {
                        sky->Sun_Light_Intensity = values.lightIntensity;
                        sky->Sun_Light_Color = color;
                    }
                }
                if (edits & SIZE) {
                    if (values.moon)
                        sky->Moon_Scale = values.lightSize;
                    else
                        sky->Sun_Radius = values.lightSize;
                }
                if (edits & (POSITION | LIGHTING | SIZE)) {
                    CallSkyFunction(sky, "Hard Reset Cache");
                    CallSkyFunction(sky, "Update Active Variables");
                }
            } else {
                if (edits & POSITION) light->K2_SetWorldRotation(rotation, false, nullptr, false);
                if (edits & LIGHTING) {
                    light->SetIntensity(values.lightIntensity);
                    light->SetLightColor(color, true);
                }
                if (edits & SIZE) light->SetLightSourceAngle(values.lightSize);
            }
            if (edits & LIGHTING) {
                light->SetUseTemperature(values.lightUseTemperature);
                if (values.lightUseTemperature) light->SetTemperature(values.lightTemperature);
            }
            if (edits & EFFECTS) {
                light->SetLightSourceSoftAngle(values.lightSoftAngle);
                light->SetShadowAmount(values.lightShadowAmount);
                light->SetBloomScale(values.lightBloomScale);
                light->SetBloomThreshold(values.lightBloomThreshold);
                light->SetVolumetricScatteringIntensity(values.lightVolumetricScatter);
                light->SetIndirectLightingIntensity(values.lightIndirectIntensity);
            }
        }
        lightEditQueued.store(false, std::memory_order_release);
    });
    if (queued)
        pendingLightEdits = 0;
    else
        lightEditQueued.store(false, std::memory_order_release);
}

void SkyEditorSection::ApplyPreset(int presetIndex) {
    if (presetIndex < 0 || presetIndex >= K_TIME_PRESET_COUNT) return;
    GameHook::QueueAction([presetIndex, world = state.world](const RuntimeContextSnapshot& runtime) {
        if (!runtime.world || runtime.world != world) return;
        auto* gameInstance = SDK::UGameplayStatics::GetGameInstance(runtime.world);
        if (gameInstance && gameInstance->IsA(SDK::UGI_Settings_C::StaticClass()))
            static_cast<SDK::UGI_Settings_C*>(gameInstance)->Day_Time = K_TIME_PRESETS[presetIndex].dayTime;
        const auto currentLevel = SDK::UGameplayStatics::GetCurrentLevelName(runtime.world, true).ToString();
        ApplyLightingPresetLevel(runtime.world, LightingLevelForPreset(currentLevel, presetIndex));
    });
    FindComponents();
}

void SkyEditorSection::RenderLightTab() {
    const char* height = state.moon ? "Moon Height" : "Sun Height";
    const char* direction = state.moon ? "Moon Direction" : "Sun Direction";
    if (ImGui::DragFloat(height, &state.lightPitch, 0.2f, -90.f, 90.f, "%.1f")) QueueApplyLightState(POSITION);
    if (ImGui::DragFloat(direction, &state.lightYaw, 0.2f, -180.f, 180.f, "%.1f")) QueueApplyLightState(POSITION);
    ImGui::Separator();
    if (ImGui::DragFloat("Brightness", &state.lightIntensity, state.moon ? 0.01f : 0.1f, 0.f, 0.f, "%.3f"))
        QueueApplyLightState(LIGHTING);
    GuiUtils::SetNextColorFieldWidth("Color");
    if (ImGui::ColorEdit3("Color", state.lightColor)) {
        state.lightUseTemperature = false;
        QueueApplyLightState(LIGHTING);
    }
    if (ImGui::DragFloat("Color Temperature", &state.lightTemperature, 50.f, 1000.f, 15000.f, "%.0f K")) {
        state.lightUseTemperature = true;
        QueueApplyLightState(LIGHTING);
    }
    const char* size = state.moon ? "Moon Size" : "Sun Size";
    if (ImGui::DragFloat(size, &state.lightSize, 0.01f, 0.f, 20.f, "%.2f")) QueueApplyLightState(SIZE);
    ImGui::Separator();
    bool changed = false;
    changed |= ImGui::DragFloat("Shadow Softness", &state.lightSoftAngle, 0.05f, 0.f, 20.f, "%.2f");
    changed |= ImGui::DragFloat("Glow", &state.lightBloomScale, 0.01f, 0.f, 0.f, "%.2f");
    changed |= ImGui::DragFloat("Glow Sensitivity", &state.lightBloomThreshold, 0.1f, 0.f, 0.f, "%.1f");
    changed |= ImGui::DragFloat("Shadow Strength", &state.lightShadowAmount, 0.01f, 0.f, 1.f, "%.2f");
    changed |= ImGui::DragFloat("Atmospheric Light", &state.lightVolumetricScatter, 0.01f, 0.f, 0.f, "%.2f");
    changed |= ImGui::DragFloat("Indirect Light", &state.lightIndirectIntensity, 0.01f, 0.f, 0.f, "%.2f");
    if (changed) QueueApplyLightState(EFFECTS);
}

void SkyEditorSection::RenderAtmoTab() {
    if (!state.atmoComp) {
        ImGui::TextDisabled("Atmosphere controls are unavailable in this map.");
        return;
    }
    bool changed = false;
    changed |= GuiUtils::DebouncedDragFloat("Sky Color Strength", &state.rayleighScale, 0.01f, 0.0f, 0.0f, "%.3f");
    float rc[3] = {state.rayleighColor[0], state.rayleighColor[1], state.rayleighColor[2]};
    GuiUtils::SetNextColorFieldWidth("Sky Color");
    if (ImGui::ColorEdit3("Sky Color", rc)) {
        state.rayleighColor[0] = rc[0];
        state.rayleighColor[1] = rc[1];
        state.rayleighColor[2] = rc[2];
        changed = true;
    }
    changed |= GuiUtils::DebouncedDragFloat("Haze Strength", &state.mieScale, 0.01f, 0.0f, 0.0f, "%.3f");
    changed |= GuiUtils::DebouncedDragFloat("Haze Focus", &state.mieAnisotropy, 0.005f, 0.0f, 1.0f, "%.3f");
    changed |= GuiUtils::DebouncedDragFloat("Light Scattering", &state.multiScatter, 0.01f, 0.0f, 0.0f, "%.3f");
    changed |= GuiUtils::DebouncedDragFloat("Atmosphere Height", &state.atmoHeight, 0.5f, 0.0f, 0.0f, "%.1f km");
    float sl[4] = {state.skyLuminance[0], state.skyLuminance[1], state.skyLuminance[2], state.skyLuminance[3]};
    GuiUtils::SetNextColorFieldWidth("Sky Tint");
    if (ImGui::ColorEdit4("Sky Tint", sl)) {
        state.skyLuminance[0] = sl[0];
        state.skyLuminance[1] = sl[1];
        state.skyLuminance[2] = sl[2];
        state.skyLuminance[3] = sl[3];
        changed = true;
    }
    if (changed) {
        auto* comp = state.atmoComp;
        float rs = state.rayleighScale, ms = state.mieScale, ma = state.mieAnisotropy, msc = state.multiScatter,
              ah = state.atmoHeight;
        SDK::FLinearColor rayleigh{state.rayleighColor[0], state.rayleighColor[1], state.rayleighColor[2], 1.f};
        SDK::FLinearColor luminance{
            state.skyLuminance[0], state.skyLuminance[1], state.skyLuminance[2], state.skyLuminance[3]
        };
        GameHook::QueueAction([comp, index = state.indices[1], world = state.world, rs, rayleigh, ms, ma, msc,
                               luminance, ah](const RuntimeContextSnapshot& runtime) {
            if (runtime.world != world || !IsEditableComponent(comp, index, world)) return;
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
    if (!state.skyLightComp) {
        ImGui::TextDisabled("Ambient light controls are unavailable in this map.");
        return;
    }
    bool changed = false;
    changed |= GuiUtils::DebouncedDragFloat("Brightness", &state.skyLightIntensity, 0.01f, 0.0f, 0.0f, "%.3f");
    float col[3] = {state.skyLightColor[0], state.skyLightColor[1], state.skyLightColor[2]};
    GuiUtils::SetNextColorFieldWidth("Color");
    if (ImGui::ColorEdit3("Color", col)) {
        state.skyLightColor[0] = col[0];
        state.skyLightColor[1] = col[1];
        state.skyLightColor[2] = col[2];
        changed = true;
    }
    float lh[4] = {state.lowerHemiColor[0], state.lowerHemiColor[1], state.lowerHemiColor[2], state.lowerHemiColor[3]};
    GuiUtils::SetNextColorFieldWidth("Ground Light");
    if (ImGui::ColorEdit4("Ground Light", lh)) {
        state.lowerHemiColor[0] = lh[0];
        state.lowerHemiColor[1] = lh[1];
        state.lowerHemiColor[2] = lh[2];
        state.lowerHemiColor[3] = lh[3];
        changed = true;
    }
    if (changed) {
        auto* comp = state.skyLightComp;
        float intensity = state.skyLightIntensity;
        SDK::FLinearColor color{state.skyLightColor[0], state.skyLightColor[1], state.skyLightColor[2], 1.f};
        SDK::FLinearColor lowerHemi{
            state.lowerHemiColor[0], state.lowerHemiColor[1], state.lowerHemiColor[2], state.lowerHemiColor[3]
        };
        GameHook::QueueAction([comp, index = state.indices[2], world = state.world, intensity, color,
                               lowerHemi](const RuntimeContextSnapshot& runtime) {
            if (runtime.world != world || !IsEditableComponent(comp, index, world)) return;
            comp->SetIntensity(intensity);
            comp->SetLightColor(color);
            comp->SetLowerHemisphereColor(lowerHemi);
        });
    }
}

void SkyEditorSection::RenderFogTab() {
    if (!state.fogComp) {
        ImGui::TextDisabled("Fog controls are unavailable in this map.");
        return;
    }
    bool changed = false;
    changed |= GuiUtils::DebouncedDragFloat("Density", &state.fogDensity, 0.001f, 0.0f, 0.0f, "%.4f");
    changed |= GuiUtils::DebouncedDragFloat("Vertical Fade", &state.fogFalloff, 0.01f, 0.0f, 0.0f, "%.3f");
    changed |= GuiUtils::DebouncedDragFloat("Start Distance", &state.fogStartDist, 10.f, 0.0f, 0.0f, "%.0f");
    changed |= GuiUtils::DebouncedDragFloat("Maximum Thickness", &state.fogMaxOpacity, 0.01f, 0.0f, 1.0f, "%.2f");
    float col[3] = {state.fogColor[0], state.fogColor[1], state.fogColor[2]};
    GuiUtils::SetNextColorFieldWidth("Fog Color");
    if (ImGui::ColorEdit3("Fog Color", col)) {
        state.fogColor[0] = col[0];
        state.fogColor[1] = col[1];
        state.fogColor[2] = col[2];
        changed = true;
    }
    if (changed) {
        auto* comp = state.fogComp;
        float d = state.fogDensity, f = state.fogFalloff, s = state.fogStartDist, m = state.fogMaxOpacity;
        SDK::FLinearColor c{state.fogColor[0], state.fogColor[1], state.fogColor[2], 1.f};
        GameHook::QueueAction([comp, index = state.indices[3], world = state.world, d, f, c, s,
                               m](const RuntimeContextSnapshot& runtime) {
            if (runtime.world != world || !IsEditableComponent(comp, index, world)) return;
            comp->SetFogDensity(d);
            comp->SetFogHeightFalloff(f);
            comp->SetFogInscatteringColor(c);
            comp->SetStartDistance(s);
            comp->SetFogMaxOpacity(m);
        });
    }
}

void SkyEditorSection::RenderCloudsTab() {
    if (!state.cloudComp) {
        ImGui::TextDisabled("Cloud controls are unavailable in this map.");
        return;
    }
    bool changed = false;
    changed |= GuiUtils::DebouncedDragFloat("Base Altitude", &state.cloudBottomAlt, 0.1f, 0.0f, 50.0f, "%.1f km");
    changed |= GuiUtils::DebouncedDragFloat("Layer Height", &state.cloudHeight, 0.1f, 0.1f, 100.0f, "%.1f km");
    changed |= GuiUtils::DebouncedDragFloat("Visual Quality", &state.cloudViewSamples, 0.05f, 0.1f, 4.0f, "%.2f");
    changed |= GuiUtils::DebouncedDragFloat("Shadow Quality", &state.cloudShadowSamples, 0.05f, 0.1f, 4.0f, "%.2f");
    changed |= GuiUtils::DebouncedDragFloat("Shadow Range", &state.cloudShadowDist, 1.0f, 1.0f, 200.0f, "%.0f km");
    if (changed) {
        auto* comp = state.cloudComp;
        float ba = state.cloudBottomAlt, h = state.cloudHeight, vs = state.cloudViewSamples;
        float ss = state.cloudShadowSamples, sd = state.cloudShadowDist;
        GameHook::QueueAction([comp, index = state.indices[4], world = state.world, ba, h, vs, ss,
                               sd](const RuntimeContextSnapshot& runtime) {
            if (runtime.world != world || !IsEditableComponent(comp, index, world)) return;
            comp->SetLayerBottomAltitude(ba);
            comp->SetLayerHeight(h);
            comp->SetViewSampleCountScale(vs);
            comp->SetShadowViewSampleCountScale(ss);
            comp->SetShadowTracingDistance(sd);
        });
    }
}


void SkyEditorSection::UpdateComponentScan() {
    auto* world = RenderWorld();
    if (world != state.world) {
        generation.fetch_add(1, std::memory_order_acq_rel);
        state = {};
        state.world = world;
        pendingLightEdits = 0;
        nextScan = {};
    }
    {
        const std::scoped_lock lock(scanMutex);
        if (scanResult) {
            if (scanResult->generation == generation.load(std::memory_order_acquire) &&
                (scanResult->Targets() != state.Targets() || scanResult->indices != state.indices ||
                 scanResult->moon != state.moon)) {
                state = *scanResult;
                pendingLightEdits = 0;
                const auto targets = state.Targets();
                if (!targets[activeTab]) activeTab = 0;
                selectTab = true;
            }
            scanResult.reset();
        }
    }
    const auto now = std::chrono::steady_clock::now();
    if (!world || now < nextScan || scanPending.exchange(true, std::memory_order_acq_rel)) return;
    nextScan = now + std::chrono::seconds(1);
    const auto revision = generation.load(std::memory_order_acquire);
    if (!GameHook::QueueAction([this, world, revision](const RuntimeContextSnapshot& runtime) {
            if (runtime.world == world && revision == generation.load(std::memory_order_acquire)) {
                auto result = ScanComponents(world);
                result.generation = revision;
                const std::scoped_lock lock(scanMutex);
                scanResult = result;
            }
            scanPending.store(false, std::memory_order_release);
        }))
        scanPending.store(false, std::memory_order_release);
}

void SkyEditorSection::Render() {
    ImGui::PushID("SkyEdit");
    UpdateComponentScan();
    for (int i = 0; i < K_TIME_PRESET_COUNT; ++i) {
        if (i > 0) (void)GuiUtils::SameLineIfFitsButton(K_TIME_PRESETS[i].label);
        if (GuiUtils::Button(K_TIME_PRESETS[i].label)) ApplyPreset(i);
    }
    ImGui::Spacing();
    const std::array<const char*, 5> labels{
        state.moon ? "Moon" : "Sun", "Atmosphere", "Ambient Light", "Fog", "Clouds"
    };
    const auto targets = state.Targets();
    if (std::none_of(targets.begin(), targets.begin() + 5, [](auto* object) { return object != nullptr; })) {
        ImGui::TextDisabled(
            scanPending.load(std::memory_order_acquire) ? "Detecting sky controls..."
                                                        : "No editable sky components are active."
        );
    } else if (ImGui::BeginTabBar("##SkyTabs", ImGuiTabBarFlags_FittingPolicyResizeDown)) {
        for (int i = 0; i < 5; ++i) {
            const auto flags = selectTab && activeTab == i ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            if (!targets[i] || !ImGui::BeginTabItem(labels[i], nullptr, flags)) continue;
            activeTab = i;
            ImGui::PushItemWidth(GuiUtils::K_DRAG_WIDTH);
            switch (i) {
                case 0: RenderLightTab(); break;
                case 1: RenderAtmoTab(); break;
                case 2: RenderSkyLightTab(); break;
                case 3: RenderFogTab(); break;
                case 4: RenderCloudsTab(); break;
                default: break;
            }
            ImGui::PopItemWidth();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        selectTab = false;
    }
    QueueApplyLightState();
    ImGui::PopID();
}
