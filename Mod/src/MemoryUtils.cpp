#include "MemoryUtils.h"
#include <Windows.h>
#include <map>
#include <vector>
#include <Psapi.h>
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

    static bool IsAddressHooked(uintptr_t address)
    {
        unsigned char buffer[6];
        MemCopy((uintptr_t)buffer, address, 6);
        
        return buffer[0] == 0xe9 || 
               (buffer[0] == 0x48 && buffer[1] == 0xff && buffer[2] == 0x25) ||
               (buffer[0] == 0xff && buffer[1] == 0x25 && 
                buffer[2] == 0x00 && buffer[3] == 0x00 && buffer[4] == 0x00 && buffer[5] == 0x00);
    }

    static uintptr_t FollowJump(uintptr_t address)
    {
        unsigned char buffer[7];
        MemCopy((uintptr_t)buffer, address, 7);
        
        if (buffer[0] == 0xe9) {
            int32_t offset = *(int32_t*)(buffer + 1);
            return address + 5 + offset;
        }
        else if (buffer[0] == 0x48 && buffer[1] == 0xff && buffer[2] == 0x25) {
            int32_t offset = *(int32_t*)(buffer + 3);
            return *(uintptr_t*)(address + 7 + offset);
        }
        else if (buffer[0] == 0xff && buffer[1] == 0x25) {
            return *(uintptr_t*)(address + 6);
        }
        return address;
    }

    static size_t CalculateRequiredAsmClearance(uintptr_t address, size_t minimumClearance)
    {
        constexpr size_t maxBytes = 30;
        std::vector<uint8_t> buffer(maxBytes);
        MemCopy((uintptr_t)buffer.data(), address, maxBytes);

        unsigned char* bytes = buffer.data();
        if (bytes[0] == 0xff && bytes[1] == 0x25 && 
            bytes[2] == 0x00 && bytes[3] == 0x00 && bytes[4] == 0x00 && bytes[5] == 0x00) {
            return 14;
        }

        for (size_t byteCount = 0; byteCount < maxBytes;)
        {
            size_t instructionSize = nmd_x86_ldisasm(&buffer[byteCount], maxBytes - byteCount, NMD_X86_MODE_64);
            if (instructionSize <= 0) return minimumClearance;
            if (byteCount >= minimumClearance) return byteCount;
            byteCount += instructionSize;
        }
        return minimumClearance;
    }

    static void PlaceJump(uintptr_t address, uintptr_t destination, bool absolute = false, size_t clearance = 5)
    {
        MemSet(address, NOP_INSTRUCTION, clearance);
        
        if (absolute) {
            unsigned char jump[6] = { 0xff, 0x25, 0x00, 0x00, 0x00, 0x00 };
            MemCopy(address, (uintptr_t)jump, 6);
            MemCopy(address + 6, (uintptr_t)&destination, 8);
        } else {
            unsigned char jump[5] = { 0xe9, 0x00, 0x00, 0x00, 0x00 };
            MemCopy(address, (uintptr_t)jump, 5);
            int32_t offset = -int32_t(address + 5 - destination);
            MemCopy(address + 1, (uintptr_t)&offset, 4);
        }
    }

    void PlaceHook(uintptr_t addressToHook, uintptr_t destinationAddress, uintptr_t* returnAddress)
    {
        constexpr int maxAttempts = 50;
        int attempts = 0;
        
        while (IsAddressHooked(addressToHook) && attempts < maxAttempts) {
            addressToHook = FollowJump(addressToHook);
            attempts++;
        }

        constexpr size_t jumpSize = 5;
        constexpr size_t farJumpSize = 14;
        constexpr size_t protectionBuffer = farJumpSize;
        
        size_t clearance = CalculateRequiredAsmClearance(addressToHook, jumpSize);
        size_t trampolineSize = farJumpSize * 3 + clearance + protectionBuffer;
        
        uintptr_t trampoline = AllocateMemoryWithin32BitRange(trampolineSize, addressToHook);
        if (!trampoline) {
            logger.Log("Failed to allocate trampoline memory");
            return;
        }
        
        uintptr_t originalInstructions = trampoline + farJumpSize + protectionBuffer;
        MemCopy(originalInstructions, addressToHook, clearance);

        HookInformation hookInfo;
        hookInfo.originalBytes.resize(clearance);
        hookInfo.trampolineInstructionsAddress = originalInstructions;
        MemCopy((uintptr_t)hookInfo.originalBytes.data(), originalInstructions, clearance);
        InfoBufferForHookedAddresses[addressToHook] = hookInfo;

        PlaceJump(trampoline + protectionBuffer, destinationAddress, true, farJumpSize);
        PlaceJump(trampoline + trampolineSize - farJumpSize, addressToHook + clearance, true, farJumpSize);
        
        *returnAddress = originalInstructions;
        PlaceJump(addressToHook, trampoline, false, clearance);
    }

    void Unhook(uintptr_t hookedAddress)
    {
        auto it = InfoBufferForHookedAddresses.find(hookedAddress);
        if (it != InfoBufferForHookedAddresses.end()) {
            auto& hookInfo = it->second;
            MemCopy(hookedAddress, (uintptr_t)hookInfo.originalBytes.data(), hookInfo.originalBytes.size());
            InfoBufferForHookedAddresses.erase(it);
        }
    }

    uintptr_t GetProcessBaseAddress(DWORD processId) noexcept
    {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (!hProcess) return 0;

        HMODULE hModule;
        DWORD cbNeeded;
        uintptr_t baseAddress = 0;

        if (EnumProcessModules(hProcess, &hModule, sizeof(hModule), &cbNeeded)) {
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
        uintptr_t baseAddress = GetProcessBaseAddress(GetCurrentProcessId());
        if (!baseAddress) return 0;

        MEMORY_BASIC_INFORMATION mbi;
        uintptr_t searchAddress = baseAddress;
        size_t patternSize = pattern.size();

        while (VirtualQuery((LPCVOID)searchAddress, &mbi, sizeof(mbi))) {
            if (mbi.State == MEM_COMMIT && 
                (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_READWRITE | PAGE_READONLY)) &&
                !(mbi.Protect & PAGE_GUARD) && mbi.Protect != PAGE_NOACCESS) {
                
                auto* buffer = static_cast<uint8_t*>(mbi.BaseAddress);
                size_t regionSize = mbi.RegionSize;

                if (regionSize >= patternSize) {
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
            }

            searchAddress = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            if (searchAddress < reinterpret_cast<uintptr_t>(mbi.BaseAddress)) break;
        }

        return 0;
    }
} 