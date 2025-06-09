#pragma once

#include <Windows.h>
#include <vector>
#include <Psapi.h>
#include <sstream>
#include <unordered_map>
#include <iomanip>
#include <array>

#define NMD_ASSEMBLY_IMPLEMENTATION
#define NMD_ASSEMBLY_PRIVATE
#include "nmd_assembly.h"

#include "Logger.h"

// Contains various memory manipulation functions related to hooking or modding
namespace MemoryUtils
{
    extern Logger logger;
    static constexpr int maskBytes = 0xffff;
    static constexpr unsigned char NOP_INSTRUCTION = 0x90;

    struct HookInformation
    {
        std::vector<unsigned char> originalBytes = { 0 };
        uintptr_t trampolineInstructionsAddress = 0;
    };


    extern std::unordered_map<uintptr_t, HookInformation> InfoBufferForHookedAddresses;
    uintptr_t SigScanRegion(uint8_t* buffer, size_t regionSize, const uint16_t* pattern, size_t patternSize) noexcept;

    constexpr uint64_t HashPattern(const std::vector<uint16_t>& pattern) noexcept {
        uint64_t hash = 14695981039346656037ULL;
        for (auto byte : pattern) {
            hash ^= byte;
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    void ToggleMemoryProtection(bool enableProtection, uintptr_t address, size_t size) noexcept;

    static inline void MemCopy(uintptr_t destination, uintptr_t source, size_t numBytes) noexcept
    {
        ToggleMemoryProtection(false, destination, numBytes);
        ToggleMemoryProtection(false, source, numBytes);
        std::memcpy(reinterpret_cast<void*>(destination), reinterpret_cast<const void*>(source), numBytes);
        ToggleMemoryProtection(true, source, numBytes);
        ToggleMemoryProtection(true, destination, numBytes);
    }

    static inline void MemSet(uintptr_t address, unsigned char byte, size_t numBytes) noexcept
    {
        ToggleMemoryProtection(false, address, numBytes);
        std::memset(reinterpret_cast<void*>(address), byte, numBytes);
        ToggleMemoryProtection(true, address, numBytes);
    }

    uintptr_t GetProcessBaseAddress(DWORD processId) noexcept;

    static inline const std::string& GetCurrentModuleName() noexcept
    {
        static const std::string cachedName = []() {
            HMODULE module = NULL;
            static char dummy = 'x';
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, &dummy, &module);

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
        return cachedName;
    }

    static inline void ShowErrorPopup(std::string_view error) noexcept
    {
        logger.Log("Raised error: {}", error);
        MessageBoxA(NULL, error.data(), GetCurrentModuleName().c_str(), MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
    }

    uintptr_t SigScanCached(const std::vector<uint16_t>& pattern) noexcept;
    uintptr_t SigScan(const std::vector<uint16_t>& pattern) noexcept;

    static uintptr_t AllocateMemoryWithin32BitRange(size_t numBytes, uintptr_t origin)
    {
        constexpr size_t range = 0x7fffffff;
        constexpr size_t increment = 65536;
        
        uintptr_t lowerBound = origin > range ? origin - range : 0;
        uintptr_t higherBound = origin + range;

        SYSTEM_INFO si;
        GetSystemInfo(&si);
        size_t alignedSize = (numBytes + si.dwPageSize - 1) & ~(static_cast<unsigned long long>(si.dwPageSize) - 1);

        for (uintptr_t i = lowerBound; i < higherBound; i += increment)
        {
            if (uintptr_t addr = (uintptr_t)VirtualAlloc((void*)i, alignedSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE))
            {
                if (addr >= lowerBound && addr <= higherBound) {
                    MemSet(addr, NOP_INSTRUCTION, numBytes);
                    return addr;
                }
                VirtualFree((void*)addr, 0, MEM_RELEASE);
            }
        }
        return 0;
    }

    void PlaceHook(uintptr_t addressToHook, uintptr_t destinationAddress, uintptr_t* returnAddress);
    void Unhook(uintptr_t hookedAddress);

    static uintptr_t ReadPointerChain(const std::vector<uintptr_t>& pointerOffsets)
    {
        uintptr_t pointer = GetProcessBaseAddress(GetCurrentProcessId());
        
        for (size_t i = 0; i < pointerOffsets.size(); i++)
        {
            pointer += pointerOffsets[i];
            if (i < pointerOffsets.size() - 1) {
                MemCopy((uintptr_t)&pointer, pointer, sizeof(uintptr_t));
            }
            if (pointer == 0) return 0;
        }
        return pointer;
    }
}