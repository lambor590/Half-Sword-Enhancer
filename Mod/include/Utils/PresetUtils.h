#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <cstdio>
#include <Windows.h>
#include <shellapi.h>

#include "ConfigManager.h"
#include "SDK/CoreUObject_classes.hpp"

struct PresetListEntry {
    std::string name;
    std::string filename;
    std::filesystem::path path;
};

namespace PresetUtils {

    inline void OpenInExplorer(const std::filesystem::path& dir) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        ShellExecuteW(NULL, L"open", dir.wstring().c_str(), NULL, NULL, SW_SHOWDEFAULT);
    }

    [[nodiscard]] inline std::string ObjectToAbsolutePath(const SDK::UObject* obj) {
        if (!obj) return "";
        std::string result = obj->GetName();
        for (auto* outer = obj->Outer; outer; outer = outer->Outer) {
            if (!outer->Outer)
                result = outer->Name.GetRawString() + "." + result;
            else
                result = outer->GetName() + "." + result;
        }
        return result;
    }

    inline std::string VecToString(const SDK::FVector& v) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%.6g,%.6g,%.6g", v.X, v.Y, v.Z);
        return buf;
    }

    inline SDK::FVector StringToVec(const char* str, const SDK::FVector& def = {1.0, 1.0, 1.0}) {
        if (!str || !str[0]) return def;
        SDK::FVector v = def;
        sscanf_s(str, "%lf,%lf,%lf", &v.X, &v.Y, &v.Z);
        return v;
    }

    inline std::string RotToString(const SDK::FRotator& r) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%.6g,%.6g,%.6g", r.Pitch, r.Yaw, r.Roll);
        return buf;
    }

    inline SDK::FRotator StringToRot(const char* str, const SDK::FRotator& def = {0.0, 0.0, 0.0}) {
        if (!str || !str[0]) return def;
        SDK::FRotator r = def;
        sscanf_s(str, "%lf,%lf,%lf", &r.Pitch, &r.Yaw, &r.Roll);
        return r;
    }

    inline std::string ColorToString(const SDK::FLinearColor& c) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%.4g,%.4g,%.4g,%.4g", c.R, c.G, c.B, c.A);
        return buf;
    }

    inline SDK::FLinearColor StringToColor(const char* str, SDK::FLinearColor def) {
        if (!str || !str[0]) return def;
        SDK::FLinearColor c = def;
        sscanf_s(str, "%f,%f,%f,%f", &c.R, &c.G, &c.B, &c.A);
        return c;
    }

    inline bool IsDefaultColor(const SDK::FLinearColor& c, const SDK::FLinearColor& def) {
        return std::abs(c.R - def.R) < 1e-3f && std::abs(c.G - def.G) < 1e-3f && std::abs(c.B - def.B) < 1e-3f &&
               std::abs(c.A - def.A) < 1e-3f;
    }

    inline std::string DoubleOverrideToString(bool enabled, double value) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d,%.6g", enabled ? 1 : 0, value);
        return buf;
    }

    inline std::string IntOverrideToString(bool enabled, int value) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d,%d", enabled ? 1 : 0, value);
        return buf;
    }

    inline void ParseDoubleOverride(const char* str, bool& enabled, double& value) {
        if (!str || !str[0]) {
            enabled = false;
            value = 0.0;
            return;
        }
        int en = 0;
        sscanf_s(str, "%d,%lf", &en, &value);
        enabled = en != 0;
    }

    inline void ParseIntOverride(const char* str, bool& enabled, int& value) {
        if (!str || !str[0]) {
            enabled = false;
            value = 0;
            return;
        }
        int en = 0;
        sscanf_s(str, "%d,%d", &en, &value);
        enabled = en != 0;
    }

    inline void ParseBoolOverride(const char* str, bool& enabled, bool& value) {
        if (!str || !str[0]) {
            enabled = false;
            value = false;
            return;
        }
        int en = 0, val = 0;
        sscanf_s(str, "%d,%d", &en, &val);
        enabled = en != 0;
        value = val != 0;
    }

    [[nodiscard]] inline std::string SanitizeFilename(std::string_view name) noexcept {
        std::string result;
        result.reserve(name.size());
        for (char c : name) {
            if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' ||
                c == '*' || c < 32)
                result += '_';
            else
                result += c;
        }
        while (!result.empty() && (result.back() == ' ' || result.back() == '.'))
            result.pop_back();
        if (result.empty()) result = "preset";
        return result;
    }

    inline std::filesystem::path EnsureDirectory(const std::filesystem::path& dir) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir;
    }

    [[nodiscard]] inline bool SaveStringToFile(const std::filesystem::path& path, const std::string& content) {
        FILE* f = nullptr;
        if (fopen_s(&f, path.string().c_str(), "wb") != 0 || !f) return false;
        std::fwrite(content.data(), 1, content.size(), f);
        std::fclose(f);
        return true;
    }

    inline std::string LoadStringFromFile(const std::filesystem::path& path) {
        FILE* f = nullptr;
        if (fopen_s(&f, path.string().c_str(), "rb") != 0 || !f) return {};
        std::fseek(f, 0, SEEK_END);
        long size = std::ftell(f);
        if (size <= 0) {
            std::fclose(f);
            return {};
        }
        std::fseek(f, 0, SEEK_SET);
        std::string content(size, '\0');
        std::fread(content.data(), 1, size, f);
        std::fclose(f);
        return content;
    }

    inline bool DeletePreset(const std::filesystem::path& path) {
        std::error_code ec;
        return std::filesystem::remove(path, ec);
    }

    inline void CleanEmptyDirectories(const std::filesystem::path& dir) {
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec)) return;
        for (auto it = std::filesystem::directory_iterator(dir, ec); it != std::filesystem::directory_iterator();
             ++it) {
            if (it->is_directory()) CleanEmptyDirectories(it->path());
        }
        if (std::filesystem::is_empty(dir, ec)) std::filesystem::remove(dir, ec);
    }

    inline std::pair<std::string, std::string> SanitizePresetPath(const std::string& input) {
        std::string normalized = input; // NOLINT(performance-unnecessary-copy-initialization)
        for (char& c : normalized)
            if (c == '\\') c = '/';

        size_t lastSlash = normalized.rfind('/');
        if (lastSlash == std::string::npos) return {"", SanitizeFilename(normalized)};

        std::string folder = normalized.substr(0, lastSlash);
        std::string filename = normalized.substr(lastSlash + 1);

        std::string cleanFolder;
        cleanFolder.reserve(folder.size());
        for (char c : folder) {
            if (c == '/') {
                cleanFolder += '/';
            } else if (c == '<' || c == '>' || c == ':' || c == '"' || c == '|' || c == '?' || c == '*' || c < 32) {
                cleanFolder += '_';
            } else {
                cleanFolder += c;
            }
        }

        return {cleanFolder, SanitizeFilename(filename)};
    }

    struct PresetTreeNode {
        std::string name;
        std::vector<PresetTreeNode> children;
        std::vector<PresetListEntry> presets;
    };

    inline PresetTreeNode ListPresetsRecursive(const std::filesystem::path& rootDir) {
        PresetTreeNode root;
        root.name = rootDir.filename().string();
        std::error_code ec;
        if (!std::filesystem::exists(rootDir, ec)) return root;

        for (const auto& entry : std::filesystem::directory_iterator(rootDir, ec)) {
            if (entry.is_directory()) {
                auto child = ListPresetsRecursive(entry.path());
                if (!child.presets.empty() || !child.children.empty()) root.children.push_back(std::move(child));
            } else if (entry.is_regular_file() && entry.path().extension() == ".ini") {
                CSimpleIniA ini;
                if (ini.LoadFile(entry.path().string().c_str()) < 0) continue;
                const char* name = ini.GetValue("Preset", "name", nullptr);
                if (!name) continue;
                root.presets.push_back({name, entry.path().filename().string(), entry.path()});
            }
        }
        return root;
    }

}
