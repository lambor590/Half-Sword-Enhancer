#pragma once

#include <array>
#include <cstring>
#include <type_traits>

#include "SDK/Engine_classes.hpp"

namespace EngineMemory {
    using FreeFunction = void (*)(void*);
    inline FreeFunction freeBuffer = nullptr;

    inline bool Initialize() noexcept {
        // CL-2705 FMemory::Free, verified through FString destruction and the
        // FScriptArray allocator. Revalidate alongside the generated SDK.
        const auto address = SDK::InSDKUtils::GetImageBase() + 0x0119DDA0;
        constexpr std::array<unsigned char, 13> PREFIX{0x48, 0x85, 0xC9, 0x74, 0x2E, 0x53, 0x48,
                                                       0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9};
        constexpr std::array<unsigned char, 9> SUFFIX{0xFF, 0x50, 0x48, 0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3};
        if (std::memcmp(reinterpret_cast<const void*>(address), PREFIX.data(), PREFIX.size()) != 0 ||
            std::memcmp(reinterpret_cast<const void*>(address + 0x2B), SUFFIX.data(), SUFFIX.size()) != 0)
            return false;
        freeBuffer = reinterpret_cast<FreeFunction>(address);
        return true;
    }
}

// Owns an array returned by the engine, not a borrowed UObject property.
// Construct and destroy on the game thread after EngineMemory::Initialize.
// Dumper-7's TArray does not free its storage; CRT free uses the wrong allocator.
template <typename Element> class EngineArray final : public SDK::TArray<Element> {
    // SDK destructors are absent, so explicitly allow only elements with no owned allocations.
    static_assert(std::is_pointer_v<Element> || std::is_same_v<Element, SDK::FConstraintInstanceAccessor> ||
                  std::is_same_v<Element, SDK::FHitResult>);

public:
    EngineArray() = default;
    explicit EngineArray(SDK::TArray<Element> array) : SDK::TArray<Element>(array) {}
    ~EngineArray() {
        if (this->GetDataPtr()) EngineMemory::freeBuffer(const_cast<Element*>(this->GetDataPtr()));
    }

    EngineArray(const EngineArray&) = delete;
    EngineArray& operator=(const EngineArray&) = delete;
};
