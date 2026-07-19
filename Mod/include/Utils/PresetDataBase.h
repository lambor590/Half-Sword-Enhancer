#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

inline constexpr int K_CURRENT_PRESET_VERSION = 2;
inline constexpr char K_CURRENT_PRESET_VERSION_TEXT[] = "2";

struct PresetOperationResult {
    bool success = false;
    std::filesystem::path path;
    std::string error;
    std::string id;

    [[nodiscard]] explicit operator bool() const noexcept { return success; }
};

struct PresetFileReadResult {
    bool success = false;
    std::filesystem::path path;
    std::string content;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return success; }
};

template <typename T> struct PresetLoadResult {
    bool success = false;
    T value{};
    std::filesystem::path path;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return success; }
};

struct PresetReference {
    std::string id;
};

template <typename T> using PresetCopy = std::shared_ptr<const T>;
template <typename T> using PresetLink = std::variant<std::monostate, PresetCopy<T>, PresetReference>;

template <typename T> [[nodiscard]] bool IsEmptyPresetLink(const PresetLink<T>& link) noexcept {
    return std::holds_alternative<std::monostate>(link);
}

template <typename T> [[nodiscard]] const T* GetPresetCopy(const PresetLink<T>& link) noexcept {
    const auto* copy = std::get_if<PresetCopy<T>>(&link);
    return copy ? copy->get() : nullptr;
}

template <typename T> [[nodiscard]] const PresetReference* GetPresetReference(const PresetLink<T>& link) noexcept {
    return std::get_if<PresetReference>(&link);
}

template <typename T> [[nodiscard]] PresetLink<T> MakePresetCopyLink(T value) {
    return std::make_shared<const T>(std::move(value));
}

template <typename T> [[nodiscard]] PresetLink<T> MakePresetReferenceLink(std::string id) {
    return PresetReference{std::move(id)};
}

struct PresetResolveContext {
    std::unordered_map<std::filesystem::path, PresetFileReadResult> fileReads;
};

template <typename T> struct PresetResolveResult {
    bool success = false;
    std::optional<T> value;
    std::filesystem::path path;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return success; }
};

template <typename Target, typename Source>
[[nodiscard]] PresetResolveResult<Target> PresetResolveFailure(
    std::string_view component, PresetResolveResult<Source> cause
) {
    PresetResolveResult<Target> result;
    result.path = std::move(cause.path);
    if (component.empty()) {
        result.error = std::move(cause.error);
    } else {
        result.error.reserve(component.size() + cause.error.size() + 2);
        result.error.append(component).append(": ").append(cause.error);
    }
    return result;
}

template <typename T> [[nodiscard]] PresetResolveResult<T> ResolvedPreset(T value) {
    PresetResolveResult<T> result;
    result.success = true;
    result.value = std::move(value);
    return result;
}

struct PresetDataBase {
    std::string name;
    std::string id;
};
