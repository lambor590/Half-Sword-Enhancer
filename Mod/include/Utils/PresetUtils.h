#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include <Windows.h>
#include <shellapi.h>

#include "SimpleIni.h"
#include "ConfigManager.h"
#include "Utils/PresetDataBase.h"
#include "SDK/CoreUObject_classes.hpp"

struct PresetListEntry {
    std::string name;
    std::filesystem::path path;
    std::string id;
    bool valid = true;
    std::string error;
};

namespace PresetUtils {

    inline constexpr std::uintmax_t K_MAX_PRESET_FILE_SIZE_BYTES = 8U * 1024U * 1024U;

    enum class AtomicSaveDisposition {
        ReplaceExisting,
        CreateNew,
    };

    [[nodiscard]] inline std::string PathToUtf8(const std::filesystem::path& path) {
        const auto encoded = path.generic_u8string();
        return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
    }

    inline void OpenInExplorer(const std::filesystem::path& dir) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        ShellExecuteW(nullptr, L"open", dir.wstring().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
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

    [[nodiscard]] inline std::string_view TrimAscii(std::string_view value) noexcept {
        while (!value.empty() &&
               (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' || value.front() == '\n'))
            value.remove_prefix(1);
        while (!value.empty() &&
               (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n'))
            value.remove_suffix(1);
        return value;
    }

    [[nodiscard]] inline bool IsValidUtf8(std::string_view value) noexcept {
        if (value.empty()) return true;
        if (value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return false;
        return MultiByteToWideChar(
                   CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0
               ) > 0;
    }

    [[nodiscard]] inline bool TryUtf8ToWide(std::string_view value, std::wstring& result) {
        result.clear();
        if (value.empty()) return true;
        if (value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return false;

        const int wideLength = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0
        );
        if (wideLength <= 0) return false;
        result.resize(static_cast<std::size_t>(wideLength));
        if (MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), wideLength
            ) != wideLength) {
            result.clear();
            return false;
        }
        return true;
    }

    /// SimpleIni is intentionally used in single-line mode. Keep every value
    /// representable without trimming, line injection, or invalid UTF-8.
    [[nodiscard]] inline bool IsSafeIniValue(std::string_view value) noexcept {
        if (!IsValidUtf8(value)) return false;
        for (const unsigned char character : value) {
            if (character < 32 || character == 127) return false;
        }
        return value.empty() || (value.front() != ' ' && value.back() != ' ');
    }

    [[nodiscard]] inline bool TryParseInt(std::string_view text, int& result) noexcept {
        text = TrimAscii(text);
        if (text.empty()) return false;
        int parsed = 0;
        const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), parsed);
        if (ec != std::errc{} || ptr != text.data() + text.size()) return false;
        result = parsed;
        return true;
    }

    [[nodiscard]] inline bool TryParseDouble(std::string_view text, double& result) noexcept {
        text = TrimAscii(text);
        if (text.empty()) return false;
        double parsed = 0.0;
        const auto [ptr, ec] =
            std::from_chars(text.data(), text.data() + text.size(), parsed, std::chars_format::general);
        if (ec != std::errc{} || ptr != text.data() + text.size() || !std::isfinite(parsed)) return false;
        result = parsed;
        return true;
    }

    [[nodiscard]] inline bool TryParseFloat(std::string_view text, float& result) noexcept {
        text = TrimAscii(text);
        if (text.empty()) return false;
        float parsed = 0.0f;
        const auto [ptr, ec] =
            std::from_chars(text.data(), text.data() + text.size(), parsed, std::chars_format::general);
        if (ec != std::errc{} || ptr != text.data() + text.size() || !std::isfinite(parsed)) return false;
        result = parsed;
        return true;
    }

    [[nodiscard]] inline bool TryParseBool(std::string_view text, bool& result) noexcept {
        int parsed = 0;
        if (!TryParseInt(text, parsed) || (parsed != 0 && parsed != 1)) return false;
        result = parsed != 0;
        return true;
    }

    template <typename Float> [[nodiscard]] inline std::string FormatFloating(Float value) {
        static_assert(std::is_floating_point_v<Float>);
        if (!std::isfinite(value)) return "0";
        char buffer[128];
        const auto [ptr, ec] = std::to_chars(
            buffer, buffer + sizeof(buffer), value, std::chars_format::general, std::numeric_limits<Float>::max_digits10
        );
        return ec == std::errc{} ? std::string(buffer, ptr) : std::string("0");
    }

    [[nodiscard]] inline std::string VecToString(const SDK::FVector& value) {
        return FormatFloating(value.X) + "," + FormatFloating(value.Y) + "," + FormatFloating(value.Z);
    }

    template <std::size_t Count, typename Number>
    [[nodiscard]] inline bool TryParseNumberList(const char* text, std::array<Number, Count>& values) {
        if (!text || !text[0]) return false;
        std::string_view remaining(text);
        for (std::size_t index = 0; index < Count; ++index) {
            const auto comma = remaining.find(',');
            const bool last = index + 1 == Count;
            if (last != (comma == std::string_view::npos)) return false;
            const auto token = last ? remaining : remaining.substr(0, comma);
            if constexpr (std::is_same_v<Number, double>) {
                if (!TryParseDouble(token, values[index])) return false;
            } else {
                if (!TryParseFloat(token, values[index])) return false;
            }
            if (!last) remaining.remove_prefix(comma + 1);
        }
        return true;
    }

    [[nodiscard]] inline bool TryStringToVec(const char* text, SDK::FVector& result) {
        std::array<double, 3> values{};
        if (!TryParseNumberList(text, values)) return false;
        result = {values[0], values[1], values[2]};
        return true;
    }

    [[nodiscard]] inline std::string RotToString(const SDK::FRotator& value) {
        return FormatFloating(value.Pitch) + "," + FormatFloating(value.Yaw) + "," + FormatFloating(value.Roll);
    }

    [[nodiscard]] inline bool TryStringToRot(const char* text, SDK::FRotator& result) {
        SDK::FVector parsed{};
        if (!TryStringToVec(text, parsed)) return false;
        result = {parsed.X, parsed.Y, parsed.Z};
        return true;
    }

    [[nodiscard]] inline std::string ColorToString(const SDK::FLinearColor& value) {
        return FormatFloating(value.R) + "," + FormatFloating(value.G) + "," + FormatFloating(value.B) + "," +
               FormatFloating(value.A);
    }

    [[nodiscard]] inline bool TryStringToColor(const char* text, SDK::FLinearColor& result) {
        std::array<float, 4> values{};
        if (!TryParseNumberList(text, values)) return false;
        result = {values[0], values[1], values[2], values[3]};
        return true;
    }

    [[nodiscard]] inline bool TryNormalizePresetRelativePath(
        std::string_view input, std::filesystem::path& result, std::string& error
    ) {
        error.clear();
        if (input.empty()) {
            error = "Enter a preset name";
            return false;
        }

        std::string normalized(input);
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        if (normalized.empty() || normalized.front() == '/' || normalized.back() == '/') {
            error = "Enter a name, optionally inside a folder";
            return false;
        }

        std::string rebuilt;
        std::size_t start = 0;
        while (start <= normalized.size()) {
            const auto slash = normalized.find('/', start);
            std::string component =
                normalized.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
            if (component.empty() || component == "." || component == "..") {
                error = "Use a valid folder name";
                return false;
            }
            if (component.back() == ' ' || component.back() == '.') {
                error = "Folder and preset names cannot end with a space or dot";
                return false;
            }
            for (const unsigned char c : component) {
                if (c < 32 || c == '<' || c == '>' || c == ':' || c == '"' || c == '|' || c == '?' || c == '*') {
                    error = "The name contains a character Windows does not allow";
                    return false;
                }
            }
            if (!rebuilt.empty()) rebuilt.push_back('/');
            rebuilt += component;
            if (slash == std::string::npos) break;
            start = slash + 1;
        }

        auto hasIniExtension = [](std::string_view value) {
            if (value.size() < 4) return false;
            value.remove_prefix(value.size() - 4);
            return (value[0] == '.') && (value[1] == 'i' || value[1] == 'I') && (value[2] == 'n' || value[2] == 'N') &&
                   (value[3] == 'i' || value[3] == 'I');
        };
        if (hasIniExtension(rebuilt))
            rebuilt.replace(rebuilt.size() - 4, 4, ".ini");
        else
            rebuilt += ".ini";

        const auto filenameOffset = rebuilt.rfind('/');
        const std::string_view filename = filenameOffset == std::string::npos
                                              ? std::string_view(rebuilt)
                                              : std::string_view(rebuilt).substr(filenameOffset + 1);
        if (filename == ".ini") {
            error = "Enter a preset name";
            return false;
        }

        std::wstring widePath;
        if (!TryUtf8ToWide(rebuilt, widePath)) {
            error = "The name contains text that cannot be used";
            return false;
        }
        try {
            result = std::filesystem::path(widePath);
        } catch (const std::filesystem::filesystem_error&) {
            error = "Windows cannot use this preset name";
            result.clear();
            return false;
        }
        if (result.is_absolute() || result.has_root_directory() || result.has_root_name()) {
            error = "Enter a name, optionally inside a folder";
            result.clear();
            return false;
        }
        return true;
    }

    [[nodiscard]] inline bool PathComponentEquals(const std::filesystem::path& lhs, const std::filesystem::path& rhs) {
        const auto& left = lhs.native();
        const auto& right = rhs.native();
        return CompareStringOrdinal(
                   left.c_str(), static_cast<int>(left.size()), right.c_str(), static_cast<int>(right.size()), TRUE
               ) == CSTR_EQUAL;
    }

    [[nodiscard]] inline bool PathLess(const std::filesystem::path& lhs, const std::filesystem::path& rhs) {
        const auto left = lhs.generic_wstring();
        const auto right = rhs.generic_wstring();
        return CompareStringOrdinal(
                   left.c_str(), static_cast<int>(left.size()), right.c_str(), static_cast<int>(right.size()), TRUE
               ) == CSTR_LESS_THAN;
    }

    [[nodiscard]] inline std::filesystem::path CanonicalAbsolute(
        const std::filesystem::path& path, std::error_code& error
    ) {
        auto absolute = std::filesystem::absolute(path, error);
        if (error) return {};
        auto canonical = std::filesystem::weakly_canonical(absolute, error);
        if (error) return {};
        return canonical.lexically_normal();
    }

    [[nodiscard]] inline bool PresetPathsEqual(const std::filesystem::path& lhs, const std::filesystem::path& rhs) {
        const auto left = lhs.lexically_normal();
        const auto right = rhs.lexically_normal();
        return PathComponentEquals(left, right);
    }

    [[nodiscard]] inline bool IsPathContained(
        const std::filesystem::path& allowedRoot, const std::filesystem::path& candidate
    ) {
        std::error_code error;
        const auto root = CanonicalAbsolute(allowedRoot, error);
        if (error) return false;
        const auto resolved = CanonicalAbsolute(candidate, error);
        if (error) return false;

        auto rootIt = root.begin();
        auto pathIt = resolved.begin();
        for (; rootIt != root.end(); ++rootIt, ++pathIt) {
            if (pathIt == resolved.end() || !PathComponentEquals(*rootIt, *pathIt)) return false;
        }
        return true;
    }

    [[nodiscard]] inline bool HasIniExtension(const std::filesystem::path& path) {
        const auto extension = path.extension().wstring();
        return CompareStringOrdinal(extension.c_str(), static_cast<int>(extension.size()), L".ini", 4, TRUE) ==
               CSTR_EQUAL;
    }

    [[nodiscard]] inline PresetOperationResult EnsureDirectoryResult(const std::filesystem::path& dir) {
        PresetOperationResult result{.path = dir};
        std::error_code error;
        std::filesystem::create_directories(dir, error);
        if (error) {
            result.error = "Couldn't create the presets folder";
            return result;
        }
        if (!std::filesystem::is_directory(dir, error)) {
            result.error = "The presets folder is unavailable";
            return result;
        }
        result.success = true;
        return result;
    }

    [[nodiscard]] inline PresetOperationResult ValidatePresetFilePath(
        const std::filesystem::path& path, const std::filesystem::path& allowedRoot, bool requireExisting
    ) {
        PresetOperationResult result{.path = path};
        std::error_code rootError;
        if (!std::filesystem::is_directory(allowedRoot, rootError) || rootError) {
            result.error = "The presets folder is unavailable";
            return result;
        }
        for (const auto& component : path) {
            if (component == L"." || component == L"..") {
                result.error = "This preset is unavailable";
                return result;
            }
        }
        if (!HasIniExtension(path)) {
            result.error = "This is not a preset file";
            return result;
        }
        if (!IsPathContained(allowedRoot, path)) {
            result.error = "This preset is unavailable";
            return result;
        }
        std::error_code error;
        if (requireExisting) {
            if (!std::filesystem::is_regular_file(path, error) || error) {
                result.error = "This preset no longer exists";
                return result;
            }
        } else if (std::filesystem::exists(path, error) && !std::filesystem::is_regular_file(path, error)) {
            result.error = "A folder already uses this preset name";
            return result;
        }
        result.success = true;
        return result;
    }

    [[nodiscard]] inline std::string GeneratePresetId() {
        static std::atomic<uint64_t> counter{0};
        const auto now = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        const uint64_t sequence = counter.fetch_add(1, std::memory_order_relaxed);
        const uint64_t high = now ^ (static_cast<uint64_t>(GetCurrentProcessId()) << 32U);
        const uint64_t low = sequence ^ (now << 17U) ^ (now >> 13U);
        char buffer[33];
        std::snprintf(
            buffer, sizeof(buffer), "%016llx%016llx", static_cast<unsigned long long>(high),
            static_cast<unsigned long long>(low)
        );
        return buffer;
    }

    [[nodiscard]] inline PresetOperationResult SaveStringToFileAtomic(
        const std::filesystem::path& path, const std::string& content, const std::filesystem::path& allowedRoot,
        AtomicSaveDisposition disposition = AtomicSaveDisposition::ReplaceExisting
    ) {
        auto ensuredRoot = EnsureDirectoryResult(allowedRoot);
        if (!ensuredRoot) return {.success = false, .path = path, .error = std::move(ensuredRoot.error)};

        auto validation = ValidatePresetFilePath(path, allowedRoot, false);
        if (!validation) return validation;

        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) return {.success = false, .path = path, .error = "Couldn't create the preset folder"};

        auto temporary = path;
        temporary += L".tmp." + std::filesystem::path(GeneratePresetId()).wstring();
        HANDLE file =
            CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return {.success = false, .path = path, .error = "Couldn't start saving the preset"};

        bool wrote = true;
        std::size_t offset = 0;
        while (offset < content.size()) {
            const DWORD chunk =
                static_cast<DWORD>(std::min<std::size_t>(content.size() - offset, (std::numeric_limits<DWORD>::max)()));
            DWORD written = 0;
            if (!WriteFile(file, content.data() + offset, chunk, &written, nullptr) || written != chunk) {
                wrote = false;
                break;
            }
            offset += written;
        }
        if (wrote) wrote = FlushFileBuffers(file) != FALSE;
        CloseHandle(file);

        if (!wrote) {
            std::filesystem::remove(temporary, error);
            return {.success = false, .path = path, .error = "Couldn't save the preset"};
        }
        DWORD moveFlags = MOVEFILE_WRITE_THROUGH;
        if (disposition == AtomicSaveDisposition::ReplaceExisting) moveFlags |= MOVEFILE_REPLACE_EXISTING;
        if (!MoveFileExW(temporary.c_str(), path.c_str(), moveFlags)) {
            const DWORD moveError = GetLastError();
            std::filesystem::remove(temporary, error);
            if (disposition == AtomicSaveDisposition::CreateNew &&
                (moveError == ERROR_ALREADY_EXISTS || moveError == ERROR_FILE_EXISTS)) {
                return {
                    .success = false,
                    .path = path,
                    .error = "Another preset used this name while saving. Refresh the list and try again.",
                };
            }
            return {.success = false, .path = path, .error = "Couldn't replace the preset"};
        }
        return {.success = true, .path = path};
    }

    [[nodiscard]] inline PresetFileReadResult LoadStringFromFileResult(
        const std::filesystem::path& path, const std::filesystem::path& allowedRoot
    ) {
        PresetFileReadResult result{.path = path};
        const auto validation = ValidatePresetFilePath(path, allowedRoot, true);
        if (!validation) {
            result.error = validation.error;
            return result;
        }
        std::error_code sizeError;
        const auto fileSize = std::filesystem::file_size(path, sizeError);
        if (sizeError) {
            result.error = "Couldn't open the preset";
            return result;
        }
        if (fileSize > K_MAX_PRESET_FILE_SIZE_BYTES) {
            result.error = "This preset is too large to open";
            return result;
        }
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            result.error = "Couldn't open this preset";
            return result;
        }
        result.content.resize(static_cast<std::size_t>(fileSize));
        if (fileSize != 0 && !file.read(result.content.data(), static_cast<std::streamsize>(result.content.size()))) {
            result.error = "Couldn't read this preset";
            result.content.clear();
            return result;
        }
        if (!IsValidUtf8(result.content) || result.content.find('\0') != std::string::npos) {
            result.error = "This preset contains text that cannot be used";
            result.content.clear();
            return result;
        }
        result.success = true;
        return result;
    }

    [[nodiscard]] inline const PresetFileReadResult& LoadStringFromFileCached(
        const std::filesystem::path& path, const std::filesystem::path& allowedRoot, PresetResolveContext& context
    ) {
        const auto key = path.lexically_normal();
        auto [cached, inserted] = context.fileReads.try_emplace(key);
        if (inserted) cached->second = LoadStringFromFileResult(path, allowedRoot);
        return cached->second;
    }

    [[nodiscard]] inline PresetOperationResult DeletePresetResult(
        const std::filesystem::path& path, const std::filesystem::path& allowedRoot
    ) {
        auto validation = ValidatePresetFilePath(path, allowedRoot, true);
        if (!validation) return validation;
        std::error_code error;
        if (!std::filesystem::remove(path, error) || error)
            return {.success = false, .path = path, .error = "Couldn't delete the preset"};
        return {.success = true, .path = path};
    }

    inline void CleanEmptyDirectories(const std::filesystem::path& root) {
        std::error_code error;
        if (!std::filesystem::is_directory(root, error)) return;

        auto clean = [&](auto&& self, const std::filesystem::path& dir) -> void {
            std::filesystem::directory_iterator it(
                dir, std::filesystem::directory_options::skip_permission_denied, error
            );
            const std::filesystem::directory_iterator end;
            while (!error && it != end) {
                const auto entry = *it;
                it.increment(error);
                std::error_code entryError;
                if (entry.is_directory(entryError) && !entry.is_symlink(entryError)) {
                    self(self, entry.path());
                    if (std::filesystem::is_empty(entry.path(), entryError))
                        std::filesystem::remove(entry.path(), entryError);
                }
            }
            error.clear();
        };
        clean(clean, root);
    }

    struct PresetTreeNode {
        std::string name;
        std::vector<PresetTreeNode> children;
        std::vector<PresetListEntry> presets;
    };

    [[nodiscard]] inline PresetTreeNode ListPresetsRecursive(
        const std::filesystem::path& rootDir, std::string_view expectedKind = {}
    ) {
        PresetTreeNode root;
        root.name = PathToUtf8(rootDir.filename());
        std::error_code error;
        if (!std::filesystem::is_directory(rootDir, error) || error) return root;

        auto visit = [&](auto&& self, const std::filesystem::path& dir, PresetTreeNode& node) -> void {
            std::filesystem::directory_iterator it(
                dir, std::filesystem::directory_options::skip_permission_denied, error
            );
            const std::filesystem::directory_iterator end;
            while (!error && it != end) {
                const auto entry = *it;
                it.increment(error);
                std::error_code entryError;
                if (entry.is_symlink(entryError)) continue;
                if (entry.is_directory(entryError)) {
                    PresetTreeNode child;
                    child.name = PathToUtf8(entry.path().filename());
                    self(self, entry.path(), child);
                    if (!child.presets.empty() || !child.children.empty()) node.children.push_back(std::move(child));
                    continue;
                }
                if (!entry.is_regular_file(entryError) || !HasIniExtension(entry.path())) continue;

                PresetListEntry preset;
                preset.path = entry.path();
                preset.name = PathToUtf8(entry.path().stem());

                const auto loaded = LoadStringFromFileResult(entry.path(), rootDir);
                CSimpleIniA ini;
                ini.SetUnicode(false);
                if (!loaded || ini.LoadData(loaded.content) < 0) {
                    preset.valid = false;
                    preset.error = loaded ? "This preset file is damaged" : loaded.error;
                } else {
                    int version = 0;
                    const char* versionText = ini.GetValue("Preset", "version", nullptr);
                    if (!versionText || !TryParseInt(versionText, version) || version != K_CURRENT_PRESET_VERSION) {
                        preset.valid = false;
                        preset.error = "This preset was made by an unsupported mod version or is damaged";
                    }
                    const char* name = ini.GetValue("Preset", "name", nullptr);
                    if (name && name[0])
                        preset.name = name;
                    else {
                        preset.valid = false;
                        if (preset.error.empty()) preset.error = "This preset has no name";
                    }
                    const char* kind = ini.GetValue("Preset", "type", nullptr);
                    if (!kind || !kind[0]) {
                        preset.valid = false;
                        if (preset.error.empty()) preset.error = "This preset is incomplete";
                    } else if (!expectedKind.empty() && kind != expectedKind) {
                        preset.valid = false;
                        if (preset.error.empty()) preset.error = "This preset belongs to a different feature";
                    }
                    const char* id = ini.GetValue("Preset", "id", nullptr);
                    if (id && id[0]) {
                        preset.id = id;
                    } else {
                        preset.valid = false;
                        if (preset.error.empty()) preset.error = "This preset is incomplete";
                    }
                }
                node.presets.push_back(std::move(preset));
            }
            error.clear();
        };

        visit(visit, rootDir, root);
        return root;
    }

    inline void FlattenPresetTree(const PresetTreeNode& node, std::vector<PresetListEntry>& output) {
        output.insert(output.end(), node.presets.begin(), node.presets.end());
        for (const auto& child : node.children)
            FlattenPresetTree(child, output);
    }

} // namespace PresetUtils
