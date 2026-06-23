#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <unordered_map>

#include "Logger.h"
#include "MemoryUtils.h"

namespace {
    constexpr unsigned char NOP_INSTRUCTION = 0x90;
    constexpr size_t MAX_ASM_BYTES = 30;
    constexpr size_t FAR_JUMP_SIZE = 14;
    constexpr size_t NEAR_JUMP_SIZE = 5;
    constexpr size_t TRAMPOLINE_BUFFER_SIZE = FAR_JUMP_SIZE * 3;
    constexpr size_t PROTECTION_BUFFER = FAR_JUMP_SIZE;
    constexpr size_t MEMORY_RANGE_32BIT = 0x7fffffff;
    constexpr size_t ALLOCATION_INCREMENT = 65536;
    constexpr size_t ABS_JUMP_HEADER_SIZE = 6;
    constexpr size_t ABS_JUMP_FULL_SIZE = 14;
    constexpr size_t REL_JUMP_SIZE = 5;

    static constexpr uint8_t ABS_JUMP_HEADER[ABS_JUMP_HEADER_SIZE] = {0xff, 0x25, 0x00, 0x00, 0x00, 0x00};

    Logger logger{"MemoryUtils"};

    struct HookRecord {
        std::array<uint8_t, 32> originalBytes{};
        size_t originalBytesSize = 0;
        uintptr_t trampolineBase = 0;
    };

    std::unordered_map<uintptr_t, HookRecord> g_hooks;

    class ScopedPageProtection {
      public:
        ScopedPageProtection(uintptr_t address, size_t size) noexcept : address(address), size(size) {
            if (!address || !size) return;
            active = VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtection) != 0;
        }

        ~ScopedPageProtection() noexcept {
            if (!active) return;
            DWORD dummy = 0;
            VirtualProtect(reinterpret_cast<void*>(address), size, oldProtection, &dummy);
        }

        [[nodiscard]] bool IsActive() const noexcept { return active; }

        ScopedPageProtection(const ScopedPageProtection&) = delete;
        ScopedPageProtection& operator=(const ScopedPageProtection&) = delete;

      private:
        uintptr_t address = 0;
        size_t size = 0;
        DWORD oldProtection = 0;
        bool active = false;
    };

    static bool FlushCode(uintptr_t address, size_t size) noexcept {
        return FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<const void*>(address), size) != 0;
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

    static bool IsAbsoluteJumpStub(const uint8_t* code, size_t size) noexcept {
        return size >= ABS_JUMP_HEADER_SIZE && std::memcmp(code, ABS_JUMP_HEADER, ABS_JUMP_HEADER_SIZE) == 0;
    }

    struct InstructionInfo {
        size_t length = 0;
        size_t opcodeOffset = 0;
        size_t ripRelativeDispOffset = 0;
        uint8_t opcode = 0;
        bool ripRelative = false;
        bool twoByteOpcode = false;
    };

    static InstructionInfo DecodeInstruction(const uint8_t* code, size_t maxLength) {
        InstructionInfo info;
        if (maxLength < 1) return info;

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

        if (offset >= maxLength) return info;

        info.opcodeOffset = offset;
        info.opcode = code[offset++];
        bool hasModRM = false;
        uint8_t immSize = 0;

        if (info.opcode == 0x0F) {
            if (offset >= maxLength) return info;
            info.twoByteOpcode = true;
            info.opcodeOffset = offset;
            info.opcode = code[offset++];

            if (info.opcode >= 0x80 && info.opcode <= 0x8F) {
                immSize = 4;
            } else if ((info.opcode >= 0x10 && info.opcode <= 0x17) ||
                       (info.opcode >= 0x28 && info.opcode <= 0x2F) ||
                       (info.opcode >= 0x40 && info.opcode <= 0x76) || info.opcode == 0xAE ||
                       info.opcode == 0xAF || (info.opcode >= 0xB0 && info.opcode <= 0xB7) ||
                       (info.opcode >= 0xC2 && info.opcode <= 0xC6)) {
                hasModRM = true;
            }
        } else {
            const uint8_t opcode = info.opcode;
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
                (opcode >= 0x70 && opcode <= 0x7F) || (opcode >= 0xE0 && opcode <= 0xE3)) {
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
            if (offset >= maxLength) return info;
            uint8_t modrm = code[offset++];
            uint8_t mod = (modrm >> 6) & 0x03;
            uint8_t rm = modrm & 0x07;

            if (mod != 0x03 && rm == 0x04) {
                if (offset >= maxLength) return info;
                uint8_t sib = code[offset++];
                uint8_t base = sib & 0x07;
                if (mod == 0x00 && base == 0x05) {
                    offset += 4;
                }
            } else if (mod == 0x00 && rm == 0x05) {
                info.ripRelative = true;
                info.ripRelativeDispOffset = offset;
                offset += 4;
            }

            if (mod == 0x01) {
                offset += 1;
            } else if (mod == 0x02) {
                offset += 4;
            }
        }

        offset += immSize;
        if (offset > maxLength) return {};
        info.length = offset;
        return info;
    }

    static size_t CalculateRequiredAsmClearance(uintptr_t address, size_t minimumClearance) {
        uint8_t buffer[MAX_ASM_BYTES];
        if (!SafeReadMemoryArray(address, buffer)) return 0;

        if (IsAbsoluteJumpStub(buffer, sizeof(buffer))) {
            return FAR_JUMP_SIZE;
        }

        for (size_t byteCount = 0; byteCount < MAX_ASM_BYTES;) {
            size_t instructionSize = DecodeInstruction(&buffer[byteCount], MAX_ASM_BYTES - byteCount).length;
            if (instructionSize == 0) return 0;
            byteCount += instructionSize;
            if (byteCount >= minimumClearance) return byteCount;
        }
        return 0;
    }

    static bool PlaceJump(uintptr_t address, uintptr_t destination, bool absolute, size_t clearance) {
        const size_t jumpSize = absolute ? ABS_JUMP_FULL_SIZE : REL_JUMP_SIZE;
        if (!address || clearance < jumpSize) return false;

        int32_t offset = 0;
        if (!absolute) {
            int64_t rel64 = static_cast<int64_t>(destination) - static_cast<int64_t>(address + REL_JUMP_SIZE);
            if (rel64 < std::numeric_limits<int32_t>::min() || rel64 > std::numeric_limits<int32_t>::max()) {
                logger.Log("Near jump target out of 32-bit range at {:#x}", address);
                return false;
            }
            offset = static_cast<int32_t>(rel64);
        }

        ScopedPageProtection protection(address, clearance);
        if (!protection.IsActive()) {
            logger.Log("Failed to make code page writable at {:#x}", address);
            return false;
        }

        uint8_t* ptr = reinterpret_cast<uint8_t*>(address);

        if (absolute) {
            std::memcpy(ptr, ABS_JUMP_HEADER, ABS_JUMP_HEADER_SIZE);
            std::memcpy(ptr + ABS_JUMP_HEADER_SIZE, &destination, sizeof(destination));
            std::memset(ptr + ABS_JUMP_FULL_SIZE, NOP_INSTRUCTION, clearance - ABS_JUMP_FULL_SIZE);
        } else {
            ptr[0] = 0xe9;
            std::memcpy(ptr + 1, &offset, sizeof(offset));
            std::memset(ptr + REL_JUMP_SIZE, NOP_INSTRUCTION, clearance - REL_JUMP_SIZE);
        }

        FlushCode(address, clearance);
        return true;
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

    static bool FixupRelativeOffsets(uintptr_t trampolineAddr, uintptr_t originalAddr, size_t size) {
        uint8_t* code = reinterpret_cast<uint8_t*>(trampolineAddr);

        for (size_t pos = 0; pos < size;) {
            if (IsAbsoluteJumpStub(code + pos, size - pos)) {
                pos += FAR_JUMP_SIZE;
                continue;
            }

            InstructionInfo instr = DecodeInstruction(code + pos, size - pos);
            size_t instrLen = instr.length;
            if (instrLen == 0) {
                logger.Log("Failed to decode instruction at trampoline+{:#x}", pos);
                return false;
            }

            const bool shortBranch =
                !instr.twoByteOpcode && (instr.opcode == 0xEB || (instr.opcode >= 0x70 && instr.opcode <= 0x7F) ||
                                         (instr.opcode >= 0xE0 && instr.opcode <= 0xE3));
            if (shortBranch || (instr.twoByteOpcode && instr.opcode >= 0x80 && instr.opcode <= 0x8F)) {
                logger.Log("Unsupported relative branch at trampoline+{:#x}", pos);
                return false;
            }

            if (!instr.twoByteOpcode && (instr.opcode == 0xE9 || instr.opcode == 0xE8) &&
                instrLen >= instr.opcodeOffset + REL_JUMP_SIZE) {
                int32_t* rel = reinterpret_cast<int32_t*>(code + pos + instr.opcodeOffset + 1);
                uintptr_t target = (originalAddr + pos + instrLen) + static_cast<int64_t>(*rel);
                int64_t newRel =
                    static_cast<int64_t>(target) - static_cast<int64_t>(trampolineAddr + pos + instrLen);
                if (newRel >= std::numeric_limits<int32_t>::min() &&
                    newRel <= std::numeric_limits<int32_t>::max()) {
                    *rel = static_cast<int32_t>(newRel);
                } else {
                    logger.Log("Relative offset fixup out of 32-bit range at trampoline+{:#x}", pos);
                    return false;
                }
            } else if (instr.ripRelative) {
                int32_t* disp = reinterpret_cast<int32_t*>(code + pos + instr.ripRelativeDispOffset);
                uintptr_t target = (originalAddr + pos + instrLen) + static_cast<int64_t>(*disp);
                int64_t newDisp =
                    static_cast<int64_t>(target) - static_cast<int64_t>(trampolineAddr + pos + instrLen);
                if (newDisp >= std::numeric_limits<int32_t>::min() &&
                    newDisp <= std::numeric_limits<int32_t>::max()) {
                    *disp = static_cast<int32_t>(newDisp);
                } else {
                    logger.Log("RIP-relative fixup out of 32-bit range at trampoline+{:#x}", pos);
                    return false;
                }
            }

            pos += instrLen;
        }
        return true;
    }

} // namespace

namespace MemoryUtils {
    bool PlaceHook(uintptr_t addressToHook, uintptr_t destinationAddress, uintptr_t* returnAddress) {
        if (!addressToHook || !destinationAddress || !returnAddress) {
            if (returnAddress) *returnAddress = 0;
            return false;
        }

        *returnAddress = 0;
        if (g_hooks.contains(addressToHook)) {
            logger.Log("Hook already installed at {:#x}", addressToHook);
            return false;
        }

        size_t clearance = CalculateRequiredAsmClearance(addressToHook, NEAR_JUMP_SIZE);
        if (clearance < NEAR_JUMP_SIZE) {
            logger.Log("Failed to calculate hook clearance at {:#x}", addressToHook);
            return false;
        }

        size_t trampolineSize = TRAMPOLINE_BUFFER_SIZE + clearance + PROTECTION_BUFFER;

        uintptr_t trampoline = AllocateMemoryWithin32BitRange(trampolineSize, addressToHook);
        if (!trampoline) {
            logger.Log("Failed to allocate trampoline memory");
            return false;
        }

        uintptr_t originalInstructions = trampoline + FAR_JUMP_SIZE + PROTECTION_BUFFER;
        std::memcpy(reinterpret_cast<void*>(originalInstructions), reinterpret_cast<void*>(addressToHook), clearance);

        HookRecord hookInfo;
        if (clearance > hookInfo.originalBytes.size()) {
            VirtualFree(reinterpret_cast<void*>(trampoline), 0, MEM_RELEASE);
            return false;
        }

        hookInfo.originalBytesSize = clearance;
        hookInfo.trampolineBase = trampoline;
        std::memcpy(hookInfo.originalBytes.data(), reinterpret_cast<void*>(originalInstructions), clearance);

        if (!PlaceJump(trampoline + PROTECTION_BUFFER, destinationAddress, true, FAR_JUMP_SIZE) ||
            !PlaceJump(trampoline + trampolineSize - FAR_JUMP_SIZE, addressToHook + clearance, true, FAR_JUMP_SIZE)) {
            VirtualFree(reinterpret_cast<void*>(trampoline), 0, MEM_RELEASE);
            return false;
        }

        uintptr_t resolvedReturnAddress = 0;
        uintptr_t absoluteJumpTarget = 0;
        uint8_t firstByte = 0;
        uint8_t jumpStub[ABS_JUMP_HEADER_SIZE];
        if (!SafeReadMemory(originalInstructions, firstByte)) {
            VirtualFree(reinterpret_cast<void*>(trampoline), 0, MEM_RELEASE);
            return false;
        }

        if (SafeReadMemoryArray(originalInstructions, jumpStub) &&
            IsAbsoluteJumpStub(jumpStub, sizeof(jumpStub)) &&
            SafeReadMemory(originalInstructions + ABS_JUMP_HEADER_SIZE, absoluteJumpTarget) && absoluteJumpTarget) {
            resolvedReturnAddress = absoluteJumpTarget;
        } else if (firstByte == 0xE9) {
            int32_t rel = 0;
            if (!SafeReadMemory(originalInstructions + 1, rel)) {
                VirtualFree(reinterpret_cast<void*>(trampoline), 0, MEM_RELEASE);
                return false;
            }
            resolvedReturnAddress = (addressToHook + REL_JUMP_SIZE) + static_cast<int64_t>(rel);
        } else {
            if (!FixupRelativeOffsets(originalInstructions, addressToHook, clearance) ||
                !FlushCode(originalInstructions, clearance)) {
                VirtualFree(reinterpret_cast<void*>(trampoline), 0, MEM_RELEASE);
                return false;
            }
            resolvedReturnAddress = originalInstructions;
        }

        g_hooks.emplace(addressToHook, hookInfo);

        if (!PlaceJump(addressToHook, trampoline, false, clearance)) {
            g_hooks.erase(addressToHook);
            VirtualFree(reinterpret_cast<void*>(trampoline), 0, MEM_RELEASE);
            return false;
        }

        *returnAddress = resolvedReturnAddress;
        return true;
    }

    void Unhook(uintptr_t hookedAddress) noexcept {
        auto it = g_hooks.find(hookedAddress);
        if (it == g_hooks.end()) return;

        auto& hookInfo = it->second;

        uint8_t firstByte = 0;
        int32_t jumpOffset = 0;
        if (!SafeReadMemory(hookedAddress, firstByte) || firstByte != 0xE9 ||
            !SafeReadMemory(hookedAddress + 1, jumpOffset)) {
            return;
        }

        const uintptr_t jumpTarget = hookedAddress + REL_JUMP_SIZE + jumpOffset;
        const uintptr_t trampBase = hookInfo.trampolineBase;
        const bool isOurHook = trampBase != 0 && jumpTarget == trampBase;
        if (!isOurHook) return;

        {
            ScopedPageProtection protection(hookedAddress, hookInfo.originalBytesSize);
            if (!protection.IsActive()) return;

            std::memcpy(reinterpret_cast<void*>(hookedAddress), hookInfo.originalBytes.data(), hookInfo.originalBytesSize);
            FlushCode(hookedAddress, hookInfo.originalBytesSize);
        }

        if (hookInfo.trampolineBase) {
            VirtualFree(reinterpret_cast<void*>(hookInfo.trampolineBase), 0, MEM_RELEASE);
        }

        g_hooks.erase(it);
    }

}
