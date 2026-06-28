#include "Utils/AssetOverrideManager.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <system_error>
#include <utility>

#include "ConfigManager.h"
#include "Hooks/GameHook.h"
#include "Logger.h"
#include "Utils/PresetUtils.h"
#include "SDK/Basic.hpp"
#include "SDK/Blood_BP_P4_classes.hpp"
#include "SDK/Blood_BP_P4_parameters.hpp"
#include "SDK/Blood_BP_PT_classes.hpp"
#include "SDK/BP_BloodDecal_classes.hpp"
#include "SDK/BP_MeshBloodSim_classes.hpp"
#include "SDK/BP_MeshBloodSimManager_classes.hpp"
#include "SDK/BP_MeshBloodSimManager_parameters.hpp"
#include "SDK/BP_MeshBloodSim_parameters.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/Engine_parameters.hpp"
#include "SDK/HSComputeShaders_classes.hpp"
#include "SDK/RunningBlood_BP_classes.hpp"

namespace {
    Logger g_logger("AssetOverrides");
    constexpr std::string_view GAME_PREFIX = "/Game/Assets/";

    uint64_t Fnv1a(std::string_view text) noexcept {
        uint64_t hash = 14695981039346656037ull;
        for (char c : text) {
            hash ^= static_cast<unsigned char>(c);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    bool IsSupportedImage(const std::filesystem::path& path) {
        auto ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
    }

    std::string TargetPathFromFile(const std::filesystem::path& root, const std::filesystem::path& file) {
        std::error_code ec;
        auto rel = std::filesystem::relative(file, root, ec);
        if (ec) return {};

        rel.replace_extension();
        auto target = std::string(GAME_PREFIX);
        target += rel.generic_string();
        target += ".";
        target += file.stem().string();
        return target;
    }

    SDK::UMaterialInstance* AsMaterialInstance(SDK::UMaterialInterface* material) {
        return material && material->IsA(SDK::UMaterialInstance::StaticClass())
                   ? static_cast<SDK::UMaterialInstance*>(material)
                   : nullptr;
    }

    SDK::UMaterialInstanceDynamic* AsDynamicMaterial(SDK::UMaterialInterface* material) {
        return material && material->IsA(SDK::UMaterialInstanceDynamic::StaticClass())
                   ? static_cast<SDK::UMaterialInstanceDynamic*>(material)
                   : nullptr;
    }

    bool IsBloodDebugActor(SDK::UObject* object) {
        return object &&
               (object->IsA(SDK::ABlood_BP_P4_C::StaticClass()) ||
                object->IsA(SDK::ABlood_BP_PT_C::StaticClass()) ||
                object->IsA(SDK::ABP_BloodDecal_C::StaticClass()) ||
                object->IsA(SDK::ABP_MeshBloodSim_C::StaticClass()) ||
                object->IsA(SDK::ABP_MeshBloodSimManager_C::StaticClass()) ||
                object->IsA(SDK::ACSBloodSimActor::StaticClass()) ||
                object->IsA(SDK::AHSWoundsController::StaticClass()) ||
                object->IsA(SDK::ARunningBlood_BP_C::StaticClass()));
    }

    bool MaterialChainContains(SDK::UMaterialInterface* material, SDK::UMaterialInterface* expected) {
        if (!material || !expected) return false;
        for (auto* current = material; current;) {
            if (current == expected) return true;
            auto* instance = AsMaterialInstance(current);
            current = instance ? instance->Parent : nullptr;
        }
        return false;
    }

    bool IsBloodRtParameter(const SDK::FName& name) {
        return name.GetRawString() == "BloodRT";
    }

    bool IsRenderTargetTexture(SDK::UTexture* texture) {
        return texture && texture->IsA(SDK::UTextureRenderTarget::StaticClass());
    }

    const SDK::FMaterialParameterInfo& BloodRtParameterInfo() {
        static const SDK::FMaterialParameterInfo PARAMETER = [] {
            SDK::FMaterialParameterInfo info{};
            info.Name = SDK::BasicFilesImplUtils::StringToName(L"BloodRT");
            info.Association = SDK::EMaterialParameterAssociation::GlobalParameter;
            info.Index = -1;
            return info;
        }();
        return PARAMETER;
    }

    SDK::UTexture* GetBloodRenderTarget(SDK::UMaterialInstanceDynamic* dynamicMaterial) {
        if (!dynamicMaterial) return nullptr;

        auto* texture = dynamicMaterial->K2_GetTextureParameterValueByInfo(BloodRtParameterInfo());
        if (IsRenderTargetTexture(texture)) return texture;

        for (int p = 0; p < dynamicMaterial->TextureParameterValues.Num(); ++p) {
            auto& value = dynamicMaterial->TextureParameterValues[p];
            if (IsBloodRtParameter(value.ParameterInfo.Name) && IsRenderTargetTexture(value.ParameterValue))
                return value.ParameterValue;
        }
        return nullptr;
    }

    void CopyExplicitDynamicParameters(
        SDK::UMaterialInstanceDynamic* target,
        SDK::UMaterialInstanceDynamic* source
    ) {
        if (!target || !source) return;

        for (int p = 0; p < source->ScalarParameterValues.Num(); ++p) {
            auto& value = source->ScalarParameterValues[p];
            target->SetScalarParameterValueByInfo(value.ParameterInfo, value.ParameterValue);
        }
        for (int p = 0; p < source->VectorParameterValues.Num(); ++p) {
            auto& value = source->VectorParameterValues[p];
            target->SetVectorParameterValueByInfo(value.ParameterInfo, value.ParameterValue);
        }
        for (int p = 0; p < source->TextureParameterValues.Num(); ++p) {
            auto& value = source->TextureParameterValues[p];
            if (value.ParameterValue) target->SetTextureParameterValueByInfo(value.ParameterInfo, value.ParameterValue);
        }
    }

    bool HasRuntimeBloodTarget(SDK::UMaterialInterface* material) {
        for (auto* current = AsMaterialInstance(material); current;) {
            for (int p = 0; p < current->TextureParameterValues.Num(); ++p) {
                auto& value = current->TextureParameterValues[p];
                if (IsBloodRtParameter(value.ParameterInfo.Name) && IsRenderTargetTexture(value.ParameterValue))
                    return true;
            }
            current = AsMaterialInstance(current->Parent);
        }
        return false;
    }

    void PrepareOverrideTexture(SDK::UTexture* source, SDK::UTexture2D* replacement) {
        if (!source || !replacement) return;

        replacement->CompressionSettings = source->CompressionSettings;
        replacement->Filter = source->Filter;
        replacement->MipLoadOptions = source->MipLoadOptions;
        replacement->CookPlatformTilingSettings = source->CookPlatformTilingSettings;
        replacement->bOodlePreserveExtremes = source->bOodlePreserveExtremes;
        replacement->LODGroup = source->LODGroup;
        replacement->Downscale = source->Downscale;
        replacement->DownscaleOptions = source->DownscaleOptions;
        replacement->SRGB = source->SRGB;
        replacement->bNoTiling = source->bNoTiling;
        replacement->NeverStream = true;

        if (auto* source2D = source->IsA(SDK::UTexture2D::StaticClass()) ? static_cast<SDK::UTexture2D*>(source)
                                                                         : nullptr) {
            replacement->AddressX = source2D->AddressX;
            replacement->AddressY = source2D->AddressY;
        }
        replacement->SetForceMipLevelsToBeResident(30.0f, 0);
    }

    template <typename Func> void ForEachPrimitiveComponent(SDK::UWorld* world, Func&& func) {
        if (!world) return;
        for (auto* level : world->Levels) {
            if (!level) continue;
            for (auto* actor : level->Actors) {
                if (!actor) continue;
                if (IsBloodDebugActor(actor)) continue;
                SDK::TArray<SDK::UActorComponent*> components =
                    actor->K2_GetComponentsByClass(SDK::UPrimitiveComponent::StaticClass());
                for (auto* component : components) {
                    if (component && component->IsA(SDK::UPrimitiveComponent::StaticClass()))
                        func(static_cast<SDK::UPrimitiveComponent*>(component));
                }
            }
        }
    }

    template <typename Func> void ForEachActorPrimitiveComponent(SDK::AActor* actor, Func&& func) {
        if (!actor) return;
        if (IsBloodDebugActor(actor)) return;
        SDK::TArray<SDK::UActorComponent*> components =
            actor->K2_GetComponentsByClass(SDK::UPrimitiveComponent::StaticClass());
        for (auto* component : components) {
            if (component && component->IsA(SDK::UPrimitiveComponent::StaticClass()))
                func(static_cast<SDK::UPrimitiveComponent*>(component));
        }
    }

    template <typename LookupTexture, typename PathCache, typename Func>
    int ApplyMatchedTextureParameters(
        SDK::UMaterialInterface* material, LookupTexture&& lookupTexture, PathCache& pathCache,
        std::vector<uint8_t>* matchedTargets, size_t* matchedTargetCount, Func&& func
    ) {
        auto* instance = AsMaterialInstance(material);
        if (!instance) return 0;

        int updates = 0;
        for (auto* current = instance; current;) {
            for (int p = 0; p < current->TextureParameterValues.Num(); ++p) {
                auto& value = current->TextureParameterValues[p];
                if (!value.ParameterValue) continue;

                auto [pathIt, insertedPath] = pathCache.try_emplace(value.ParameterValue);
                if (insertedPath) {
                    pathIt->second.path = PresetUtils::ObjectToAbsolutePath(value.ParameterValue);
                    pathIt->second.hash = Fnv1a(pathIt->second.path);
                }

                const auto match = lookupTexture(pathIt->second.path, pathIt->second.hash);
                if (!match.texture) continue;

                if (matchedTargets && matchedTargetCount && match.index < matchedTargets->size() &&
                    (*matchedTargets)[match.index] == 0) {
                    (*matchedTargets)[match.index] = 1;
                    ++(*matchedTargetCount);
                }
                updates += func(value.ParameterInfo, value.ParameterValue, match.texture) ? 1 : 0;
            }
            current = AsMaterialInstance(current->Parent);
        }
        return updates;
    }
}

AssetOverrideManager& AssetOverrideManager::Get() {
    static AssetOverrideManager manager;
    return manager;
}

bool AssetOverrideManager::Initialize() {
    if (initialized) return true;
    if (!ScanFiles()) return false;
    initialized = true;

    GameHook::QueueAction([this](const RuntimeContextSnapshot&) {
        auto applyActor = [this](GameHook::ProcessEventContext& context) {
            if (!context.object || !context.object->IsA(SDK::AActor::StaticClass())) return;
            if (IsBloodDebugActor(context.object)) return;
            ApplyToActor(SDK::UWorld::GetWorld(), static_cast<SDK::AActor*>(context.object));
        };
        auto getPrimitiveComponent = [](GameHook::ProcessEventContext& context) -> SDK::UPrimitiveComponent* {
            if (!context.object || !context.object->IsA(SDK::UPrimitiveComponent::StaticClass())) return nullptr;
            return static_cast<SDK::UPrimitiveComponent*>(context.object);
        };
        auto applySetMaterial = [this, getPrimitiveComponent](GameHook::ProcessEventContext& context) {
            auto* component = getPrimitiveComponent(context);
            auto* params = context.Params<SDK::Params::PrimitiveComponent_SetMaterial>();
            if (!component || !params) return;

            ApplyToComponentNow(component, params->ElementIndex, true);
        };
        auto applySetMaterialByName = [this, getPrimitiveComponent](GameHook::ProcessEventContext& context) {
            auto* component = getPrimitiveComponent(context);
            auto* params = context.Params<SDK::Params::PrimitiveComponent_SetMaterialByName>();
            if (!component || !params) return;

            const int slot = component->GetMaterialIndex(params->MaterialSlotName);
            ApplyToComponentNow(component, slot, true);
        };
        auto applyCreatedMaterial = [this, getPrimitiveComponent](GameHook::ProcessEventContext& context) {
            auto* component = getPrimitiveComponent(context);
            auto* params = context.Params<SDK::Params::PrimitiveComponent_CreateDynamicMaterialInstance>();
            if (!component || !params) return;

            ApplyToCreatedMaterial(component, params->ElementIndex, params->ReturnValue, params->SourceMaterial);
        };
        auto applyCreatedAndSetMaterial = [this, getPrimitiveComponent](GameHook::ProcessEventContext& context) {
            auto* component = getPrimitiveComponent(context);
            auto* params = context.Params<SDK::Params::PrimitiveComponent_CreateAndSetMaterialInstanceDynamic>();
            if (!component || !params) return;

            ApplyToCreatedMaterial(component, params->ElementIndex, params->ReturnValue, nullptr);
        };
        auto applyCreatedAndSetMaterialFromMaterial = [this,
                                                       getPrimitiveComponent](GameHook::ProcessEventContext& context) {
            auto* component = getPrimitiveComponent(context);
            auto* params =
                context.Params<SDK::Params::PrimitiveComponent_CreateAndSetMaterialInstanceDynamicFromMaterial>();
            if (!component || !params) return;

            ApplyToCreatedMaterial(component, params->ElementIndex, params->ReturnValue, params->Parent);
        };
        auto replaceTextureParameter = [this](SDK::UMaterialInstanceDynamic* dynamicMaterial, SDK::UTexture*& value) {
            if (!value || IsRenderTargetTexture(value) || HasRuntimeBloodTarget(dynamicMaterial) ||
                !PrepareWorld(SDK::UWorld::GetWorld()) || textures.empty())
                return;

            const auto path = PresetUtils::ObjectToAbsolutePath(value);
            if (path.empty()) return;

            auto* texture = FindTexture(path, Fnv1a(path)).texture;
            if (!texture || texture == value) return;

            PrepareOverrideTexture(value, texture);
            value = texture;
        };
        auto applyTextureParameter = [replaceTextureParameter](GameHook::ProcessEventContext& context) {
            if (!context.object || !context.object->IsA(SDK::UMaterialInstanceDynamic::StaticClass())) return;

            auto* params = context.Params<SDK::Params::MaterialInstanceDynamic_SetTextureParameterValue>();
            if (!params) return;
            auto* dynamicMaterial = static_cast<SDK::UMaterialInstanceDynamic*>(context.object);
            if (IsBloodRtParameter(params->ParameterName)) return;
            replaceTextureParameter(dynamicMaterial, params->Value);
        };
        auto applyTextureParameterByInfo = [replaceTextureParameter](GameHook::ProcessEventContext& context) {
            if (!context.object || !context.object->IsA(SDK::UMaterialInstanceDynamic::StaticClass())) return;

            auto* params = context.Params<SDK::Params::MaterialInstanceDynamic_SetTextureParameterValueByInfo>();
            if (!params) return;
            auto* dynamicMaterial = static_cast<SDK::UMaterialInstanceDynamic*>(context.object);
            if (IsBloodRtParameter(params->ParameterInfo.Name)) return;
            replaceTextureParameter(dynamicMaterial, params->Value);
        };
        auto prepareCreatedMaterial = [this,
                                       getPrimitiveComponent](GameHook::ProcessEventContext& context) {
            auto* component = getPrimitiveComponent(context);
            auto* params = context.Params<SDK::Params::PrimitiveComponent_CreateDynamicMaterialInstance>();
            if (!component || !params) return;

            auto* rebasedSource = RebaseSourceMaterial(component, params->ElementIndex, params->SourceMaterial);
            if (rebasedSource == params->SourceMaterial) return;

            params->SourceMaterial = rebasedSource;
        };
        auto prepareCreatedAndSetMaterial = [this,
                                             getPrimitiveComponent](GameHook::ProcessEventContext& context) {
            auto* component = getPrimitiveComponent(context);
            auto* params = context.Params<SDK::Params::PrimitiveComponent_CreateAndSetMaterialInstanceDynamic>();
            if (!component || !params || params->ElementIndex < 0 || params->ElementIndex >= component->GetNumMaterials())
                return;

            auto* sourceMaterial = GetTrackedSourceMaterial(component, params->ElementIndex);
            if (!sourceMaterial) return;

            auto* currentMaterial = component->GetMaterial(params->ElementIndex);
            if (currentMaterial == sourceMaterial || !MaterialChainContains(currentMaterial, sourceMaterial)) return;

            component->SetMaterial(params->ElementIndex, sourceMaterial);
        };
        auto prepareCreatedAndSetMaterialFromMaterial = [this,
                                                         getPrimitiveComponent](GameHook::ProcessEventContext& context) {
            auto* component = getPrimitiveComponent(context);
            auto* params =
                context.Params<SDK::Params::PrimitiveComponent_CreateAndSetMaterialInstanceDynamicFromMaterial>();
            if (!component || !params) return;

            auto* rebasedSource = RebaseSourceMaterial(component, params->ElementIndex, params->Parent);
            if (rebasedSource == params->Parent) return;

            params->Parent = rebasedSource;
        };
        auto prepareBloodMeshInitialize = [this](GameHook::ProcessEventContext& context) {
            if (!context.object || !context.object->IsA(SDK::ABP_MeshBloodSim_C::StaticClass())) return;

            auto* params = context.Params<SDK::Params::BP_MeshBloodSim_C_Initialize>();
            auto* component = params ? params->MeshSimulatedOn : nullptr;
            auto* sourceMaterial = GetTrackedSourceMaterial(component, 0);
            if (!component || !sourceMaterial) return;

            auto* currentMaterial = component->GetMaterial(0);
            if (currentMaterial == sourceMaterial || !MaterialChainContains(currentMaterial, sourceMaterial)) return;

            component->SetMaterial(0, sourceMaterial);
        };
        auto applyBloodMeshInitialize = [this](GameHook::ProcessEventContext& context) {
            if (!context.object || !context.object->IsA(SDK::ABP_MeshBloodSim_C::StaticClass())) return;

            auto* sim = static_cast<SDK::ABP_MeshBloodSim_C*>(context.object);
            auto* params = context.Params<SDK::Params::BP_MeshBloodSim_C_Initialize>();
            auto* component = params && params->MeshSimulatedOn ? params->MeshSimulatedOn : sim->MeshToSimOn;
            auto* sourceMaterial = GetTrackedSourceMaterial(component, 0);
            auto* dynamicMaterial = sim->MaterialOfTheMesh;
            if (!component || !sourceMaterial || !dynamicMaterial) return;

            const int updates = ApplyBloodDynamicMaterial(dynamicMaterial, sourceMaterial);

            Stats next = GetStats();
            next.appliedMaterials = updates;
            next.scannedComponents = 1;
            next.scannedMaterials = 1;
            StoreStats(next);

            TrackOverriddenMaterial(component, 0, sourceMaterial, dynamicMaterial);
        };
        auto applyReceiveParticleData = [this](GameHook::ProcessEventContext& context) {
            if (!context.object || !context.object->IsA(SDK::ABlood_BP_P4_C::StaticClass())) return;

            auto* params = context.Params<SDK::Params::Blood_BP_P4_C_ReceiveParticleData>();
            auto* blood = static_cast<SDK::ABlood_BP_P4_C*>(context.object);
            if (!params) return;

            (void)ApplyBloodComponentMaterial(blood->Hit_Component, 0);
            (void)RepairBloodMaterials();
        };
        auto applyBloodDoMesh = [this](GameHook::ProcessEventContext& context) {
            if (!context.object || !context.object->IsA(SDK::ABP_MeshBloodSimManager_C::StaticClass())) return;

            auto* params = context.Params<SDK::Params::BP_MeshBloodSimManager_C_DoMeshBloodSim>();
            if (!params) return;

            auto* sim = params->CallFunc_FinishSpawningActor_ReturnValue
                            ? params->CallFunc_FinishSpawningActor_ReturnValue
                            : params->CallFunc_Map_Find_Value;
            if (sim && sim->MeshToSimOn) {
                auto* sourceMaterial = GetTrackedSourceMaterial(sim->MeshToSimOn, 0);
                ApplyBloodDynamicMaterial(sim->MaterialOfTheMesh, sourceMaterial);
            }
            ApplyBloodComponentMaterial(params->MeshToSimOn, 0);
        };
        auto applyBloodUpdateSimulations = [this](GameHook::ProcessEventContext& context) {
            if (!context.object || !context.object->IsA(SDK::ABP_MeshBloodSimManager_C::StaticClass())) return;

            auto* params = context.Params<SDK::Params::BP_MeshBloodSimManager_C_UpdateSimulations>();
            if (!params) return;

            auto* sim = params->ExistingSimActor;
            if (!sim) sim = params->CallFunc_FinishSpawningActor_ReturnValue;
            if (!sim) sim = params->CallFunc_Map_Find_Value;
            if (sim) {
                ApplyBloodComputeActor(sim);
            } else {
                ApplyBloodComponentMaterial(params->MeshToSimOn, 0);
            }
        };
        auto applyBloodAddNewParticle = [this](GameHook::ProcessEventContext& context) {
            if (!context.object || !context.object->IsA(SDK::ACSBloodSimActor::StaticClass())) return;
            ApplyBloodComputeActor(static_cast<SDK::ACSBloodSimActor*>(context.object));
        };

        auto& hook = GameHook::Get();
        (void)hook.Subscribe("ReceiveBeginPlay", GameHook::HookPhase::After, applyActor);
        (void)hook.Subscribe("SetMaterial", GameHook::HookPhase::After, applySetMaterial);
        (void)hook.Subscribe("SetMaterialByName", GameHook::HookPhase::After, applySetMaterialByName);
        (void)hook.Subscribe("CreateDynamicMaterialInstance", GameHook::HookPhase::Before, prepareCreatedMaterial);
        (void)hook.Subscribe("CreateDynamicMaterialInstance", GameHook::HookPhase::After, applyCreatedMaterial);
        (void)hook.Subscribe(
            "CreateAndSetMaterialInstanceDynamic", GameHook::HookPhase::Before, prepareCreatedAndSetMaterial
        );
        (void)hook.Subscribe(
            "CreateAndSetMaterialInstanceDynamic", GameHook::HookPhase::After, applyCreatedAndSetMaterial
        );
        (void)hook.Subscribe(
            "CreateAndSetMaterialInstanceDynamicFromMaterial", GameHook::HookPhase::Before,
            prepareCreatedAndSetMaterialFromMaterial
        );
        (void)hook.Subscribe(
            "CreateAndSetMaterialInstanceDynamicFromMaterial", GameHook::HookPhase::After,
            applyCreatedAndSetMaterialFromMaterial
        );
        (void)hook.Subscribe("SetTextureParameterValue", GameHook::HookPhase::Before, applyTextureParameter);
        (void)hook.Subscribe("SetTextureParameterValueByInfo", GameHook::HookPhase::Before, applyTextureParameterByInfo);
        (void)hook.Subscribe("Initialize", GameHook::HookPhase::Before, prepareBloodMeshInitialize);
        (void)hook.Subscribe("Initialize", GameHook::HookPhase::After, applyBloodMeshInitialize);
        (void)hook.Subscribe("DoMeshBloodSim", GameHook::HookPhase::After, applyBloodDoMesh);
        (void)hook.Subscribe("UpdateSimulations", GameHook::HookPhase::After, applyBloodUpdateSimulations);
        (void)hook.Subscribe("AddNewParticle", GameHook::HookPhase::After, applyBloodAddNewParticle);
        (void)hook.Subscribe("ReceiveParticleData", GameHook::HookPhase::After, applyReceiveParticleData);
    });
    return true;
}

std::filesystem::path AssetOverrideManager::GetRootPath() const {
    return ConfigManager::GetAppDataPath() / ROOT_FOLDER;
}

void AssetOverrideManager::RequestRefresh() {
    GameHook::QueueAction([this](const RuntimeContextSnapshot& runtime) {
        needsScan = true;
        needsLoad = true;
        needsApply = true;
        if (runtime.world) ApplyNow(runtime.world);
    });
}

void AssetOverrideManager::RequestApply() {
    if (files.empty() && !needsScan) return;

    bool expected = false;
    if (!applyQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;

    GameHook::QueueAction([this](const RuntimeContextSnapshot& runtime) {
        applyQueued.store(false, std::memory_order_release);
        needsApply = true;
        if (runtime.world) ApplyNow(runtime.world);
    });
}

void AssetOverrideManager::RequestActorApply(SDK::AActor* actor) {
    if (!actor) return;
    const int actorObjectIndex = actor->Index;
    GameHook::QueueAction([this, actor, actorObjectIndex](const RuntimeContextSnapshot& runtime) {
        if (actorObjectIndex < 0 || SDK::UObject::GObjects->GetByIndex(actorObjectIndex) != actor) return;
        if (runtime.world) ApplyToActor(runtime.world, actor);
    });
}

bool AssetOverrideManager::PrepareWorld(SDK::UWorld* world) {
    if (!world) return false;
    if (needsScan) (void)ScanFiles();
    const bool changedWorld = (loadedWorld && loadedWorld != world) || (appliedWorld && appliedWorld != world);
    if (loadedWorld != world) {
        needsLoad = true;
        needsApply = true;
        if (changedWorld) {
            trackedMaterials.clear();
            trackedMaterialSlots.clear();
        }
    }
    if (needsLoad) LoadTextures(world);
    return true;
}

void AssetOverrideManager::ApplyNow(SDK::UWorld* world) {
    if (!PrepareWorld(world)) return;
    if (needsApply) ApplyToWorld(world);
}

AssetOverrideManager::Stats AssetOverrideManager::GetStats() const {
    std::lock_guard lock(statsMutex);
    return stats;
}

void AssetOverrideManager::StoreStats(Stats next) const {
    std::lock_guard lock(statsMutex);
    stats = next;
}

bool AssetOverrideManager::ScanFiles() {
    files.clear();
    ClearTextures();

    Stats next{};
    const auto root = GetRootPath();
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec) {
        next.errors = 1;
        StoreStats(next);
        g_logger.Log("Failed to create override folder: %s", ec.message().c_str());
        return false;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec) || !IsSupportedImage(entry.path())) continue;

        auto target = TargetPathFromFile(root, entry.path());
        if (target.empty()) {
            ++next.errors;
            continue;
        }
        files.push_back({entry.path(), std::move(target)});
    }

    next.files = static_cast<int>(files.size());
    StoreStats(next);
    needsScan = false;
    needsLoad = true;
    needsApply = true;
    return true;
}

void AssetOverrideManager::LoadTextures(SDK::UWorld* world) {
    ClearTextures();

    Stats next = GetStats();
    next.loaded = 0;
    next.appliedMaterials = 0;
    next.scannedComponents = 0;
    next.scannedMaterials = 0;
    next.unmatched = 0;

    std::unordered_map<std::string, size_t> textureIndex;
    textureIndex.reserve(files.size());

    for (const auto& file : files) {
        const auto widePath = file.filePath.wstring();
        auto* texture = SDK::UKismetRenderingLibrary::ImportFileAsTexture2D(world, SDK::FString(widePath.c_str()));
        if (!texture) {
            ++next.errors;
            g_logger.Log("Failed to import texture override: %s", file.filePath.string().c_str());
            continue;
        }
        texture->Flags = static_cast<SDK::EObjectFlags>(
            static_cast<uint32_t>(texture->Flags) | static_cast<uint32_t>(SDK::EObjectFlags::MarkAsRootSet)
        );
        rootedTextures.push_back(texture);
        const auto targetHash = Fnv1a(file.targetPath);
        auto [it, inserted] = textureIndex.emplace(file.targetPath, textures.size());
        if (inserted) {
            textures.push_back({file.targetPath, targetHash, texture});
        } else {
            textures[it->second].texture = texture;
        }
        ++next.loaded;
    }

    SortTexturesForLookup();
    needsLoad = false;
    loadedWorld = world;
    StoreStats(next);
}

void AssetOverrideManager::ClearTextures() {
    for (auto* texture : rootedTextures) {
        if (!texture) continue;
        texture->Flags = static_cast<SDK::EObjectFlags>(
            static_cast<uint32_t>(texture->Flags) & ~static_cast<uint32_t>(SDK::EObjectFlags::MarkAsRootSet)
        );
    }
    rootedTextures.clear();
    textures.clear();
    loadedWorld = nullptr;
}

void AssetOverrideManager::SortTexturesForLookup() {
    std::ranges::sort(textures, [](const TextureOverride& a, const TextureOverride& b) {
        if (a.targetHash != b.targetHash) return a.targetHash < b.targetHash;
        return a.targetPath < b.targetPath;
    });
}

AssetOverrideManager::TextureLookupResult AssetOverrideManager::FindTexture(
    std::string_view targetPath, uint64_t targetHash
) const {
    if (textures.size() <= LINEAR_TEXTURE_LOOKUP_LIMIT) {
        for (size_t i = 0; i < textures.size(); ++i) {
            const auto& entry = textures[i];
            if (entry.targetHash == targetHash && entry.targetPath == targetPath) return {i, entry.texture};
        }
        return {};
    }

    auto it = std::lower_bound(
        textures.begin(), textures.end(), targetHash,
        [](const TextureOverride& entry, uint64_t value) { return entry.targetHash < value; }
    );
    for (; it != textures.end() && it->targetHash == targetHash; ++it) {
        if (it->targetPath == targetPath) return {static_cast<size_t>(it - textures.begin()), it->texture};
    }
    return {};
}

SDK::UMaterialInterface* AssetOverrideManager::GetTrackedSourceMaterial(
    SDK::UPrimitiveComponent* component, int materialIndex
) {
    if (!component || materialIndex < 0 || materialIndex >= component->GetNumMaterials()) return nullptr;

    const MaterialSlot slotKey{component, materialIndex, component->Index};
    auto trackedIt = trackedMaterials.find(slotKey);
    return trackedIt != trackedMaterials.end() ? trackedIt->second.source : nullptr;
}

SDK::UMaterialInterface* AssetOverrideManager::RebaseSourceMaterial(
    SDK::UPrimitiveComponent* component, int materialIndex, SDK::UMaterialInterface* requestedSource
) {
    auto* storedSource = GetTrackedSourceMaterial(component, materialIndex);
    if (!storedSource) return requestedSource;

    auto* effectiveSource = requestedSource ? requestedSource : component->GetMaterial(materialIndex);
    if (effectiveSource != storedSource && MaterialChainContains(effectiveSource, storedSource)) return storedSource;

    return requestedSource;
}

void AssetOverrideManager::ForgetComponentSourceMaterials(SDK::UPrimitiveComponent* component, int materialIndex) {
    if (!component || materialIndex < 0) return;

    const MaterialSlot slotKey{component, materialIndex, component->Index};
    trackedMaterials.erase(slotKey);
    std::erase_if(trackedMaterialSlots, [component, materialIndex](const MaterialSlot& slot) {
        return slot.component == component && slot.materialIndex == materialIndex;
    });
}

void AssetOverrideManager::TrackOverriddenMaterial(
    SDK::UPrimitiveComponent* component, int materialIndex, SDK::UMaterialInterface* sourceMaterial,
    SDK::UMaterialInstanceDynamic* dynamicMaterial
) {
    if (!component || materialIndex < 0 || !sourceMaterial || !dynamicMaterial) return;

    const MaterialSlot slotKey{component, materialIndex, component->Index};
    const bool wasUntracked = trackedMaterials.find(slotKey) == trackedMaterials.end();
    trackedMaterials[slotKey] = {sourceMaterial, dynamicMaterial};
    if (wasUntracked) trackedMaterialSlots.push_back(slotKey);
}

int AssetOverrideManager::ApplyBloodDynamicMaterial(
    SDK::UMaterialInstanceDynamic* dynamicMaterial, SDK::UMaterialInterface* sourceMaterial
) {
    if (!dynamicMaterial || !sourceMaterial || !PrepareWorld(SDK::UWorld::GetWorld()) || textures.empty()) return 0;

    ObjectPathCache pathCache;
    pathCache.reserve(32);
    return ApplyToMaterialInstance(dynamicMaterial, sourceMaterial, nullptr, nullptr, pathCache, true);
}

int AssetOverrideManager::ApplyBloodComponentMaterial(SDK::UPrimitiveComponent* component, int materialIndex) {
    int updates = 0;
    if (!component || materialIndex < 0 || materialIndex >= component->GetNumMaterials()) return updates;

    const MaterialSlot slotKey{component, materialIndex, component->Index};
    auto trackedIt = trackedMaterials.find(slotKey);
    if (trackedIt == trackedMaterials.end() || !trackedIt->second.source) return updates;

    auto* sourceMaterial = trackedIt->second.source;
    auto* currentMaterial = component->GetMaterial(materialIndex);
    auto* dynamicMaterial = AsDynamicMaterial(currentMaterial);
    auto* storedOverrideMaterial = trackedIt->second.dynamic;

    if (dynamicMaterial && dynamicMaterial != storedOverrideMaterial) {
        auto* bloodTexture = GetBloodRenderTarget(dynamicMaterial);
        if (bloodTexture) {
            auto* brokenDynamicMaterial = dynamicMaterial;
            auto* repairedMaterial = component->CreateDynamicMaterialInstance(materialIndex, sourceMaterial, SDK::FName());
            if (!repairedMaterial) return updates;

            CopyExplicitDynamicParameters(repairedMaterial, brokenDynamicMaterial);
            repairedMaterial->SetTextureParameterValueByInfo(BloodRtParameterInfo(), bloodTexture);

            dynamicMaterial = repairedMaterial;
            currentMaterial = repairedMaterial;
            TrackOverriddenMaterial(component, materialIndex, sourceMaterial, dynamicMaterial);
        }
    }

    updates += ApplyBloodDynamicMaterial(dynamicMaterial, sourceMaterial);

    if (!dynamicMaterial && currentMaterial == sourceMaterial) {
        if (storedOverrideMaterial) {
            component->SetMaterial(materialIndex, storedOverrideMaterial);
            dynamicMaterial = storedOverrideMaterial;
        }
    }
    if (!dynamicMaterial) return updates;

    TrackOverriddenMaterial(component, materialIndex, sourceMaterial, dynamicMaterial);
    return updates;
}

int AssetOverrideManager::ApplyBloodComputeActor(SDK::ACSBloodSimActor* sim) {
    if (!sim || !sim->BoundMesh) return 0;

    auto* component = static_cast<SDK::UPrimitiveComponent*>(sim->BoundMesh);
    auto* sourceMaterial = GetTrackedSourceMaterial(component, 0);
    int updates = 0;
    updates += ApplyBloodDynamicMaterial(sim->BoundMeshMatInstance, sourceMaterial);
    for (auto* dynamicMaterial : sim->BoundMeshMatInstances) {
        updates += ApplyBloodDynamicMaterial(dynamicMaterial, sourceMaterial);
    }
    updates += ApplyBloodComponentMaterial(component, 0);
    return updates;
}

int AssetOverrideManager::RepairBloodMaterials() {
    if (trackedMaterials.empty()) return 0;

    int updates = 0;
    for (size_t i = 0; i < trackedMaterialSlots.size();) {
        const auto slot = trackedMaterialSlots[i];
        auto forgetSlot = [this, i, slot]() {
            trackedMaterials.erase(slot);
            trackedMaterialSlots[i] = trackedMaterialSlots.back();
            trackedMaterialSlots.pop_back();
        };

        auto* component = slot.component;
        if (!component || SDK::UObject::GObjects->GetByIndex(slot.componentObjectIndex) != component ||
            !component->IsA(SDK::UPrimitiveComponent::StaticClass())) {
            forgetSlot();
            continue;
        }
        if (slot.materialIndex < 0 || slot.materialIndex >= component->GetNumMaterials()) {
            forgetSlot();
            continue;
        }

        auto trackedIt = trackedMaterials.find(slot);
        if (trackedIt == trackedMaterials.end() || !trackedIt->second.source || !trackedIt->second.dynamic) {
            forgetSlot();
            continue;
        }

        auto* currentMaterial = component->GetMaterial(slot.materialIndex);
        if (currentMaterial == trackedIt->second.dynamic) {
            ++i;
            continue;
        }

        if (!AsDynamicMaterial(currentMaterial) && currentMaterial != trackedIt->second.source) {
            ++i;
            continue;
        }

        updates += ApplyBloodComponentMaterial(component, slot.materialIndex);
        ++i;
    }

    return updates;
}

int AssetOverrideManager::ApplyToMaterialInstance(
    SDK::UMaterialInstanceDynamic* dynamicMaterial, SDK::UMaterialInterface* sourceMaterial,
    std::vector<uint8_t>* matchedTargets, size_t* matchedTargetCount, ObjectPathCache& pathCache,
    bool allowRuntimeBloodTarget
) {
    if (!dynamicMaterial || !sourceMaterial) return 0;
    if ((!allowRuntimeBloodTarget && HasRuntimeBloodTarget(dynamicMaterial)) || HasRuntimeBloodTarget(sourceMaterial))
        return 0;

    auto lookupTexture = [this](std::string_view targetPath, uint64_t targetHash) {
        return FindTexture(targetPath, targetHash);
    };

    return ApplyMatchedTextureParameters(
        sourceMaterial, lookupTexture, pathCache, matchedTargets, matchedTargetCount,
        [dynamicMaterial](
            const SDK::FMaterialParameterInfo& parameter, SDK::UTexture* sourceTexture, SDK::UTexture2D* texture
        ) {
            if (IsBloodRtParameter(parameter.Name)) return false;
            if (dynamicMaterial->K2_GetTextureParameterValueByInfo(parameter) == texture) return false;

            PrepareOverrideTexture(sourceTexture, texture);
            dynamicMaterial->SetTextureParameterValueByInfo(parameter, texture);
            return true;
        }
    );
}

void AssetOverrideManager::ApplyToComponent(
    SDK::UPrimitiveComponent* component, Stats& next, std::vector<uint8_t>* matchedTargets,
    size_t* matchedTargetCount, ObjectPathCache& pathCache
) {
    if (!component) return;

    ++next.scannedComponents;
    const int materialCount = component->GetNumMaterials();
    for (int materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
        ApplyToComponentSlot(component, materialIndex, next, matchedTargets, matchedTargetCount, pathCache);
    }
}

void AssetOverrideManager::ApplyToComponentSlot(
    SDK::UPrimitiveComponent* component, int materialIndex, Stats& next, std::vector<uint8_t>* matchedTargets,
    size_t* matchedTargetCount, ObjectPathCache& pathCache
) {
    if (!component || materialIndex < 0 || materialIndex >= component->GetNumMaterials()) return;

    auto lookupTexture = [this](std::string_view targetPath, uint64_t targetHash) {
        return FindTexture(targetPath, targetHash);
    };

    ++next.scannedMaterials;
    auto* material = component->GetMaterial(materialIndex);
    if (HasRuntimeBloodTarget(material)) return;

    auto* dynamicMaterial = AsDynamicMaterial(material);
    const MaterialSlot slotKey{component, materialIndex, component->Index};
    auto trackedIt = trackedMaterials.find(slotKey);
    auto* sourceMaterial =
        trackedIt != trackedMaterials.end() ? trackedIt->second.source : (dynamicMaterial ? dynamicMaterial->Parent
                                                                                          : material);
    if (!sourceMaterial) return;

    next.appliedMaterials += ApplyMatchedTextureParameters(
        sourceMaterial, lookupTexture, pathCache, matchedTargets, matchedTargetCount,
        [this, component, materialIndex, sourceMaterial, &dynamicMaterial,
         &next](
            const SDK::FMaterialParameterInfo& parameter, SDK::UTexture* sourceTexture, SDK::UTexture2D* texture
        ) {
            if (IsBloodRtParameter(parameter.Name)) return false;
            if (dynamicMaterial && dynamicMaterial->K2_GetTextureParameterValueByInfo(parameter) == texture)
                return false;

            const MaterialSlot slotKey{component, materialIndex, component->Index};
            if (!dynamicMaterial) {
                if (auto trackedIt = trackedMaterials.find(slotKey);
                    trackedIt != trackedMaterials.end() && trackedIt->second.dynamic) {
                    dynamicMaterial = trackedIt->second.dynamic;
                    component->SetMaterial(materialIndex, dynamicMaterial);
                } else {
                    dynamicMaterial =
                        component->CreateDynamicMaterialInstance(materialIndex, sourceMaterial, SDK::FName());
                }
            }
            if (!dynamicMaterial) {
                ++next.errors;
                return false;
            }
            if (dynamicMaterial->K2_GetTextureParameterValueByInfo(parameter) == texture) return false;

            TrackOverriddenMaterial(component, materialIndex, sourceMaterial, dynamicMaterial);
            PrepareOverrideTexture(sourceTexture, texture);
            dynamicMaterial->SetTextureParameterValueByInfo(parameter, texture);
            return true;
        }
    );
}

void AssetOverrideManager::ApplyToComponentNow(
    SDK::UPrimitiveComponent* component, int materialIndex, bool resetSource
) {
    auto* world = SDK::UWorld::GetWorld();
    if (!component || materialIndex < 0 || materialIndex >= component->GetNumMaterials() || !PrepareWorld(world) ||
        textures.empty())
        return;
    if (IsBloodDebugActor(component->GetOwner())) return;

    const MaterialSlot slotKey{component, materialIndex, component->Index};
    auto trackedIt = trackedMaterials.find(slotKey);
    if (resetSource || (trackedIt != trackedMaterials.end() &&
                        !MaterialChainContains(component->GetMaterial(materialIndex), trackedIt->second.source))) {
        ForgetComponentSourceMaterials(component, materialIndex);
    }

    Stats next = GetStats();
    next.appliedMaterials = 0;
    next.scannedComponents = 0;
    next.scannedMaterials = 0;

    ObjectPathCache pathCache;
    pathCache.reserve(32);
    ++next.scannedComponents;
    ApplyToComponentSlot(component, materialIndex, next, nullptr, nullptr, pathCache);

    StoreStats(next);
}

void AssetOverrideManager::ApplyToCreatedMaterial(
    SDK::UPrimitiveComponent* component, int materialIndex, SDK::UMaterialInstanceDynamic* dynamicMaterial,
    SDK::UMaterialInterface* explicitSource
) {
    auto* world = SDK::UWorld::GetWorld();
    if (!dynamicMaterial || !PrepareWorld(world) || textures.empty()) return;
    if (component && IsBloodDebugActor(component->GetOwner())) return;

    SDK::UMaterialInterface* sourceMaterial = explicitSource ? explicitSource : dynamicMaterial->Parent;
    if (HasRuntimeBloodTarget(dynamicMaterial) || HasRuntimeBloodTarget(sourceMaterial)) return;

    const bool validSlot = component && materialIndex >= 0 && materialIndex < component->GetNumMaterials();
    if (validSlot) {
        const MaterialSlot slotKey{component, materialIndex, component->Index};
        auto trackedIt = trackedMaterials.find(slotKey);
        if (trackedIt != trackedMaterials.end()) {
            if (auto* inheritedDynamic = AsDynamicMaterial(sourceMaterial);
                inheritedDynamic && MaterialChainContains(inheritedDynamic, trackedIt->second.source)) {
                return;
            }
            if (MaterialChainContains(dynamicMaterial, trackedIt->second.source)) {
                sourceMaterial = trackedIt->second.source;
            } else {
                ForgetComponentSourceMaterials(component, materialIndex);
            }
        }
    }

    if (!sourceMaterial) return;

    Stats next = GetStats();
    next.appliedMaterials = 0;
    next.scannedComponents = validSlot ? 1 : 0;
    next.scannedMaterials = 1;

    ObjectPathCache pathCache;
    pathCache.reserve(32);
    const int updates = ApplyToMaterialInstance(dynamicMaterial, sourceMaterial, nullptr, nullptr, pathCache);
    next.appliedMaterials += updates;

    if (updates > 0) {
        if (validSlot) TrackOverriddenMaterial(component, materialIndex, sourceMaterial, dynamicMaterial);
    }
    StoreStats(next);
}

void AssetOverrideManager::ApplyToActor(SDK::UWorld* world, SDK::AActor* actor) {
    if (!actor || !PrepareWorld(world) || textures.empty()) return;
    if (needsApply) {
        ApplyToWorld(world);
        return;
    }

    Stats next = GetStats();
    next.appliedMaterials = 0;
    next.scannedComponents = 0;
    next.scannedMaterials = 0;

    ObjectPathCache pathCache;
    pathCache.reserve(64);
    ForEachActorPrimitiveComponent(actor, [this, &next, &pathCache](SDK::UPrimitiveComponent* component) {
        ApplyToComponent(component, next, nullptr, nullptr, pathCache);
    });

    StoreStats(next);
}

void AssetOverrideManager::ApplyToWorld(SDK::UWorld* world) {
    Stats next = GetStats();
    next.appliedMaterials = 0;
    next.scannedComponents = 0;
    next.scannedMaterials = 0;
    next.unmatched = static_cast<int>(textures.size());

    if (!textures.empty()) {
        std::vector<uint8_t> matchedTargets(textures.size(), 0);
        size_t matchedTargetCount = 0;
        ObjectPathCache pathCache;
        pathCache.reserve(256);

        ForEachPrimitiveComponent(
            world, [this, &next, &matchedTargets, &matchedTargetCount,
                    &pathCache](SDK::UPrimitiveComponent* component) {
                ApplyToComponent(component, next, &matchedTargets, &matchedTargetCount, pathCache);
            }
        );
        next.unmatched = static_cast<int>(textures.size() - matchedTargetCount);
    }

    appliedWorld = world;
    needsApply = false;
    StoreStats(next);
    if (next.files > 0) {
        g_logger.Log(
            "Texture overrides applied: components=%d materials=%d updated=%d unmatched=%d errors=%d",
            next.scannedComponents, next.scannedMaterials, next.appliedMaterials, next.unmatched, next.errors
        );
    }
}
