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

    consteval uint64_t operator""_hash(const char* str, size_t len) {
        uint64_t hash = FNV_OFFSET_BASIS;
        for (size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(str[i]));
            hash *= FNV_PRIME;
        }
        return hash;
    }

    namespace Unreal {
        constexpr uint64_t WILLIE_BP_C = "Willie_BP_C"_hash;
        constexpr uint64_t PROCESSEVENT = "ProcessEvent"_hash;
    }

    namespace Events {
        constexpr uint64_t EXECUTE_UBERGRAPH_UI_BEGINFIGHT = "ExecuteUbergraph_UI_BeginFight"_hash;
        constexpr uint64_t EXECUTE_UBERGRAPH_ABYSS_MAP = "ExecuteUbergraph_Abyss_Map_Open_Intermediate"_hash;
        constexpr uint64_t ON_WALKING_OFF_LEDGE = "OnWalkingOffLedge"_hash;
        constexpr uint64_t RECEIVE_TICK = "ReceiveTick"_hash;
    }
}

static_assert(HS::Hash::FNV1A("test") != 0, "FNV1A hash function validation");
