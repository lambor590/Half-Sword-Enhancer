#pragma once

#include <string>
#include <expected>
#include <filesystem>
#include <vector>
#include <cstdint>

#include "Util.h"

namespace hse {

    enum class SteamError : std::uint8_t {
        RegistryNotFound,
        SteamNotInstalled,
        VdfParsingFailed,
        GameNotFound,
        PathDoesNotExist
    };

    struct GameLocation {
        std::filesystem::path binariesPath;
        GameEdition edition;
    };

    class SteamLocator {
    public:
        static SteamLocator& Instance() noexcept {
            static SteamLocator instance;
            return instance;
        }

        [[nodiscard]] std::expected<GameLocation, SteamError> LocateGame() noexcept;
        [[nodiscard]] std::expected<GameLocation, SteamError> LocateGameAt(
            const std::filesystem::path& manualPath
        ) noexcept;

    private:
        static constexpr const char* STEAM_REGISTRY_KEY = R"(SOFTWARE\Wow6432Node\Valve\Steam)";
        static constexpr const char* STEAM_INSTALL_VALUE = "InstallPath";
        static constexpr const char* FULL_GAME_APP_ID = "2397300";
        static constexpr const char* DEMO_APP_ID = "2642680";
        static constexpr const char* FULL_GAME_FOLDER = "Half Sword";
        static constexpr const char* DEMO_GAME_FOLDER = "Half Sword Demo";
        static constexpr const char* BINARIES_SUBPATH = R"(HalfSwordUE5\Binaries\Win64)";

        struct LibraryFolder {
            std::filesystem::path path;
            std::vector<std::string> appIds;
        };

        SteamLocator() = default;
        ~SteamLocator() = default;
        SteamLocator(const SteamLocator&) = delete;
        SteamLocator& operator=(const SteamLocator&) = delete;
        SteamLocator(SteamLocator&&) = delete;
        SteamLocator& operator=(SteamLocator&&) = delete;

        [[nodiscard]] std::expected<std::string, SteamError> ReadSteamInstallPath() const noexcept;
        [[nodiscard]] std::expected<std::vector<LibraryFolder>, SteamError> ParseLibraryFolders(
            const std::filesystem::path& vdfPath
        ) const noexcept;
        [[nodiscard]] std::expected<GameLocation, SteamError> ResolveGamePath(
            const std::filesystem::path& libraryPath,
            GameEdition edition
        ) const noexcept;
        [[nodiscard]] static GameEdition DetectEditionFromPath(const std::filesystem::path& path) noexcept;
        [[nodiscard]] static std::filesystem::path FindWin64Directory(const std::filesystem::path& basePath) noexcept;
    };

}
