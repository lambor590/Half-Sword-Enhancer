#include <sstream>
#include <vector>
#include <array>
#include <charconv>
#include <fstream>
#include <Windows.h>

#include "../include/UpdateManager.h"
#include "../include/NetworkManager.h"
#include "../include/LauncherConfig.h"
#include "../include/Logger.h"
#include "../include/Util.h"

namespace hse {

    Version::Version(std::string_view versionString) noexcept {
        if (versionString.empty() || versionString == "0.0.0") return;

        std::string cleanVersion{ versionString };
        if (!cleanVersion.empty() && cleanVersion[0] == 'v') {
            cleanVersion = cleanVersion.substr(1);
        }

        std::array<std::uint16_t*, 3> components{ &major_, &minor_, &patch_ };
        size_t start = 0;

        for (size_t i = 0; i < 3; ++i) {
            const auto dotPos = cleanVersion.find('.', start);
            const auto end = (dotPos != std::string::npos) ? dotPos : cleanVersion.length();

            if (start < end) {
                const auto segment = cleanVersion.substr(start, end - start);
                std::from_chars(segment.data(), segment.data() + segment.size(), *components[i]);
            }

            if (dotPos == std::string::npos) break;
            start = dotPos + 1;
        }
    }

    std::string Version::ToString() const {
        return std::to_string(major_) + "." + std::to_string(minor_) + "." + std::to_string(patch_);
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
            info.downloadUrlLauncher = "https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v" +
                versionStr + "/HSEnhancerLauncher.exe";
            info.downloadUrlMod = "https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v" +
                versionStr + "/HSEnhancer.dll";
            hse::Logger::info("Update available: " + versionStr);
        }

        return info;
    }

    std::expected<void, UpdateError> UpdateManager::UpdateMod(const Version& version) noexcept {
        try {
            const auto versionStr = version.ToString();
            const auto downloadUrl = "https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v" +
                versionStr + "/HSEnhancer.dll";

            const auto modPath = LauncherConfig::GetModFilePath();

            DownloadConfig config{
                .url = downloadUrl,
                .outputPath = modPath.string(),
                .description = "Downloading mod update",
                .minFileSize = 300000
            };

            auto result = NetworkManager::Instance().DownloadFile(config);
            if (!result) {
                return std::unexpected(UpdateError::NetworkError);
            }

            hse::Logger::info("Mod updated successfully");
            return {};
        }
        catch (...) {
            return std::unexpected(UpdateError::UpdateFailed);
        }
    }

    std::expected<void, UpdateError> UpdateManager::UpdateLauncher(
        std::string_view downloadUrl,
        std::string_view timestamp
    ) noexcept {
        try {
            std::array<char, MAX_PATH> currentPath{};
            if (!GetModuleFileNameA(nullptr, currentPath.data(), MAX_PATH)) {
                Logger::error("Failed to get current executable path");
                return std::unexpected(UpdateError::FileSystemError);
            }

            const std::string currentExePath{ currentPath.data() };
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

            batchFile << "@echo off\n";
            batchFile << "echo Updating Half Sword Enhancer Launcher...\n";
            batchFile << "timeout /t 2 /nobreak >nul\n";
            batchFile << "move \"" << tempPath.string() << "\" \"" << currentExePath << "\"\n";
            batchFile << "if exist \"" << currentExePath << "\" (\n";
            batchFile << "    echo Update completed successfully!\n";
            batchFile << "    echo Starting updated launcher...\n";
            batchFile << "    start \"\" \"" << currentExePath << "\"\n";
            batchFile << ") else (\n";
            batchFile << "    echo Update failed! Please download manually.\n";
            batchFile << "    pause\n";
            batchFile << ")\n";
            batchFile << "del \"" << batchPath.string() << "\"\n";
            batchFile.close();

            Logger::info("Launching update script and exiting...");
            
            STARTUPINFOA startupInfo{};
            PROCESS_INFORMATION processInfo{};
            startupInfo.cb = sizeof(startupInfo);
            
            std::string cmdLine = "cmd.exe /c \"" + batchPath.string() + "\"";
            
            if (!CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, FALSE,
                              0, nullptr, nullptr, &startupInfo, &processInfo)) {
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
            Logger::error("Exception during launcher update: " + std::string(e.what()));
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

            const DWORD verSize = GetFileVersionInfoSizeA(filePath.data(), nullptr);
            if (verSize == 0) {
                return std::unexpected(UpdateError::VersionParsingFailed);
            }

            std::vector<BYTE> verData(verSize);
            VS_FIXEDFILEINFO* fileInfo = nullptr;
            UINT size = 0;

            if (!GetFileVersionInfoA(filePath.data(), 0, verSize, verData.data()) ||
                !VerQueryValueA(verData.data(), "\\", reinterpret_cast<void**>(&fileInfo), &size)) {
                return std::unexpected(UpdateError::VersionParsingFailed);
            }

            const auto major = static_cast<std::uint16_t>(HIWORD(fileInfo->dwFileVersionMS));
            const auto minor = static_cast<std::uint16_t>(LOWORD(fileInfo->dwFileVersionMS));
            const auto patch = static_cast<std::uint16_t>(HIWORD(fileInfo->dwFileVersionLS));

            return Version(std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch));
        }
        catch (...) {
            return std::unexpected(UpdateError::VersionParsingFailed);
        }
    }

    std::expected<std::string, UpdateError> UpdateManager::FetchGitHubReleaseInfo() const noexcept {
        auto result = NetworkManager::Instance().DownloadToString(std::string(GITHUB_API_URL));
        if (!result) {
            return std::unexpected(UpdateError::NetworkError);
        }
        
        return *result;
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
        }
        catch (...) {
            return std::unexpected(UpdateError::VersionParsingFailed);
        }
    }

#ifdef EXPERIMENTAL_VERSION
    std::expected<std::string_view, UpdateError> UpdateManager::ExtractAssetObject(
        std::string_view json,
        std::string_view assetName
    ) const noexcept {
        try {
            const std::string searchPattern = "\"name\":\"" + std::string(assetName) + "\"";
            const auto assetPos = json.find(searchPattern);
            if (assetPos == std::string_view::npos) {
                Logger::warn("Asset not found: " + std::string(assetName));
                return std::unexpected(UpdateError::InvalidResponse);
            }

            const auto objectStart = json.rfind('{', assetPos);
            if (objectStart == std::string_view::npos) {
                return std::unexpected(UpdateError::InvalidResponse);
            }

            size_t braceCount = 1;
            size_t objectEnd = objectStart + 1;
            while (objectEnd < json.length() && braceCount > 0) {
                if (json[objectEnd] == '{') braceCount++;
                else if (json[objectEnd] == '}') braceCount--;
                objectEnd++;
            }

            return json.substr(objectStart, objectEnd - objectStart);
        }
        catch (...) {
            return std::unexpected(UpdateError::InvalidResponse);
        }
    }

    std::expected<std::string, UpdateError> UpdateManager::ParseAssetField(
        std::string_view assetObject,
        std::string_view fieldName
    ) const noexcept {
        try {
            const std::string fieldPrefix = "\"" + std::string(fieldName) + "\":\"";
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
        }
        catch (...) {
            return std::unexpected(UpdateError::InvalidResponse);
        }
    }

    std::expected<ExperimentalUpdateInfo, UpdateError> UpdateManager::CheckForExperimentalUpdates() noexcept {
        ExperimentalUpdateInfo info;

        // === STEP 1: Check stable releases (priority) ===
        auto stableResult = CheckForUpdates();
        if (stableResult && stableResult->remoteVersion >= stableResult->currentVersion) {
            info.stableRelease = *stableResult;
            info.stableRelease->available = true;
            Logger::info("Stable release " + stableResult->remoteVersion.ToString() + " available for migration");
            return info;
        }

        // === STEP 2: Fallback to experimental-latest (timestamp-based) ===
        auto jsonResult = NetworkManager::Instance().DownloadToString(std::string(GITHUB_EXPERIMENTAL_API_URL));
        if (!jsonResult) {
            Logger::error("Failed to fetch experimental release info from GitHub");
            return std::unexpected(UpdateError::NetworkError);
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

        info.modUpdateAvailable = !info.modTimestamp.empty() &&
            (storedModTimestamp.empty() || info.modTimestamp > storedModTimestamp);

        info.launcherUpdateAvailable = !info.launcherTimestamp.empty() &&
            (storedLauncherTimestamp.empty() || info.launcherTimestamp > storedLauncherTimestamp);

        if (info.modUpdateAvailable) {
            Logger::info("Experimental mod update available. Timestamp: " + info.modTimestamp);
        }

        if (info.launcherUpdateAvailable) {
            Logger::info("Experimental launcher update available. Timestamp: " + info.launcherTimestamp);
        }

        return info;
    }

    std::expected<void, UpdateError> UpdateManager::UpdateExperimentalMod(
        std::string_view downloadUrl,
        std::string_view timestamp
    ) noexcept {
        try {
            const auto modPath = LauncherConfig::GetModFilePath();

            DownloadConfig config{
                .url = std::string(downloadUrl),
                .outputPath = modPath.string(),
                .description = "Downloading experimental mod update",
                .minFileSize = 30000
            };

            auto result = NetworkManager::Instance().DownloadFile(config);
            if (!result) {
                return std::unexpected(UpdateError::NetworkError);
            }

            auto configResult = LauncherConfig::Instance().SetString("ExperimentalUpdate", "mod_timestamp", timestamp);
            if (!configResult) {
                Logger::warn("Failed to save mod timestamp, update detection may not work correctly");
            }

            Logger::info("Experimental mod updated successfully");
            return {};
        }
        catch (...) {
            return std::unexpected(UpdateError::UpdateFailed);
        }
    }
#endif

}