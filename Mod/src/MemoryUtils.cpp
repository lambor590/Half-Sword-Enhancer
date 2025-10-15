#include <Windows.h>
#include <Psapi.h>
#include <unordered_map>

#include "MemoryUtils.h"

namespace MemoryUtils 
{
    Logger logger{ "MemoryUtils" };
    std::unordered_map<uintptr_t, HookInformation> InfoBufferForHookedAddresses;

    static std::unordered_map<uintptr_t, DWORD> g_protectionHistory;

    void ToggleMemoryProtection(bool enableProtection, uintptr_t address, size_t size) noexcept
    {
        auto it = g_protectionHistory.find(address);

        if (enableProtection) {
            if (it != g_protectionHistory.end()) {
                DWORD dummy;
                VirtualProtect((void*)address, size, it->second, &dummy);
                g_protectionHistory.erase(it);
            }
        } else {
            if (it == g_protectionHistory.end()) {
                DWORD oldProtection;
                VirtualProtect((void*)address, size, PAGE_EXECUTE_READWRITE, &oldProtection);
                g_protectionHistory[address] = oldProtection;
            }
        }
    }

    template<typename T>
    static inline bool SafeReadMemory(uintptr_t address, T& output) noexcept
    {
        __try {
            output = *reinterpret_cast<const T*>(address);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    template<size_t N>
    static inline bool SafeReadMemoryArray(uintptr_t address, uint8_t(&output)[N]) noexcept
    {
        __try {
            std::memcpy(output, reinterpret_cast<const void*>(address), N);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool IsAddressHooked(uintptr_t address)
    {
        uint8_t buffer[HOOK_DETECTION_SIZE];
        if (!SafeReadMemoryArray(address, buffer)) return false;

        if (buffer[0] == 0xe9) return true;
        if (buffer[0] == 0x48 && buffer[1] == 0xff && buffer[2] == 0x25) return true;
        if (buffer[0] == 0xff && buffer[1] == 0x25) {
            return buffer[2] == 0x00 && buffer[3] == 0x00 && buffer[4] == 0x00 && buffer[5] == 0x00;
        }
        return false;
    }

    static uintptr_t FollowJump(uintptr_t address)
    {
        uint8_t buffer[FOLLOW_JUMP_BUFFER_SIZE];
        if (!SafeReadMemoryArray(address, buffer)) return address;

        if (buffer[0] == 0xe9) {
            return address + 5 + *reinterpret_cast<int32_t*>(buffer + 1);
        }

        if (buffer[0] == 0x48 && buffer[1] == 0xff && buffer[2] == 0x25) {
            uintptr_t targetAddr = address + 7 + *reinterpret_cast<int32_t*>(buffer + 3);
            uintptr_t result;
            if (!SafeReadMemory(targetAddr, result)) return address;
            return result;
        }

        if (buffer[0] == 0xff && buffer[1] == 0x25) {
            uintptr_t result;
            if (!SafeReadMemory(address + 6, result)) return address;
            return result;
        }

        return address;
    }

    static size_t CalculateRequiredAsmClearance(uintptr_t address, size_t minimumClearance)
    {
        uint8_t buffer[MAX_ASM_BYTES];
        if (!SafeReadMemoryArray(address, buffer)) return minimumClearance;

        if (buffer[0] == 0xff && buffer[1] == 0x25 &&
            buffer[2] == 0x00 && buffer[3] == 0x00 && buffer[4] == 0x00 && buffer[5] == 0x00) {
            return FAR_JUMP_SIZE;
        }

        for (size_t byteCount = 0; byteCount < MAX_ASM_BYTES;)
        {
            size_t instructionSize = nmd_x86_ldisasm(&buffer[byteCount], MAX_ASM_BYTES - byteCount, NMD_X86_MODE_64);
            if (instructionSize <= 0) return minimumClearance;
            if (byteCount >= minimumClearance) return byteCount;
            byteCount += instructionSize;
        }
        return minimumClearance;
    }

    static void PlaceJump(uintptr_t address, uintptr_t destination, bool absolute = false, size_t clearance = MIN_CLEARANCE)
    {
        uint8_t buffer[32];
        std::memset(buffer, NOP_INSTRUCTION, clearance);

        if (absolute) {
            static constexpr uint8_t absJumpHeader[ABS_JUMP_HEADER_SIZE] = { 0xff, 0x25, 0x00, 0x00, 0x00, 0x00 };
            std::memcpy(buffer, absJumpHeader, ABS_JUMP_HEADER_SIZE);
            std::memcpy(buffer + ABS_JUMP_HEADER_SIZE, &destination, sizeof(destination));
        } else {
            buffer[0] = 0xe9;
            int32_t offset = -int32_t(address + REL_JUMP_SIZE - destination);
            std::memcpy(buffer + 1, &offset, sizeof(offset));
        }

        MemCopy(address, (uintptr_t)buffer, clearance);
    }

    void PlaceHook(uintptr_t addressToHook, uintptr_t destinationAddress, uintptr_t* returnAddress)
    {
        int attempts = 0;

        while (IsAddressHooked(addressToHook) && attempts < MAX_HOOK_FOLLOW_ATTEMPTS) {
            addressToHook = FollowJump(addressToHook);
            attempts++;
        }

        size_t clearance = CalculateRequiredAsmClearance(addressToHook, NEAR_JUMP_SIZE);
        size_t trampolineSize = TRAMPOLINE_BUFFER_SIZE + clearance + PROTECTION_BUFFER;
        
        uintptr_t trampoline = AllocateMemoryWithin32BitRange(trampolineSize, addressToHook);
        if (!trampoline) {
            logger.Log("Failed to allocate trampoline memory");
            return;
        }

        uintptr_t originalInstructions = trampoline + FAR_JUMP_SIZE + PROTECTION_BUFFER;
        MemCopy(originalInstructions, addressToHook, clearance);

        HookInformation hookInfo;
        hookInfo.originalBytesSize = clearance;
        hookInfo.trampolineInstructionsAddress = originalInstructions;
        MemCopy((uintptr_t)hookInfo.originalBytes.data(), originalInstructions, clearance);
        InfoBufferForHookedAddresses[addressToHook] = hookInfo;

        PlaceJump(trampoline + PROTECTION_BUFFER, destinationAddress, true, FAR_JUMP_SIZE);
        PlaceJump(trampoline + trampolineSize - FAR_JUMP_SIZE, addressToHook + clearance, true, FAR_JUMP_SIZE);

        *returnAddress = originalInstructions;
        PlaceJump(addressToHook, trampoline, false, clearance);
    }

    void Unhook(uintptr_t hookedAddress)
    {
        auto it = InfoBufferForHookedAddresses.find(hookedAddress);
        if (it != InfoBufferForHookedAddresses.end()) {
            auto& hookInfo = it->second;
            MemCopy(hookedAddress, (uintptr_t)hookInfo.originalBytes.data(), hookInfo.originalBytesSize);
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
}