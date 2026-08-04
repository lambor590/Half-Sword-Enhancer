#pragma once

#include <cstdint>

namespace MemoryUtils {
    enum class HookIntegrity : std::uint8_t { NotTracked, Intact, Replaced, Unreadable };

    [[nodiscard]] const char* HookIntegrityName(HookIntegrity integrity) noexcept;
    [[nodiscard]] bool PlaceHook(uintptr_t addressToHook, uintptr_t destinationAddress, uintptr_t* returnAddress);
    [[nodiscard]] HookIntegrity InspectHook(uintptr_t hookedAddress) noexcept;
    void Unhook(uintptr_t hookedAddress);
}
