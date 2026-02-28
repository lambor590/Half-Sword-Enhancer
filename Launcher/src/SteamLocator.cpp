#include <fstream>
#include <string>
#include <algorithm>
#include <Windows.h>

#include "../include/SteamLocator.h"
#include "../include/Logger.h"

namespace hse {

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
            bool hasFullGame = std::ranges::find(library.appIds, FULL_GAME_APP_ID) != library.appIds.end();
            bool hasDemo = std::ranges::find(library.appIds, DEMO_APP_ID) != library.appIds.end();

            if (hasFullGame) {
                auto result = ResolveGamePath(library.path, GameEdition::FullGame);
                if (result) {
                    return result;
                }
            }

            if (hasDemo && !demoResult) {
                demoResult = ResolveGamePath(library.path, GameEdition::Demo);
            }
        }

        if (demoResult) {
            return demoResult;
        }

        return std::unexpected(SteamError::GameNotFound);
    }

    std::expected<GameLocation, SteamError> SteamLocator::LocateGameAt(
        const std::filesystem::path& manualPath
    ) noexcept {
        try {
            auto win64Dir = FindWin64Directory(manualPath);
            if (win64Dir.empty() || !std::filesystem::exists(win64Dir)) {
                return std::unexpected(SteamError::PathDoesNotExist);
            }

            bool hasExecutable = false;
            for (const auto& entry : std::filesystem::directory_iterator(win64Dir)) {
                if (entry.path().extension() == ".exe") {
                    hasExecutable = true;
                    break;
                }
            }

            if (!hasExecutable) {
                return std::unexpected(SteamError::PathDoesNotExist);
            }

            return GameLocation{
                .binariesPath = win64Dir,
                .edition = DetectEditionFromPath(win64Dir)
            };
        }
        catch (...) {
            return std::unexpected(SteamError::PathDoesNotExist);
        }
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
        } guard{ hKey };

        char buffer[MAX_PATH]{};
        DWORD bufferSize = sizeof(buffer);
        DWORD type = REG_SZ;

        result = RegQueryValueExA(hKey, STEAM_INSTALL_VALUE, nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &bufferSize);
        if (result != ERROR_SUCCESS) {
            Logger::error("Steam InstallPath registry value not found");
            return std::unexpected(SteamError::RegistryNotFound);
        }

        std::string installPath(buffer);
        if (installPath.empty() || !std::filesystem::exists(installPath)) {
            Logger::error("Steam install path does not exist: %s", installPath.c_str());
            return std::unexpected(SteamError::SteamNotInstalled);
        }

        return installPath;
    }

    std::expected<std::vector<SteamLocator::LibraryFolder>, SteamError> SteamLocator::ParseLibraryFolders(
        const std::filesystem::path& vdfPath
    ) const noexcept {
        try {
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

            auto extractQuotedValue = [](const std::string& str) -> std::string {
                size_t first = str.find('"');
                if (first == std::string::npos) return {};
                size_t second = str.find('"', first + 1);
                if (second == std::string::npos) return {};
                size_t third = str.find('"', second + 1);
                if (third == std::string::npos) return {};
                size_t fourth = str.find('"', third + 1);
                if (fourth == std::string::npos) return {};
                return str.substr(third + 1, fourth - third - 1);
            };

            auto extractKey = [](const std::string& str) -> std::string {
                size_t first = str.find('"');
                if (first == std::string::npos) return {};
                size_t second = str.find('"', first + 1);
                if (second == std::string::npos) return {};
                return str.substr(first + 1, second - first - 1);
            };

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

                std::string key = extractKey(line);
                if (key.empty()) continue;

                if (braceDepth == 2 && key == "path") {
                    std::string value = extractQuotedValue(line);
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
        catch (const std::exception& e) {
            Logger::error("VDF parsing error: %s", e.what());
            return std::unexpected(SteamError::VdfParsingFailed);
        }
        catch (...) {
            return std::unexpected(SteamError::VdfParsingFailed);
        }
    }

    std::expected<GameLocation, SteamError> SteamLocator::ResolveGamePath(
        const std::filesystem::path& libraryPath,
        GameEdition edition
    ) const noexcept {
        try {
            const char* gameFolder = (edition == GameEdition::FullGame) ? FULL_GAME_FOLDER : DEMO_GAME_FOLDER;
            auto binariesPath = libraryPath / "steamapps" / "common" / gameFolder / BINARIES_SUBPATH;

            if (!std::filesystem::exists(binariesPath)) {
                return std::unexpected(SteamError::PathDoesNotExist);
            }

            return GameLocation{
                .binariesPath = std::move(binariesPath),
                .edition = edition
            };
        }
        catch (const std::filesystem::filesystem_error&) {
            return std::unexpected(SteamError::PathDoesNotExist);
        }
    }

    GameEdition SteamLocator::DetectEditionFromPath(const std::filesystem::path& path) noexcept {
        try {
            std::string pathStr = path.string();
            std::ranges::transform(pathStr, pathStr.begin(), ::tolower);
            if (pathStr.find("demo") != std::string::npos) {
                return GameEdition::Demo;
            }
        }
        catch (...) {}

        return GameEdition::FullGame;
    }

    std::filesystem::path SteamLocator::FindWin64Directory(const std::filesystem::path& basePath) noexcept {
        try {
            if (basePath.filename() == "Win64") {
                return basePath;
            }

            auto win64Under = basePath / BINARIES_SUBPATH;
            if (std::filesystem::exists(win64Under)) {
                return win64Under;
            }

            for (auto current = basePath; current.has_parent_path() && current != current.root_path(); current = current.parent_path()) {
                if (current.filename() == "Win64") {
                    return current;
                }
            }
        }
        catch (...) {}

        return {};
    }

}
