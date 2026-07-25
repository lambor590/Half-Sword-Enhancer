#include <fstream>
#include <string>
#include <string_view>
#include <array>
#include <Windows.h>

#include "../include/SteamLocator.h"
#include "../include/Logger.h"

namespace hse {
    namespace {
        constexpr wchar_t STEAM_REGISTRY_KEY[] = LR"(SOFTWARE\Wow6432Node\Valve\Steam)";
        constexpr wchar_t STEAM_INSTALL_VALUE[] = L"InstallPath";
        constexpr const char* BINARIES_SUBPATH = R"(HalfSwordUE5\Binaries\Win64)";

        struct LibraryFolder {
            std::filesystem::path path;
            std::array<bool, GAME_EDITIONS.size()> installedEditions{};
        };

        [[nodiscard]] std::expected<std::filesystem::path, SteamError> ReadSteamInstallPath();
        [[nodiscard]] std::expected<std::vector<LibraryFolder>, SteamError> ParseLibraryFolders(
            const std::filesystem::path& vdfPath
        );
        [[nodiscard]] std::expected<GameLocation, SteamError> ResolveGamePath(
            const std::filesystem::path& libraryPath, GameEdition edition
        );
        [[nodiscard]] GameEdition DetectEditionFromPath(const std::filesystem::path& path);
        struct QuotedKeyValue {
            std::string_view key;
            std::string_view value;
        };

        [[nodiscard]] QuotedKeyValue ExtractQuotedKeyValue(std::string_view line) noexcept {
            const auto keyStart = line.find('"');
            if (keyStart == std::string_view::npos) return {};

            const auto keyEnd = line.find('"', keyStart + 1);
            if (keyEnd == std::string_view::npos) return {};

            const auto valueStart = line.find('"', keyEnd + 1);
            if (valueStart == std::string_view::npos) {
                return {line.substr(keyStart + 1, keyEnd - keyStart - 1), {}};
            }

            const auto valueEnd = line.find('"', valueStart + 1);
            if (valueEnd == std::string_view::npos) {
                return {line.substr(keyStart + 1, keyEnd - keyStart - 1), {}};
            }

            return {
                line.substr(keyStart + 1, keyEnd - keyStart - 1),
                line.substr(valueStart + 1, valueEnd - valueStart - 1),
            };
        }

        [[nodiscard]] constexpr std::size_t EditionIndex(GameEdition edition) noexcept {
            return edition == GameEdition::Demo ? 1 : 0;
        }

        [[nodiscard]] bool ContainsGameExecutable(const std::filesystem::path& directory, GameEdition edition) {
            std::error_code error;
            return std::filesystem::is_regular_file(directory / DescribeGameEdition(edition).executableName, error) &&
                   !error;
        }

    }

    std::expected<GameLocation, SteamError> LocateGame() {
        auto steamPath = ReadSteamInstallPath();
        if (!steamPath) {
            return std::unexpected(steamPath.error());
        }

        auto vdfPath = *steamPath / "steamapps" / "libraryfolders.vdf";
        auto libraries = ParseLibraryFolders(vdfPath);
        if (!libraries) {
            return std::unexpected(libraries.error());
        }

        std::expected<GameLocation, SteamError> fallback = std::unexpected(SteamError::GameNotFound);

        for (const auto& library : *libraries) {
            for (const auto& descriptor : GAME_EDITIONS) {
                if (!library.installedEditions[EditionIndex(descriptor.edition)]) continue;
                auto result = ResolveGamePath(library.path, descriptor.edition);
                if (!result) continue;
                if (descriptor.edition == GameEdition::FullGame) return result;
                fallback = std::move(result);
            }
        }

        if (fallback) return fallback;

        return std::unexpected(SteamError::GameNotFound);
    }

    std::expected<GameLocation, SteamError> LocateGameAt(
        const std::filesystem::path& manualPath, std::optional<GameEdition> knownEdition
    ) {
        auto win64Dir = manualPath.filename() == "Win64" ? manualPath : manualPath / BINARIES_SUBPATH;

        const auto edition = knownEdition.value_or(DetectEditionFromPath(win64Dir));
        if (!ContainsGameExecutable(win64Dir, edition)) {
            return std::unexpected(SteamError::PathDoesNotExist);
        }

        return GameLocation{.binariesPath = win64Dir, .edition = edition};
    }

    namespace {

        std::expected<std::filesystem::path, SteamError> ReadSteamInstallPath() {
            HKEY hKey;
            LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, STEAM_REGISTRY_KEY, 0, KEY_READ, &hKey);
            if (result != ERROR_SUCCESS) {
                Logger::warn("Steam could not be found automatically");
                return std::unexpected(SteamError::SteamNotInstalled);
            }

            struct RegKeyGuard {
                HKEY key;
                ~RegKeyGuard() { RegCloseKey(key); }
            } guard{hKey};

            DWORD bufferSize = 0;
            result = RegGetValueW(hKey, nullptr, STEAM_INSTALL_VALUE, RRF_RT_REG_SZ, nullptr, nullptr, &bufferSize);
            if (result != ERROR_SUCCESS) {
                Logger::warn("The Steam installation folder could not be found automatically");
                return std::unexpected(SteamError::RegistryNotFound);
            }

            std::wstring installPath(bufferSize / sizeof(wchar_t), L'\0');
            result = RegGetValueW(
                hKey, nullptr, STEAM_INSTALL_VALUE, RRF_RT_REG_SZ, nullptr, installPath.data(), &bufferSize
            );
            if (result != ERROR_SUCCESS) return std::unexpected(SteamError::RegistryNotFound);
            if (!installPath.empty() && installPath.back() == L'\0') installPath.pop_back();

            std::filesystem::path path(installPath);
            std::error_code ec;
            if (installPath.empty() || !std::filesystem::is_directory(path, ec)) {
                Logger::warn("The saved Steam installation folder is no longer available");
                return std::unexpected(SteamError::SteamNotInstalled);
            }

            return path;
        }

        std::expected<std::vector<LibraryFolder>, SteamError> ParseLibraryFolders(
            const std::filesystem::path& vdfPath
        ) {
            std::ifstream file(vdfPath);
            if (!file.is_open()) {
                Logger::error("Could not read the Steam game library list: %s", vdfPath.string().c_str());
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

                const auto [key, value] = ExtractQuotedKeyValue(line);
                if (key.empty()) continue;

                if (braceDepth == 2 && key == "path") {
                    if (!value.empty()) {
                        current.path = std::filesystem::path(value);
                    }
                }

                if (braceDepth == 2 && key == "apps") {
                    insideApps = true;
                    continue;
                }

                if (insideApps && braceDepth == 3)
                    for (const auto& descriptor : GAME_EDITIONS)
                        if (key == descriptor.steamAppId)
                            current.installedEditions[EditionIndex(descriptor.edition)] = true;
            }

            if (libraries.empty()) {
                Logger::warn("No Steam game libraries were found");
                return std::unexpected(SteamError::VdfParsingFailed);
            }

            return libraries;
        }

        std::expected<GameLocation, SteamError> ResolveGamePath(
            const std::filesystem::path& libraryPath, GameEdition edition
        ) {
            const auto& descriptor = DescribeGameEdition(edition);
            auto binariesPath = libraryPath / "steamapps" / "common" / descriptor.installFolder / BINARIES_SUBPATH;

            std::error_code ec;
            if (!std::filesystem::is_directory(binariesPath, ec) || !ContainsGameExecutable(binariesPath, edition)) {
                if (ec) {
                    Logger::error("Could not access the Half Sword folder: %s", ec.message().c_str());
                }
                return std::unexpected(SteamError::PathDoesNotExist);
            }

            return GameLocation{.binariesPath = std::move(binariesPath), .edition = edition};
        }

        GameEdition DetectEditionFromPath(const std::filesystem::path& path) {
            const auto equalsIgnoringCase = [](std::wstring_view left, std::string_view right) {
                if (left.size() != right.size()) return false;
                for (std::size_t index = 0; index < left.size(); ++index) {
                    wchar_t leftCharacter = left[index];
                    char rightCharacter = right[index];
                    if (leftCharacter >= L'A' && leftCharacter <= L'Z') leftCharacter += L'a' - L'A';
                    if (rightCharacter >= 'A' && rightCharacter <= 'Z') rightCharacter += 'a' - 'A';
                    if (leftCharacter != static_cast<unsigned char>(rightCharacter)) return false;
                }
                return true;
            };

            for (const auto& component : path) {
                if (equalsIgnoringCase(component.native(), DescribeGameEdition(GameEdition::Demo).installFolder))
                    return GameEdition::Demo;
            }
            return GameEdition::FullGame;
        }

    } // namespace

}
