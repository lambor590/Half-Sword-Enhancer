#include <vector>
#include <array>
#include <charconv>
#include <format>
#include <fstream>
#include <Windows.h>

#include "../include/UpdateManager.h"
#include "../include/NetworkManager.h"
#include "../include/InstallManager.h"
#include "../include/LauncherConfig.h"
#include "../include/Logger.h"
#include "../include/Util.h"

namespace hse {
    namespace {

        class ScopedDirectoryCleanup {
        public:
            explicit ScopedDirectoryCleanup(std::filesystem::path path) noexcept : path_(std::move(path)) {}

            ~ScopedDirectoryCleanup() {
                std::error_code ec;
                std::filesystem::remove_all(path_, ec);
            }

        private:
            std::filesystem::path path_;
        };

        void RemoveFileIfPresent(const std::filesystem::path& path) noexcept {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }

        [[nodiscard]] std::expected<std::filesystem::path, UpdateError> GetCurrentExecutablePath() noexcept {
            std::filesystem::path executablePath;
            if (!TryGetCurrentExecutablePath(executablePath)) {
                return std::unexpected(UpdateError::FileSystemError);
            }

            return executablePath;
        }

        [[nodiscard]] std::expected<void, UpdateError> FinalizeDownloadedFile(
            const std::filesystem::path& tempPath, const std::filesystem::path& outputPath
        ) noexcept {
            std::error_code ec;
            if (std::filesystem::exists(outputPath, ec)) {
                if (ec) {
                    RemoveFileIfPresent(tempPath);
                    return std::unexpected(UpdateError::FileSystemError);
                }

                std::filesystem::remove(outputPath, ec);
                if (ec) {
                    RemoveFileIfPresent(tempPath);
                    return std::unexpected(UpdateError::FileSystemError);
                }
            }

            std::filesystem::rename(tempPath, outputPath, ec);
            if (ec) {
                RemoveFileIfPresent(tempPath);
                return std::unexpected(UpdateError::FileSystemError);
            }

            return {};
        }

    }


    Version::Version(std::string_view versionString) noexcept {
        if (versionString.empty() || versionString == "0.0.0") return;

        if (versionString.front() == 'v') {
            versionString.remove_prefix(1);
        }

        std::uint16_t* components[] = {&major_, &minor_, &patch_};

        for (size_t i = 0; i < 3 && !versionString.empty(); ++i) {
            const auto dotPos = versionString.find('.');
            const auto segEnd = (dotPos != std::string_view::npos) ? dotPos : versionString.size();
            std::from_chars(versionString.data(), versionString.data() + segEnd, *components[i]);
            if (dotPos == std::string_view::npos) break;
            versionString.remove_prefix(dotPos + 1);
        }
    }

    std::string Version::ToString() const {
        return std::format("{}.{}.{}", major_, minor_, patch_);
    }

    std::string UpdateManager::BuildReleaseUrl(std::string_view version, std::string_view filename) {
        return std::format(
            "https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v{}/{}", version, filename
        );
    }

    std::expected<Version, UpdateError> UpdateManager::GetLocalVersion() noexcept {
        std::lock_guard lock(mutex_);

        if (cachedLocalVersion_) {
            return *cachedLocalVersion_;
        }

        auto result = ExtractVersionFromExecutable();
        if (result) {
            cachedLocalVersion_ = *result;
        }

        return result;
    }

    std::expected<UpdateInfo, UpdateError> UpdateManager::CheckForUpdates() noexcept {
        auto localVersionResult = GetLocalVersion();
        if (!localVersionResult) {
            return std::unexpected(localVersionResult.error());
        }

        auto jsonResult = FetchGitHubReleaseInfo();
        if (!jsonResult) {
            return std::unexpected(jsonResult.error());
        }

        auto remoteVersionResult = ParseVersionFromJson(*jsonResult);
        if (!remoteVersionResult) {
            return std::unexpected(remoteVersionResult.error());
        }

        UpdateInfo info;
        info.currentVersion = *localVersionResult;
        info.remoteVersion = *remoteVersionResult;
        info.available = info.remoteVersion != info.currentVersion;

        if (info.available) {
            const auto versionStr = info.remoteVersion.ToString();
            info.downloadUrlLauncher = BuildReleaseUrl(versionStr, "HSEnhancerLauncher.exe");
            info.downloadUrlMod = BuildReleaseUrl(versionStr, "HSEnhancer.dll");
            info.downloadUrlProxy = BuildReleaseUrl(versionStr, "winmm.dll");
            hse::Logger::info(std::format("Update available: {}", versionStr));
        }

        return info;
    }

    std::expected<Version, UpdateError> UpdateManager::GetInstalledModVersion(const std::filesystem::path& gameBinPath
    ) noexcept {
        const auto dllPath = gameBinPath / MOD_FILENAME;
        if (!std::filesystem::exists(dllPath)) {
            return std::unexpected(UpdateError::FileSystemError);
        }
        return ExtractVersionFromFile(dllPath);
    }

    std::expected<void, UpdateError> UpdateManager::DownloadToTempAndInstall(
        std::string_view modUrl, std::string_view proxyUrl, const std::filesystem::path& gameBinPath,
        std::uint32_t modMinSize
    ) noexcept {
        const auto tempDir = getAppDataDirectory() / TEMP_FOLDER;
        [[maybe_unused]] const ScopedDirectoryCleanup cleanup(tempDir);

        std::error_code ec;
        std::filesystem::create_directories(tempDir, ec);
        if (ec) {
            Logger::error("Failed to create temp update directory: %s", ec.message().c_str());
            return std::unexpected(UpdateError::FileSystemError);
        }

        auto modResult = DownloadModToPath(modUrl, tempDir / MOD_FILENAME, modMinSize);
        if (!modResult) {
            return modResult;
        }

        if (!proxyUrl.empty()) {
            auto proxyResult = DownloadModToPath(proxyUrl, tempDir / PROXY_FILENAME, 10000);
            if (!proxyResult) {
                Logger::warn("Proxy not available in this release, skipping");
            }
        }

        auto installResult = InstallManager::Instance().InstallFiles(tempDir, gameBinPath);
        if (!installResult) {
            return std::unexpected(UpdateError::FileSystemError);
        }

        return {};
    }

    std::expected<void, UpdateError> UpdateManager::DownloadAndInstallMod(
        const Version& version, const std::filesystem::path& gameBinPath
    ) noexcept {
        const auto versionStr = version.ToString();
        auto result = DownloadToTempAndInstall(
            BuildReleaseUrl(versionStr, MOD_FILENAME), BuildReleaseUrl(versionStr, PROXY_FILENAME), gameBinPath
        );
        if (result) {
            Logger::info(std::format("Mod installed successfully (v{})", versionStr));
        }
        return result;
    }

    std::expected<void, UpdateError> UpdateManager::DownloadModToPath(
        std::string_view downloadUrl, const std::filesystem::path& outputPath, std::uint32_t minFileSize
    ) noexcept {
        const auto tempPath = std::filesystem::path(outputPath.string() + ".tmp");

        DownloadConfig config{
            .url = std::string(downloadUrl),
            .outputPath = tempPath,
            .description = std::format("Downloading {}", outputPath.filename().string()),
            .minFileSize = minFileSize};

        auto result = NetworkManager::Instance().DownloadFile(config);
        if (!result) {
            RemoveFileIfPresent(tempPath);
            return std::unexpected(UpdateError::NetworkError);
        }

        return FinalizeDownloadedFile(tempPath, outputPath);
    }

    std::expected<void, UpdateError> UpdateManager::UpdateLauncher(
        std::string_view downloadUrl, std::string_view timestamp
    ) noexcept {
        auto currentExePath = GetCurrentExecutablePath();
        if (!currentExePath) {
            Logger::error("Failed to get current executable path");
            return std::unexpected(UpdateError::FileSystemError);
        }

        const auto tempPath = getAppDataDirectory() / "HSEnhancerLauncher_Update.exe";
        const auto batchPath = getAppDataDirectory() / "HSEnhancer_Update.bat";

        Logger::info("Downloading launcher update...");

        DownloadConfig config{
            .url = std::string(downloadUrl),
            .outputPath = tempPath,
            .description = "Downloading launcher update",
            .minFileSize = 50000};

        auto downloadResult = NetworkManager::Instance().DownloadFile(config);
        if (!downloadResult) {
            Logger::error("Failed to download launcher update");
            return std::unexpected(UpdateError::NetworkError);
        }

        Logger::info("Creating update script...");

        std::ofstream batchFile(batchPath);
        if (!batchFile) {
            Logger::error("Failed to create update script");
            return std::unexpected(UpdateError::FileSystemError);
        }

        const auto tempStr = tempPath.string();
        const auto batchStr = batchPath.string();
        const auto currentExePathStr = currentExePath->string();

        auto script = std::format(
            "@echo off\n"
            "echo Updating Half Sword Enhancer Launcher...\n"
            "timeout /t 2 /nobreak >nul\n"
            "move \"{}\" \"{}\"\n"
            "if exist \"{}\" (\n"
            "    echo Update completed successfully!\n"
            "    echo Starting updated launcher...\n"
            "    start \"\" \"{}\"\n"
            ") else (\n"
            "    echo Update failed! Please download manually.\n"
            "    pause\n"
            ")\n"
            "del \"{}\"\n",
            tempStr, currentExePathStr, currentExePathStr, currentExePathStr, batchStr
        );

        batchFile.write(script.data(), static_cast<std::streamsize>(script.size()));
        batchFile.close();
        if (!batchFile) {
            Logger::error("Failed to write update script");
            return std::unexpected(UpdateError::FileSystemError);
        }

        Logger::info("Launching update script and exiting...");

        STARTUPINFOA startupInfo{};
        PROCESS_INFORMATION processInfo{};
        startupInfo.cb = sizeof(startupInfo);

        auto cmdLine = std::format("cmd.exe /c \"{}\"", batchStr);

        if (!CreateProcessA(
                nullptr, cmdLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo, &processInfo
            )) {
            Logger::error("Failed to start update script");
            return std::unexpected(UpdateError::UpdateFailed);
        }

        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);

        Logger::info("Update script started successfully. Launcher will restart automatically.");

        if (!timestamp.empty()) {
            auto saveResult =
                LauncherConfig::Instance().SetString("ExperimentalUpdate", "launcher_timestamp", timestamp);
            if (!saveResult) {
                Logger::warn("Failed to persist launcher update timestamp");
            }
        }

        ExitProcess(0);
    }

    std::expected<Version, UpdateError> UpdateManager::ExtractVersionFromExecutable() const noexcept {
        auto filePath = GetCurrentExecutablePath();
        if (!filePath) {
            return std::unexpected(UpdateError::FileSystemError);
        }

        return ExtractVersionFromFile(*filePath);
    }

    std::expected<Version, UpdateError> UpdateManager::ExtractVersionFromFile(const std::filesystem::path& filePath
    ) noexcept {
        const auto pathStr = filePath.string();
        const DWORD verSize = GetFileVersionInfoSizeA(pathStr.c_str(), nullptr);
        if (verSize == 0) {
            return std::unexpected(UpdateError::VersionParsingFailed);
        }

        std::vector<BYTE> verData(verSize);
        VS_FIXEDFILEINFO* fileInfo = nullptr;
        UINT size = 0;

        if (!GetFileVersionInfoA(pathStr.c_str(), 0, verSize, verData.data()) ||
            !VerQueryValueA(verData.data(), "\\", reinterpret_cast<void**>(&fileInfo), &size)) {
            return std::unexpected(UpdateError::VersionParsingFailed);
        }

        return Version(
            static_cast<std::uint16_t>(HIWORD(fileInfo->dwFileVersionMS)),
            static_cast<std::uint16_t>(LOWORD(fileInfo->dwFileVersionMS)),
            static_cast<std::uint16_t>(HIWORD(fileInfo->dwFileVersionLS))
        );
    }

    std::expected<std::string, UpdateError> UpdateManager::FetchGitHubReleaseInfo() const noexcept {
        auto result = NetworkManager::Instance().DownloadToString(std::string(GITHUB_API_URL));
        if (!result) {
            return std::unexpected(UpdateError::NetworkError);
        }

        return std::move(*result);
    }

    std::expected<std::string, UpdateError> UpdateManager::ParseJsonStringField(
        std::string_view json, std::string_view fieldName
    ) noexcept {
        std::string fieldPrefix;
        fieldPrefix.reserve(fieldName.size() + 5);
        fieldPrefix += '"';
        fieldPrefix += fieldName;
        fieldPrefix += "\":\"";

        const auto fieldPos = json.find(fieldPrefix);
        if (fieldPos == std::string_view::npos) {
            return std::unexpected(UpdateError::InvalidResponse);
        }

        const auto startPos = fieldPos + fieldPrefix.length();
        const auto endPos = json.find('"', startPos);
        if (endPos == std::string_view::npos) {
            return std::unexpected(UpdateError::InvalidResponse);
        }

        return std::string(json.substr(startPos, endPos - startPos));
    }

    std::expected<Version, UpdateError> UpdateManager::ParseVersionFromJson(std::string_view json) const noexcept {
        auto versionStr = ParseJsonStringField(json, "tag_name");
        if (!versionStr) {
            return std::unexpected(UpdateError::VersionParsingFailed);
        }

        return Version(*versionStr);
    }

#ifdef EXPERIMENTAL_VERSION
    std::expected<std::string_view, UpdateError> UpdateManager::ExtractExperimentalAsset(
        std::string_view json, std::string_view assetName
    ) noexcept {
        std::string namePattern;
        namePattern.reserve(assetName.size() + 10);
        namePattern += "\"name\":\"";
        namePattern += assetName;
        namePattern += '"';

        const size_t namePos = json.find(namePattern);
        if (namePos == std::string_view::npos) return std::unexpected(UpdateError::InvalidResponse);

        const size_t objectStart = json.rfind('{', namePos);
        if (objectStart == std::string_view::npos) return std::unexpected(UpdateError::InvalidResponse);

        size_t braceCount = 1;
        size_t objectEnd = objectStart + 1;
        while (objectEnd < json.length() && braceCount > 0) {
            if (json[objectEnd] == '{')
                ++braceCount;
            else if (json[objectEnd] == '}')
                --braceCount;
            ++objectEnd;
        }
        if (braceCount != 0) return std::unexpected(UpdateError::InvalidResponse);
        return json.substr(objectStart, objectEnd - objectStart);
    }

    std::expected<void, UpdateError> UpdateManager::StoreExperimentalAsset(
        ExperimentalAssets& assets, std::string_view assetName, std::string_view object
    ) noexcept {
        const bool isMod = assetName == "HSEnhancer.dll";
        const bool isProxy = assetName == "winmm.dll";
        const bool isLauncher = assetName == "HSEnhancerLauncher.exe";
        if (!isMod && !isProxy && !isLauncher) return {};

        auto url = ParseJsonStringField(object, "browser_download_url");
        if (!url) return std::unexpected(UpdateError::InvalidResponse);

        if (isProxy) {
            assets.proxyUrl = std::move(*url);
            return {};
        }

        auto timestamp = ParseJsonStringField(object, "updated_at");
        if (!timestamp) return std::unexpected(UpdateError::InvalidResponse);

        if (isMod) {
            assets.modTimestamp = std::move(*timestamp);
            assets.modUrl = std::move(*url);
        } else if (isLauncher) {
            assets.launcherTimestamp = std::move(*timestamp);
            assets.launcherUrl = std::move(*url);
        }
        return {};
    }

    std::expected<UpdateManager::ExperimentalAssets, UpdateError> UpdateManager::ParseExperimentalAssets(
        std::string_view json
    ) noexcept {
        ExperimentalAssets assets;

        for (std::string_view assetName : {"HSEnhancer.dll", "winmm.dll", "HSEnhancerLauncher.exe"}) {
            auto object = ExtractExperimentalAsset(json, assetName);
            if (!object) return std::unexpected(object.error());

            if (auto stored = StoreExperimentalAsset(assets, assetName, *object); !stored) {
                return std::unexpected(stored.error());
            }
        }

        if (assets.modTimestamp.empty() || assets.modUrl.empty() || assets.proxyUrl.empty() ||
            assets.launcherTimestamp.empty() || assets.launcherUrl.empty()) {
            return std::unexpected(UpdateError::InvalidResponse);
        }

        return assets;
    }

    std::expected<ExperimentalUpdateInfo, UpdateError> UpdateManager::CheckForExperimentalUpdates() noexcept {
        ExperimentalUpdateInfo info;

        auto stableResult = CheckForUpdates();
        if (stableResult && stableResult->remoteVersion >= stableResult->currentVersion) {
            info.stableRelease = *stableResult;
            info.stableRelease->available = true;
            Logger::info(
                std::format("Stable release {} available for migration", stableResult->remoteVersion.ToString())
            );
            return info;
        }

        auto jsonResult = NetworkManager::Instance().DownloadToString(std::string(GITHUB_EXPERIMENTAL_API_URL));
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
        info.downloadUrlLauncher = std::move(assets->launcherUrl);

        const std::string storedModTimestamp =
            LauncherConfig::Instance().GetString("ExperimentalUpdate", "mod_timestamp", "").value_or("");
        const std::string storedLauncherTimestamp =
            LauncherConfig::Instance().GetString("ExperimentalUpdate", "launcher_timestamp", "").value_or("");

        info.modUpdateAvailable =
            !info.modTimestamp.empty() && (storedModTimestamp.empty() || info.modTimestamp > storedModTimestamp);

        info.launcherUpdateAvailable =
            !info.launcherTimestamp.empty() &&
            (storedLauncherTimestamp.empty() || info.launcherTimestamp > storedLauncherTimestamp);

        if (info.modUpdateAvailable) {
            Logger::info(std::format("Experimental mod update available. Timestamp: {}", info.modTimestamp));
        }

        if (info.launcherUpdateAvailable) {
            Logger::info(std::format("Experimental launcher update available. Timestamp: {}", info.launcherTimestamp));
        }

        return info;
    }

    std::expected<void, UpdateError> UpdateManager::DownloadAndInstallExperimentalMod(
        const ExperimentalUpdateInfo& info, const std::filesystem::path& gameBinPath
    ) noexcept {
        auto result = DownloadToTempAndInstall(info.downloadUrlMod, info.downloadUrlProxy, gameBinPath, 30000);
        if (!result) return result;

        auto configResult =
            LauncherConfig::Instance().SetString("ExperimentalUpdate", "mod_timestamp", info.modTimestamp);
        if (!configResult) {
            Logger::warn("Failed to save mod timestamp, update detection may not work correctly");
        }

        Logger::info("Experimental mod installed successfully");
        return {};
    }
#endif

}
