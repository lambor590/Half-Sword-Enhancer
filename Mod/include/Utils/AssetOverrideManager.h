#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SDK {
    class UMaterialInterface;
    class UPrimitiveComponent;
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
    void ApplyNow(SDK::UWorld* world);

    [[nodiscard]] Stats GetStats() const;
    [[nodiscard]] std::filesystem::path GetRootPath() const;

    AssetOverrideManager(const AssetOverrideManager&) = delete;
    AssetOverrideManager& operator=(const AssetOverrideManager&) = delete;

private:
    AssetOverrideManager() = default;

    [[nodiscard]] bool ScanFiles();
    void LoadTextures(SDK::UWorld* world);
    void ApplyToWorld(SDK::UWorld* world);
    void ClearTextures();
    void StoreStats(Stats next) const;
    void RepairBloodMaterials(SDK::UWorld* world);
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

    [[nodiscard]] TextureLookupResult FindTexture(std::string_view targetPath, uint64_t targetHash) const;

    std::vector<FileEntry> files;
    std::vector<SDK::UTexture2D*> rootedTextures;
    std::vector<TextureOverride> textures;

    struct SlotSource {
        SDK::UPrimitiveComponent* component = nullptr;
        int materialIndex = 0;
        SDK::UMaterialInterface* material = nullptr;
    };

    struct MaterialSlot {
        SDK::UPrimitiveComponent* component = nullptr;
        int materialIndex = 0;

        bool operator==(const MaterialSlot& other) const noexcept {
            return component == other.component && materialIndex == other.materialIndex;
        }
    };

    struct MaterialSlotHash {
        size_t operator()(const MaterialSlot& slot) const noexcept {
            return std::hash<SDK::UPrimitiveComponent*>{}(slot.component) ^
                   (std::hash<int>{}(slot.materialIndex) + 0x9E3779B9u);
        }
    };

    std::unordered_map<MaterialSlot, SDK::UMaterialInterface*, MaterialSlotHash> sourceMaterials;
    std::vector<SlotSource> touchedSlots;

    mutable Stats stats;
    mutable std::mutex statsMutex;

    bool initialized = false;
    bool needsScan = true;
    bool needsLoad = true;
    bool needsApply = true;
    uint32_t generation = 0;
    uint32_t appliedGeneration = 0;
    uint32_t repairedBloodGeneration = 0;
    SDK::UWorld* loadedWorld = nullptr;
    SDK::UWorld* appliedWorld = nullptr;
    SDK::UWorld* repairedBloodWorld = nullptr;
    std::unordered_set<uintptr_t> repairedBloodSlots;
};
