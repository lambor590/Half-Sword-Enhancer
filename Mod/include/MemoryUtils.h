#pragma once

#include <Windows.h>
#include <vector>
#include <Psapi.h>
#include <sstream>
#include <unordered_map>
#include <iomanip>

#define NMD_ASSEMBLY_IMPLEMENTATION
#define NMD_ASSEMBLY_PRIVATE
#include "nmd_assembly.h"

#include "Logger.h"

// Contains various memory manipulation functions related to hooking or modding
namespace MemoryUtils
{
    extern Logger logger;
    static constexpr int maskBytes = 0xffff;

    struct HookInformation
    {
        std::vector<unsigned char> originalBytes = { 0 };
        uintptr_t trampolineInstructionsAddress = 0;
    };
    extern std::unordered_map<uintptr_t, HookInformation> InfoBufferForHookedAddresses;

    void ToggleMemoryProtection(bool enableProtection, uintptr_t address, size_t size);

    // Copies memory after changing the permissions at both the source and destination so we don't get an access violation.
    static void MemCopy(uintptr_t destination, uintptr_t source, size_t numBytes)
    {
        ToggleMemoryProtection(false, destination, numBytes);
        ToggleMemoryProtection(false, source, numBytes);
        memcpy((void*)destination, (void*)source, numBytes);
        ToggleMemoryProtection(true, source, numBytes);
        ToggleMemoryProtection(true, destination, numBytes);
    }

    // Simple wrapper around memset
    static void MemSet(uintptr_t address, unsigned char byte, size_t numBytes)
    {
        ToggleMemoryProtection(false, address, numBytes);
        memset((void*)address, byte, numBytes);
        ToggleMemoryProtection(true, address, numBytes);
    }

    // Gets the base address of the game's memory.
    static uintptr_t GetProcessBaseAddress(DWORD processId)
    {
        DWORD_PTR baseAddress = 0;
        HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

        if (processHandle)
        {
            DWORD bytesRequired = 0;
            if (EnumProcessModules(processHandle, NULL, 0, &bytesRequired))
            {
                if (bytesRequired)
                {
                    LPBYTE moduleArrayBytes = (LPBYTE)LocalAlloc(LPTR, bytesRequired);
                    if (moduleArrayBytes)
                    {
                        HMODULE* moduleArray = (HMODULE*)moduleArrayBytes;
                        if (EnumProcessModules(processHandle, moduleArray, bytesRequired, &bytesRequired))
                        {
                            baseAddress = (DWORD_PTR)moduleArray[0];
                        }

                        LocalFree(moduleArrayBytes);
                    }
                }
            }

            CloseHandle(processHandle);
        }

        return baseAddress;
    }

    static std::string GetCurrentProcessName()
    {
        char lpFilename[MAX_PATH];
        GetModuleFileNameA(NULL, lpFilename, sizeof(lpFilename));
        std::string moduleName = strrchr(lpFilename, '\\');
        moduleName = moduleName.substr(1, moduleName.length());
        return moduleName;
    }

    static std::string GetCurrentModuleName()
    {
        HMODULE module = NULL;

        static char dummyStaticVariableToGetModuleName = 'x';
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            &dummyStaticVariableToGetModuleName,
            &module);

        char lpFilename[MAX_PATH];
        GetModuleFileNameA(module, lpFilename, sizeof(lpFilename));
        char* lastSlash = strrchr(lpFilename, '\\');
        std::string moduleName = "";
        if (lastSlash != nullptr)
        {
            moduleName = lastSlash;
            moduleName = moduleName.substr(1, moduleName.length());
            moduleName.erase(moduleName.find(".dll"), moduleName.length());
        }
        return moduleName;
    }

    static void ShowErrorPopup(std::string error)
    {
        logger.Log("Raised error: %s", error.c_str());
        MessageBox(NULL, error.c_str(), GetCurrentModuleName().c_str(), MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
    }

    static void PrintPattern(std::vector<uint16_t> pattern)
    {
        std::string patternString = "";
        for (auto bytes : pattern)
        {
            std::stringstream stream;
            std::string byte = "";
            if (bytes == maskBytes)
            {
                byte = "?";
            }
            else
            {
                stream << "0x" << std::hex << bytes;
                byte = stream.str();
            }
            patternString.append(byte + " ");
        }
        logger.Log("Pattern: %s", patternString.c_str());
    }

    // Scans the memory of the main process module for the given signature.
    static uintptr_t SigScan(std::vector<uint16_t> pattern)
    {
        DWORD processId = GetCurrentProcessId();
        uintptr_t regionStart = GetProcessBaseAddress(processId);
        logger.Log("Process name: %s", GetCurrentProcessName().c_str());
        logger.Log("Process ID: %i", processId);
        logger.Log("Process base address: 0x%llX", regionStart);
        PrintPattern(pattern);

        size_t numRegionsChecked = 0;
        size_t maxNumberOfRegionsToCheck = 10000;
        uintptr_t currentAddress = 0;
        while (numRegionsChecked < maxNumberOfRegionsToCheck)
        {
            MEMORY_BASIC_INFORMATION memoryInfo = { 0 };
            if (VirtualQuery((void*)regionStart, &memoryInfo, sizeof(MEMORY_BASIC_INFORMATION)) == 0)
            {
                DWORD error = GetLastError();
                if (error == ERROR_INVALID_PARAMETER)
                {
                    logger.Log("Reached end of scannable memory.");
                }
                else
                {
                    logger.Log("VirtualQuery failed, error code: %i.", error);
                }
                break;
            }

            regionStart = (uintptr_t)memoryInfo.BaseAddress;
            size_t regionSize = memoryInfo.RegionSize;
            uintptr_t regionEnd = regionStart + regionSize;
            DWORD protection = memoryInfo.Protect;
            DWORD state = memoryInfo.State;

            bool isMemoryReadable = (
                protection == PAGE_EXECUTE_READWRITE
                || protection == PAGE_READWRITE
                || protection == PAGE_READONLY
                || protection == PAGE_WRITECOPY
                || protection == PAGE_EXECUTE_WRITECOPY)
                && state == MEM_COMMIT;

            if (isMemoryReadable)
            {
                logger.Log("Checking region: %p", regionStart);
                currentAddress = regionStart;
                while (currentAddress < regionEnd - pattern.size())
                {
                    for (size_t i = 0; i < pattern.size(); i++)
                    {
                        if (pattern[i] == maskBytes)
                        {
                            currentAddress++;
                            continue;
                        }
                        else if (*(unsigned char*)currentAddress != (unsigned char)pattern[i])
                        {
                            currentAddress++;
                            break;
                        }
                        else if (i == pattern.size() - 1)
                        {
                            uintptr_t signature = currentAddress - pattern.size() + 1;
                            logger.Log("Found signature at %p", signature);
                            return signature;
                        }
                        currentAddress++;
                    }
                }
            }
            else
            {
                logger.Log("Skipped region: %p", regionStart);
            }

            numRegionsChecked++;
            regionStart += regionSize;
        }

        logger.Log("Stopped at: %p, num regions checked: %i", currentAddress, numRegionsChecked);
        ShowErrorPopup("Could not find signature!");
        return 0;
    }

    static uintptr_t AllocateMemoryWithin32BitRange(size_t numBytes, uintptr_t origin)
    {
        uintptr_t memoryAddress = NULL;
        size_t unidirectionalRange = 0x7fffffff;
        uintptr_t lowerBound = origin > unidirectionalRange ? origin - unidirectionalRange : 0;
        uintptr_t higherBound = origin + unidirectionalRange;
        size_t numAttempts = 0;
        size_t arbitraryIncrement = 10000;

        for (uintptr_t i = lowerBound; i < higherBound; i += arbitraryIncrement)
        {
            numAttempts++;
            memoryAddress = (uintptr_t)VirtualAlloc((void*)i, numBytes, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

            if (memoryAddress != NULL)
            {
                if (memoryAddress >= lowerBound && memoryAddress <= higherBound)
                    break;
                else
                {
                    MEMORY_BASIC_INFORMATION info;
                    VirtualQuery((void*)memoryAddress, &info, sizeof(MEMORY_BASIC_INFORMATION));
                    i += info.RegionSize;
                    VirtualFree((void*)memoryAddress, 0, MEM_RELEASE);
                    memoryAddress = NULL;
                }
            }
        }

        if (memoryAddress == NULL)
        {
            logger.Log(
                "Failed to allocate %i bytes of memory. Origin: %p, lower: %p, higher: %p, attempts: %i",
                numBytes,
                origin,
                lowerBound,
                higherBound,
                numAttempts);
        }
        else
        {
            logger.Log("Allocated %i bytes of memory at %p", numBytes, memoryAddress);
            MemSet(memoryAddress, 0x90, numBytes);
        }

        return memoryAddress;
    }

    static bool IsRelativeNearJumpPresentAtAddress(uintptr_t address)
    {
        std::vector<unsigned char> buffer(1, 0x90);
        std::vector<unsigned char> assemblyRelativeNearJumpByte = { 0xe9 };
        MemCopy((uintptr_t)&buffer[0], address, 1);
        if (buffer == assemblyRelativeNearJumpByte)
        {
            return true;
        };
        return false;
    }

    static bool IsAbsoluteIndirectNearJumpPresentAtAddress(uintptr_t address)
    {
        std::vector<unsigned char> buffer(3, 0x90);
        std::vector<unsigned char> absoluteIndirectNearJumpBytes = { 0x48, 0xff, 0x25 };
        MemCopy((uintptr_t)&buffer[0], address, 3);
        if (buffer == absoluteIndirectNearJumpBytes)
        {
            return true;
        }
        return false;
    }

    static bool IsAbsoluteDirectFarJumpPresentAtAddress(uintptr_t address)
    {
        std::vector<unsigned char> absoluteDirectFarJump = { 0xff, 0x25, 0x00, 0x00, 0x00, 0x00 };
        std::vector<unsigned char> buffer(absoluteDirectFarJump.size(), 0x90);
        MemCopy((uintptr_t)&buffer[0], address, buffer.size());
        if (buffer == absoluteDirectFarJump)
        {
            return true;
        }
        return false;
    }

    static bool IsAddressHooked(uintptr_t address)
    {
        if (
            IsRelativeNearJumpPresentAtAddress(address)
            || IsAbsoluteIndirectNearJumpPresentAtAddress(address)
            || IsAbsoluteDirectFarJumpPresentAtAddress(address))
        {
            return true;
        }
        return false;
    }

    static size_t CalculateRequiredAsmClearance(uintptr_t address, size_t minimumClearance)
    {
        size_t maximumAmountOfBytesToCheck = 30;
        std::vector<uint8_t> bytesBuffer(maximumAmountOfBytesToCheck, 0x90);
        MemCopy((uintptr_t)&bytesBuffer[0], address, maximumAmountOfBytesToCheck);

        if (IsAbsoluteDirectFarJumpPresentAtAddress(address))
        {
            return 14;
        }

        size_t requiredClearance = 0;
        for (size_t byteCount = 0; byteCount < maximumAmountOfBytesToCheck;)
        {
            size_t instructionSize = nmd_x86_ldisasm(
                &bytesBuffer[byteCount],
                maximumAmountOfBytesToCheck - byteCount,
                NMD_X86_MODE_64);

            if (instructionSize <= 0)
            {
                logger.Log("Instruction invalid, could not check length!");
                return minimumClearance;
            }

            if (byteCount >= minimumClearance)
            {
                requiredClearance = byteCount;
                break;
            }

            byteCount += instructionSize;
        }

        return requiredClearance;
    }

    static uintptr_t CalculateAbsoluteDestinationFromRelativeNearJumpAtAddress(uintptr_t relativeNearJumpMemoryLocation)
    {
        int32_t offset = 0;
        MemCopy((uintptr_t)&offset, relativeNearJumpMemoryLocation + 1, 4);
        uintptr_t absoluteAddress = relativeNearJumpMemoryLocation + 5 + offset;
        return absoluteAddress;
    }

    static uintptr_t CalculateAbsoluteDestinationFromAbsoluteIndirectNearJumpAtAddress(uintptr_t absoluteIndirectNearJumpMemoryLocation)
    {
        int32_t offset = 0;
        MemCopy((uintptr_t)&offset, absoluteIndirectNearJumpMemoryLocation + 3, 4);
        uintptr_t memoryContainingAbsoluteAddress = absoluteIndirectNearJumpMemoryLocation + 7 + offset;
        uintptr_t absoluteAddress = *(uintptr_t*)memoryContainingAbsoluteAddress;
        return absoluteAddress;
    }

    static uintptr_t CalculateAbsoluteDestinationFromAbsoluteDirectFarJumpAtAddress(uintptr_t absoluteDirectFarJumpMemoryLocation)
    {
        uintptr_t absoluteAddress = 0;
        MemCopy((uintptr_t)&absoluteAddress, absoluteDirectFarJumpMemoryLocation + 6, 8);
        return absoluteAddress;
    }

    static int32_t CalculateRelativeDisplacementForRelativeJump(uintptr_t relativeJumpAddress, uintptr_t destinationAddress)
    {
        return -int32_t(relativeJumpAddress + 5 - destinationAddress);
    }

    // Places a 14-byte absolutely addressed jump from A to B. 
    // Add extra clearance when the jump doesn't fit cleanly.
    static void PlaceAbsoluteJump(uintptr_t address, uintptr_t destinationAddress, size_t clearance = 14)
    {
        MemSet(address, 0x90, clearance);
        unsigned char absoluteJumpBytes[6] = { 0xff, 0x25, 0x00, 0x00, 0x00, 0x00 };
        MemCopy(address, (uintptr_t)&absoluteJumpBytes[0], 6);
        MemCopy(address + 6, (uintptr_t)&destinationAddress, 8);
        logger.Log("Created absolute jump from %p to %p with a clearance of %i", address, destinationAddress, clearance);
    }

    // Places a 5-byte relatively addressed jump from A to B. 
    // Add extra clearance when the jump doesn't fit cleanly.
    static void PlaceRelativeJump(uintptr_t address, uintptr_t destinationAddress, size_t clearance = 5)
    {
        MemSet(address, 0x90, clearance);
        unsigned char relativeJumpBytes[5] = { 0xe9, 0x00, 0x00, 0x00, 0x00 };
        MemCopy(address, (uintptr_t)&relativeJumpBytes[0], 5);
        int32_t relativeAddress = CalculateRelativeDisplacementForRelativeJump(address, destinationAddress);
        MemCopy((address + 1), (uintptr_t)&relativeAddress, 4);
        logger.Log("Created relative jump from %p to %p with a clearance of %i", address, destinationAddress, clearance);
    }

    static std::string ConvertVectorOfBytesToStringOfHex(std::vector<uint8_t> bytes)
    {
        std::string hexString = "";
        for (auto byte : bytes)
        {
            std::stringstream stream;
            std::string byteAsHex = "";
            stream << std::hex << std::setfill('0') << std::setw(2) << (int)byte;
            byteAsHex = stream.str();
            hexString.append("0x" + byteAsHex + " ");
        }
        return hexString;
    }

    static void PrintBytesAtAddress(uintptr_t address, size_t numBytes)
    {
        std::vector<uint8_t> bytesBuffer(numBytes, 0x90);
        MemCopy((uintptr_t)&bytesBuffer[0], address, bytesBuffer.size());
        std::string hexString = ConvertVectorOfBytesToStringOfHex(bytesBuffer);
        logger.Log("Existing bytes: %s", hexString.c_str());
    }

    // Place a trampoline hook from A to B while taking third-party hooks into consideration.
    // Add extra clearance when the jump doesn't fit cleanly.
    void PlaceHook(uintptr_t addressToHook, uintptr_t destinationAddress, uintptr_t* returnAddress);

    void Unhook(uintptr_t hookedAddress);

    static uintptr_t ReadPointerChain(std::vector<uintptr_t> pointerOffsets)
    {
        DWORD processId = GetCurrentProcessId();
        uintptr_t baseAddress = GetProcessBaseAddress(processId);
        uintptr_t pointer = baseAddress;
        for (size_t i = 0; i < pointerOffsets.size(); i++)
        {
            pointer += pointerOffsets[i];
            if (pointerOffsets[i] != pointerOffsets.back())
            {
                MemCopy((uintptr_t)&pointer, pointer, sizeof(uintptr_t));
            }
            if (pointer == 0)
            {
                return 0;
            }
        }
        return pointer;
    }
}