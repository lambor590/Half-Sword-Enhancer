#pragma once

#include <cstdint>

namespace MemoryUtils {
    [[nodiscard]] bool PlaceHook(uintptr_t addressToHook, uintptr_t destinationAddress, uintptr_t* returnAddress);
    void Unhook(uintptr_t hookedAddress);
}
