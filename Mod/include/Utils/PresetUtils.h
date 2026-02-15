#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdio>
#include <cstdint>

#include "ConfigManager.h"
#include "SDK/CoreUObject_classes.hpp"

struct PresetListEntry {
    std::string name;
    std::string filename;
    std::filesystem::path path;
};

namespace PresetUtils {

    inline std::string Base64Encode(const std::string& input) {
        static constexpr const char TABLE[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((input.size() + 2) / 3) * 4);
        const auto* data = reinterpret_cast<const unsigned char*>(input.data());
        size_t len = input.size();
        for (size_t i = 0; i < len; i += 3) {
            uint32_t n = static_cast<uint32_t>(data[i]) << 16;
            if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
            if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);
            out += TABLE[(n >> 18) & 0x3F];
            out += TABLE[(n >> 12) & 0x3F];
            out += (i + 1 < len) ? TABLE[(n >> 6) & 0x3F] : '=';
            out += (i + 2 < len) ? TABLE[n & 0x3F] : '=';
        }
        return out;
    }

    inline std::string Base64Decode(const char* data, size_t len) {
        static constexpr unsigned char DTABLE[] = {
            64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
            64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,52,53,54,55,56,57,58,59,60,61,64,64,64,65,64,64,
            64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
            64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64
        };
        std::string out;
        out.reserve((len / 4) * 3);
        uint32_t buf = 0;
        int bits = 0;
        for (size_t i = 0; i < len; ++i) {
            char c = data[i];
            if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
            unsigned char idx = (static_cast<unsigned char>(c) < 128) ? DTABLE[static_cast<unsigned char>(c)] : 64;
            if (idx == 64) continue;
            buf = (buf << 6) | idx;
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                out += static_cast<char>((buf >> bits) & 0xFF);
            }
        }
        return out;
    }

    inline std::string Base64Decode(const std::string& input) {
        return Base64Decode(input.data(), input.size());
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
