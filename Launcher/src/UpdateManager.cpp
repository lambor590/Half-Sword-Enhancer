#include <array>
#include <algorithm>
#include <atomic>
#include <utility>
#include <vector>
#include <Windows.h>
#include <process.h>

#include "../include/UpdateManager.h"
#include "../include/InstallManager.h"
#include "../include/JsonString.h"
#include "../include/LauncherConfig.h"
#include "../include/Logger.h"
#include "../include/NetworkManager.h"
#include "../include/PackageCache.h"
#include "../include/SelfUpdate.h"
#include "../include/Util.h"

namespace hse {
    namespace {
#ifdef EXPERIMENTAL_VERSION
        constexpr PackageChannel CURRENT_CHANNEL = PackageChannel::Experimental;
        constexpr const char* EXPERIMENTAL_SECTION = "ExperimentalUpdate";
        constexpr const char* PACKAGE_BUILD_KEY = "package_build";
#define HSE_STRINGIZE_DETAIL(value) #value
#define HSE_STRINGIZE(value) HSE_STRINGIZE_DETAIL(value)
        constexpr std::string_view CURRENT_LAUNCHER_BUILD_ID = HSE_STRINGIZE(HSE_LAUNCHER_BUILD_ID);
#undef HSE_STRINGIZE
#undef HSE_STRINGIZE_DETAIL
#else
        constexpr PackageChannel CURRENT_CHANNEL = PackageChannel::Release;
#endif

        [[nodiscard]] UpdateError MapPackageError(PackageCacheError error) noexcept {
            return error == PackageCacheError::InvalidManifest || error == PackageCacheError::InvalidPackage
                       ? UpdateError::InvalidResponse
                       : UpdateError::FileSystemError;
        }

        [[nodiscard]] std::expected<ScopedDirectory, UpdateError> CreateUpdateTempDirectory() {
            const auto root = getAppDataDirectory() / "updates";
            std::error_code error;
            std::filesystem::create_directories(root, error);
            if (error) {
                Logger::error("Could not create the update folder: %s", error.message().c_str());
                return std::unexpected(UpdateError::FileSystemError);
            }

            static std::atomic<std::uint64_t> nextId{1};
            const auto directoryName = std::to_wstring(GetCurrentProcessId()) + L"-" +
                                       std::to_wstring(GetTickCount64()) + L"-" +
                                       std::to_wstring(nextId.fetch_add(1, std::memory_order_relaxed));
            auto candidate = root / directoryName;
            if (std::filesystem::create_directory(candidate, error)) return ScopedDirectory(std::move(candidate));
            Logger::error("Could not prepare the update folder: %s", error.message().c_str());
            return std::unexpected(UpdateError::FileSystemError);
        }

        [[nodiscard]] std::expected<void, UpdateError> ExtractBundle(
            const std::filesystem::path& packagePath, const std::filesystem::path& destination
        ) {
            std::error_code error;
            std::filesystem::create_directories(destination, error);
            if (error) return std::unexpected(UpdateError::FileSystemError);

            const auto quote = [](const std::filesystem::path& value) {
                return L"\"" + value.native() + L"\"";
            };
            const auto package = quote(packagePath);
            const auto target = quote(destination);
            std::array<wchar_t, MAX_PATH> systemDirectory{};
            const auto capacity = static_cast<UINT>(systemDirectory.size());
            const auto length = GetSystemDirectoryW(systemDirectory.data(), capacity);
            if (length == 0 || length >= capacity) return std::unexpected(UpdateError::FileSystemError);

            const auto tarPath = std::filesystem::path(systemDirectory.data()) / L"tar.exe";
            if (_wspawnl(
                    _P_WAIT, tarPath.c_str(), L"tar.exe", L"-xf", package.c_str(), L"-C", target.c_str(),
                    L"HSEnhancerLauncher.exe", L"\"Manual Install/HSEnhancer.dll\"", L"\"Manual Install/winmm.dll\"",
                    L"\"Manual Install/main.dll\"", L"\"Manual Install/package.ini\"",
                    static_cast<const wchar_t*>(nullptr)
                ) != 0) {
                Logger::error("The downloaded Half Sword Enhancer package is invalid");
                return std::unexpected(UpdateError::InvalidResponse);
            }
            return {};
        }

        [[nodiscard]] std::expected<CachedPackage, UpdateError> DownloadBundleToCache(
            std::string_view packageUrl, std::optional<Version> expectedVersion, std::string_view expectedBuildId
        ) {
            auto staging = CreateUpdateTempDirectory();
            if (!staging) return std::unexpected(staging.error());

            const auto packagePath = staging->Path() / PACKAGE_FILENAME;
            DownloadConfig config{
                .url = packageUrl,
                .outputPath = packagePath,
                .description = "Downloading Half Sword Enhancer",
                .minFileSize = 30'000,
            };
            if (auto download = DownloadFile(config); !download) return std::unexpected(UpdateError::NetworkError);

            const auto bundlePath = staging->Path() / "bundle";
            if (auto extraction = ExtractBundle(packagePath, bundlePath); !extraction)
                return std::unexpected(extraction.error());

            auto cached = CacheBundle(bundlePath, CURRENT_CHANNEL, expectedVersion, expectedBuildId);
            if (!cached) return std::unexpected(MapPackageError(cached.error()));
            return std::move(*cached);
        }

        [[nodiscard]] std::expected<void, UpdateError> InstallPackage(
            const CachedPackage& package, const std::filesystem::path& gameBinPath, InstallMode installMode
        ) {
            if (auto installed = InstallFiles(package.filesPath, gameBinPath, installMode); !installed)
                return std::unexpected(UpdateError::FileSystemError);
            return {};
        }
    }

    std::expected<void, UpdateError> UpdateManager::PrepareBundledPackage() {
        std::filesystem::path launcherPath;
        if (!TryGetCurrentExecutablePath(launcherPath)) return std::unexpected(UpdateError::FileSystemError);
        auto localVersion = GetLocalVersion();
        if (!localVersion) return std::unexpected(localVersion.error());

        auto imported = ImportBundledPackage(launcherPath, CURRENT_CHANNEL, *localVersion);
        if (!imported) return std::unexpected(MapPackageError(imported.error()));
#ifdef EXPERIMENTAL_VERSION
        if (*imported) {
            auto& config = LauncherConfig::Instance();
            if (!config.SetString(EXPERIMENTAL_SECTION, PACKAGE_BUILD_KEY, (*imported)->manifest.buildId))
                return std::unexpected(UpdateError::FileSystemError);
        }
#endif
        return {};
    }

    std::string UpdateManager::BuildReleaseUrl(std::string_view version, std::string_view filename) {
        constexpr std::string_view PREFIX = "https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v";
        std::string url;
        url.reserve(PREFIX.size() + version.size() + filename.size() + 1);
        url.append(PREFIX).append(version).push_back('/');
        url.append(filename);
        return url;
    }

    std::expected<Version, UpdateError> UpdateManager::GetLocalVersion() {
        static const auto LOCAL_VERSION = []() -> std::expected<Version, UpdateError> {
            std::filesystem::path filePath;
            if (!TryGetCurrentExecutablePath(filePath)) return std::unexpected(UpdateError::FileSystemError);
            return ExtractVersionFromFile(filePath);
        }();
        return LOCAL_VERSION;
    }

    std::expected<UpdateInfo, UpdateError> UpdateManager::CheckForUpdates() {
        auto localVersion = GetLocalVersion();
        if (!localVersion) return std::unexpected(localVersion.error());

        auto json = DownloadToString(GITHUB_API_URL);
        if (!json) return std::unexpected(UpdateError::NetworkError);

        auto remoteVersion = ParseVersionFromJson(*json);
        if (!remoteVersion) return std::unexpected(remoteVersion.error());

        UpdateInfo info{
            .available = *remoteVersion > *localVersion,
            .currentVersion = *localVersion,
            .remoteVersion = *remoteVersion,
            .downloadUrlBundle = BuildReleaseUrl(remoteVersion->ToString(), PACKAGE_FILENAME),
        };
        if (info.available) Logger::info("Update available: %s", info.remoteVersion.ToString().c_str());
        return info;
    }

    std::expected<Version, UpdateError> UpdateManager::GetInstalledModVersion(
        const std::filesystem::path& gameBinPath
    ) {
        return ExtractVersionFromFile(gameBinPath / MOD_FILENAME);
    }

    std::expected<void, UpdateError> UpdateManager::DownloadPackageAndInstall(
        std::string_view packageUrl, const std::filesystem::path& gameBinPath, InstallMode installMode,
        std::optional<Version> expectedVersion, std::string_view expectedBuildId
    ) {
        auto cached = FindCachedPackage(CURRENT_CHANNEL, expectedVersion, expectedBuildId);
        if (!cached) return std::unexpected(MapPackageError(cached.error()));
        if (*cached) return InstallPackage(**cached, gameBinPath, installMode);

        auto downloaded = DownloadBundleToCache(packageUrl, expectedVersion, expectedBuildId);
        if (!downloaded) return std::unexpected(downloaded.error());
        return InstallPackage(*downloaded, gameBinPath, installMode);
    }

    std::expected<void, UpdateError> UpdateManager::DownloadAndInstallMod(
        const Version& version, const std::filesystem::path& gameBinPath, InstallMode installMode
    ) {
        const auto versionText = version.ToString();
        auto result = DownloadPackageAndInstall(
            BuildReleaseUrl(versionText, PACKAGE_FILENAME), gameBinPath, installMode, version
        );
        if (result) Logger::info("Half Sword Enhancer installed successfully (v%s)", versionText.c_str());
        return result;
    }

    std::expected<bool, UpdateError> UpdateManager::InstallPreparedPackage(
        const std::filesystem::path& gameBinPath, InstallMode installMode
    ) {
#ifdef EXPERIMENTAL_VERSION
        const auto buildId = LauncherConfig::Instance().GetString(EXPERIMENTAL_SECTION, PACKAGE_BUILD_KEY, "");
        if (buildId.empty()) return false;
        auto cached = FindCachedPackage(CURRENT_CHANNEL, std::nullopt, buildId);
#else
        auto version = GetLocalVersion();
        if (!version) return std::unexpected(version.error());
        auto cached = FindCachedPackage(CURRENT_CHANNEL, *version);
#endif
        if (!cached) return std::unexpected(MapPackageError(cached.error()));
        if (!*cached) return false;
        if (auto installed = InstallPackage(**cached, gameBinPath, installMode); !installed)
            return std::unexpected(installed.error());
        return true;
    }

    std::expected<void, UpdateError> UpdateManager::UpdateLauncher(
        std::string_view downloadUrl, std::optional<Version> expectedVersion, std::string_view buildId
    ) {
        std::filesystem::path currentLauncher;
        if (!TryGetCurrentExecutablePath(currentLauncher)) {
            Logger::error("Could not find the launcher file");
            return std::unexpected(UpdateError::FileSystemError);
        }

        auto stagingResult = CreateSelfUpdateStaging(getAppDataDirectory(), currentLauncher);
        if (!stagingResult) {
            Logger::error("Could not prepare the launcher update");
            return std::unexpected(UpdateError::FileSystemError);
        }
        auto staging = std::move(*stagingResult);
        const auto stagingDirectory = staging.PayloadPath().parent_path();
        const auto packagePath = stagingDirectory / PACKAGE_FILENAME;
        const auto bundlePath = stagingDirectory / "bundle";

        Logger::info("Downloading launcher update...");
        DownloadConfig config{
            .url = downloadUrl,
            .outputPath = packagePath,
            .description = "Downloading Half Sword Enhancer",
            .minFileSize = 30'000,
        };
        if (auto download = DownloadFile(config); !download) {
            Logger::error("The launcher update could not be downloaded");
            return std::unexpected(UpdateError::NetworkError);
        }
        if (auto extraction = ExtractBundle(packagePath, bundlePath); !extraction) return extraction;

        auto cached = CacheBundle(bundlePath, CURRENT_CHANNEL, expectedVersion, buildId);
        if (!cached) return std::unexpected(MapPackageError(cached.error()));

        const auto sourceLauncher = bundlePath / LAUNCHER_FILENAME;
        std::error_code error;
        std::filesystem::copy_file(sourceLauncher, staging.PayloadPath(), std::filesystem::copy_options::none, error);
        if (error) return std::unexpected(UpdateError::FileSystemError);
        if (auto validation = ValidateLauncherPayload(staging.PayloadPath(), cached->manifest.version); !validation)
            return std::unexpected(UpdateError::InvalidResponse);

        Logger::info("Applying the launcher update...");
        if (auto launched = LaunchSelfUpdateWorker(staging, cached->manifest.version, buildId, GetCurrentProcessId());
            !launched) {
            Logger::error("The launcher update could not be applied");
            return std::unexpected(UpdateError::UpdateFailed);
        }
        staging.Release();

        Logger::info("Launcher update ready. The launcher will restart automatically.");
        ExitProcess(0);
    }

    std::expected<Version, UpdateError> UpdateManager::ExtractVersionFromFile(const std::filesystem::path& filePath) {
        const auto& path = filePath.native();
        const DWORD versionBytes = GetFileVersionInfoSizeW(path.c_str(), nullptr);
        if (versionBytes == 0) return std::unexpected(UpdateError::VersionParsingFailed);

        std::vector<BYTE> versionData(versionBytes);
        VS_FIXEDFILEINFO* fileInfo = nullptr;
        UINT size = 0;
        if (!GetFileVersionInfoW(path.c_str(), 0, versionBytes, versionData.data()) ||
            !VerQueryValueW(versionData.data(), L"\\", reinterpret_cast<void**>(&fileInfo), &size))
            return std::unexpected(UpdateError::VersionParsingFailed);

        return Version(
            static_cast<std::uint16_t>(HIWORD(fileInfo->dwFileVersionMS)),
            static_cast<std::uint16_t>(LOWORD(fileInfo->dwFileVersionMS)),
            static_cast<std::uint16_t>(HIWORD(fileInfo->dwFileVersionLS))
        );
    }

    std::expected<std::string, UpdateError> UpdateManager::ParseJsonStringField(
        std::string_view json, std::string_view fieldName
    ) {
        auto field = FindJsonStringField(json, fieldName);
        if (!field) return std::unexpected(UpdateError::InvalidResponse);
        return std::move(field->value);
    }

    std::expected<Version, UpdateError> UpdateManager::ParseVersionFromJson(std::string_view json) {
        auto versionText = ParseJsonStringField(json, "tag_name");
        if (!versionText) return std::unexpected(UpdateError::VersionParsingFailed);

        Version version(*versionText);
        if (!version.IsValid()) return std::unexpected(UpdateError::VersionParsingFailed);
        return version;
    }

#ifdef EXPERIMENTAL_VERSION
    std::expected<ExperimentalUpdateInfo, UpdateError> UpdateManager::CheckForExperimentalUpdates() {
        auto json = DownloadToString(GITHUB_EXPERIMENTAL_API_URL);
        if (!json) return std::unexpected(UpdateError::NetworkError);

        auto buildId = ParseJsonStringField(*json, "target_commitish");
        if (!buildId || buildId->empty()) return std::unexpected(UpdateError::InvalidResponse);
        auto asset = FindJsonObjectByStringField(*json, "name", PACKAGE_FILENAME);
        if (!asset) return std::unexpected(UpdateError::InvalidResponse);
        auto downloadUrl = ParseJsonStringField(*asset, "browser_download_url");
        if (!downloadUrl || downloadUrl->empty()) return std::unexpected(UpdateError::InvalidResponse);

        auto releaseBody = ParseJsonStringField(*json, "body");
        if (!releaseBody) return std::unexpected(UpdateError::InvalidResponse);
        constexpr std::string_view MARKER = "<!-- hse-launcher:";
        constexpr std::size_t BUILD_ID_LENGTH = 40;
        constexpr std::string_view MARKER_END = " -->";
        const auto marker = releaseBody->find(MARKER);
        if (marker == std::string::npos) return std::unexpected(UpdateError::InvalidResponse);
        const auto buildIdStart = marker + MARKER.size();
        if (releaseBody->size() < buildIdStart + BUILD_ID_LENGTH + MARKER_END.size())
            return std::unexpected(UpdateError::InvalidResponse);
        const std::string_view publishedBuild(*releaseBody);
        const auto publishedBuildId = publishedBuild.substr(buildIdStart, BUILD_ID_LENGTH);
        if (!std::ranges::all_of(
                publishedBuildId,
                [](char value) { return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'); }
            ) ||
            publishedBuild.substr(buildIdStart + BUILD_ID_LENGTH, MARKER_END.size()) != MARKER_END)
            return std::unexpected(UpdateError::InvalidResponse);

        const auto& config = LauncherConfig::Instance();
        ExperimentalUpdateInfo info{
            .packageUpdateAvailable = *buildId != config.GetString(EXPERIMENTAL_SECTION, PACKAGE_BUILD_KEY, ""),
            .launcherUpdateAvailable = publishedBuildId != CURRENT_LAUNCHER_BUILD_ID,
            .buildId = std::move(*buildId),
            .downloadUrlBundle = std::move(*downloadUrl),
        };
        if (info.packageUpdateAvailable) Logger::info("An experimental Half Sword Enhancer update is available");
        if (info.launcherUpdateAvailable) Logger::info("An experimental launcher update is available");
        return info;
    }

    std::expected<void, UpdateError> UpdateManager::DownloadAndInstallExperimentalMod(
        const ExperimentalUpdateInfo& info, const std::filesystem::path& gameBinPath, InstallMode installMode
    ) {
        auto result =
            DownloadPackageAndInstall(info.downloadUrlBundle, gameBinPath, installMode, std::nullopt, info.buildId);
        if (!result) return result;

        if (!LauncherConfig::Instance().SetString(EXPERIMENTAL_SECTION, PACKAGE_BUILD_KEY, info.buildId))
            Logger::warn("Could not remember this update. It may be offered again next time.");

        Logger::info("Experimental Half Sword Enhancer version installed successfully");
        return {};
    }
#endif
}
