#include <Windows.h>
#include <Psapi.h>
#include <unordered_map>

#include "MemoryUtils.h"

namespace MemoryUtils
{
    Logger logger{ "MemoryUtils" };
    std::unordered_map<uintptr_t, HookInformation> InfoBufferForHookedAddresses;

    static std::unordered_map<uintptr_t, DWORD> g_protectionHistory;
    static constexpr size_t MAX_PROTECTION_HISTORY = 128;

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
                if (g_protectionHistory.size() >= MAX_PROTECTION_HISTORY) [[unlikely]] {
                    g_protectionHistory.erase(g_protectionHistory.begin());
                }

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

    static size_t GetInstructionLength(const uint8_t* code, size_t maxLength)
    {
        if (maxLength < 1) return 0;

        size_t offset = 0;
        bool hasRex = false;
        bool operandSize16 = false;

        while (offset < maxLength) {
            uint8_t byte = code[offset];

            if (byte >= 0x40 && byte <= 0x4F) {
                hasRex = true;
                offset++;
            } else if (byte == 0x66) {
                operandSize16 = true;
                offset++;
            } else if (byte == 0x67 || byte == 0xF0 || byte == 0xF2 || byte == 0xF3 ||
                       byte == 0x26 || byte == 0x2E || byte == 0x36 || byte == 0x3E ||
                       byte == 0x64 || byte == 0x65) {
                offset++;
            } else {
                break;
            }
        }

        if (offset >= maxLength) return 0;

        uint8_t opcode = code[offset++];
        bool hasModRM = false;
        uint8_t immSize = 0;

        if (opcode == 0x0F) {
            if (offset >= maxLength) return 0;
            opcode = code[offset++];

            if (opcode >= 0x80 && opcode <= 0x8F) {
                immSize = 4;
            } else if ((opcode >= 0x10 && opcode <= 0x17) || (opcode >= 0x28 && opcode <= 0x2F) ||
                       (opcode >= 0x40 && opcode <= 0x76) || opcode == 0xAE || opcode == 0xAF ||
                       (opcode >= 0xB0 && opcode <= 0xB7) || (opcode >= 0xC2 && opcode <= 0xC6)) {
                hasModRM = true;
            }
        } else {
            if ((opcode >= 0x00 && opcode <= 0x03) || (opcode >= 0x08 && opcode <= 0x0B) ||
                (opcode >= 0x10 && opcode <= 0x13) || (opcode >= 0x18 && opcode <= 0x1B) ||
                (opcode >= 0x20 && opcode <= 0x23) || (opcode >= 0x28 && opcode <= 0x2B) ||
                (opcode >= 0x30 && opcode <= 0x33) || (opcode >= 0x38 && opcode <= 0x3B) ||
                (opcode >= 0x62 && opcode <= 0x63) || (opcode >= 0x69 && opcode <= 0x6B) ||
                (opcode >= 0x80 && opcode <= 0x8F) || opcode == 0xC0 || opcode == 0xC1 ||
                opcode == 0xC6 || opcode == 0xC7 || opcode == 0xD0 || opcode == 0xD1 ||
                opcode == 0xD2 || opcode == 0xD3 || opcode == 0xF6 || opcode == 0xF7 ||
                opcode == 0xFE || opcode == 0xFF) {
                hasModRM = true;
            }

            if (opcode == 0x6A || opcode == 0x6B || opcode == 0xA8 || opcode == 0xEB ||
                (opcode >= 0x70 && opcode <= 0x7F)) {
                immSize = 1;
            } else if (opcode == 0x80 || opcode == 0x82 || opcode == 0x83 || opcode == 0xC6) {
                immSize = 1;
            } else if (opcode == 0x81 || opcode == 0x69 || opcode == 0xC7) {
                immSize = operandSize16 ? 2 : 4;
            } else if (opcode == 0x68 || opcode == 0xE8 || opcode == 0xE9) {
                immSize = 4;
            } else if (opcode == 0xA0 || opcode == 0xA1 || opcode == 0xA2 || opcode == 0xA3) {
                immSize = 8;
            } else if (opcode >= 0xB0 && opcode <= 0xB7) {
                immSize = 1;
            } else if (opcode >= 0xB8 && opcode <= 0xBF) {
                immSize = hasRex ? 8 : 4;
            }
        }

        if (hasModRM) {
            if (offset >= maxLength) return 0;
            uint8_t modrm = code[offset++];
            uint8_t mod = (modrm >> 6) & 0x03;
            uint8_t rm = modrm & 0x07;

            if (mod != 0x03 && rm == 0x04) {
                if (offset >= maxLength) return 0;
                offset++;
            }

            if (mod == 0x01) {
                offset += 1;
            } else if (mod == 0x02) {
                offset += 4;
            } else if (mod == 0x00 && rm == 0x05) {
                offset += 4;
            }
        }

        offset += immSize;
        return offset;
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
            size_t instructionSize = GetInstructionLength(&buffer[byteCount], MAX_ASM_BYTES - byteCount);
            if (instructionSize <= 0) return minimumClearance;
            if (byteCount >= minimumClearance) return byteCount;
            byteCount += instructionSize;
        }
        return minimumClearance;
    }

    static void PlaceJump(uintptr_t address, uintptr_t destination, bool absolute = false, size_t clearance = MIN_CLEARANCE)
    {
        ToggleMemoryProtection(false, address, clearance);

        uint8_t* ptr = reinterpret_cast<uint8_t*>(address);

        if (absolute) {
            static constexpr uint8_t absJumpHeader[ABS_JUMP_HEADER_SIZE] = { 0xff, 0x25, 0x00, 0x00, 0x00, 0x00 };
            std::memcpy(ptr, absJumpHeader, ABS_JUMP_HEADER_SIZE);
            std::memcpy(ptr + ABS_JUMP_HEADER_SIZE, &destination, sizeof(destination));
            std::memset(ptr + ABS_JUMP_FULL_SIZE, NOP_INSTRUCTION, clearance - ABS_JUMP_FULL_SIZE);
        } else {
            ptr[0] = 0xe9;
            int32_t offset = -int32_t(address + REL_JUMP_SIZE - destination);
            std::memcpy(ptr + 1, &offset, sizeof(offset));
            std::memset(ptr + REL_JUMP_SIZE, NOP_INSTRUCTION, clearance - REL_JUMP_SIZE);
        }

        ToggleMemoryProtection(true, address, clearance);
    }

    static size_t GetExistingHookJumpSize(uintptr_t address)
    {
        uint8_t buffer[FAR_JUMP_SIZE + 1];
        if (!SafeReadMemoryArray(address, buffer)) return 0;

        if (buffer[0] == 0xe9)
            return REL_JUMP_SIZE;

        if (buffer[0] == 0xff && buffer[1] == 0x25 &&
            buffer[2] == 0x00 && buffer[3] == 0x00 &&
            buffer[4] == 0x00 && buffer[5] == 0x00)
            return FAR_JUMP_SIZE;

        if (buffer[0] == 0x48 && buffer[1] == 0xff && buffer[2] == 0x25) {
            int32_t disp = *reinterpret_cast<int32_t*>(buffer + 3);
            return (disp == 0) ? 15 : 7;
        }

        return 0;
    }

    void PlaceHook(uintptr_t addressToHook, uintptr_t destinationAddress, uintptr_t* returnAddress)
    {
        if (IsAddressHooked(addressToHook)) {
            uintptr_t existingTarget = FollowJump(addressToHook);

            if (existingTarget != addressToHook) {
                logger.Log("Existing hook at %p -> %p, chaining", (void*)addressToHook, (void*)existingTarget);

                size_t clearance = GetExistingHookJumpSize(addressToHook);
                if (clearance < NEAR_JUMP_SIZE) clearance = NEAR_JUMP_SIZE;

                size_t trampolineSize = FAR_JUMP_SIZE + PROTECTION_BUFFER;
                uintptr_t trampoline = AllocateMemoryWithin32BitRange(trampolineSize, addressToHook);
                if (!trampoline) {
                    logger.Log("Failed to allocate trampoline for chained hook");
                    return;
                }

                HookInformation hookInfo;
                hookInfo.originalBytesSize = clearance;
                hookInfo.trampolineInstructionsAddress = 0;
                hookInfo.trampolineBase = trampoline;
                MemCopy((uintptr_t)hookInfo.originalBytes.data(), addressToHook, clearance);
                InfoBufferForHookedAddresses[addressToHook] = hookInfo;

                PlaceJump(trampoline, destinationAddress, true, FAR_JUMP_SIZE);
                *returnAddress = existingTarget;
                PlaceJump(addressToHook, trampoline, false, clearance);

                logger.Log("Hook chain installed: %p -> detour -> %p", (void*)addressToHook, (void*)existingTarget);
                return;
            }
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
        hookInfo.trampolineBase = trampoline;
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
        if (it == InfoBufferForHookedAddresses.end()) return;

        auto& hookInfo = it->second;

        uint8_t firstByte = 0;
        if (SafeReadMemory(hookedAddress, firstByte) && firstByte == 0xE9) {
            int32_t jumpOffset = 0;
            if (SafeReadMemory(hookedAddress + 1, jumpOffset)) {
                uintptr_t jumpTarget = hookedAddress + REL_JUMP_SIZE + jumpOffset;
                uintptr_t trampBase = hookInfo.trampolineBase;

                bool isOurHook = trampBase != 0
                              && (jumpTarget >= trampBase)
                              && (jumpTarget <= trampBase + TRAMPOLINE_BUFFER_SIZE * 2);

                if (isOurHook) {
                    MemCopy(hookedAddress, (uintptr_t)hookInfo.originalBytes.data(), hookInfo.originalBytesSize);
                }
            }
        }

        InfoBufferForHookedAddresses.erase(it);
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