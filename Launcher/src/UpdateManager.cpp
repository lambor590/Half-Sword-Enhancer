#include <array>
#include <atomic>
#include <vector>
#include <utility>
#include <Windows.h>
#include <process.h>

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

        [[nodiscard]] std::expected<void, UpdateError> ExtractInstallPackage(
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
            const auto systemDirectoryCapacity = static_cast<UINT>(systemDirectory.size());
            const auto systemDirectoryLength = GetSystemDirectoryW(systemDirectory.data(), systemDirectoryCapacity);
            if (systemDirectoryLength == 0 || systemDirectoryLength >= systemDirectoryCapacity) {
                return std::unexpected(UpdateError::FileSystemError);
            }

            const auto tarPath = std::filesystem::path(systemDirectory.data()) / L"tar.exe";
            if (_wspawnl(
                    _P_WAIT, tarPath.c_str(), L"tar.exe", L"-xf", package.c_str(), L"-C", target.c_str(),
                    L"HSEnhancer.dll", L"winmm.dll", L"main.dll", static_cast<const wchar_t*>(nullptr)
                ) != 0) {
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
        if (auto extraction = ExtractInstallPackage(packagePath, filesPath); !extraction) return extraction;

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
    std::expected<ExperimentalUpdateInfo, UpdateError> UpdateManager::CheckForExperimentalUpdates() {
        auto jsonResult = DownloadToString(GITHUB_EXPERIMENTAL_API_URL);
        if (!jsonResult) return std::unexpected(UpdateError::NetworkError);

        ExperimentalUpdateInfo info;
        const auto storeAsset = [&](std::string_view name, std::string& url, std::string& timestamp) {
            auto object = FindJsonObjectByStringField(*jsonResult, "name", name);
            if (!object) return false;

            auto parsedUrl = ParseJsonStringField(*object, "browser_download_url");
            auto parsedTimestamp = ParseJsonStringField(*object, "updated_at");
            if (!parsedUrl || parsedUrl->empty() || !parsedTimestamp || parsedTimestamp->empty()) return false;

            url = std::move(*parsedUrl);
            timestamp = std::move(*parsedTimestamp);
            return true;
        };

        if (!storeAsset(PACKAGE_FILENAME, info.downloadUrlPackage, info.packageTimestamp) ||
            !storeAsset("HSEnhancerLauncher.exe", info.downloadUrlLauncher, info.launcherTimestamp))
            return std::unexpected(UpdateError::InvalidResponse);

        auto& config = LauncherConfig::Instance();
        info.packageUpdateAvailable =
            info.packageTimestamp > config.GetString("ExperimentalUpdate", "package_timestamp", "");
        info.launcherUpdateAvailable =
            info.launcherTimestamp > config.GetString("ExperimentalUpdate", "launcher_timestamp", "");

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
