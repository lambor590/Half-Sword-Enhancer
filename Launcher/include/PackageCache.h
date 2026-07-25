#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "Version.h"

namespace hse {
    enum class PackageChannel : std::uint8_t { Release, Experimental };

    enum class PackageCacheError : std::uint8_t {
        InvalidManifest,
        InvalidPackage,
        FileSystemError,
        HashFailed,
    };

    struct PackageManifest {
        PackageChannel channel = PackageChannel::Release;
        Version version;
        std::string buildId;
        std::uint64_t sequence = 0;
    };

    struct CachedPackage {
        PackageManifest manifest;
        std::filesystem::path filesPath;
    };

    [[nodiscard]] std::expected<std::optional<CachedPackage>, PackageCacheError> ImportBundledPackage(
        const std::filesystem::path& launcherPath, PackageChannel channel, const Version& version
    );

    [[nodiscard]] std::expected<CachedPackage, PackageCacheError> CacheBundle(
        const std::filesystem::path& bundlePath, PackageChannel channel,
        std::optional<Version> expectedVersion = std::nullopt, std::string_view expectedBuildId = {}
    );

    [[nodiscard]] std::expected<std::optional<CachedPackage>, PackageCacheError> FindCachedPackage(
        PackageChannel channel, std::optional<Version> version = std::nullopt, std::string_view buildId = {}
    );
}
