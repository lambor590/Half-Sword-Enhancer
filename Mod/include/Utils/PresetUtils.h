#pragma once

#include <string>
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

    inline std::string ClassToString(SDK::UClass* cls) {
        return cls ? cls->GetName() : "";
    }

    inline SDK::UClass* StringToClass(const std::string& name) {
        if (name.empty()) return nullptr;
        return SDK::UObject::FindClassFast(name);
    }

    inline std::string VecToString(const SDK::FVector& v) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%.6g,%.6g,%.6g", v.X, v.Y, v.Z);
        return buf;
    }

    inline SDK::FVector StringToVec(const char* str, SDK::FVector def = {1.0, 1.0, 1.0}) {
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

    inline SDK::FRotator StringToRot(const char* str, SDK::FRotator def = {0.0, 0.0, 0.0}) {
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
        if (!str || !str[0]) { enabled = false; value = 0.0; return; }
        int en = 0;
        sscanf_s(str, "%d,%lf", &en, &value);
        enabled = en != 0;
    }

    inline void ParseIntOverride(const char* str, bool& enabled, int& value) {
        if (!str || !str[0]) { enabled = false; value = 0; return; }
        int en = 0;
        sscanf_s(str, "%d,%d", &en, &value);
        enabled = en != 0;
    }

    inline void ParseBoolOverride(const char* str, bool& enabled, bool& value) {
        if (!str || !str[0]) { enabled = false; value = false; return; }
        int en = 0, val = 0;
        sscanf_s(str, "%d,%d", &en, &val);
        enabled = en != 0;
        value = val != 0;
    }

    inline std::string SanitizeFilename(const std::string& name) {
        std::string result;
        result.reserve(name.size());
        for (char c : name) {
            if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' ||
                c == '\\' || c == '|' || c == '?' || c == '*' || c < 32)
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

    inline bool SaveStringToFile(const std::filesystem::path& path, const std::string& content) {
        FILE* f = nullptr;
        if (fopen_s(&f, path.string().c_str(), "w") != 0 || !f)
            return false;
        std::fwrite(content.data(), 1, content.size(), f);
        std::fclose(f);
        return true;
    }

    inline std::string LoadStringFromFile(const std::filesystem::path& path) {
        FILE* f = nullptr;
        if (fopen_s(&f, path.string().c_str(), "r") != 0 || !f)
            return {};
        std::fseek(f, 0, SEEK_END);
        long size = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        std::string content(size, '\0');
        std::fread(content.data(), 1, size, f);
        std::fclose(f);
        return content;
    }

    inline std::vector<PresetListEntry> ListPresetsInDir(const std::filesystem::path& dir) {
        std::vector<PresetListEntry> entries;
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec)) return entries;

        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".ini") continue;

            CSimpleIniA ini;
            if (ini.LoadFile(entry.path().string().c_str()) < 0) continue;
            const char* name = ini.GetValue("Preset", "name", nullptr);
            if (!name) continue;

            entries.push_back({
                name,
                entry.path().filename().string(),
                entry.path()
            });
        }
        return entries;
    }

    inline bool DeletePreset(const std::filesystem::path& path) {
        std::error_code ec;
        return std::filesystem::remove(path, ec);
    }

}
