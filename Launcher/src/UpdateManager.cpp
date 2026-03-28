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

    std::string Version::ToCompactString() const {
        return std::format("{}{}{}", major_, minor_, patch_);
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
        try {
            const auto tempDir = std::filesystem::path(getAppDataPath()) / TEMP_FOLDER;
            std::filesystem::create_directories(tempDir);

            auto modResult = DownloadModToPath(modUrl, tempDir / MOD_FILENAME, modMinSize);
            if (!modResult) {
                std::filesystem::remove_all(tempDir);
                return modResult;
            }

            if (!proxyUrl.empty()) {
                auto proxyResult = DownloadModToPath(proxyUrl, tempDir / PROXY_FILENAME, 10000);
                if (!proxyResult) {
                    Logger::warn("Proxy not available in this release, skipping");
                }
            }

            auto installResult = InstallManager::Instance().InstallFiles(tempDir, gameBinPath);
            std::filesystem::remove_all(tempDir);

            if (!installResult) {
                return std::unexpected(UpdateError::FileSystemError);
            }

            return {};
        } catch (...) {
            return std::unexpected(UpdateError::UpdateFailed);
        }
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
        try {
            const auto tempPath = outputPath.string() + ".tmp";

            DownloadConfig config{
                .url = std::string(downloadUrl),
                .outputPath = tempPath,
                .description = "Downloading mod",
                .minFileSize = minFileSize
            };

            auto result = NetworkManager::Instance().DownloadFile(config);
            if (!result) {
                std::filesystem::remove(tempPath);
                return std::unexpected(UpdateError::NetworkError);
            }

            try {
                std::filesystem::rename(tempPath, outputPath);
            } catch (...) {
                std::filesystem::remove(tempPath);
                return std::unexpected(UpdateError::FileSystemError);
            }
            return {};
        } catch (...) {
            return std::unexpected(UpdateError::UpdateFailed);
        }
    }

    std::expected<void, UpdateError> UpdateManager::UpdateLauncher(
        std::string_view downloadUrl, std::string_view timestamp
    ) noexcept {
        try {
            std::array<char, MAX_PATH> currentPath{};
            if (!GetModuleFileNameA(nullptr, currentPath.data(), MAX_PATH)) {
                Logger::error("Failed to get current executable path");
                return std::unexpected(UpdateError::FileSystemError);
            }

            const std::string currentExePath{currentPath.data()};
            const auto& appDataPath = getAppDataPath();
            const auto tempPath = std::filesystem::path(appDataPath) / "HSEnhancerLauncher_Update.exe";
            const auto batchPath = std::filesystem::path(appDataPath) / "HSEnhancer_Update.bat";

            Logger::info("Downloading launcher update...");

            DownloadConfig config{
                .url = std::string(downloadUrl),
                .outputPath = tempPath.string(),
                .description = "Downloading launcher update",
                .minFileSize = 50000
            };

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
                tempStr, currentExePath, currentExePath, currentExePath, batchStr
            );

            batchFile.write(script.data(), static_cast<std::streamsize>(script.size()));
            batchFile.close();

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
                (void)LauncherConfig::Instance().SetString("ExperimentalUpdate", "launcher_timestamp", timestamp);
            }

            ExitProcess(0);

        } catch (const std::exception& e) {
            Logger::error(std::format("Exception during launcher update: {}", e.what()));
            return std::unexpected(UpdateError::UpdateFailed);
        } catch (...) {
            Logger::error("Unknown error during launcher update");
            return std::unexpected(UpdateError::UpdateFailed);
        }
    }

    std::expected<Version, UpdateError> UpdateManager::ExtractVersionFromExecutable() const noexcept {
        try {
            std::array<char, MAX_PATH> filePath{};
            if (!GetModuleFileNameA(nullptr, filePath.data(), MAX_PATH)) {
                return std::unexpected(UpdateError::FileSystemError);
            }

            return ExtractVersionFromFile(filePath.data());
        } catch (...) {
            return std::unexpected(UpdateError::VersionParsingFailed);
        }
    }

    std::expected<Version, UpdateError> UpdateManager::ExtractVersionFromFile(const std::filesystem::path& filePath
    ) noexcept {
        try {
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
        } catch (...) {
            return std::unexpected(UpdateError::VersionParsingFailed);
        }
    }

    std::expected<std::string, UpdateError> UpdateManager::FetchGitHubReleaseInfo() const noexcept {
        auto result = NetworkManager::Instance().DownloadToString(std::string(GITHUB_API_URL));
        if (!result) {
            return std::unexpected(UpdateError::NetworkError);
        }

        return std::move(*result);
    }

    std::expected<Version, UpdateError> UpdateManager::ParseVersionFromJson(std::string_view json) const noexcept {
        try {
            constexpr std::string_view tagPrefix = "\"tag_name\":\"";
            const auto tagPos = json.find(tagPrefix);

            if (tagPos == std::string_view::npos) {
                return std::unexpected(UpdateError::VersionParsingFailed);
            }

            const auto startPos = tagPos + tagPrefix.length();
            const auto endPos = json.find("\"", startPos);

            if (endPos == std::string_view::npos) {
                return std::unexpected(UpdateError::VersionParsingFailed);
            }

            const auto versionStr = json.substr(startPos, endPos - startPos);
            return Version(versionStr);
        } catch (...) {
            return std::unexpected(UpdateError::VersionParsingFailed);
        }
    }

#ifdef EXPERIMENTAL_VERSION
    std::expected<std::string_view, UpdateError> UpdateManager::ExtractAssetObject(
        std::string_view json, std::string_view assetName
    ) const noexcept {
        try {
            auto searchPattern = std::format("\"name\":\"{}\"", assetName);
            const auto assetPos = json.find(searchPattern);
            if (assetPos == std::string_view::npos) {
                Logger::warn(std::format("Asset not found: {}", assetName));
                return std::unexpected(UpdateError::InvalidResponse);
            }

            const auto objectStart = json.rfind('{', assetPos);
            if (objectStart == std::string_view::npos) {
                return std::unexpected(UpdateError::InvalidResponse);
            }

            size_t braceCount = 1;
            size_t objectEnd = objectStart + 1;
            while (objectEnd < json.length() && braceCount > 0) {
                if (json[objectEnd] == '{')
                    braceCount++;
                else if (json[objectEnd] == '}')
                    braceCount--;
                objectEnd++;
            }

            return json.substr(objectStart, objectEnd - objectStart);
        } catch (...) {
            return std::unexpected(UpdateError::InvalidResponse);
        }
    }

    std::expected<std::string, UpdateError> UpdateManager::ParseAssetField(
        std::string_view assetObject, std::string_view fieldName
    ) const noexcept {
        try {
            auto fieldPrefix = std::format("\"{}\":\"", fieldName);
            const auto fieldPos = assetObject.find(fieldPrefix);
            if (fieldPos == std::string_view::npos) {
                return std::unexpected(UpdateError::InvalidResponse);
            }

            const auto startPos = fieldPos + fieldPrefix.length();
            const auto endPos = assetObject.find('"', startPos);
            if (endPos == std::string_view::npos) {
                return std::unexpected(UpdateError::InvalidResponse);
            }

            return std::string(assetObject.substr(startPos, endPos - startPos));
        } catch (...) {
            return std::unexpected(UpdateError::InvalidResponse);
        }
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

        if (auto modAsset = ExtractAssetObject(json, "HSEnhancer.dll")) {
            if (auto timestamp = ParseAssetField(*modAsset, "updated_at")) {
                info.modTimestamp = *timestamp;
            }
            if (auto url = ParseAssetField(*modAsset, "browser_download_url")) {
                info.downloadUrlMod = *url;
            }
        }

        if (auto proxyAsset = ExtractAssetObject(json, "winmm.dll")) {
            if (auto url = ParseAssetField(*proxyAsset, "browser_download_url")) {
                info.downloadUrlProxy = *url;
            }
        }

        if (auto launcherAsset = ExtractAssetObject(json, "HSEnhancerLauncher.exe")) {
            if (auto timestamp = ParseAssetField(*launcherAsset, "updated_at")) {
                info.launcherTimestamp = *timestamp;
            }
            if (auto url = ParseAssetField(*launcherAsset, "browser_download_url")) {
                info.downloadUrlLauncher = *url;
            }
        }

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
