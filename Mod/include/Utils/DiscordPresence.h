#pragma once

#include <cstdint>
#include <string>

#include "Core/ModContext.h"

namespace DiscordPresence {
    inline constexpr char COMMUNITY_URL[] = "https://halfswordenhancer.com/discord";

    struct Activity {
        std::string details;
        std::string state;
        std::uint64_t sessionStart = 0;
        bool operator==(const Activity&) const = default;
    };

    void Start() noexcept;
    void Poll();
    // Call on the game thread before the runtime hooks are removed.
    void Shutdown() noexcept;

    [[nodiscard]] Activity Describe(const RuntimeContextSnapshot& runtime);
}
