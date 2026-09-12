#pragma once

#include <cstdint>
#include <string_view>

namespace HS::Hash {
    constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;

    constexpr uint64_t FNV1A(std::string_view str) noexcept {
        uint64_t hash = FNV_OFFSET_BASIS;
        for (char c : str) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
            hash *= FNV_PRIME;
        }
        return hash;
    }

}

static_assert(HS::Hash::FNV1A("") == 0xcbf29ce484222325ULL);
static_assert(HS::Hash::FNV1A("foobar") == 0x85944171f73967e8ULL);
static_assert(HS::Hash::FNV1A(std::string_view("a\0b", 3)) == 0xe5d29919042666b2ULL);
static_assert(HS::Hash::FNV1A("\x80") == 0xaf643d4c8602915fULL);
