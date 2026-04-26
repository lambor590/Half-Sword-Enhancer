#include <fstream>
#include <string>
#include <algorithm>
#include <Windows.h>

#include "../include/SteamLocator.h"
#include "../include/Logger.h"

namespace hse {
    namespace {

        [[nodiscard]] std::string ExtractQuotedToken(std::string_view line, std::size_t tokenIndex) {
            std::size_t searchFrom = 0;

            for (std::size_t currentIndex = 0; currentIndex <= tokenIndex; ++currentIndex) {
                const auto tokenStart = line.find('"', searchFrom);
                if (tokenStart == std::string_view::npos) {
                    return {};
                }

                const auto tokenEnd = line.find('"', tokenStart + 1);
                if (tokenEnd == std::string_view::npos) {
                    return {};
                }

                if (currentIndex == tokenIndex) {
                    return std::string(line.substr(tokenStart + 1, tokenEnd - tokenStart - 1));
                }

                searchFrom = tokenEnd + 1;
            }

            return {};
        }

        [[nodiscard]] bool LibraryContainsAppId(
            const std::vector<std::string>& appIds, std::string_view appId
        ) noexcept {
            return std::ranges::find(appIds, appId) != appIds.end();
        }

        [[nodiscard]] bool DirectoryContainsExecutable(const std::filesystem::path& directory) {
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (entry.path().extension() == ".exe") {
                    return true;
                }
            }

            return false;
        }

    }

    std::expected<GameLocation, SteamError> SteamLocator::LocateGame() noexcept {
        auto steamPath = ReadSteamInstallPath();
        if (!steamPath) {
            return std::unexpected(steamPath.error());
        }

        auto vdfPath = std::filesystem::path(*steamPath) / "steamapps" / "libraryfolders.vdf";
        auto libraries = ParseLibraryFolders(vdfPath);
        if (!libraries) {
            return std::unexpected(libraries.error());
        }

        std::expected<GameLocation, SteamError> demoResult = std::unexpected(SteamError::GameNotFound);

        for (const auto& library : *libraries) {
            if (LibraryContainsAppId(library.appIds, FULL_GAME_APP_ID)) {
                auto result = ResolveGamePath(library.path, GameEdition::FullGame);
                if (result) {
                    return result;
                }
            }

            if (LibraryContainsAppId(library.appIds, DEMO_APP_ID)) {
                if (auto result = ResolveGamePath(library.path, GameEdition::Demo)) {
                    demoResult = std::move(result);
                }
            }
        }

        if (demoResult) {
            return demoResult;
        }

        return std::unexpected(SteamError::GameNotFound);
    }

    std::expected<GameLocation, SteamError> SteamLocator::LocateGameAt(const std::filesystem::path& manualPath
    ) noexcept {
        auto win64Dir = FindWin64Directory(manualPath);
        if (win64Dir.empty()) {
            return std::unexpected(SteamError::PathDoesNotExist);
        }

        std::error_code ec;
        if (!std::filesystem::exists(win64Dir, ec)) {
            if (ec) {
                Logger::error("Failed to inspect Win64 directory: %s", ec.message().c_str());
            }
            return std::unexpected(SteamError::PathDoesNotExist);
        }

        if (!DirectoryContainsExecutable(win64Dir)) {
            return std::unexpected(SteamError::PathDoesNotExist);
        }

        return GameLocation{.binariesPath = win64Dir, .edition = DetectEditionFromPath(win64Dir)};
    }

    std::expected<std::string, SteamError> SteamLocator::ReadSteamInstallPath() const noexcept {
        HKEY hKey;
        LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, STEAM_REGISTRY_KEY, 0, KEY_READ, &hKey);
        if (result != ERROR_SUCCESS) {
            Logger::error("Steam registry key not found");
            return std::unexpected(SteamError::SteamNotInstalled);
        }

        struct RegKeyGuard {
            HKEY key;
            ~RegKeyGuard() { RegCloseKey(key); }
        } guard{hKey};

        char buffer[MAX_PATH]{};
        DWORD bufferSize = sizeof(buffer);
        DWORD type = REG_SZ;

        result =
            RegQueryValueExA(hKey, STEAM_INSTALL_VALUE, nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &bufferSize);
        if (result != ERROR_SUCCESS) {
            Logger::error("Steam InstallPath registry value not found");
            return std::unexpected(SteamError::RegistryNotFound);
        }

        std::string installPath(buffer);
        std::error_code ec;
        if (installPath.empty() || !std::filesystem::exists(installPath, ec)) {
            Logger::error("Steam install path does not exist: %s", installPath.c_str());
            return std::unexpected(SteamError::SteamNotInstalled);
        }

        return installPath;
    }

    std::expected<std::vector<SteamLocator::LibraryFolder>, SteamError> SteamLocator::ParseLibraryFolders(
        const std::filesystem::path& vdfPath
    ) const noexcept {
        std::ifstream file(vdfPath);
        if (!file.is_open()) {
            Logger::error("Failed to open libraryfolders.vdf: %s", vdfPath.string().c_str());
            return std::unexpected(SteamError::VdfParsingFailed);
        }

        std::vector<LibraryFolder> libraries;
        std::string line;
        int braceDepth = 0;
        bool insideApps = false;
        LibraryFolder current;

        while (std::getline(file, line)) {
            if (line.find('{') != std::string::npos) {
                braceDepth++;
                continue;
            }

            if (line.find('}') != std::string::npos) {
                braceDepth--;

                if (insideApps && braceDepth == 2) {
                    insideApps = false;
                }

                if (braceDepth == 1 && !current.path.empty()) {
                    libraries.push_back(std::move(current));
                    current = {};
                }
                continue;
            }

            std::string key = ExtractQuotedToken(line, 0);
            if (key.empty()) continue;

            if (braceDepth == 2 && key == "path") {
                std::string value = ExtractQuotedToken(line, 1);
                if (!value.empty()) {
                    current.path = value;
                }
            }

            if (braceDepth == 2 && key == "apps") {
                insideApps = true;
                continue;
            }

            if (insideApps && braceDepth == 3) {
                current.appIds.push_back(key);
            }
        }

        if (libraries.empty()) {
            Logger::error("No Steam library folders found in VDF");
            return std::unexpected(SteamError::VdfParsingFailed);
        }

        return libraries;
    }

    std::expected<GameLocation, SteamError> SteamLocator::ResolveGamePath(
        const std::filesystem::path& libraryPath, GameEdition edition
    ) const noexcept {
        const char* gameFolder = (edition == GameEdition::FullGame) ? FULL_GAME_FOLDER : DEMO_GAME_FOLDER;
        auto binariesPath = libraryPath / "steamapps" / "common" / gameFolder / BINARIES_SUBPATH;

        std::error_code ec;
        if (!std::filesystem::exists(binariesPath, ec)) {
            if (ec) {
                Logger::error("Failed to inspect game binaries path: %s", ec.message().c_str());
            }
            return std::unexpected(SteamError::PathDoesNotExist);
        }

        return GameLocation{.binariesPath = std::move(binariesPath), .edition = edition};
    }

    GameEdition SteamLocator::DetectEditionFromPath(const std::filesystem::path& path) noexcept {
        std::string pathStr = path.string();
        std::ranges::transform(pathStr, pathStr.begin(), ::tolower);
        if (pathStr.find("demo") != std::string::npos) {
            return GameEdition::Demo;
        }

        return GameEdition::FullGame;
    }

    std::filesystem::path SteamLocator::FindWin64Directory(const std::filesystem::path& basePath) noexcept {
        if (basePath.filename() == "Win64") {
            return basePath;
        }

        std::error_code ec;
        auto win64Under = basePath / BINARIES_SUBPATH;
        if (std::filesystem::exists(win64Under, ec)) {
            return win64Under;
        }

        if (ec) {
            Logger::warn("Failed to inspect candidate Win64 path: %s", ec.message().c_str());
        }

        for (auto current = basePath; current.has_parent_path() && current != current.root_path();
             current = current.parent_path()) {
            if (current.filename() == "Win64") {
                return current;
            }
        }

        return {};
    }

}
