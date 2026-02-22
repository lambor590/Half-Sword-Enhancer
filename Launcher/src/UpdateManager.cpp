#include <vector>
#include <array>
#include <charconv>
#include <cstdio>
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

        if (versionString.front() == 'v') {
            versionString.remove_prefix(1);
        }

        std::uint16_t* components[] = { &major_, &minor_, &patch_ };

        for (size_t i = 0; i < 3 && !versionString.empty(); ++i) {
            const auto dotPos = versionString.find('.');
            const auto segEnd = (dotPos != std::string_view::npos) ? dotPos : versionString.size();
            std::from_chars(versionString.data(), versionString.data() + segEnd, *components[i]);
            if (dotPos == std::string_view::npos) break;
            versionString.remove_prefix(dotPos + 1);
        }
    }

    std::string Version::ToString() const {
        char buf[18];
        const auto len = std::snprintf(buf, sizeof(buf), "%u.%u.%u", major_, minor_, patch_);
        return std::string(buf, static_cast<size_t>(len));
    }

    std::string Version::ToCompactString() const {
        char buf[16];
        const auto len = std::snprintf(buf, sizeof(buf), "%u%u%u", major_, minor_, patch_);
        return std::string(buf, static_cast<size_t>(len));
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
            constexpr std::string_view baseUrl = "https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v";

            info.downloadUrlLauncher.reserve(baseUrl.size() + versionStr.size() + 24);
            info.downloadUrlLauncher.append(baseUrl);
            info.downloadUrlLauncher.append(versionStr);
            info.downloadUrlLauncher.append("/HSEnhancerLauncher.exe");

            info.downloadUrlMod.reserve(baseUrl.size() + versionStr.size() + 16);
            info.downloadUrlMod.append(baseUrl);
            info.downloadUrlMod.append(versionStr);
            info.downloadUrlMod.append("/HSEnhancer.dll");

            hse::Logger::info("Update available: " + versionStr);
        }

        return info;
    }

    std::expected<void, UpdateError> UpdateManager::DownloadModToPath(
        std::string_view downloadUrl,
        const std::filesystem::path& outputPath,
        std::uint32_t minFileSize
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
            }
            catch (...) {
                std::filesystem::remove(tempPath);
                return std::unexpected(UpdateError::FileSystemError);
            }
            return {};
        }
        catch (...) {
            return std::unexpected(UpdateError::UpdateFailed);
        }
    }

    std::expected<void, UpdateError> UpdateManager::UpdateMod(const Version& version) noexcept {
        try {
            const auto versionStr = version.ToString();
            std::string downloadUrl;
            downloadUrl.reserve(80 + versionStr.size());
            downloadUrl.append("https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v");
            downloadUrl.append(versionStr);
            downloadUrl.append("/HSEnhancer.dll");

            const auto cachePath = LauncherConfig::GetOfficialDllPath(GameMode::FullGame, version);
            auto result = DownloadModToPath(downloadUrl, cachePath);
            if (!result) return result;

            (void)LauncherConfig::Instance().SetString("DLL", "official_version", versionStr);
            CleanupOldVersions(GameMode::FullGame, version);

            hse::Logger::info("Mod updated successfully");
            return {};
        }
        catch (...) {
            return std::unexpected(UpdateError::UpdateFailed);
        }
    }

    void UpdateManager::CleanupCachedDlls(std::string_view prefix, std::string_view keepFilename) noexcept {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(LauncherConfig::GetCacheDir())) {
                if (!entry.is_regular_file()) continue;
                const auto& path = entry.path();
                const auto filename = path.filename().string();
                if (filename != keepFilename && filename.starts_with(prefix) && filename.ends_with(".dll")) {
                    std::filesystem::remove(path);
                    Logger::info("Cleaned up old version: " + filename);
                }
            }
        }
        catch (...) {}
    }

    void UpdateManager::CleanupOldVersions(GameMode mode, const Version& keepVersion) noexcept {
        const std::string_view prefix = (mode == GameMode::Demo) ? "HSEnhancer_demo_v" : "HSEnhancer_v";
        const auto compact = keepVersion.ToCompactString();
        std::string keepFilename;
        keepFilename.reserve(prefix.size() + compact.size() + 4);
        keepFilename.append(prefix);
        keepFilename.append(compact);
        keepFilename.append(".dll");
        CleanupCachedDlls(prefix, keepFilename);
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

            const auto tempStr = tempPath.string();
            const auto batchStr = batchPath.string();

            std::string script;
            script.reserve(512);
            script.append("@echo off\n"
                "echo Updating Half Sword Enhancer Launcher...\n"
                "timeout /t 2 /nobreak >nul\n"
                "move \"");
            script.append(tempStr);
            script.append("\" \"");
            script.append(currentExePath);
            script.append("\"\nif exist \"");
            script.append(currentExePath);
            script.append("\" (\n"
                "    echo Update completed successfully!\n"
                "    echo Starting updated launcher...\n"
                "    start \"\" \"");
            script.append(currentExePath);
            script.append("\"\n) else (\n"
                "    echo Update failed! Please download manually.\n"
                "    pause\n)\ndel \"");
            script.append(batchStr);
            script.append("\"\n");

            batchFile.write(script.data(), static_cast<std::streamsize>(script.size()));
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

            return Version(
                static_cast<std::uint16_t>(HIWORD(fileInfo->dwFileVersionMS)),
                static_cast<std::uint16_t>(LOWORD(fileInfo->dwFileVersionMS)),
                static_cast<std::uint16_t>(HIWORD(fileInfo->dwFileVersionLS))
            );
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
            constexpr std::string_view patternPrefix = "\"name\":\"";
            std::string searchPattern;
            searchPattern.reserve(patternPrefix.size() + assetName.size() + 1);
            searchPattern.append(patternPrefix);
            searchPattern.append(assetName);
            searchPattern.push_back('"');
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
            std::string fieldPrefix;
            fieldPrefix.reserve(fieldName.size() + 4);
            fieldPrefix.push_back('"');
            fieldPrefix.append(fieldName);
            fieldPrefix.append("\":\"");
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
            const auto sanitized = SanitizeTimestamp(timestamp);
            const auto cachePath = LauncherConfig::GetExperimentalDllPath(sanitized);

            auto result = DownloadModToPath(downloadUrl, cachePath, 30000);
            if (!result) {
                return std::unexpected(UpdateError::NetworkError);
            }

            auto configResult = LauncherConfig::Instance().SetString("ExperimentalUpdate", "mod_timestamp", timestamp);
            if (!configResult) {
                Logger::warn("Failed to save mod timestamp, update detection may not work correctly");
            }

            CleanupOldExperimentalVersions(sanitized);

            Logger::info("Experimental mod updated successfully");
            return {};
        }
        catch (...) {
            return std::unexpected(UpdateError::UpdateFailed);
        }
    }

    void UpdateManager::CleanupOldExperimentalVersions(std::string_view keepTimestamp) noexcept {
        constexpr std::string_view prefix = "HSEnhancer_exp_";
        std::string keepFilename;
        keepFilename.reserve(prefix.size() + keepTimestamp.size() + 4);
        keepFilename.append(prefix);
        keepFilename.append(keepTimestamp);
        keepFilename.append(".dll");
        CleanupCachedDlls(prefix, keepFilename);
    }
#endif

}