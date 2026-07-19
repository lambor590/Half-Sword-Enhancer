#include <algorithm>
#include <atomic>
#include <vector>
#include <utility>
#include <Windows.h>

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

        [[nodiscard]] std::string_view ArtifactUrl(
            InstallArtifact artifact, std::string_view modUrl, std::string_view proxyUrl, std::string_view bridgeUrl
        ) noexcept {
            switch (artifact) {
                case InstallArtifact::Mod: return modUrl;
                case InstallArtifact::Proxy: return proxyUrl;
                case InstallArtifact::Ue4ssBridge: return bridgeUrl;
            }
            std::unreachable();
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

    std::expected<Version, UpdateError> UpdateManager::GetInstalledModVersion(
        const std::filesystem::path& gameBinPath
    ) {
        const auto dllPath = gameBinPath / MOD_FILENAME;
        return ExtractVersionFromFile(dllPath);
    }

    std::expected<void, UpdateError> UpdateManager::DownloadToTempAndInstall(
        std::string_view modUrl, std::string_view proxyUrl, std::string_view bridgeUrl,
        const std::filesystem::path& gameBinPath, InstallMode installMode, std::uint32_t modMinSize
    ) {
        auto staging = CreateUpdateTempDirectory();
        if (!staging) return std::unexpected(staging.error());

        for (const auto& artifact : GetInstallPlan(installMode)) {
            const auto url = ArtifactUrl(artifact.artifact, modUrl, proxyUrl, bridgeUrl);
            if (url.empty()) {
                Logger::error("The selected version does not include a required file: %s", artifact.filename.data());
                return std::unexpected(UpdateError::InvalidResponse);
            }

            const std::uint32_t minimumSize = artifact.artifact == InstallArtifact::Mod
                                                  ? (std::max)(artifact.minimumFileSize, modMinSize)
                                                  : artifact.minimumFileSize;
            auto download =
                DownloadModToPath(url, staging->Path() / std::filesystem::path(artifact.filename), minimumSize);
            if (!download) return download;
        }

        auto installResult = InstallFiles(staging->Path(), gameBinPath, installMode);
        if (!installResult) {
            return std::unexpected(UpdateError::FileSystemError);
        }
        return {};
    }

    std::expected<void, UpdateError> UpdateManager::DownloadAndInstallMod(
        const Version& version, const std::filesystem::path& gameBinPath, InstallMode installMode
    ) {
        const auto versionStr = version.ToString();
        auto result = DownloadToTempAndInstall(
            BuildReleaseUrl(versionStr, MOD_FILENAME), BuildReleaseUrl(versionStr, PROXY_FILENAME),
            BuildReleaseUrl(versionStr, UE4SS_BRIDGE_FILENAME), gameBinPath, installMode
        );
        if (result) {
            Logger::info("Half Sword Enhancer installed successfully (v%s)", versionStr.c_str());
        }
        return result;
    }

    std::expected<void, UpdateError> UpdateManager::DownloadModToPath(
        std::string_view downloadUrl, const std::filesystem::path& outputPath, std::uint32_t minFileSize
    ) {
        auto tempPath = outputPath;
        tempPath += L".tmp";

        DownloadConfig config{
            .url = downloadUrl,
            .outputPath = tempPath,
            .description = "Downloading installation files",
            .minFileSize = minFileSize
        };

        auto result = DownloadFile(config);
        auto removeTemp = [&tempPath]() {
            std::error_code ignored;
            std::filesystem::remove(tempPath, ignored);
        };
        if (!result) {
            removeTemp();
            return std::unexpected(UpdateError::NetworkError);
        }

        std::error_code ec;
        std::filesystem::rename(tempPath, outputPath, ec);
        if (ec) {
            removeTemp();
            return std::unexpected(UpdateError::FileSystemError);
        }

        return {};
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
            .minFileSize = 50000
        };

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

        const auto storeAsset = [&](std::string_view name, std::string& url, std::string* timestamp,
                                    bool required) -> std::expected<void, UpdateError> {
            auto object = FindJsonObjectByStringField(json, "name", name);
            if (!object) {
                if (required) return std::unexpected(UpdateError::InvalidResponse);
                return {};
            }
            auto parsedUrl = ParseJsonStringField(*object, "browser_download_url");
            if (!parsedUrl || parsedUrl->empty()) return std::unexpected(UpdateError::InvalidResponse);
            url = std::move(*parsedUrl);
            if (!timestamp) return {};
            auto parsedTimestamp = ParseJsonStringField(*object, "updated_at");
            if (!parsedTimestamp || parsedTimestamp->empty()) return std::unexpected(UpdateError::InvalidResponse);
            *timestamp = std::move(*parsedTimestamp);
            return {};
        };

        if (auto result = storeAsset("HSEnhancer.dll", assets.modUrl, &assets.modTimestamp, true); !result)
            return std::unexpected(result.error());
        if (auto result = storeAsset("winmm.dll", assets.proxyUrl, nullptr, true); !result)
            return std::unexpected(result.error());
        if (auto result = storeAsset("HSEnhancerLauncher.exe", assets.launcherUrl, &assets.launcherTimestamp, true);
            !result)
            return std::unexpected(result.error());
        if (auto result = storeAsset(UE4SS_BRIDGE_FILENAME, assets.bridgeUrl, nullptr, false); !result)
            return std::unexpected(result.error());

        return assets;
    }

    std::expected<ExperimentalUpdateInfo, UpdateError> UpdateManager::CheckForExperimentalUpdates() {
        ExperimentalUpdateInfo info;

        auto stableResult = CheckForUpdates();
        if (stableResult && stableResult->remoteVersion >= stableResult->currentVersion) {
            info.stableRelease = *stableResult;
            info.stableRelease->available = true;
            Logger::info("Stable version %s is available", stableResult->remoteVersion.ToString().c_str());
            return info;
        }

        auto jsonResult = DownloadToString(GITHUB_EXPERIMENTAL_API_URL);
        if (!jsonResult) {
            Logger::warn("No experimental release found, using stable releases only");
            return info;
        }

        const auto& json = *jsonResult;

        auto assets = ParseExperimentalAssets(json);
        if (!assets) return std::unexpected(assets.error());
        info.modTimestamp = std::move(assets->modTimestamp);
        info.launcherTimestamp = std::move(assets->launcherTimestamp);
        info.downloadUrlMod = std::move(assets->modUrl);
        info.downloadUrlProxy = std::move(assets->proxyUrl);
        info.downloadUrlBridge = std::move(assets->bridgeUrl);
        info.downloadUrlLauncher = std::move(assets->launcherUrl);

        const std::string storedModTimestamp =
            LauncherConfig::Instance().GetString("ExperimentalUpdate", "mod_timestamp", "");
        const std::string storedLauncherTimestamp =
            LauncherConfig::Instance().GetString("ExperimentalUpdate", "launcher_timestamp", "");

        info.modUpdateAvailable =
            !info.modTimestamp.empty() && (storedModTimestamp.empty() || info.modTimestamp > storedModTimestamp);

        info.launcherUpdateAvailable =
            !info.launcherTimestamp.empty() &&
            (storedLauncherTimestamp.empty() || info.launcherTimestamp > storedLauncherTimestamp);

        if (info.modUpdateAvailable) {
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
        auto result = DownloadToTempAndInstall(
            info.downloadUrlMod, info.downloadUrlProxy, info.downloadUrlBridge, gameBinPath, installMode, 30000
        );
        if (!result) return result;

        auto configResult =
            LauncherConfig::Instance().SetString("ExperimentalUpdate", "mod_timestamp", info.modTimestamp);
        if (!configResult) {
            Logger::warn("Could not remember this update. It may be offered again next time.");
        }

        Logger::info("Experimental Half Sword Enhancer version installed successfully");
        return {};
    }
#endif

}
