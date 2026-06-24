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

    [[nodiscard]] std::expected<GameLocation, SteamError> LocateGame();
    [[nodiscard]] std::expected<GameLocation, SteamError> LocateGameAt(const std::filesystem::path& manualPath
    );

}
