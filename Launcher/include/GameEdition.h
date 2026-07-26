#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace hse {
    enum class GameEdition : std::uint8_t { FullGame, Demo };

    struct GameEditionDescriptor {
        GameEdition edition;
        std::string_view displayName;
        std::string_view configValue;
        std::string_view steamAppId;
        std::string_view installFolder;
        std::string_view executableName;
    };

    inline constexpr std::array GAME_EDITIONS{
        GameEditionDescriptor{
            GameEdition::FullGame,
            "Full Game",
            "full",
            "2397300",
            "Half Sword",
            "HalfSwordUE5-Win64-Shipping.exe",
        },
        GameEditionDescriptor{
            GameEdition::Demo,
            "Demo",
            "demo",
            "2642680",
            "Half Sword Demo",
            "HalfSwordUE5-Win64-Shipping.exe",
        },
    };

    [[nodiscard]] constexpr const GameEditionDescriptor& DescribeGameEdition(GameEdition edition) noexcept {
        return edition == GameEdition::Demo ? GAME_EDITIONS[1] : GAME_EDITIONS[0];
    }

    [[nodiscard]] constexpr GameEdition ParseGameEdition(std::string_view value) noexcept {
        for (const auto& descriptor : GAME_EDITIONS)
            if (descriptor.configValue == value) return descriptor.edition;
        return GameEdition::FullGame;
    }
}
