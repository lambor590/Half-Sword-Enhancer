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

#ifdef DEV_VERSION
        info.available = info.remoteVersion > info.currentVersion ||
            info.remoteVersion == info.currentVersion;
#else
        info.available = info.remoteVersion != info.currentVersion;
#endif

        if (info.available) {
            const auto versionStr = info.remoteVersion.ToString();
            info.downloadUrlLauncher = "https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v" +
                versionStr + "/HS_Enhancer_Launcher.exe";
            info.downloadUrlMod = "https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v" +
                versionStr + "/HS-Enhancer.dll";
            hse::Logger::info("Update available: " + versionStr);
        }

        return info;
    }

    std::expected<void, UpdateError> UpdateManager::UpdateMod(const Version& version) noexcept {
        try {
            const auto versionStr = version.ToString();
            const auto downloadUrl = "https://github.com/lambor590/Half-Sword-Enhancer/releases/download/v" +
                versionStr + "/HS-Enhancer.dll";

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

    std::expected<void, UpdateError> UpdateManager::UpdateLauncher(std::string_view downloadUrl) noexcept {
        try {
            std::array<char, MAX_PATH> currentPath{};
            if (!GetModuleFileNameA(nullptr, currentPath.data(), MAX_PATH)) {
                Logger::error("Failed to get current executable path");
                return std::unexpected(UpdateError::FileSystemError);
            }

            const std::string currentExePath{ currentPath.data() };
            const auto appDataPath = getAppDataPath();
            const auto tempPath = std::filesystem::path(appDataPath) / "HS_Enhancer_Launcher_Update.exe";
            const auto batchPath = std::filesystem::path(appDataPath) / "HS_Enhancer_Update.bat";

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
                              CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo)) {
                Logger::error("Failed to start update script");
                return std::unexpected(UpdateError::UpdateFailed);
            }

            CloseHandle(processInfo.hProcess);
            CloseHandle(processInfo.hThread);

            Logger::info("Update script started successfully. Launcher will restart automatically.");
            
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

}