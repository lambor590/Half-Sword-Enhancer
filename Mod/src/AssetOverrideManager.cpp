#include "Utils/AssetOverrideManager.h"

#include <algorithm>
#include <cctype>
#include <system_error>

#include "ConfigManager.h"
#include "Hooks/GameHook.h"
#include "Logger.h"
#include "Menu/EventBus.h"
#include "Utils/PresetUtils.h"
#include "SDK/HSComputeShaders_classes.hpp"
#include "SDK/Engine_classes.hpp"

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

    template <typename Func> void ForEachPrimitiveComponent(SDK::UWorld* world, Func&& func) {
        if (!world) return;
        for (auto* level : world->Levels) {
            if (!level) continue;
            for (auto* actor : level->Actors) {
                if (!actor) continue;
                SDK::TArray<SDK::UActorComponent*> components =
                    actor->K2_GetComponentsByClass(SDK::UPrimitiveComponent::StaticClass());
                for (auto* component : components) {
                    if (component && component->IsA(SDK::UPrimitiveComponent::StaticClass()))
                        func(static_cast<SDK::UPrimitiveComponent*>(component));
                }
            }
        }
    }

    template <typename Func> void ForEachBloodActor(SDK::UWorld* world, Func&& func) {
        if (!world) return;
        for (auto* level : world->Levels) {
            if (!level) continue;
            for (auto* actor : level->Actors) {
                if (actor && actor->IsA(SDK::ACSBloodSimActor::StaticClass()))
                    func(static_cast<SDK::ACSBloodSimActor*>(actor));
            }
        }
    }

    template <typename LookupTexture, typename Func>
    int ApplyMatchedTextureParameters(
        SDK::UMaterialInterface* material, LookupTexture&& lookupTexture, std::unordered_set<size_t>* matchedTargets,
        Func&& func
    ) {
        auto* instance = AsMaterialInstance(material);
        if (!instance) return 0;

        int updates = 0;
        for (auto* current = instance; current;) {
            for (int p = 0; p < current->TextureParameterValues.Num(); ++p) {
                auto& value = current->TextureParameterValues[p];
                if (!value.ParameterValue) continue;

                auto matchedPath = PresetUtils::ObjectToAbsolutePath(value.ParameterValue);
                const auto match = lookupTexture(matchedPath, Fnv1a(matchedPath));
                if (!match.texture) continue;

                if (matchedTargets) matchedTargets->insert(match.index);
                updates += func(value.ParameterInfo, match.texture) ? 1 : 0;
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

    EventBus::Get().Subscribe(GameEvent::BeginFight, this, [this](const RuntimeContextSnapshot& runtime) {
        if (runtime.world) ApplyNow(runtime.world);
    });
    EventBus::Get().Subscribe(GameEvent::InAbyss, this, [this](const RuntimeContextSnapshot& runtime) {
        if (runtime.world) ApplyNow(runtime.world);
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
    if (files.empty()) return;
    GameHook::QueueAction([this](const RuntimeContextSnapshot& runtime) {
        needsApply = true;
        if (runtime.world) ApplyNow(runtime.world);
    });
}

void AssetOverrideManager::ApplyNow(SDK::UWorld* world) {
    if (!world) return;
    if (needsScan) (void)ScanFiles();
    if (loadedWorld != world) {
        needsLoad = true;
        needsApply = true;
        repairedBloodSlots.clear();
        if (appliedWorld && appliedWorld != world) {
            sourceMaterials.clear();
            touchedSlots.clear();
        }
    }
    if (needsLoad) LoadTextures(world);
    if (needsApply) ApplyToWorld(world);
    if (!sourceMaterials.empty()) RepairBloodMaterials(world);
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
    repairedBloodSlots.clear();

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
        g_logger.Log("Texture override target: %s -> %s", entry.path().string().c_str(), target.c_str());
        files.push_back({entry.path(), std::move(target)});
    }

    next.files = static_cast<int>(files.size());
    StoreStats(next);
    needsScan = false;
    needsLoad = true;
    needsApply = true;
    ++generation;
    g_logger.Log("Found %d texture override file(s)", next.files);
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
        auto existing = std::ranges::find_if(textures, [&file, targetHash](const TextureOverride& overrideEntry) {
            return overrideEntry.targetHash == targetHash && overrideEntry.targetPath == file.targetPath;
        });
        if (existing != textures.end()) {
            existing->texture = texture;
        } else {
            textures.push_back({file.targetPath, targetHash, texture});
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

void AssetOverrideManager::ApplyToWorld(SDK::UWorld* world) {
    Stats next = GetStats();
    next.appliedMaterials = 0;
    next.scannedComponents = 0;
    next.scannedMaterials = 0;
    next.unmatched = static_cast<int>(textures.size());

    if (!textures.empty()) {
        std::unordered_set<size_t> matchedTargets;
        auto lookupTexture = [this](std::string_view targetPath, uint64_t targetHash) {
            return FindTexture(targetPath, targetHash);
        };

        touchedSlots.clear();
        ForEachPrimitiveComponent(world, [this, &next, &matchedTargets, &lookupTexture](
                                      SDK::UPrimitiveComponent* component
                                  ) {
            if (!component) return;

            ++next.scannedComponents;
            const int materialCount = component->GetNumMaterials();
            for (int materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
                ++next.scannedMaterials;

                auto* material = component->GetMaterial(materialIndex);
                auto* dynamicMaterial = material && material->IsA(SDK::UMaterialInstanceDynamic::StaticClass())
                                            ? static_cast<SDK::UMaterialInstanceDynamic*>(material)
                                            : nullptr;
                const MaterialSlot slotKey{component, materialIndex};
                auto sourceIt = sourceMaterials.find(slotKey);
                auto* sourceMaterial = sourceIt != sourceMaterials.end() ? sourceIt->second : material;
                bool sourceStored = sourceIt != sourceMaterials.end();
                bool touchedRecorded = false;

                next.appliedMaterials += ApplyMatchedTextureParameters(
                    sourceMaterial, lookupTexture, &matchedTargets,
                    [this, component, materialIndex, sourceMaterial, &sourceStored, &dynamicMaterial, &touchedRecorded,
                     &next](const SDK::FMaterialParameterInfo& parameter, SDK::UTexture2D* texture) {
                        if (dynamicMaterial && dynamicMaterial->K2_GetTextureParameterValueByInfo(parameter) == texture)
                            return false;

                        if (!sourceStored) {
                            sourceMaterials[{component, materialIndex}] = sourceMaterial;
                            sourceStored = true;
                        }
                        if (!touchedRecorded) {
                            touchedSlots.push_back({component, materialIndex, sourceMaterial});
                            touchedRecorded = true;
                        }

                        if (!dynamicMaterial)
                            dynamicMaterial =
                                component->CreateDynamicMaterialInstance(materialIndex, sourceMaterial, SDK::FName());
                        if (!dynamicMaterial) {
                            ++next.errors;
                            return false;
                        }

                        dynamicMaterial->SetTextureParameterValueByInfo(parameter, texture);
                        return true;
                    }
                );
            }
        });
        next.unmatched = static_cast<int>(textures.size() - matchedTargets.size());
    }

    appliedWorld = world;
    appliedGeneration = generation;
    repairedBloodWorld = nullptr;
    repairedBloodSlots.clear();
    needsApply = false;
    StoreStats(next);
    if (next.files > 0) {
        g_logger.Log(
            "Texture overrides applied: components=%d materials=%d updated=%d unmatched=%d errors=%d",
            next.scannedComponents, next.scannedMaterials, next.appliedMaterials, next.unmatched, next.errors
        );
    }
}

void AssetOverrideManager::RepairBloodMaterials(SDK::UWorld* world) {
    if (!world) return;
    if (repairedBloodWorld == world && repairedBloodGeneration == appliedGeneration) return;
    auto lookupTexture = [this](std::string_view targetPath, uint64_t targetHash) {
        return FindTexture(targetPath, targetHash);
    };
    static const SDK::FMaterialParameterInfo bloodRt = [] {
        SDK::FMaterialParameterInfo parameter{};
        parameter.Name = SDK::BasicFilesImpleUtils::StringToName(L"BloodRT");
        return parameter;
    }();

    ForEachBloodActor(world, [this, &lookupTexture](SDK::ACSBloodSimActor* actor) {
        if (!actor->BoundMesh) return;

        for (const auto& source : touchedSlots) {
            if (source.component != actor->BoundMesh) continue;
            if (source.materialIndex < 0 || source.materialIndex >= actor->BoundMeshMatInstances.Num()) continue;

            const auto slotKey = reinterpret_cast<uintptr_t>(actor) ^ static_cast<uintptr_t>(source.materialIndex);
            if (!repairedBloodSlots.insert(slotKey).second) continue;

            auto* oldBloodMaterial = actor->BoundMeshMatInstances[source.materialIndex];
            auto* bloodTexture =
                oldBloodMaterial ? oldBloodMaterial->K2_GetTextureParameterValueByInfo(bloodRt) : nullptr;
            auto* repaired =
                actor->BoundMesh->CreateDynamicMaterialInstance(source.materialIndex, source.material, SDK::FName());
            if (!repaired) continue;

            if (bloodTexture) repaired->SetTextureParameterValueByInfo(bloodRt, bloodTexture);
            ApplyMatchedTextureParameters(
                source.material, lookupTexture, nullptr,
                [repaired](const SDK::FMaterialParameterInfo& parameter, SDK::UTexture2D* texture) {
                    repaired->SetTextureParameterValueByInfo(parameter, texture);
                    return false;
                }
            );
            actor->BoundMeshMatInstances[source.materialIndex] = repaired;
            if (source.materialIndex == 0) actor->BoundMeshMatInstance = repaired;
        }
    });
    repairedBloodWorld = world;
    repairedBloodGeneration = appliedGeneration;
}
