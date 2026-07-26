#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <cstdint>

#include "GameEdition.h"

namespace hse {

    enum class SteamError : std::uint8_t {
        RegistryNotFound,
        SteamNotInstalled,
        VdfParsingFailed,
        GameNotFound,
        PathDoesNotExist,
        LaunchFailed
    };

    struct GameLocation {
        std::filesystem::path binariesPath;
        GameEdition edition;
    };

    [[nodiscard]] std::expected<GameLocation, SteamError> LocateGame();
    [[nodiscard]] std::expected<GameLocation, SteamError> LocateGameAt(
        const std::filesystem::path& manualPath, std::optional<GameEdition> knownEdition = std::nullopt
    );
    [[nodiscard]] std::expected<void, SteamError> LaunchGameThroughSteam(GameEdition edition);

}
