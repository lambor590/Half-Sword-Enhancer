#include <fstream>
#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <Windows.h>
#include <shellapi.h>

#include "../include/SteamLocator.h"
#include "../include/Logger.h"
#include "../include/Util.h"

namespace hse {
    namespace {
        constexpr wchar_t STEAM_REGISTRY_KEY[] = LR"(SOFTWARE\Valve\Steam)";
        constexpr wchar_t STEAM_INSTALL_VALUE[] = L"SteamPath";
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

        [[nodiscard]] std::filesystem::path ParseVdfPath(std::string_view value) {
            std::string decoded;
            decoded.reserve(value.size());
            for (std::size_t index = 0; index < value.size(); ++index) {
                if (value[index] == '\\' && index + 1 < value.size() &&
                    (value[index + 1] == '\\' || value[index + 1] == '"'))
                    ++index;
                decoded.push_back(value[index]);
            }
            return PathFromUtf8(decoded);
        }

        [[nodiscard]] bool ContainsGameExecutable(const std::filesystem::path& directory, GameEdition edition) {
            std::error_code error;
            return std::filesystem::is_regular_file(directory / DescribeGameEdition(edition).executableName, error) &&
                   !error;
        }

    }

    std::expected<GameLocation, SteamError> LocateGame() {
        auto steamPath = ReadSteamInstallPath();
        if (!steamPath) return std::unexpected(steamPath.error());

        auto vdfPath = *steamPath / "steamapps" / "libraryfolders.vdf";
        auto libraries = ParseLibraryFolders(vdfPath);
        if (!libraries) return std::unexpected(libraries.error());

        for (const auto& descriptor : GAME_EDITIONS) {
            for (const auto& library : *libraries) {
                if (!library.installedEditions[EditionIndex(descriptor.edition)]) continue;
                if (auto result = ResolveGamePath(library.path, descriptor.edition)) return result;
            }
        }

        return std::unexpected(SteamError::GameNotFound);
    }

    std::expected<GameLocation, SteamError> LocateGameAt(
        const std::filesystem::path& manualPath, std::optional<GameEdition> knownEdition
    ) {
        if (manualPath.empty()) return std::unexpected(SteamError::PathDoesNotExist);

        const auto edition = knownEdition.value_or(DetectEditionFromPath(manualPath));
        auto binariesPath = manualPath.lexically_normal();
        if (!ContainsGameExecutable(binariesPath, edition)) binariesPath /= BINARIES_SUBPATH;
        if (!ContainsGameExecutable(binariesPath, edition)) return std::unexpected(SteamError::PathDoesNotExist);
        return GameLocation{.binariesPath = std::move(binariesPath), .edition = edition};
    }

    std::expected<void, SteamError> LaunchGameThroughSteam(GameEdition edition) {
        auto steamPath = ReadSteamInstallPath();
        if (!steamPath) return std::unexpected(steamPath.error());

        const auto& descriptor = DescribeGameEdition(edition);
        const auto steamExecutable = *steamPath / "steam.exe";
        std::error_code error;
        if (!std::filesystem::is_regular_file(steamExecutable, error) || error)
            return std::unexpected(SteamError::LaunchFailed);

        const std::wstring parameters =
            L"-applaunch " + std::wstring(descriptor.steamAppId.begin(), descriptor.steamAppId.end());
        SHELLEXECUTEINFOW launch{
            .cbSize = sizeof(SHELLEXECUTEINFOW),
            .fMask = SEE_MASK_NOASYNC,
            .lpVerb = L"open",
            .lpFile = steamExecutable.c_str(),
            .lpParameters = parameters.c_str(),
            .lpDirectory = steamPath->c_str(),
            .nShow = SW_SHOWNORMAL,
        };
        if (!ShellExecuteExW(&launch)) return std::unexpected(SteamError::LaunchFailed);
        return {};
    }

    namespace {

        std::expected<std::filesystem::path, SteamError> ReadSteamInstallPath() {
            DWORD bufferSize = 0;
            if (RegGetValueW(
                    HKEY_CURRENT_USER, STEAM_REGISTRY_KEY, STEAM_INSTALL_VALUE, RRF_RT_REG_SZ, nullptr, nullptr,
                    &bufferSize
                ) != ERROR_SUCCESS ||
                bufferSize <= sizeof(wchar_t)) {
                Logger::warn("Steam could not be found automatically");
                return std::unexpected(SteamError::RegistryNotFound);
            }

            std::wstring value(bufferSize / sizeof(wchar_t), L'\0');
            if (RegGetValueW(
                    HKEY_CURRENT_USER, STEAM_REGISTRY_KEY, STEAM_INSTALL_VALUE, RRF_RT_REG_SZ, nullptr, value.data(),
                    &bufferSize
                ) != ERROR_SUCCESS)
                return std::unexpected(SteamError::RegistryNotFound);
            while (!value.empty() && value.back() == L'\0')
                value.pop_back();

            std::filesystem::path path(value);
            std::error_code error;
            if (path.empty() || !std::filesystem::is_directory(path, error) || error)
                return std::unexpected(SteamError::SteamNotInstalled);
            return path;
        }

        std::expected<std::vector<LibraryFolder>, SteamError> ParseLibraryFolders(
            const std::filesystem::path& vdfPath
        ) {
            std::ifstream file(vdfPath);
            if (!file.is_open()) {
                Logger::error("Could not read the Steam game library list: %s", PathToUtf8(vdfPath).c_str());
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
                        current.path = ParseVdfPath(value);
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
