#pragma once

#include <cstdint>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace SDK {
    class AActor;
    class ACSBloodSimActor;
    class UMaterialInstanceDynamic;
    class UMaterialInterface;
    class UObject;
    class UPrimitiveComponent;
    class UTexture;
    class UTexture2D;
    class UWorld;
}

class AssetOverrideManager {
public:
    struct Stats {
        int files = 0;
        int loaded = 0;
        int appliedMaterials = 0;
        int scannedComponents = 0;
        int scannedMaterials = 0;
        int unmatched = 0;
        int errors = 0;
    };

    static AssetOverrideManager& Get();

    [[nodiscard]] bool Initialize();
    void RequestRefresh();
    void RequestApply();
    void RequestActorApply(SDK::AActor* actor);

    [[nodiscard]] Stats GetStats() const;
    [[nodiscard]] std::filesystem::path GetRootPath() const;

    AssetOverrideManager(const AssetOverrideManager&) = delete;
    AssetOverrideManager& operator=(const AssetOverrideManager&) = delete;

private:
    AssetOverrideManager() = default;

    [[nodiscard]] bool PrepareWorld(SDK::UWorld* world);
    void ApplyNow(SDK::UWorld* world);
    void ApplyToActor(SDK::UWorld* world, SDK::AActor* actor);
    void ApplyToComponentNow(SDK::UPrimitiveComponent* component, int materialIndex, bool resetSource);
    void ApplyToCreatedMaterial(
        SDK::UPrimitiveComponent* component, int materialIndex, SDK::UMaterialInstanceDynamic* dynamicMaterial,
        SDK::UMaterialInterface* explicitSource
    );
    [[nodiscard]] bool ScanFiles();
    void LoadTextures(SDK::UWorld* world);
    void ApplyToWorld(SDK::UWorld* world);
    void ClearTextures();
    void StoreStats(Stats next) const;
    void SortTexturesForLookup();

    static constexpr std::string_view ROOT_FOLDER = "asset_overrides";
    static constexpr size_t TEXTURE_NOT_FOUND = static_cast<size_t>(-1);
    static constexpr size_t LINEAR_TEXTURE_LOOKUP_LIMIT = 64;

    struct FileEntry {
        std::filesystem::path filePath;
        std::string targetPath;
    };

    struct TextureOverride {
        std::string targetPath;
        uint64_t targetHash = 0;
        SDK::UTexture2D* texture = nullptr;
    };

    struct TextureLookupResult {
        size_t index = TEXTURE_NOT_FOUND;
        SDK::UTexture2D* texture = nullptr;
    };

    struct ObjectPathCacheEntry {
        std::string path;
        uint64_t hash = 0;
    };

    using ObjectPathCache = std::unordered_map<const SDK::UObject*, ObjectPathCacheEntry>;

    [[nodiscard]] TextureLookupResult FindTexture(std::string_view targetPath, uint64_t targetHash) const;
    [[nodiscard]] SDK::UMaterialInterface* GetTrackedSourceMaterial(
        SDK::UPrimitiveComponent* component, int materialIndex
    );
    [[nodiscard]] SDK::UMaterialInterface* RebaseSourceMaterial(
        SDK::UPrimitiveComponent* component, int materialIndex, SDK::UMaterialInterface* requestedSource
    );
    void ForgetComponentSourceMaterials(SDK::UPrimitiveComponent* component, int materialIndex);
    void TrackOverriddenMaterial(
        SDK::UPrimitiveComponent* component, int materialIndex, SDK::UMaterialInterface* sourceMaterial,
        SDK::UMaterialInstanceDynamic* dynamicMaterial
    );
    int ApplyBloodDynamicMaterial(
        SDK::UMaterialInstanceDynamic* dynamicMaterial, SDK::UMaterialInterface* sourceMaterial
    );
    int ApplyBloodComponentMaterial(SDK::UPrimitiveComponent* component, int materialIndex);
    int ApplyBloodComputeActor(SDK::ACSBloodSimActor* sim);
    int RepairBloodMaterials();
    int ApplyToMaterialInstance(
        SDK::UMaterialInstanceDynamic* dynamicMaterial, SDK::UMaterialInterface* sourceMaterial,
        std::vector<uint8_t>* matchedTargets, size_t* matchedTargetCount, ObjectPathCache& pathCache,
        bool allowRuntimeBloodTarget = false
    );
    void ApplyToComponentSlot(
        SDK::UPrimitiveComponent* component, int materialIndex, Stats& next, std::vector<uint8_t>* matchedTargets,
        size_t* matchedTargetCount, ObjectPathCache& pathCache
    );
    void ApplyToComponent(
        SDK::UPrimitiveComponent* component, Stats& next, std::vector<uint8_t>* matchedTargets,
        size_t* matchedTargetCount, ObjectPathCache& pathCache
    );

    std::vector<FileEntry> files;
    std::vector<SDK::UTexture2D*> rootedTextures;
    std::vector<TextureOverride> textures;

    struct MaterialSlot {
        SDK::UPrimitiveComponent* component = nullptr;
        int materialIndex = 0;
        int componentObjectIndex = -1;

        bool operator==(const MaterialSlot& other) const noexcept {
            return component == other.component && materialIndex == other.materialIndex &&
                   componentObjectIndex == other.componentObjectIndex;
        }
    };

    struct MaterialSlotHash {
        size_t operator()(const MaterialSlot& slot) const noexcept {
            return std::hash<SDK::UPrimitiveComponent*>{}(slot.component) ^
                   (std::hash<int>{}(slot.materialIndex) + 0x9E3779B9u) ^
                   (std::hash<int>{}(slot.componentObjectIndex) + 0x85EBCA6Bu);
        }
    };

    struct TrackedMaterial {
        SDK::UMaterialInterface* source = nullptr;
        SDK::UMaterialInstanceDynamic* dynamic = nullptr;
    };

    std::unordered_map<MaterialSlot, TrackedMaterial, MaterialSlotHash> trackedMaterials;
    std::vector<MaterialSlot> trackedMaterialSlots;

    mutable Stats stats;
    mutable std::mutex statsMutex;

    bool initialized = false;
    std::atomic<bool> applyQueued{false};
    bool needsScan = true;
    bool needsLoad = true;
    bool needsApply = true;
    SDK::UWorld* loadedWorld = nullptr;
    SDK::UWorld* appliedWorld = nullptr;
};
