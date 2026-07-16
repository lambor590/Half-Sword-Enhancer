#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

namespace hse {
    class Version {
    public:
        constexpr Version() noexcept = default;
        constexpr Version(std::uint16_t major, std::uint16_t minor, std::uint16_t patch) noexcept
            : major_(major), minor_(minor), patch_(patch) {}
        constexpr explicit Version(std::string_view versionString) noexcept {
            if (versionString.starts_with('v')) versionString.remove_prefix(1);

            std::uint16_t components[3]{};
            for (std::size_t index = 0; index < 3; ++index) {
                const auto separator = versionString.find('.');
                if ((index < 2) != (separator != std::string_view::npos)) return;

                const auto component = versionString.substr(0, separator);
                if (component.empty()) return;

                std::uint32_t value = 0;
                for (const char digit : component) {
                    if (digit < '0' || digit > '9') return;
                    value = value * 10 + static_cast<std::uint32_t>(digit - '0');
                    if (value > 65'535) return;
                }
                components[index] = static_cast<std::uint16_t>(value);

                if (separator == std::string_view::npos)
                    versionString = {};
                else
                    versionString.remove_prefix(separator + 1);
            }

            major_ = components[0];
            minor_ = components[1];
            patch_ = components[2];
        }

        constexpr auto operator<=>(const Version&) const noexcept = default;
        constexpr bool operator==(const Version&) const noexcept = default;

        [[nodiscard]] std::string ToString() const;
        [[nodiscard]] constexpr bool IsValid() const noexcept { return major_ > 0 || minor_ > 0 || patch_ > 0; }

        [[nodiscard]] constexpr std::uint16_t major() const noexcept { return major_; }
        [[nodiscard]] constexpr std::uint16_t minor() const noexcept { return minor_; }
        [[nodiscard]] constexpr std::uint16_t patch() const noexcept { return patch_; }

    private:
        std::uint16_t major_ = 0;
        std::uint16_t minor_ = 0;
        std::uint16_t patch_ = 0;
    };
}
