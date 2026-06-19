#include <Windows.h>
#include <Psapi.h>
#include <unordered_map>

#include "MemoryUtils.h"

namespace MemoryUtils {
    Logger logger{"MemoryUtils"};
    std::unordered_map<uintptr_t, HookInformation> InfoBufferForHookedAddresses;

    static std::unordered_map<uintptr_t, DWORD> g_protectionHistory;
    static constexpr size_t MAX_PROTECTION_HISTORY = 128;

    void ToggleMemoryProtection(bool enableProtection, uintptr_t address, size_t size) noexcept {
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

    template <typename T> static inline bool SafeReadMemory(uintptr_t address, T& output) noexcept {
        __try {
            output = *reinterpret_cast<const T*>(address);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    template <size_t N> static inline bool SafeReadMemoryArray(uintptr_t address, uint8_t (&output)[N]) noexcept {
        __try {
            std::memcpy(output, reinterpret_cast<const void*>(address), N);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static size_t GetInstructionLength(const uint8_t* code, size_t maxLength) {
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
            } else if (byte == 0x67 || byte == 0xF0 || byte == 0xF2 || byte == 0xF3 || byte == 0x26 || byte == 0x2E || byte == 0x36 || byte == 0x3E || byte == 0x64 || byte == 0x65) {
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
            } else if ((opcode >= 0x10 && opcode <= 0x17) || (opcode >= 0x28 && opcode <= 0x2F) || (opcode >= 0x40 && opcode <= 0x76) || opcode == 0xAE || opcode == 0xAF || (opcode >= 0xB0 && opcode <= 0xB7) || (opcode >= 0xC2 && opcode <= 0xC6)) {
                hasModRM = true;
            }
        } else {
            if ((opcode >= 0x00 && opcode <= 0x03) || (opcode >= 0x08 && opcode <= 0x0B) ||
                (opcode >= 0x10 && opcode <= 0x13) || (opcode >= 0x18 && opcode <= 0x1B) ||
                (opcode >= 0x20 && opcode <= 0x23) || (opcode >= 0x28 && opcode <= 0x2B) ||
                (opcode >= 0x30 && opcode <= 0x33) || (opcode >= 0x38 && opcode <= 0x3B) ||
                (opcode >= 0x62 && opcode <= 0x63) || (opcode >= 0x69 && opcode <= 0x6B) ||
                (opcode >= 0x80 && opcode <= 0x8F) || opcode == 0xC0 || opcode == 0xC1 || opcode == 0xC6 ||
                opcode == 0xC7 || opcode == 0xD0 || opcode == 0xD1 || opcode == 0xD2 || opcode == 0xD3 ||
                opcode == 0xF6 || opcode == 0xF7 || opcode == 0xFE || opcode == 0xFF) {
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

    static size_t CalculateRequiredAsmClearance(uintptr_t address, size_t minimumClearance) {
        uint8_t buffer[MAX_ASM_BYTES];
        if (!SafeReadMemoryArray(address, buffer)) return minimumClearance;

        if (buffer[0] == 0xff && buffer[1] == 0x25 && buffer[2] == 0x00 && buffer[3] == 0x00 && buffer[4] == 0x00 &&
            buffer[5] == 0x00) {
            return FAR_JUMP_SIZE;
        }

        for (size_t byteCount = 0; byteCount < MAX_ASM_BYTES;) {
            size_t instructionSize = GetInstructionLength(&buffer[byteCount], MAX_ASM_BYTES - byteCount);
            if (instructionSize == 0) return minimumClearance;
            if (byteCount >= minimumClearance) return byteCount;
            byteCount += instructionSize;
        }
        return minimumClearance;
    }

    static void PlaceJump(
        uintptr_t address, uintptr_t destination, bool absolute = false, size_t clearance = MIN_CLEARANCE
    ) {
        ToggleMemoryProtection(false, address, clearance);

        uint8_t* ptr = reinterpret_cast<uint8_t*>(address);

        if (absolute) {
            static constexpr uint8_t absJumpHeader[ABS_JUMP_HEADER_SIZE] = {0xff, 0x25, 0x00, 0x00, 0x00, 0x00};
            std::memcpy(ptr, absJumpHeader, ABS_JUMP_HEADER_SIZE);
            std::memcpy(ptr + ABS_JUMP_HEADER_SIZE, &destination, sizeof(destination));
            std::memset(ptr + ABS_JUMP_FULL_SIZE, NOP_INSTRUCTION, clearance - ABS_JUMP_FULL_SIZE);
        } else {
            ptr[0] = 0xe9;
            int64_t rel64 = static_cast<int64_t>(destination) - static_cast<int64_t>(address + REL_JUMP_SIZE);
            int32_t offset = static_cast<int32_t>(rel64);
            std::memcpy(ptr + 1, &offset, sizeof(offset));
            std::memset(ptr + REL_JUMP_SIZE, NOP_INSTRUCTION, clearance - REL_JUMP_SIZE);
        }

        ToggleMemoryProtection(true, address, clearance);
    }

    static uintptr_t AllocateMemoryWithin32BitRange(size_t numBytes, uintptr_t origin) {
        uintptr_t lowerBound = origin > MEMORY_RANGE_32BIT ? origin - MEMORY_RANGE_32BIT : 0;
        uintptr_t higherBound =
            (origin <= UINTPTR_MAX - MEMORY_RANGE_32BIT) ? origin + MEMORY_RANGE_32BIT : UINTPTR_MAX;

        SYSTEM_INFO si;
        GetSystemInfo(&si);
        size_t alignedSize = (numBytes + si.dwPageSize - 1) & ~(static_cast<unsigned long long>(si.dwPageSize) - 1);

        for (uintptr_t i = lowerBound; i < higherBound; i += ALLOCATION_INCREMENT) {
            if (uintptr_t addr =
                    (uintptr_t)VirtualAlloc((void*)i, alignedSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) {
                if (addr >= lowerBound && addr <= higherBound) {
                    std::memset(reinterpret_cast<void*>(addr), NOP_INSTRUCTION, numBytes);
                    return addr;
                }
                VirtualFree((void*)addr, 0, MEM_RELEASE);
            }
        }
        return 0;
    }

    static void FixupRelativeOffsets(uintptr_t trampolineAddr, uintptr_t originalAddr, size_t size) {
        uint8_t* code = reinterpret_cast<uint8_t*>(trampolineAddr);

        for (size_t pos = 0; pos < size;) {
            if (pos + FAR_JUMP_SIZE <= size && code[pos] == 0xFF && code[pos + 1] == 0x25 && code[pos + 2] == 0x00 &&
                code[pos + 3] == 0x00 && code[pos + 4] == 0x00 && code[pos + 5] == 0x00) {
                pos += FAR_JUMP_SIZE;
                continue;
            }

            size_t instrLen = GetInstructionLength(code + pos, size - pos);
            if (instrLen == 0) break;

            if ((code[pos] == 0xE9 || code[pos] == 0xE8) && instrLen == 5) {
                int32_t* rel = reinterpret_cast<int32_t*>(code + pos + 1);
                uintptr_t target = (originalAddr + pos + 5) + static_cast<int64_t>(*rel);
                int64_t newRel = static_cast<int64_t>(target) - static_cast<int64_t>(trampolineAddr + pos + 5);
                if (newRel >= INT32_MIN && newRel <= INT32_MAX) {
                    *rel = static_cast<int32_t>(newRel);
                } else {
                    logger.Log("Relative offset fixup out of 32-bit range at trampoline+{:#x}", pos);
                }
            }

            pos += instrLen;
        }
    }

    bool PlaceHook(uintptr_t addressToHook, uintptr_t destinationAddress, uintptr_t* returnAddress) {
        if (!addressToHook || !destinationAddress || !returnAddress) {
            logger.Log("Invalid hook request");
            if (returnAddress) *returnAddress = 0;
            return false;
        }

        *returnAddress = 0;

        size_t clearance = CalculateRequiredAsmClearance(addressToHook, NEAR_JUMP_SIZE);
        size_t trampolineSize = TRAMPOLINE_BUFFER_SIZE + clearance + PROTECTION_BUFFER;

        uintptr_t trampoline = AllocateMemoryWithin32BitRange(trampolineSize, addressToHook);
        if (!trampoline) {
            logger.Log("Failed to allocate trampoline memory");
            return false;
        }

        uintptr_t originalInstructions = trampoline + FAR_JUMP_SIZE + PROTECTION_BUFFER;
        MemCopy(reinterpret_cast<void*>(originalInstructions), reinterpret_cast<void*>(addressToHook), clearance);

        HookInformation hookInfo;
        hookInfo.originalBytesSize = clearance;
        hookInfo.trampolineInstructionsAddress = originalInstructions;
        hookInfo.trampolineBase = trampoline;
        MemCopy(hookInfo.originalBytes.data(), reinterpret_cast<void*>(originalInstructions), clearance);
        InfoBufferForHookedAddresses[addressToHook] = hookInfo;

        PlaceJump(trampoline + PROTECTION_BUFFER, destinationAddress, true, FAR_JUMP_SIZE);
        PlaceJump(trampoline + trampolineSize - FAR_JUMP_SIZE, addressToHook + clearance, true, FAR_JUMP_SIZE);

        uint8_t firstByte = *reinterpret_cast<uint8_t*>(originalInstructions);
        if (firstByte == 0xE9) {
            int32_t rel = *reinterpret_cast<int32_t*>(originalInstructions + 1);
            *returnAddress = (addressToHook + REL_JUMP_SIZE) + static_cast<int64_t>(rel);
        } else {
            FixupRelativeOffsets(originalInstructions, addressToHook, clearance);
            *returnAddress = originalInstructions;
        }

        PlaceJump(addressToHook, trampoline, false, clearance);
        return true;
    }

    void Unhook(uintptr_t hookedAddress) {
        auto it = InfoBufferForHookedAddresses.find(hookedAddress);
        if (it == InfoBufferForHookedAddresses.end()) return;

        auto& hookInfo = it->second;

        uint8_t firstByte = 0;
        int32_t jumpOffset = 0;
        if (!SafeReadMemory(hookedAddress, firstByte) || firstByte != 0xE9 ||
            !SafeReadMemory(hookedAddress + 1, jumpOffset)) {
            return;
        }

        const uintptr_t jumpTarget = hookedAddress + REL_JUMP_SIZE + jumpOffset;
        const uintptr_t trampBase = hookInfo.trampolineBase;
        const bool isOurHook =
            trampBase != 0 && jumpTarget >= trampBase && jumpTarget <= trampBase + TRAMPOLINE_BUFFER_SIZE * 2;
        if (!isOurHook) return;

        MemCopy(reinterpret_cast<void*>(hookedAddress), hookInfo.originalBytes.data(), hookInfo.originalBytesSize);

        if (hookInfo.trampolineBase) {
            VirtualFree(reinterpret_cast<void*>(hookInfo.trampolineBase), 0, MEM_RELEASE);
        }

        InfoBufferForHookedAddresses.erase(it);
    }

}
