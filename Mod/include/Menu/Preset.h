#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "SimpleIni.h"
#include "Menu/Override.h"
#include "Utils/PresetDataBase.h"
#include "Utils/PresetUtils.h"
#include "ConfigManager.h"


enum class PresetFieldType : uint8_t { String, Int, Double, Bool, Vec3, Rotator, Color };

/// Type-erased descriptor for a single preset data field.
/// Points into the owning struct's storage -- no copies, no allocations.
struct PresetFieldDescriptor {
    const char* section;
    const char* key;
    PresetFieldType type;
    void* value;
};

namespace PresetField {

    constexpr PresetFieldDescriptor String(const char* section, const char* key, std::string* value) {
        return {section, key, PresetFieldType::String, value};
    }

    constexpr PresetFieldDescriptor Int(const char* section, const char* key, int* value) {
        return {section, key, PresetFieldType::Int, value};
    }

    constexpr PresetFieldDescriptor Double(const char* section, const char* key, double* value) {
        return {section, key, PresetFieldType::Double, value};
    }

    constexpr PresetFieldDescriptor Bool(const char* section, const char* key, bool* value) {
        return {section, key, PresetFieldType::Bool, value};
    }

    constexpr PresetFieldDescriptor Vec3(const char* section, const char* key, void* value) {
        return {section, key, PresetFieldType::Vec3, value};
    }

    constexpr PresetFieldDescriptor Rotator(const char* section, const char* key, void* value) {
        return {section, key, PresetFieldType::Rotator, value};
    }

    constexpr PresetFieldDescriptor Color(const char* section, const char* key, void* value) {
        return {section, key, PresetFieldType::Color, value};
    }

} // namespace PresetField


struct PresetOverrideDescriptor {
    const char* section;
    OverrideDescriptor field;
};


[[nodiscard]] std::string PresetSectionName(std::string_view prefix, std::string_view section);

void SerializePresetFields(
    std::span<const PresetFieldDescriptor> fields, CSimpleIniA& ini, std::string_view sectionPrefix = {}
);
bool DeserializePresetFields(
    std::span<const PresetFieldDescriptor> fields, const CSimpleIniA& ini, std::string_view sectionPrefix = {},
    std::string* error = nullptr
);
void SerializePresetOverrides(
    std::span<const PresetOverrideDescriptor> fields, CSimpleIniA& ini, std::string_view sectionPrefix = {}
);
bool DeserializePresetOverrides(
    std::span<const PresetOverrideDescriptor> fields, const CSimpleIniA& ini, std::string_view sectionPrefix = {},
    std::string* error = nullptr
);
[[nodiscard]] PresetOperationResult ValidatePresetOverrideValuesForSave(
    std::span<const PresetOverrideDescriptor> fields, std::string_view presetKind
);


template <typename T>
concept HasPresetFields = requires(T& value) { T::GetPresetFields(value); };

template <typename T>
concept HasPresetOverrides = requires(T& value) { T::GetPresetOverrides(value); };

template <typename T>
concept HasCustomSerialize = requires(const T& value, CSimpleIniA& ini, std::string_view prefix) {
    { T::SerializeCustom(value, ini, prefix) };
};

template <typename T>
concept HasCustomDeserialize = requires(T& value, const CSimpleIniA& ini, std::string_view prefix) {
    { T::DeserializeCustom(value, ini, prefix) } -> std::convertible_to<PresetOperationResult>;
};

template <typename T>
concept HasPresetSaveValidation = requires(const T& value) {
    { value.ValidateForSave() } -> std::convertible_to<PresetOperationResult>;
};

template <typename T>
concept HasContextualPresetSaveValidation = requires(const T& value, const std::filesystem::path& appDataRoot) {
    { value.ValidateForSave(appDataRoot) } -> std::convertible_to<PresetOperationResult>;
};

template <typename T> [[nodiscard]] PresetOperationResult ValidatePresetDataForSave(const T& value) {
    if constexpr (std::is_base_of_v<PresetDataBase, T>) {
        if (value.name.empty()) return {.error = "Enter a preset name"};
        if (!PresetUtils::IsSafeIniValue(value.name)) return {.error = "Preset names must fit on one line"};
        if (value.id.empty()) return {.error = "This preset is incomplete"};
        if (!PresetUtils::IsSafeIniValue(value.id)) return {.error = "This preset is incomplete"};
    }
    if constexpr (HasPresetSaveValidation<T>)
        return value.ValidateForSave();
    else
        return {.success = true};
}

template <typename T>
[[nodiscard]] PresetOperationResult ValidatePresetForSave(const T& value, const std::filesystem::path& appDataRoot) {
    auto validation = ValidatePresetDataForSave(value);
    if (!validation) return validation;
    if constexpr (HasContextualPresetSaveValidation<T>)
        return value.ValidateForSave(appDataRoot);
    else
        return validation;
}

[[nodiscard]] inline std::atomic<uint64_t>& PresetCatalogRevisionCounter() noexcept {
    static std::atomic<uint64_t> revision{1};
    return revision;
}

[[nodiscard]] inline uint64_t GetPresetCatalogRevision() noexcept {
    return PresetCatalogRevisionCounter().load(std::memory_order_acquire);
}

inline void BumpPresetCatalogRevision() noexcept {
    PresetCatalogRevisionCounter().fetch_add(1, std::memory_order_acq_rel);
}


template <typename DataType> class PresetSerializer {
    struct CatalogSnapshot {
        uint64_t revision = 0;
        PresetUtils::PresetTreeNode tree;
        std::unordered_map<std::string, std::filesystem::path> pathsById;
        std::unordered_set<std::string> duplicateIds;
    };

    inline static std::mutex catalogMutex_;
    inline static std::unordered_map<std::filesystem::path, CatalogSnapshot> catalogsByRoot_;

public:
    using Data = DataType;

    static constexpr const char* K_PRESETS_SUBDIR = DataType::K_PRESETS_SUBDIR;
    static constexpr const char* K_PRESET_KIND = DataType::K_PRESET_KIND;

    [[nodiscard]] static uint64_t GetCatalogRevision() noexcept {
        return catalogRevision_.load(std::memory_order_acquire);
    }

    static void InvalidateCatalog() noexcept { BumpCatalogRevision(); }

    static const std::filesystem::path& GetPresetsDirectory() {
        static const std::filesystem::path dir = [] {
            auto path = ConfigManager::GetAppDataPath() / K_PRESETS_SUBDIR;
            (void)PresetUtils::EnsureDirectoryResult(path);
            return path;
        }();
        return dir;
    }

    static std::filesystem::path GetPresetsDirectory(const std::filesystem::path& appDataRoot) {
        return appDataRoot / K_PRESETS_SUBDIR;
    }

    static PresetLoadResult<DataType> LoadFromFileResult(const std::filesystem::path& path) {
        return LoadFromFileResult(path, ConfigManager::GetAppDataPath());
    }

    static PresetLoadResult<DataType> LoadFromFileResult(
        const std::filesystem::path& path, const std::filesystem::path& appDataRoot
    ) {
        const auto directory = GetPresetsDirectory(appDataRoot);
        const auto resolvedPath = path.is_absolute() ? path : directory / path;
        auto content = PresetUtils::LoadStringFromFileResult(resolvedPath, directory);
        return DeserializeFileReadResult(resolvedPath, content);
    }

    static PresetLoadResult<DataType> LoadFromFileResult(
        const std::filesystem::path& path, const std::filesystem::path& appDataRoot, PresetResolveContext& context
    ) {
        const auto directory = GetPresetsDirectory(appDataRoot);
        const auto resolvedPath = path.is_absolute() ? path : directory / path;
        const auto& content = PresetUtils::LoadStringFromFileCached(resolvedPath, directory, context);
        return DeserializeFileReadResult(resolvedPath, content);
    }

    static PresetUtils::PresetTreeNode ListPresetsTree() { return ListPresetsTree(ConfigManager::GetAppDataPath()); }

    static PresetUtils::PresetTreeNode ListPresetsTree(const std::filesystem::path& appDataRoot) {
        const std::scoped_lock lock(catalogMutex_);
        return GetCatalogSnapshot(appDataRoot).tree;
    }

    static PresetOperationResult FindPresetById(
        std::string_view id, const std::filesystem::path& appDataRoot = ConfigManager::GetAppDataPath()
    ) {
        PresetOperationResult result;
        if (id.empty()) {
            result.error = "Choose a saved preset";
            return result;
        }

        const std::scoped_lock lock(catalogMutex_);
        const auto& catalog = GetCatalogSnapshot(appDataRoot);

        const std::string identity(id);
        if (catalog.duplicateIds.contains(identity)) {
            result.error = "Two saved presets conflict. Delete or rename one of them.";
            return result;
        }
        if (const auto found = catalog.pathsById.find(identity); found != catalog.pathsById.end()) {
            result.success = true;
            result.path = found->second;
            return result;
        }
        result.error = "The referenced preset is no longer available";
        return result;
    }

    static PresetOperationResult DeletePresetResult(const std::filesystem::path& path) {
        return DeletePresetResult(path, ConfigManager::GetAppDataPath());
    }

    static PresetOperationResult DeletePresetResult(
        const std::filesystem::path& path, const std::filesystem::path& appDataRoot
    ) {
        const auto directory = GetPresetsDirectory(appDataRoot);
        const auto resolvedPath = path.is_absolute() ? path : directory / path;
        auto deleted = PresetUtils::DeletePresetResult(resolvedPath, directory);
        if (deleted.success) BumpCatalogRevision();
        return deleted;
    }

    static PresetOperationResult SaveToFileResult(const std::filesystem::path& path, const DataType& data) {
        return SaveToFileResult(path, data, ConfigManager::GetAppDataPath(), false);
    }

    static PresetOperationResult SaveToFileResult(
        const std::filesystem::path& path, const DataType& data, bool overwrite
    ) {
        return SaveToFileResult(path, data, ConfigManager::GetAppDataPath(), overwrite);
    }

    static PresetOperationResult SaveToFileResult(
        const std::filesystem::path& path, const DataType& data, const std::filesystem::path& appDataRoot,
        bool overwrite = false
    ) {
        static std::mutex saveMutex;
        const std::scoped_lock saveLock(saveMutex);

        const auto directory = GetPresetsDirectory(appDataRoot);
        const auto resolvedPath = path.is_absolute() ? path : directory / path;

        DataType persisted = data;
        if (persisted.name.empty()) persisted.name = PresetUtils::PathToUtf8(resolvedPath.stem());

        if (overwrite) {
            std::error_code existsError;
            const bool destinationExists = std::filesystem::exists(resolvedPath, existsError);
            if (existsError) {
                return {
                    .path = resolvedPath,
                    .error = "Couldn't check that preset name",
                };
            }
            if (destinationExists) {
                auto existing = LoadFromFileResult(resolvedPath, appDataRoot);
                if (!existing) {
                    return {
                        .path = resolvedPath,
                        .error = "This preset is damaged. Delete it before reusing the name: " + existing.error,
                    };
                }
                persisted.id = std::move(existing.value.id);
            } else {
                persisted.id = PresetUtils::GeneratePresetId();
            }
        } else if (persisted.id.empty()) {
            persisted.id = PresetUtils::GeneratePresetId();
        }

        auto dataValidation = ValidatePresetForSave(persisted, appDataRoot);
        if (!dataValidation) {
            dataValidation.path = resolvedPath;
            return dataValidation;
        }

        const auto identity = FindPresetById(persisted.id, appDataRoot);
        if (identity.success && !PresetUtils::PresetPathsEqual(identity.path, resolvedPath)) {
            return {
                .success = false,
                .path = resolvedPath,
                .error = "A conflicting preset already exists. Refresh the list and try again.",
            };
        }
        if (!identity.success && identity.error.starts_with("Two saved presets conflict")) {
            return {
                .success = false,
                .path = resolvedPath,
                .error = std::move(identity.error),
            };
        }

        const std::string serialized = SerializeToIni(persisted);
        const auto disposition = overwrite ? PresetUtils::AtomicSaveDisposition::ReplaceExisting
                                           : PresetUtils::AtomicSaveDisposition::CreateNew;
        auto saved = PresetUtils::SaveStringToFileAtomic(resolvedPath, serialized, directory, disposition);
        if (saved.success) {
            BumpCatalogRevision();
            saved.id = persisted.id;
        }
        return saved;
    }

    static PresetOperationResult SavePresetByNameResult(const std::string& name, const DataType& data) {
        return SavePresetByNameResult(name, data, ConfigManager::GetAppDataPath(), false);
    }

    static PresetOperationResult SavePresetByNameResult(const std::string& name, const DataType& data, bool overwrite) {
        return SavePresetByNameResult(name, data, ConfigManager::GetAppDataPath(), overwrite);
    }

    static PresetOperationResult SavePresetByNameResult(
        const std::string& name, const DataType& data, const std::filesystem::path& appDataRoot, bool overwrite = false
    ) {
        std::filesystem::path relative;
        std::string error;
        if (!PresetUtils::TryNormalizePresetRelativePath(name, relative, error))
            return {.success = false, .error = std::move(error)};

        DataType persisted = data;
        auto displayPath = relative;
        displayPath.replace_extension();
        persisted.name = PresetUtils::PathToUtf8(displayPath);
        return SaveToFileResult(GetPresetsDirectory(appDataRoot) / relative, persisted, appDataRoot, overwrite);
    }

    static void SerializeIntoIni(const DataType& data, CSimpleIniA& ini, std::string_view sectionPrefix = {}) {
        const auto metadataSection = PresetSectionName(sectionPrefix, "Preset");
        ini.SetValue(metadataSection.c_str(), "name", data.name.c_str());
        ini.SetValue(metadataSection.c_str(), "version", K_CURRENT_PRESET_VERSION_TEXT);
        ini.SetValue(metadataSection.c_str(), "type", K_PRESET_KIND);
        ini.SetValue(metadataSection.c_str(), "id", data.id.c_str());

        if constexpr (HasPresetFields<DataType>) {
            auto fields = DataType::GetPresetFields(const_cast<DataType&>(data));
            SerializePresetFields(fields, ini, sectionPrefix);
        }

        if constexpr (HasPresetOverrides<DataType>)
            SerializePresetOverrides(DataType::GetPresetOverrides(const_cast<DataType&>(data)), ini, sectionPrefix);

        if constexpr (HasCustomSerialize<DataType>) DataType::SerializeCustom(data, ini, sectionPrefix);
    }

    static std::string SerializeToIni(const DataType& data) {
        CSimpleIniA ini;
        ini.SetUnicode(false);
        SerializeIntoIni(data, ini);
        std::string output;
        ini.Save(output);
        return output;
    }

    static PresetLoadResult<DataType> DeserializeEmbedded(const CSimpleIniA& ini, std::string_view sectionPrefix) {
        PresetLoadResult<DataType> result;
        auto& value = result.value;
        const auto metadataSection = PresetSectionName(sectionPrefix, "Preset");

        if (ini.GetLongValue(metadataSection.c_str(), "version", 0) != K_CURRENT_PRESET_VERSION) {
            result.error = "This preset was made by an unsupported mod version or is damaged";
            return result;
        }

        const char* kind = ini.GetValue(metadataSection.c_str(), "type", nullptr);
        if (!kind || std::string_view(kind) != K_PRESET_KIND) {
            result.error = "This preset belongs to a different feature";
            return result;
        }

        const char* name = ini.GetValue(metadataSection.c_str(), "name", nullptr);
        const char* id = ini.GetValue(metadataSection.c_str(), "id", nullptr);
        if (!name || !name[0] || !id || !id[0]) {
            result.error = "This preset is incomplete";
            return result;
        }

        value.name = name;
        value.id = id;
        std::string parseError;

        if constexpr (HasPresetFields<DataType>) {
            auto fields = DataType::GetPresetFields(value);
            if (!DeserializePresetFields(fields, ini, sectionPrefix, &parseError)) {
                result.error = std::move(parseError);
                return result;
            }
        }

        if constexpr (HasPresetOverrides<DataType>) {
            if (!DeserializePresetOverrides(DataType::GetPresetOverrides(value), ini, sectionPrefix, &parseError)) {
                result.error = std::move(parseError);
                return result;
            }
        }

        if constexpr (HasCustomDeserialize<DataType>) {
            auto custom = DataType::DeserializeCustom(value, ini, sectionPrefix);
            if (!custom) {
                result.error = std::move(custom.error);
                return result;
            }
        }

        auto validation = ValidatePresetDataForSave(value);
        if (!validation.success) {
            result.error = std::move(validation.error);
            return result;
        }

        result.success = true;
        return result;
    }

    static PresetLoadResult<DataType> DeserializeFromIniResult(const std::string& iniContent) {
        PresetLoadResult<DataType> result;
        if (iniContent.size() > PresetUtils::K_MAX_PRESET_FILE_SIZE_BYTES) {
            result.error = "This preset is too large to open";
            return result;
        }
        if (!PresetUtils::IsValidUtf8(iniContent) || iniContent.find('\0') != std::string::npos) {
            result.error = "This preset contains text that cannot be used";
            return result;
        }
        return DeserializeIniData(iniContent);
    }

    template <typename ResolvedType, typename ResolveFn>
    static PresetResolveResult<ResolvedType> ResolveLinkAs(
        const PresetLink<DataType>& link, const std::filesystem::path& appDataRoot, PresetResolveContext& context,
        ResolveFn&& resolveLoaded
    ) {
        PresetResolveResult<ResolvedType> result;
        if (IsEmptyPresetLink(link)) {
            result.success = true;
            return result;
        }

        if (const auto* copy = GetPresetCopy(link)) {
            if (auto validation = ValidatePresetDataForSave(*copy); !validation.success) {
                result.error = "A copied preset inside this preset is damaged: " + validation.error;
                return result;
            }
            return std::invoke(std::forward<ResolveFn>(resolveLoaded), *copy, context);
        }

        const auto& reference = std::get<PresetReference>(link);
        const PresetOperationResult located = FindPresetById(reference.id, appDataRoot);
        if (!located.success) {
            result.error = located.error;
            return result;
        }

        auto loaded = LoadFromFileResult(located.path, appDataRoot, context);
        if (!loaded.success) {
            result.path = located.path;
            result.error = loaded.error;
            return result;
        }

        result = std::invoke(std::forward<ResolveFn>(resolveLoaded), loaded.value, context);
        if (result.path.empty()) result.path = located.path;
        return result;
    }

    static PresetResolveResult<DataType> ResolveLink(
        const PresetLink<DataType>& link, const std::filesystem::path& appDataRoot, PresetResolveContext& context
    ) {
        return ResolveLinkAs<DataType>(link, appDataRoot, context, [](const DataType& value, PresetResolveContext&) {
            PresetResolveResult<DataType> result;
            result.success = true;
            result.value = value;
            return result;
        });
    }

    static PresetResolveResult<DataType> ResolveLink(const PresetLink<DataType>& link, PresetResolveContext& context) {
        return ResolveLink(link, ConfigManager::GetAppDataPath(), context);
    }

private:
    inline static std::atomic<uint64_t> catalogRevision_{1};

    static void BumpCatalogRevision() noexcept {
        catalogRevision_.fetch_add(1, std::memory_order_acq_rel);
        BumpPresetCatalogRevision();
    }

    static CatalogSnapshot& GetCatalogSnapshot(const std::filesystem::path& appDataRoot) {
        const auto directory = GetPresetsDirectory(appDataRoot);
        const auto rootKey = directory.lexically_normal();
        const uint64_t revision = GetCatalogRevision();

        auto& catalog = catalogsByRoot_[rootKey];
        if (catalog.revision == revision) return catalog;

        catalog = {};
        catalog.revision = revision;
        catalog.tree = PresetUtils::ListPresetsRecursive(directory, K_PRESET_KIND);

        const auto indexPresets = [&catalog](auto&& self, const PresetUtils::PresetTreeNode& node) -> void {
            for (const auto& entry : node.presets) {
                if (!entry.valid || entry.id.empty() || catalog.duplicateIds.contains(entry.id)) continue;
                const auto inserted = catalog.pathsById.try_emplace(entry.id, entry.path);
                if (!inserted.second) {
                    catalog.pathsById.erase(entry.id);
                    catalog.duplicateIds.insert(entry.id);
                }
            }
            for (const auto& child : node.children)
                self(self, child);
        };
        indexPresets(indexPresets, catalog.tree);
        return catalog;
    }

    static PresetLoadResult<DataType> DeserializeFileReadResult(
        const std::filesystem::path& resolvedPath, const PresetFileReadResult& content
    ) {
        if (!content) {
            PresetLoadResult<DataType> failure;
            failure.path = resolvedPath;
            failure.error = content.error;
            return failure;
        }

        auto result = DeserializeIniData(content.content);
        result.path = resolvedPath;
        return result;
    }

    static PresetLoadResult<DataType> DeserializeIniData(const std::string& content) {
        CSimpleIniA ini;
        ini.SetUnicode(false);
        if (ini.LoadData(content) >= 0) return DeserializeEmbedded(ini, {});
        PresetLoadResult<DataType> result;
        result.error = "This preset file is damaged";
        return result;
    }
};

template <typename DataType>
void SerializePresetLink(const PresetLink<DataType>& link, CSimpleIniA& ini, std::string_view sectionPrefix) {
    const std::string section(sectionPrefix);
    if (IsEmptyPresetLink(link)) {
        ini.SetValue(section.c_str(), "mode", "empty");
        return;
    }

    if (const auto* copy = GetPresetCopy(link)) {
        ini.SetValue(section.c_str(), "mode", "copy");
        PresetSerializer<DataType>::SerializeIntoIni(*copy, ini, PresetSectionName(sectionPrefix, "Copy"));
        return;
    }

    const auto& reference = std::get<PresetReference>(link);
    ini.SetValue(section.c_str(), "mode", "reference");
    ini.SetValue(section.c_str(), "preset_id", reference.id.c_str());
}

template <typename DataType>
PresetLoadResult<PresetLink<DataType>> DeserializePresetLink(const CSimpleIniA& ini, std::string_view sectionPrefix) {
    PresetLoadResult<PresetLink<DataType>> result;
    const std::string section(sectionPrefix);
    if (section.empty()) {
        result.error = "This preset is missing one of its saved parts";
        return result;
    }

    const char* mode = ini.GetValue(section.c_str(), "mode", nullptr);
    if (!mode) {
        result.error = "This preset is missing one of its saved parts";
        return result;
    }
    const std::string_view modeText(mode);
    if (modeText == "empty") {
        result.value = std::monostate{};
    } else if (modeText == "copy") {
        const std::string copySection = PresetSectionName(sectionPrefix, "Copy");
        auto embedded = PresetSerializer<DataType>::DeserializeEmbedded(ini, copySection);
        if (!embedded.success) {
            result.error = embedded.error;
            return result;
        }
        result.value = MakePresetCopyLink(std::move(embedded.value));
    } else if (modeText == "reference") {
        const char* id = ini.GetValue(section.c_str(), "preset_id", nullptr);
        if (!id || !id[0]) {
            result.error = "The referenced preset is no longer available";
            return result;
        }
        if (!PresetUtils::IsSafeIniValue(id)) {
            result.error = "One of the saved parts in this preset is damaged";
            return result;
        }
        result.value = PresetReference{id};
    } else {
        result.error = "One of the saved parts in this preset is not supported by this mod version";
        return result;
    }

    result.success = true;
    return result;
}
