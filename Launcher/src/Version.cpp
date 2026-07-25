#include "../include/Version.h"

#include <array>
#include <charconv>

namespace hse {
    std::string Version::ToString() const {
        std::array<char, 17> buffer{};
        char* output = buffer.data();
        for (const std::uint16_t component : {major_, minor_, patch_}) {
            if (output != buffer.data()) *output++ = '.';
            output = std::to_chars(output, buffer.data() + buffer.size(), component).ptr;
        }
        return {buffer.data(), output};
    }
}
