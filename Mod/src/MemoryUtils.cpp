#include "MemoryUtils.h"
#include <Windows.h>
#include <map>
#include <vector>
#include <Psapi.h>
#include <sstream>
#include <iomanip>

namespace MemoryUtils 
{
    Logger logger{ "MemoryUtils" };
    std::unordered_map<uintptr_t, HookInformation> InfoBufferForHookedAddresses;
    
    static std::map<uintptr_t, DWORD> g_protectionHistory; 

    void ToggleMemoryProtection(bool enableProtection, uintptr_t address, size_t size)
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

    void PlaceHook(uintptr_t addressToHook, uintptr_t destinationAddress, uintptr_t* returnAddress)
    {
        logger.Log("Hooking...");

        int maxFollowAttempts = 50;
        int countFollowAttempts = 0;
        while (IsAddressHooked(addressToHook))
        {
            if (IsRelativeNearJumpPresentAtAddress(addressToHook))
            {
                addressToHook = CalculateAbsoluteDestinationFromRelativeNearJumpAtAddress(addressToHook);
            }
            else if (IsAbsoluteIndirectNearJumpPresentAtAddress(addressToHook))
            {
                addressToHook = CalculateAbsoluteDestinationFromAbsoluteIndirectNearJumpAtAddress(addressToHook);
            }
            else if (IsAbsoluteDirectFarJumpPresentAtAddress(addressToHook))
            {
                //addressToHook = CalculateAbsoluteDestinationFromAbsoluteDirectFarJumpAtAddress(addressToHook);
            }

            countFollowAttempts++;
            if (countFollowAttempts >= maxFollowAttempts)
            {
                break;
            }
        }

        PrintBytesAtAddress(addressToHook, 20);

        const size_t assemblyShortJumpSize = 5;
        const size_t assemblyFarJumpSize = 14;
        size_t trampolineSize = 0;
        uintptr_t trampolineAddress = 0;
        uintptr_t trampolineReturnAddress = 0;
        size_t thirdPartyHookProtectionBuffer = assemblyFarJumpSize;

        size_t clearance = CalculateRequiredAsmClearance(addressToHook, assemblyShortJumpSize);

        trampolineSize = assemblyFarJumpSize * 3 + clearance + thirdPartyHookProtectionBuffer;

        trampolineAddress = AllocateMemoryWithin32BitRange(trampolineSize, addressToHook + assemblyShortJumpSize);

        trampolineReturnAddress = addressToHook + clearance;

        uintptr_t originalInstructionsInTrampoline = trampolineAddress + assemblyFarJumpSize + thirdPartyHookProtectionBuffer;

        MemCopy(originalInstructionsInTrampoline, addressToHook, clearance);

        HookInformation hookInfo;
        hookInfo.originalBytes.resize(clearance);
        hookInfo.trampolineInstructionsAddress = originalInstructionsInTrampoline;
        
        MemCopy((uintptr_t)&hookInfo.originalBytes[0], originalInstructionsInTrampoline, clearance);
        
        InfoBufferForHookedAddresses[addressToHook] = hookInfo;

        PlaceAbsoluteJump(trampolineAddress + thirdPartyHookProtectionBuffer, destinationAddress);
        PlaceAbsoluteJump(trampolineAddress + trampolineSize - assemblyFarJumpSize, trampolineReturnAddress);
        
        *returnAddress = originalInstructionsInTrampoline;
        PlaceRelativeJump(addressToHook, trampolineAddress, clearance);
    }

    void Unhook(uintptr_t hookedAddress)
    {
        auto search = InfoBufferForHookedAddresses.find(hookedAddress);
        if (search != InfoBufferForHookedAddresses.end())
        {
            MemSet(
                InfoBufferForHookedAddresses[hookedAddress].trampolineInstructionsAddress,
                0x90,
                InfoBufferForHookedAddresses[hookedAddress].originalBytes.size());
            MemCopy(
                hookedAddress,
                (uintptr_t)&InfoBufferForHookedAddresses[hookedAddress].originalBytes[0],
                InfoBufferForHookedAddresses[hookedAddress].originalBytes.size());
            logger.Log("Removed hook from %p", hookedAddress);
            InfoBufferForHookedAddresses.erase(hookedAddress);
        }
    }
} 