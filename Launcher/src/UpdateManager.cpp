#include <atomic>
#include <vector>
#include <utility>
#include <Windows.h>
#include <SetupAPI.h>
#include <cwchar>

#include "../include/UpdateManager.h"
#include "../include/NetworkManager.h"
#include "../include/InstallManager.h"
#include "../include/JsonString.h"
#include "../include/LauncherConfig.h"
#include "../include/Logger.h"
#include "../include/SelfUpdate.h"
#include "../include/Util.h"

namespace hse {
    namespace {
        class ScopedTempDirectory {
        public:
            explicit ScopedTempDirectory(std::filesystem::path initialPath) : path(std::move(initialPath)) {}
            ~ScopedTempDirectory() {
                try {
                    std::error_code ignored;
                    std::filesystem::remove_all(path, ignored);
                } catch (...) {
                    OutputDebugStringW(L"Half Sword Enhancer: update temporary-directory cleanup failed.\n");
                }
            }

            ScopedTempDirectory(const ScopedTempDirectory&) = delete;
            ScopedTempDirectory& operator=(const ScopedTempDirectory&) = delete;
            ScopedTempDirectory(ScopedTempDirectory&& other) noexcept : path(std::move(other.path)) {
                other.path.clear();
            }
            ScopedTempDirectory& operator=(ScopedTempDirectory&&) = delete;

            [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path; }

        private:
            std::filesystem::path path;
        };

        [[nodiscard]] std::expected<ScopedTempDirectory, UpdateError> CreateUpdateTempDirectory() {
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
            if (std::filesystem::create_directory(candidate, error)) return ScopedTempDirectory(std::move(candidate));
            Logger::error("Could not prepare the update folder: %s", error.message().c_str());
            return std::unexpected(UpdateError::FileSystemError);
        }

        struct CabinetExtractionContext {
            std::filesystem::path destination;
            InstallMode installMode;
            std::uint8_t extractedMask = 0;
            bool invalidPackage = false;
        };

        UINT CALLBACK ExtractCabinetFile(
            PVOID rawContext, UINT notification, UINT_PTR parameter1, UINT_PTR /*parameter2*/
        ) {
            auto& context = *static_cast<CabinetExtractionContext*>(rawContext);
            if (notification == SPFILENOTIFY_NEEDNEWCABINET) return ERROR_INVALID_DATA;
            if (notification == SPFILENOTIFY_FILEEXTRACTED) {
                const auto& paths = *reinterpret_cast<const FILEPATHS_W*>(parameter1);
                return paths.Win32Error;
            }
            if (notification != SPFILENOTIFY_FILEINCABINET) return NO_ERROR;

            auto& file = *reinterpret_cast<FILE_IN_CABINET_INFO_W*>(parameter1);
            const std::filesystem::path name(file.NameInCabinet);
            const auto plan = GetInstallPlan(context.installMode);
            for (std::size_t index = 0; index < plan.size(); ++index) {
                const auto& artifact = plan[index];
                if (name != std::filesystem::path(artifact.filename)) continue;

                const auto mask = static_cast<std::uint8_t>(1U << index);
                const auto target = (context.destination / std::filesystem::path(artifact.filename)).native();
                if ((context.extractedMask & mask) != 0 || file.FileSize < artifact.minimumFileSize ||
                    target.size() >= _countof(file.FullTargetName) ||
                    wcscpy_s(file.FullTargetName, _countof(file.FullTargetName), target.c_str()) != 0) {
                    context.invalidPackage = true;
                    return FILEOP_ABORT;
                }

                context.extractedMask |= mask;
                return FILEOP_DOIT;
            }
            return FILEOP_SKIP;
        }

        [[nodiscard]] std::expected<void, UpdateError> ExtractInstallPackage(
            const std::filesystem::path& packagePath, const std::filesystem::path& destination, InstallMode installMode
        ) {
            std::error_code error;
            std::filesystem::create_directories(destination, error);
            if (error) return std::unexpected(UpdateError::FileSystemError);

            CabinetExtractionContext context{.destination = destination, .installMode = installMode};
            const auto plan = GetInstallPlan(installMode);
            const auto expectedMask = static_cast<std::uint8_t>((1U << plan.size()) - 1U);
            if (!SetupIterateCabinetW(packagePath.c_str(), 0, ExtractCabinetFile, &context) || context.invalidPackage ||
                context.extractedMask != expectedMask) {
                Logger::error("The downloaded installation package is invalid");
                return std::unexpected(UpdateError::InvalidResponse);
            }
            return {};
        }
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
        auto localVersionResult = GetLocalVersion();
        if (!localVersionResult) {
            return std::unexpected(localVersionResult.error());
        }

        auto jsonResult = DownloadToString(GITHUB_API_URL);
        if (!jsonResult) {
            return std::unexpected(UpdateError::NetworkError);
        }

        auto remoteVersionResult = ParseVersionFromJson(*jsonResult);
        if (!remoteVersionResult) {
            return std::unexpected(remoteVersionResult.error());
        }

        UpdateInfo info;
        info.currentVersion = *localVersionResult;
        info.remoteVersion = *remoteVersionResult;
        info.available = info.remoteVersion > info.currentVersion;

        const auto versionStr = info.remoteVersion.ToString();
        info.downloadUrlLauncher = BuildReleaseUrl(versionStr, "HSEnhancerLauncher.exe");
        if (info.available) {
            hse::Logger::info("Update available: %s", versionStr.c_str());
        }

        return info;
    }

    std::expected<Version, UpdateError> UpdateManager::GetInstalledModVersion(const std::filesystem::path& gameBinPath
    ) {
        const auto dllPath = gameBinPath / MOD_FILENAME;
        return ExtractVersionFromFile(dllPath);
    }

    std::expected<void, UpdateError> UpdateManager::DownloadPackageAndInstall(
        std::string_view packageUrl, const std::filesystem::path& gameBinPath, InstallMode installMode
    ) {
        auto staging = CreateUpdateTempDirectory();
        if (!staging) return std::unexpected(staging.error());

        const auto packagePath = staging->Path() / PACKAGE_FILENAME;
        DownloadConfig config{
            .url = packageUrl,
            .outputPath = packagePath,
            .description = "Downloading installation package",
            .minFileSize = 30'000};
        if (auto download = DownloadFile(config); !download) return std::unexpected(UpdateError::NetworkError);

        const auto filesPath = staging->Path() / "files";
        if (auto extraction = ExtractInstallPackage(packagePath, filesPath, installMode); !extraction)
            return extraction;

        auto installResult = InstallFiles(filesPath, gameBinPath, installMode);
        if (!installResult) {
            return std::unexpected(UpdateError::FileSystemError);
        }
        return {};
    }

    std::expected<void, UpdateError> UpdateManager::DownloadAndInstallMod(
        const Version& version, const std::filesystem::path& gameBinPath, InstallMode installMode
    ) {
        const auto versionStr = version.ToString();
        auto result =
            DownloadPackageAndInstall(BuildReleaseUrl(versionStr, PACKAGE_FILENAME), gameBinPath, installMode);
        if (result) {
            Logger::info("Half Sword Enhancer installed successfully (v%s)", versionStr.c_str());
        }
        return result;
    }

    std::expected<void, UpdateError> UpdateManager::UpdateLauncher(
        std::string_view downloadUrl, const Version& expectedVersion, std::string_view timestamp
    ) {
        std::filesystem::path currentExePath;
        if (!TryGetCurrentExecutablePath(currentExePath)) {
            Logger::error("Could not find the launcher file");
            return std::unexpected(UpdateError::FileSystemError);
        }

        auto stagingResult = CreateSelfUpdateStaging(getAppDataDirectory(), currentExePath);
        if (!stagingResult) {
            Logger::error("Could not prepare the launcher update");
            return std::unexpected(UpdateError::FileSystemError);
        }
        auto staging = std::move(*stagingResult);

        Logger::info("Downloading launcher update...");

        DownloadConfig config{
            .url = downloadUrl,
            .outputPath = staging.PayloadPath(),
            .description = "Downloading launcher update",
            .minFileSize = 50000};

        auto downloadResult = DownloadFile(config);
        if (!downloadResult) {
            Logger::error("The launcher update could not be downloaded");
            return std::unexpected(UpdateError::NetworkError);
        }

        if (auto validation = ValidateLauncherPayload(staging.PayloadPath(), expectedVersion); !validation) {
            Logger::error("The downloaded launcher update is not valid for this version");
            return std::unexpected(UpdateError::InvalidResponse);
        }

        Logger::info("Applying the launcher update...");
        if (auto launched = LaunchSelfUpdateWorker(staging, expectedVersion, timestamp, GetCurrentProcessId());
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
        const DWORD verSize = GetFileVersionInfoSizeW(path.c_str(), nullptr);
        if (verSize == 0) {
            return std::unexpected(UpdateError::VersionParsingFailed);
        }

        std::vector<BYTE> verData(verSize);
        VS_FIXEDFILEINFO* fileInfo = nullptr;
        UINT size = 0;

        if (!GetFileVersionInfoW(path.c_str(), 0, verSize, verData.data()) ||
            !VerQueryValueW(verData.data(), L"\\", reinterpret_cast<void**>(&fileInfo), &size)) {
            return std::unexpected(UpdateError::VersionParsingFailed);
        }

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
        auto versionStr = ParseJsonStringField(json, "tag_name");
        if (!versionStr) {
            return std::unexpected(UpdateError::VersionParsingFailed);
        }

        Version version(*versionStr);
        if (!version.IsValid()) return std::unexpected(UpdateError::VersionParsingFailed);
        return version;
    }

#ifdef EXPERIMENTAL_VERSION
    std::expected<UpdateManager::ExperimentalAssets, UpdateError> UpdateManager::ParseExperimentalAssets(
        std::string_view json
    ) {
        ExperimentalAssets assets;

        const auto storeAsset = [&](std::string_view name, std::string& url,
                                    std::string& timestamp) -> std::expected<void, UpdateError> {
            auto object = FindJsonObjectByStringField(json, "name", name);
            if (!object) return std::unexpected(UpdateError::InvalidResponse);
            auto parsedUrl = ParseJsonStringField(*object, "browser_download_url");
            if (!parsedUrl || parsedUrl->empty()) return std::unexpected(UpdateError::InvalidResponse);
            url = std::move(*parsedUrl);
            auto parsedTimestamp = ParseJsonStringField(*object, "updated_at");
            if (!parsedTimestamp || parsedTimestamp->empty()) return std::unexpected(UpdateError::InvalidResponse);
            timestamp = std::move(*parsedTimestamp);
            return {};
        };

        if (auto result = storeAsset(PACKAGE_FILENAME, assets.packageUrl, assets.packageTimestamp); !result)
            return std::unexpected(result.error());
        if (auto result = storeAsset("HSEnhancerLauncher.exe", assets.launcherUrl, assets.launcherTimestamp); !result)
            return std::unexpected(result.error());

        return assets;
    }

    std::expected<ExperimentalUpdateInfo, UpdateError> UpdateManager::CheckForExperimentalUpdates() {
        ExperimentalUpdateInfo info;

        auto jsonResult = DownloadToString(GITHUB_EXPERIMENTAL_API_URL);
        if (!jsonResult) return std::unexpected(UpdateError::NetworkError);

        const auto& json = *jsonResult;

        auto assets = ParseExperimentalAssets(json);
        if (!assets) return std::unexpected(assets.error());
        info.packageTimestamp = std::move(assets->packageTimestamp);
        info.launcherTimestamp = std::move(assets->launcherTimestamp);
        info.downloadUrlPackage = std::move(assets->packageUrl);
        info.downloadUrlLauncher = std::move(assets->launcherUrl);

        const std::string storedPackageTimestamp =
            LauncherConfig::Instance().GetString("ExperimentalUpdate", "package_timestamp", "");
        const std::string storedLauncherTimestamp =
            LauncherConfig::Instance().GetString("ExperimentalUpdate", "launcher_timestamp", "");

        info.packageUpdateAvailable =
            !info.packageTimestamp.empty() &&
            (storedPackageTimestamp.empty() || info.packageTimestamp > storedPackageTimestamp);

        info.launcherUpdateAvailable =
            !info.launcherTimestamp.empty() &&
            (storedLauncherTimestamp.empty() || info.launcherTimestamp > storedLauncherTimestamp);

        if (info.packageUpdateAvailable) {
            Logger::info("An experimental Half Sword Enhancer update is available");
        }

        if (info.launcherUpdateAvailable) {
            Logger::info("An experimental launcher update is available");
        }

        return info;
    }

    std::expected<void, UpdateError> UpdateManager::DownloadAndInstallExperimentalMod(
        const ExperimentalUpdateInfo& info, const std::filesystem::path& gameBinPath, InstallMode installMode
    ) {
        auto result = DownloadPackageAndInstall(info.downloadUrlPackage, gameBinPath, installMode);
        if (!result) return result;

        auto configResult =
            LauncherConfig::Instance().SetString("ExperimentalUpdate", "package_timestamp", info.packageTimestamp);
        if (!configResult) {
            Logger::warn("Could not remember this update. It may be offered again next time.");
        }

        Logger::info("Experimental Half Sword Enhancer version installed successfully");
        return {};
    }
#endif

}
