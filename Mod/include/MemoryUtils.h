#pragma once

#include <Windows.h>
#include <vector>
#include <Psapi.h>
#include <unordered_map>
#include <array>

#include "Logger.h"

namespace MemoryUtils {
    extern Logger logger;

    constexpr unsigned char NOP_INSTRUCTION = 0x90;
    constexpr size_t MAX_ASM_BYTES = 30;
    constexpr size_t MIN_CLEARANCE = 5;
    constexpr size_t FAR_JUMP_SIZE = 14;
    constexpr size_t NEAR_JUMP_SIZE = 5;
    constexpr size_t TRAMPOLINE_BUFFER_SIZE = FAR_JUMP_SIZE * 3;
    constexpr size_t PROTECTION_BUFFER = FAR_JUMP_SIZE;
    constexpr size_t MEMORY_RANGE_32BIT = 0x7fffffff;
    constexpr size_t ALLOCATION_INCREMENT = 65536;
    constexpr size_t ABS_JUMP_HEADER_SIZE = 6;
    constexpr size_t ABS_JUMP_FULL_SIZE = 14;
    constexpr size_t REL_JUMP_SIZE = 5;

    struct HookInformation {
        std::array<uint8_t, 32> originalBytes{};
        size_t originalBytesSize = 0;
        uintptr_t trampolineInstructionsAddress = 0;
        uintptr_t trampolineBase = 0;
    };

    extern std::unordered_map<uintptr_t, HookInformation> InfoBufferForHookedAddresses;

    void ToggleMemoryProtection(bool enableProtection, uintptr_t address, size_t size) noexcept;

    static inline void MemCopy(void* destination, const void* source, size_t numBytes) noexcept {
        auto addr = reinterpret_cast<uintptr_t>(destination);
        ToggleMemoryProtection(false, addr, numBytes);
        std::memcpy(destination, source, numBytes);
        ToggleMemoryProtection(true, addr, numBytes);
    }

    static inline void MemSet(void* address, unsigned char byte, size_t numBytes) noexcept {
        auto addr = reinterpret_cast<uintptr_t>(address);
        ToggleMemoryProtection(false, addr, numBytes);
        std::memset(address, byte, numBytes);
        ToggleMemoryProtection(true, addr, numBytes);
    }

    static inline const std::string& GetCurrentModuleName() noexcept {
        static const std::string CACHED_NAME = []() {
            HMODULE module = NULL;
            static char dummy = 'x';
            GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, &dummy, &module
            );

            char lpFilename[MAX_PATH];
            GetModuleFileNameA(module, lpFilename, sizeof(lpFilename));

            if (char* lastSlash = strrchr(lpFilename, '\\')) {
                std::string_view moduleName(lastSlash + 1);
                if (auto dotPos = moduleName.find(".dll"); dotPos != std::string_view::npos) {
                    return std::string(moduleName.substr(0, dotPos));
                }
            }
            return std::string();
        }();
        return CACHED_NAME;
    }

    static inline void ShowErrorPopup(std::string_view error) noexcept {
        logger.Log("Raised error: {}", error);
        std::string errorStr(error);
        MessageBoxA(NULL, errorStr.c_str(), GetCurrentModuleName().c_str(), MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
    }

    [[nodiscard]] bool PlaceHook(uintptr_t addressToHook, uintptr_t destinationAddress, uintptr_t* returnAddress);
    void Unhook(uintptr_t hookedAddress) noexcept;
}
