#pragma once

#include <string>
#include <filesystem>

#include "Utils/PresetUtils.h"
#include "ConfigManager.h"

template<typename Derived, typename DataT>
class PresetSerializerBase {
public:
    static const std::filesystem::path& GetPresetsDirectory() {
        static std::filesystem::path dir = PresetUtils::EnsureDirectory(
            ConfigManager::GetAppDataPath() / Derived::kPresetsSubdir);
        return dir;
    }

    static DataT LoadFromFile(const std::filesystem::path& path) {
        DataT result;
        std::string content = PresetUtils::LoadStringFromFile(path);
        if (content.empty()) {
            result.error = "Cannot open file: " + path.string();
            return result;
        }
        return Derived::DeserializeFromIni(content);
    }

    static PresetUtils::PresetTreeNode ListPresetsTree() {
        return PresetUtils::ListPresetsRecursive(GetPresetsDirectory());
    }

    static bool DeletePreset(const std::filesystem::path& path) {
        return PresetUtils::DeletePreset(path);
    }

    static bool SaveToFile(const std::filesystem::path& path, const DataT& data) {
        return PresetUtils::SaveStringToFile(path, Derived::SerializeToIni(data));
    }

    static bool SavePresetByName(const std::string& name, const DataT& data) {
        auto [folder, filename] = PresetUtils::SanitizePresetPath(name);
        auto dir = GetPresetsDirectory();
        if (!folder.empty()) dir /= folder;
        PresetUtils::EnsureDirectory(dir);
        return SaveToFile(dir / (filename + ".ini"), data);
    }
};
