#include "../include/PackageCache.h"

#include <Windows.h>
#include <bcrypt.h>
#include <ShlObj.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

#include "../../ext/SimpleIni.h"
#include "../include/Logger.h"
#include "../include/Util.h"

namespace hse {
    namespace {
        constexpr std::uint64_t PACKAGE_FORMAT = 1;
        constexpr std::size_t MAX_CACHED_BUILDS = 3;
        constexpr std::array PACKAGE_FILE_NAMES{
            MOD_FILENAME,
            PROXY_FILENAME,
            UE4SS_BRIDGE_FILENAME,
        };

        struct ParsedManifest {
            PackageManifest package;
            std::string launcherHash;
            std::array<std::string, PACKAGE_FILE_NAMES.size()> fileHashes;
        };

        struct AlgorithmHandle {
            BCRYPT_ALG_HANDLE value = nullptr;
            AlgorithmHandle() = default;
            ~AlgorithmHandle() {
                if (value) BCryptCloseAlgorithmProvider(value, 0);
            }
            AlgorithmHandle(const AlgorithmHandle&) = delete;
            AlgorithmHandle& operator=(const AlgorithmHandle&) = delete;
        };

        struct HashHandle {
            BCRYPT_HASH_HANDLE value = nullptr;
            HashHandle() = default;
            ~HashHandle() {
                if (value) BCryptDestroyHash(value);
            }
            HashHandle(const HashHandle&) = delete;
            HashHandle& operator=(const HashHandle&) = delete;
        };

        [[nodiscard]] constexpr std::string_view ChannelName(PackageChannel channel) noexcept {
            return channel == PackageChannel::Experimental ? "experimental" : "release";
        }

        [[nodiscard]] std::optional<PackageChannel> ParseChannel(std::string_view value) noexcept {
            if (value == "release") return PackageChannel::Release;
            if (value == "experimental") return PackageChannel::Experimental;
            return std::nullopt;
        }

        [[nodiscard]] bool IsSafeBuildId(std::string_view value) noexcept {
            return !value.empty() && value.size() <= 64 && std::ranges::all_of(value, [](const char character) {
                return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                       (character >= '0' && character <= '9') || character == '.' || character == '_' ||
                       character == '-';
            });
        }

        [[nodiscard]] bool IsSha256(std::string_view value) noexcept {
            return value.size() == 64 && std::ranges::all_of(value, [](const char character) {
                       return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') ||
                              (character >= 'A' && character <= 'F');
                   });
        }

        [[nodiscard]] std::optional<std::uint64_t> ParseUnsigned(std::string_view value) noexcept {
            std::uint64_t parsed = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) return std::nullopt;
            return parsed;
        }

        [[nodiscard]] std::expected<std::string, PackageCacheError> Sha256(const std::filesystem::path& path) {
            std::ifstream input(path, std::ios::binary);
            if (!input) return std::unexpected(PackageCacheError::HashFailed);

            AlgorithmHandle algorithm;
            if (BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
                return std::unexpected(PackageCacheError::HashFailed);

            DWORD objectBytes = 0;
            DWORD hashBytes = 0;
            DWORD resultBytes = 0;
            if (BCryptGetProperty(
                    algorithm.value, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes),
                    &resultBytes, 0
                ) < 0 ||
                BCryptGetProperty(
                    algorithm.value, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashBytes), sizeof(hashBytes),
                    &resultBytes, 0
                ) < 0)
                return std::unexpected(PackageCacheError::HashFailed);

            std::vector<UCHAR> object(objectBytes);
            HashHandle hash;
            if (BCryptCreateHash(
                    algorithm.value, &hash.value, object.data(), static_cast<ULONG>(object.size()), nullptr, 0, 0
                ) < 0)
                return std::unexpected(PackageCacheError::HashFailed);

            std::array<char, static_cast<std::size_t>(64) * 1024> buffer{};
            while (input) {
                input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                const auto bytes = input.gcount();
                if (bytes > 0 &&
                    BCryptHashData(hash.value, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(bytes), 0) <
                        0)
                    return std::unexpected(PackageCacheError::HashFailed);
            }
            if (!input.eof()) return std::unexpected(PackageCacheError::HashFailed);

            std::vector<UCHAR> digest(hashBytes);
            if (BCryptFinishHash(hash.value, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
                return std::unexpected(PackageCacheError::HashFailed);

            constexpr char HEX[] = "0123456789abcdef";
            std::string output;
            output.resize(digest.size() * 2);
            for (std::size_t index = 0; index < digest.size(); ++index) {
                output[index * 2] = HEX[digest[index] >> 4U];
                output[index * 2 + 1] = HEX[digest[index] & 0x0fU];
            }
            return output;
        }

        [[nodiscard]] std::expected<ParsedManifest, PackageCacheError> ReadManifest(const std::filesystem::path& path) {
            std::ifstream input(path, std::ios::binary);
            if (!input) return std::unexpected(PackageCacheError::InvalidManifest);
            const std::string content{
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>(),
            };
            if (input.bad()) return std::unexpected(PackageCacheError::InvalidManifest);

            CSimpleIniA ini;
            ini.SetUnicode();
            if (ini.LoadData(content) < 0) return std::unexpected(PackageCacheError::InvalidManifest);

            const std::string_view format = ini.GetValue("Package", "format", "");
            const std::string_view channelText = ini.GetValue("Package", "channel", "");
            const std::string_view versionText = ini.GetValue("Package", "version", "");
            const std::string_view buildId = ini.GetValue("Package", "build", "");
            const std::string_view sequenceText = ini.GetValue("Package", "sequence", "");
            const auto formatValue = ParseUnsigned(format);
            const auto channel = ParseChannel(channelText);
            const Version version(versionText);
            const auto sequence = ParseUnsigned(sequenceText);
            if (!formatValue || *formatValue != PACKAGE_FORMAT || !channel || !version.IsValid() ||
                !IsSafeBuildId(buildId) || !sequence || *sequence == 0)
                return std::unexpected(PackageCacheError::InvalidManifest);

            ParsedManifest result{
                .package =
                    PackageManifest{
                        .channel = *channel,
                        .version = version,
                        .buildId = std::string(buildId),
                        .sequence = *sequence,
                    },
                .launcherHash = ini.GetValue("Files", LAUNCHER_FILENAME, ""),
            };
            if (!IsSha256(result.launcherHash)) return std::unexpected(PackageCacheError::InvalidManifest);

            for (std::size_t index = 0; index < PACKAGE_FILE_NAMES.size(); ++index) {
                result.fileHashes[index] = ini.GetValue("Files", PACKAGE_FILE_NAMES[index], "");
                if (!IsSha256(result.fileHashes[index])) return std::unexpected(PackageCacheError::InvalidManifest);
            }
            return result;
        }

        [[nodiscard]] std::expected<void, PackageCacheError> ValidateHash(
            const std::filesystem::path& path, std::string_view expected
        ) {
            auto actual = Sha256(path);
            if (!actual) return std::unexpected(actual.error());
            if (!std::ranges::equal(*actual, expected, [](const char left, const char right) {
                    const auto lower = [](const char value) {
                        return value >= 'A' && value <= 'F' ? static_cast<char>(value - 'A' + 'a') : value;
                    };
                    return lower(left) == lower(right);
                }))
                return std::unexpected(PackageCacheError::InvalidPackage);
            return {};
        }

        [[nodiscard]] std::expected<ParsedManifest, PackageCacheError> ValidateBundle(
            const std::filesystem::path& bundlePath, PackageChannel expectedChannel,
            std::optional<Version> expectedVersion, std::string_view expectedBuildId
        ) {
            const auto filesPath = bundlePath / BUNDLE_FILES_DIRECTORY;
            auto manifest = ReadManifest(filesPath / PACKAGE_MANIFEST_FILENAME);
            if (!manifest || manifest->package.channel != expectedChannel ||
                (expectedVersion && manifest->package.version != *expectedVersion) ||
                (!expectedBuildId.empty() && manifest->package.buildId != expectedBuildId))
                return std::unexpected(PackageCacheError::InvalidPackage);

            if (auto launcher = ValidateHash(bundlePath / LAUNCHER_FILENAME, manifest->launcherHash); !launcher)
                return std::unexpected(launcher.error());
            for (std::size_t index = 0; index < PACKAGE_FILE_NAMES.size(); ++index) {
                if (auto file = ValidateHash(filesPath / PACKAGE_FILE_NAMES[index], manifest->fileHashes[index]); !file)
                    return std::unexpected(file.error());
            }
            return manifest;
        }

        [[nodiscard]] std::expected<void, PackageCacheError> ValidateCachedPackage(const CachedPackage& package) {
            auto manifest = ReadManifest(package.filesPath / PACKAGE_MANIFEST_FILENAME);
            if (!manifest || manifest->package.channel != package.manifest.channel ||
                manifest->package.version != package.manifest.version ||
                manifest->package.buildId != package.manifest.buildId ||
                manifest->package.sequence != package.manifest.sequence)
                return std::unexpected(PackageCacheError::InvalidPackage);

            for (std::size_t index = 0; index < PACKAGE_FILE_NAMES.size(); ++index) {
                if (auto file =
                        ValidateHash(package.filesPath / PACKAGE_FILE_NAMES[index], manifest->fileHashes[index]);
                    !file)
                    return std::unexpected(file.error());
            }
            return {};
        }

        [[nodiscard]] std::expected<std::filesystem::path, PackageCacheError> CacheRoot() {
            PWSTR localAppData = nullptr;
            if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)))
                return std::unexpected(PackageCacheError::FileSystemError);
            auto root = std::filesystem::path(localAppData) / APP_FOLDER_NAME / "cache";
            CoTaskMemFree(localAppData);
            return root;
        }

        [[nodiscard]] std::expected<std::filesystem::path, PackageCacheError> ChannelRoot(PackageChannel channel) {
            auto root = CacheRoot();
            if (!root) return std::unexpected(root.error());
            return *root / ChannelName(channel);
        }

        [[nodiscard]] std::expected<void, PackageCacheError> PruneCache(const std::filesystem::path& channelRoot) {
            struct CacheEntry {
                std::filesystem::path path;
                std::uint64_t sequence = 0;
            };

            std::vector<CacheEntry> entries;
            std::error_code error;
            for (std::filesystem::directory_iterator iterator(channelRoot, error), end; !error && iterator != end;
                 iterator.increment(error)) {
                if (!iterator->is_directory(error)) {
                    if (error) break;
                    continue;
                }
                auto manifest = ReadManifest(iterator->path() / PACKAGE_MANIFEST_FILENAME);
                if (!manifest) {
                    std::filesystem::remove_all(iterator->path(), error);
                    if (error) return std::unexpected(PackageCacheError::FileSystemError);
                    continue;
                }
                entries.push_back({.path = iterator->path(), .sequence = manifest->package.sequence});
            }
            if (error) return std::unexpected(PackageCacheError::FileSystemError);

            std::ranges::sort(entries, [](const CacheEntry& left, const CacheEntry& right) {
                if (left.sequence != right.sequence) return left.sequence > right.sequence;
                return left.path.filename().native() > right.path.filename().native();
            });
            for (std::size_t index = MAX_CACHED_BUILDS; index < entries.size(); ++index) {
                std::filesystem::remove_all(entries[index].path, error);
                if (error) return std::unexpected(PackageCacheError::FileSystemError);
            }
            return {};
        }

        [[nodiscard]] std::filesystem::path CacheDirectoryName(const PackageManifest& manifest) {
            return std::filesystem::path(manifest.buildId + "-" + std::to_string(manifest.sequence));
        }
    }

    std::expected<std::optional<CachedPackage>, PackageCacheError> ImportBundledPackage(
        const std::filesystem::path& launcherPath, PackageChannel channel, const Version& version
    ) {
        const auto bundlePath = launcherPath.parent_path();
        std::error_code error;
        if (!std::filesystem::is_regular_file(bundlePath / BUNDLE_FILES_DIRECTORY / PACKAGE_MANIFEST_FILENAME, error)) {
            if (error && error != std::errc::no_such_file_or_directory)
                return std::unexpected(PackageCacheError::FileSystemError);
            return std::optional<CachedPackage>{};
        }

        auto manifest = ValidateBundle(bundlePath, channel, version, {});
        if (!manifest) {
            Logger::warn("The bundled installation files do not belong to this launcher and will be ignored");
            return std::optional<CachedPackage>{};
        }
        auto cached = CacheBundle(bundlePath, channel, version, manifest->package.buildId);
        if (!cached) return std::unexpected(cached.error());
        Logger::info("Bundled installation files cached successfully");
        return std::optional<CachedPackage>{std::move(*cached)};
    }

    std::expected<CachedPackage, PackageCacheError> CacheBundle(
        const std::filesystem::path& bundlePath, PackageChannel channel, std::optional<Version> expectedVersion,
        std::string_view expectedBuildId
    ) {
        auto parsed = ValidateBundle(bundlePath, channel, expectedVersion, expectedBuildId);
        if (!parsed) return std::unexpected(parsed.error());

        auto channelRoot = ChannelRoot(channel);
        if (!channelRoot) return std::unexpected(channelRoot.error());
        std::error_code error;
        std::filesystem::create_directories(*channelRoot, error);
        if (error) return std::unexpected(PackageCacheError::FileSystemError);

        NamedPathMutex lock(*channelRoot, L"Local\\HalfSwordEnhancer.Cache.", INFINITE);
        if (!lock) return std::unexpected(PackageCacheError::FileSystemError);

        const auto target = *channelRoot / CacheDirectoryName(parsed->package);
        CachedPackage cached{.manifest = parsed->package, .filesPath = target};
        if (std::filesystem::is_directory(target, error) && !error) {
            if (auto validation = ValidateCachedPackage(cached); validation) {
                if (auto pruned = PruneCache(*channelRoot); !pruned) return std::unexpected(pruned.error());
                return cached;
            }
            std::filesystem::remove_all(target, error);
            if (error) return std::unexpected(PackageCacheError::FileSystemError);
        } else if (error && error != std::errc::no_such_file_or_directory) {
            return std::unexpected(PackageCacheError::FileSystemError);
        }

        const auto stagingPath = *channelRoot / (".incoming-" + std::to_string(GetCurrentProcessId()) + "-" +
                                                 std::to_string(GetTickCount64()));
        std::filesystem::remove_all(stagingPath, error);
        error.clear();
        if (!std::filesystem::create_directory(stagingPath, error) || error)
            return std::unexpected(PackageCacheError::FileSystemError);
        ScopedDirectory staging(stagingPath);

        const auto sourcePath = bundlePath / BUNDLE_FILES_DIRECTORY;
        for (const auto filename : PACKAGE_FILE_NAMES) {
            std::filesystem::copy_file(
                sourcePath / filename, staging.Path() / filename, std::filesystem::copy_options::none, error
            );
            if (error) return std::unexpected(PackageCacheError::FileSystemError);
        }
        std::filesystem::copy_file(
            sourcePath / PACKAGE_MANIFEST_FILENAME, staging.Path() / PACKAGE_MANIFEST_FILENAME,
            std::filesystem::copy_options::none, error
        );
        if (error) return std::unexpected(PackageCacheError::FileSystemError);

        CachedPackage staged{.manifest = parsed->package, .filesPath = staging.Path()};
        if (auto validation = ValidateCachedPackage(staged); !validation) return std::unexpected(validation.error());
        std::filesystem::rename(staging.Path(), target, error);
        if (error) return std::unexpected(PackageCacheError::FileSystemError);
        staging.Release();

        if (auto pruned = PruneCache(*channelRoot); !pruned) return std::unexpected(pruned.error());
        return cached;
    }

    std::expected<std::optional<CachedPackage>, PackageCacheError> FindCachedPackage(
        PackageChannel channel, std::optional<Version> version, std::string_view buildId
    ) {
        auto channelRoot = ChannelRoot(channel);
        if (!channelRoot) return std::unexpected(channelRoot.error());
        std::error_code error;
        if (!std::filesystem::is_directory(*channelRoot, error)) {
            if (error && error != std::errc::no_such_file_or_directory)
                return std::unexpected(PackageCacheError::FileSystemError);
            return std::optional<CachedPackage>{};
        }

        NamedPathMutex lock(*channelRoot, L"Local\\HalfSwordEnhancer.Cache.", INFINITE);
        if (!lock) return std::unexpected(PackageCacheError::FileSystemError);

        std::optional<CachedPackage> selected;
        for (std::filesystem::directory_iterator iterator(*channelRoot, error), end; !error && iterator != end;
             iterator.increment(error)) {
            if (!iterator->is_directory(error)) {
                if (error) break;
                continue;
            }
            auto manifest = ReadManifest(iterator->path() / PACKAGE_MANIFEST_FILENAME);
            if (!manifest || manifest->package.channel != channel ||
                (version && manifest->package.version != *version) ||
                (!buildId.empty() && manifest->package.buildId != buildId))
                continue;
            if (!selected || manifest->package.sequence > selected->manifest.sequence) {
                selected = CachedPackage{
                    .manifest = std::move(manifest->package),
                    .filesPath = iterator->path(),
                };
            }
        }
        if (error) return std::unexpected(PackageCacheError::FileSystemError);
        if (!selected) return std::optional<CachedPackage>{};

        if (auto validation = ValidateCachedPackage(*selected); !validation) {
            std::filesystem::remove_all(selected->filesPath, error);
            if (error) return std::unexpected(PackageCacheError::FileSystemError);
            return std::optional<CachedPackage>{};
        }
        return selected;
    }
}
