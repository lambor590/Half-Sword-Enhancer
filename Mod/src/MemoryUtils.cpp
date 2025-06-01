#include "MemoryUtils.h"
#include <Windows.h>
#include <map>
#include <vector>
#include <Psapi.h>
#include <sstream>
#include <iomanip>
#include <array>
#include <algorithm>

namespace MemoryUtils 
{
    Logger logger{ "MemoryUtils" };
    std::unordered_map<uintptr_t, HookInformation> InfoBufferForHookedAddresses;
    
    static std::map<uintptr_t, DWORD> g_protectionHistory; 

    std::array<PatternCacheEntry, PATTERN_CACHE_SIZE> patternCache{};
    size_t cacheIndex = 0;

    void ToggleMemoryProtection(bool enableProtection, uintptr_t address, size_t size) noexcept
    {
        if (enableProtection && g_protectionHistory.find(address) != g_protectionHistory.end())
        {
            VirtualProtect((void*)address, size, g_protectionHistory[address], &g_protectionHistory[address]);
            g_protectionHistory.erase(address);
        }
        else if (!enableProtection && g_protectionHistory.find(address) == g_protectionHistory.end())
        {
            DWORD oldProtection = 0;
            VirtualProtect((void*)address, size, PAGE_EXECUTE_READWRITE, &oldProtection);
            g_protectionHistory[address] = oldProtection;
        }
    }

    static bool IsRelativeNearJumpPresentAtAddress(uintptr_t address)
    {
        unsigned char buffer;
        MemCopy((uintptr_t)&buffer, address, 1);
        return buffer == 0xe9;
    }

    static bool IsAbsoluteIndirectNearJumpPresentAtAddress(uintptr_t address)
    {
        unsigned char buffer[3];
        MemCopy((uintptr_t)buffer, address, 3);
        return buffer[0] == 0x48 && buffer[1] == 0xff && buffer[2] == 0x25;
    }

    static bool IsAbsoluteDirectFarJumpPresentAtAddress(uintptr_t address)
    {
        unsigned char buffer[6];
        MemCopy((uintptr_t)buffer, address, 6);
        return buffer[0] == 0xff && buffer[1] == 0x25 && 
               buffer[2] == 0x00 && buffer[3] == 0x00 && buffer[4] == 0x00 && buffer[5] == 0x00;
    }

    static bool IsAddressHooked(uintptr_t address)
    {
        return IsRelativeNearJumpPresentAtAddress(address) || 
               IsAbsoluteIndirectNearJumpPresentAtAddress(address) ||
               IsAbsoluteDirectFarJumpPresentAtAddress(address);
    }

    static size_t CalculateRequiredAsmClearance(uintptr_t address, size_t minimumClearance)
    {
        constexpr size_t maximumAmountOfBytesToCheck = 30;
        std::vector<uint8_t> bytesBuffer(maximumAmountOfBytesToCheck);
        MemCopy((uintptr_t)bytesBuffer.data(), address, maximumAmountOfBytesToCheck);

        if (IsAbsoluteDirectFarJumpPresentAtAddress(address)) return 14;

        for (size_t byteCount = 0; byteCount < maximumAmountOfBytesToCheck;)
        {
            size_t instructionSize = nmd_x86_ldisasm(
                &bytesBuffer[byteCount],
                maximumAmountOfBytesToCheck - byteCount,
                NMD_X86_MODE_64);

            if (instructionSize <= 0) {
                logger.Log("Instruction invalid, could not check length!");
                return minimumClearance;
            }

            if (byteCount >= minimumClearance) return byteCount;
            byteCount += instructionSize;
        }
        return minimumClearance;
    }

    static uintptr_t CalculateAbsoluteDestinationFromRelativeNearJumpAtAddress(uintptr_t relativeNearJumpMemoryLocation)
    {
        int32_t offset = 0;
        MemCopy((uintptr_t)&offset, relativeNearJumpMemoryLocation + 1, 4);
        return relativeNearJumpMemoryLocation + 5 + offset;
    }

    static uintptr_t CalculateAbsoluteDestinationFromAbsoluteIndirectNearJumpAtAddress(uintptr_t absoluteIndirectNearJumpMemoryLocation)
    {
        int32_t offset = 0;
        MemCopy((uintptr_t)&offset, absoluteIndirectNearJumpMemoryLocation + 3, 4);
        uintptr_t memoryContainingAbsoluteAddress = absoluteIndirectNearJumpMemoryLocation + 7 + offset;
        return *(uintptr_t*)memoryContainingAbsoluteAddress;
    }

    static int32_t CalculateRelativeDisplacementForRelativeJump(uintptr_t relativeJumpAddress, uintptr_t destinationAddress)
    {
        return -int32_t(relativeJumpAddress + 5 - destinationAddress);
    }

    static void PlaceAbsoluteJump(uintptr_t address, uintptr_t destinationAddress, size_t clearance = 14)
    {
        MemSet(address, 0x90, clearance);
        unsigned char absoluteJumpBytes[6] = { 0xff, 0x25, 0x00, 0x00, 0x00, 0x00 };
        MemCopy(address, (uintptr_t)absoluteJumpBytes, 6);
        MemCopy(address + 6, (uintptr_t)&destinationAddress, 8);
        logger.Log("Created absolute jump from %p to %p with a clearance of %i", address, destinationAddress, clearance);
    }

    static void PlaceRelativeJump(uintptr_t address, uintptr_t destinationAddress, size_t clearance = 5)
    {
        MemSet(address, 0x90, clearance);
        unsigned char relativeJumpBytes[5] = { 0xe9, 0x00, 0x00, 0x00, 0x00 };
        MemCopy(address, (uintptr_t)relativeJumpBytes, 5);
        int32_t relativeAddress = CalculateRelativeDisplacementForRelativeJump(address, destinationAddress);
        MemCopy(address + 1, (uintptr_t)&relativeAddress, 4);
        logger.Log("Created relative jump from %p to %p with a clearance of %i", address, destinationAddress, clearance);
    }

    static void PrintBytesAtAddress(uintptr_t address, size_t numBytes)
    {
        std::vector<uint8_t> bytesBuffer(numBytes);
        MemCopy((uintptr_t)bytesBuffer.data(), address, numBytes);
        
        std::stringstream ss;
        ss << "Existing bytes: ";
        for (auto byte : bytesBuffer) {
            ss << "0x" << std::hex << std::setfill('0') << std::setw(2) << (int)byte << " ";
        }
        logger.Log("%s", ss.str().c_str());
    }

    void PlaceHook(uintptr_t addressToHook, uintptr_t destinationAddress, uintptr_t* returnAddress)
    {
        logger.Log("Hooking...");

        constexpr int maxFollowAttempts = 50;
        int countFollowAttempts = 0;
        
        while (IsAddressHooked(addressToHook) && countFollowAttempts < maxFollowAttempts)
        {
            if (IsRelativeNearJumpPresentAtAddress(addressToHook)) {
                addressToHook = CalculateAbsoluteDestinationFromRelativeNearJumpAtAddress(addressToHook);
            } else if (IsAbsoluteIndirectNearJumpPresentAtAddress(addressToHook)) {
                addressToHook = CalculateAbsoluteDestinationFromAbsoluteIndirectNearJumpAtAddress(addressToHook);
            }
            countFollowAttempts++;
        }

        PrintBytesAtAddress(addressToHook, 20);

        constexpr size_t assemblyShortJumpSize = 5;
        constexpr size_t assemblyFarJumpSize = 14;
        constexpr size_t thirdPartyHookProtectionBuffer = assemblyFarJumpSize;
        
        size_t clearance = CalculateRequiredAsmClearance(addressToHook, assemblyShortJumpSize);
        size_t trampolineSize = assemblyFarJumpSize * 3 + clearance + thirdPartyHookProtectionBuffer;
        
        uintptr_t trampolineAddress = AllocateMemoryWithin32BitRange(trampolineSize, addressToHook + assemblyShortJumpSize);
        uintptr_t trampolineReturnAddress = addressToHook + clearance;
        uintptr_t originalInstructionsInTrampoline = trampolineAddress + assemblyFarJumpSize + thirdPartyHookProtectionBuffer;

        MemCopy(originalInstructionsInTrampoline, addressToHook, clearance);

        HookInformation hookInfo;
        hookInfo.originalBytes.resize(clearance);
        hookInfo.trampolineInstructionsAddress = originalInstructionsInTrampoline;
        MemCopy((uintptr_t)hookInfo.originalBytes.data(), originalInstructionsInTrampoline, clearance);
        InfoBufferForHookedAddresses[addressToHook] = hookInfo;

        PlaceAbsoluteJump(trampolineAddress + thirdPartyHookProtectionBuffer, destinationAddress);
        PlaceAbsoluteJump(trampolineAddress + trampolineSize - assemblyFarJumpSize, trampolineReturnAddress);
        
        *returnAddress = originalInstructionsInTrampoline;
        PlaceRelativeJump(addressToHook, trampolineAddress, clearance);
    }

    void Unhook(uintptr_t hookedAddress)
    {
        auto it = InfoBufferForHookedAddresses.find(hookedAddress);
        if (it != InfoBufferForHookedAddresses.end())
        {
            auto& hookInfo = it->second;
            MemSet(hookInfo.trampolineInstructionsAddress, 0x90, hookInfo.originalBytes.size());
            MemCopy(hookedAddress, (uintptr_t)hookInfo.originalBytes.data(), hookInfo.originalBytes.size());
            logger.Log("Removed hook from %p", hookedAddress);
            InfoBufferForHookedAddresses.erase(it);
        }
    }

    uintptr_t GetProcessBaseAddress(DWORD processId) noexcept
    {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProcess == NULL) return 0;

        HMODULE hModule;
        DWORD cbNeeded;
        uintptr_t baseAddress = 0;

        if (EnumProcessModules(hProcess, &hModule, sizeof(hModule), &cbNeeded))
        {
            baseAddress = reinterpret_cast<uintptr_t>(hModule);
        }

        CloseHandle(hProcess);
        return baseAddress;
    }

    uintptr_t SigScanCached(const std::vector<uint16_t>& pattern) noexcept
    {
        uint64_t hash = HashPattern(pattern);
        
        for (const auto& entry : patternCache) {
            if (entry.hash == hash && entry.patternSize == pattern.size()) {
                bool match = true;
                for (size_t i = 0; i < pattern.size() && i < 32; ++i) {
                    if (entry.pattern[i] != pattern[i]) {
                        match = false;
                        break;
                    }
                }
                if (match) return entry.result;
            }
        }
        
        uintptr_t result = SigScan(pattern);
        
        auto& entry = patternCache[cacheIndex % PATTERN_CACHE_SIZE];
        entry.hash = hash;
        entry.patternSize = min(pattern.size(), size_t(32));
        entry.result = result;
        for (size_t i = 0; i < entry.patternSize; ++i) {
            entry.pattern[i] = pattern[i];
        }
        ++cacheIndex;
        
        return result;
    }

    uintptr_t SigScan(const std::vector<uint16_t>& pattern) noexcept
    {
        DWORD processId = GetCurrentProcessId();
        uintptr_t baseAddress = GetProcessBaseAddress(processId);
        if (baseAddress == 0) return 0;

        MEMORY_BASIC_INFORMATION mbi;
        uintptr_t searchAddress = baseAddress;
        size_t patternSize = pattern.size();

        while (VirtualQuery(reinterpret_cast<LPCVOID>(searchAddress), &mbi, sizeof(mbi))) {
            if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_GUARD) == 0 && mbi.Protect != PAGE_NOACCESS) {
                auto* buffer = static_cast<uint8_t*>(mbi.BaseAddress);
                size_t regionSize = mbi.RegionSize;

                for (size_t i = 0; i <= regionSize - patternSize; ++i) {
                    bool found = true;
                    for (size_t j = 0; j < patternSize; ++j) {
                        if (pattern[j] != maskBytes && pattern[j] != buffer[i + j]) {
                            found = false;
                            break;
                        }
                    }
                    if (found) return reinterpret_cast<uintptr_t>(buffer + i);
                }
            }

            searchAddress = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            if (searchAddress < reinterpret_cast<uintptr_t>(mbi.BaseAddress)) break;
        }

        return 0;
    }
} 