#pragma once

#include <cstdint>
#include <string_view>

namespace HS::Hash {
    constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;

    constexpr uint64_t FNV1A(const char* str) noexcept {
        uint64_t hash = FNV_OFFSET_BASIS;
        while (*str) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(*str++));
            hash *= FNV_PRIME;
        }
        return hash;
    }

    constexpr uint64_t FNV1A(std::string_view str) noexcept {
        uint64_t hash = FNV_OFFSET_BASIS;
        for (char c : str) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
            hash *= FNV_PRIME;
        }
        return hash;
    }

}

static_assert(HS::Hash::FNV1A("test") != 0, "FNV1A hash function validation");
