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
    static constexpr size_t PATTERN_CACHE_SIZE = 32;

    struct HookInformation
    {
        std::vector<unsigned char> originalBytes = { 0 };
        uintptr_t trampolineInstructionsAddress = 0;
    };

    struct PatternCacheEntry {
        std::array<uint16_t, 32> pattern;
        size_t patternSize;
        uintptr_t result;
        uint64_t hash;
    };

    extern std::unordered_map<uintptr_t, HookInformation> InfoBufferForHookedAddresses;
    extern std::array<PatternCacheEntry, PATTERN_CACHE_SIZE> patternCache;
    extern size_t cacheIndex;

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

    static std::string GetCurrentProcessName() noexcept
    {
        thread_local char lpFilename[MAX_PATH];
        GetModuleFileNameA(NULL, lpFilename, sizeof(lpFilename));
        if (char* lastSlash = strrchr(lpFilename, '\\')) {
            return std::string(lastSlash + 1);
        }
        return std::string(lpFilename);
    }

    static inline const std::string& GetCurrentModuleName() noexcept
    {
        static const std::string cachedName = []() {
            HMODULE module = NULL;
            static char dummy = 'x';
            GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                &dummy, &module);

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
        constexpr size_t unidirectionalRange = 0x7fffffff;
        constexpr size_t arbitraryIncrement = 10000;
        
        uintptr_t lowerBound = origin > unidirectionalRange ? origin - unidirectionalRange : 0;
        uintptr_t higherBound = origin + unidirectionalRange;

        for (uintptr_t i = lowerBound; i < higherBound; i += arbitraryIncrement)
        {
            if (uintptr_t memoryAddress = (uintptr_t)VirtualAlloc((void*)i, numBytes, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE))
            {
                if (memoryAddress >= lowerBound && memoryAddress <= higherBound) {
                    logger.Log("Allocated %i bytes of memory at %p", numBytes, memoryAddress);
                    MemSet(memoryAddress, 0x90, numBytes);
                    return memoryAddress;
                }
                
                MEMORY_BASIC_INFORMATION info;
                VirtualQuery((void*)memoryAddress, &info, sizeof(MEMORY_BASIC_INFORMATION));
                i += info.RegionSize;
                VirtualFree((void*)memoryAddress, 0, MEM_RELEASE);
            }
        }

        logger.Log("Failed to allocate %i bytes of memory. Origin: %p", numBytes, origin);
        return NULL;
    }

    void PlaceHook(uintptr_t addressToHook, uintptr_t destinationAddress, uintptr_t* returnAddress);
    void Unhook(uintptr_t hookedAddress);

    static uintptr_t ReadPointerChain(std::vector<uintptr_t> pointerOffsets)
    {
        DWORD processId = GetCurrentProcessId();
        uintptr_t pointer = GetProcessBaseAddress(processId);
        
        for (size_t i = 0; i < pointerOffsets.size(); i++)
        {
            pointer += pointerOffsets[i];
            if (pointerOffsets[i] != pointerOffsets.back()) {
                MemCopy((uintptr_t)&pointer, pointer, sizeof(uintptr_t));
            }
            if (pointer == 0) return 0;
        }
        return pointer;
    }
}