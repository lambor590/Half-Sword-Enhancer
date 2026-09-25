#pragma once

#include <array>
#include <cstdint>
#include <cstring>

#include "SDK/Engine_classes.hpp"

namespace EngineFrame {
    // GFrameCounter, read from the native thunk of KismetSystemLibrary.GetFrameCount:
    //   P_FINISH; mov rax, [rip+GFrameCounter]; mov [r8], rax; ret
    // Calling the function instead costs a ProcessEvent (~2.5 us) on hot paths such as every ReceiveTick.
    inline const volatile std::uint64_t* LocateCounter() noexcept {
        auto* library = SDK::UKismetSystemLibrary::StaticClass();
        auto* function = library ? library->GetFunction("KismetSystemLibrary", "GetFrameCount") : nullptr;
        if (!function || !function->ExecFunction) return nullptr;

        constexpr std::array<unsigned char, 22> PREFIX{0x48, 0x8B, 0x42, 0x20, 0x33, 0xC9, 0x48, 0x85,
                                                       0xC0, 0x0F, 0x95, 0xC1, 0x48, 0x03, 0xC8, 0x48,
                                                       0x89, 0x4A, 0x20, 0x48, 0x8B, 0x05};
        constexpr std::array<unsigned char, 4> SUFFIX{0x49, 0x89, 0x00, 0xC3};
        const auto* code = reinterpret_cast<const unsigned char*>(function->ExecFunction);
        const auto* next = code + PREFIX.size() + sizeof(std::int32_t);
        if (std::memcmp(code, PREFIX.data(), PREFIX.size()) != 0 || std::memcmp(next, SUFFIX.data(), SUFFIX.size()) != 0)
            return nullptr;

        std::int32_t displacement = 0;
        std::memcpy(&displacement, code + PREFIX.size(), sizeof(displacement));
        return reinterpret_cast<const volatile std::uint64_t*>(next + displacement);
    }

    // Engine frame number; game thread only. Falls back to the reflected call if the thunk changes.
    inline std::int64_t Current() {
        static const volatile std::uint64_t* const COUNTER = LocateCounter();
        return COUNTER ? static_cast<std::int64_t>(*COUNTER) : SDK::UKismetSystemLibrary::GetFrameCount();
    }
}
