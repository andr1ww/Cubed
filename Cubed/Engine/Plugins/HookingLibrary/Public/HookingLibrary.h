#pragma once

#include "pch.h"

enum EHook
{
    Exec,
    VFT,
    Address,
    Modify,
    ModifyCustom,
    RTrue,
    RFalse,
    FTrue,
    Byte,
    EveryVFT
};

class UHook
{
public:
    uintptr_t Address;
    uintptr_t Swap;
    std::string Path;
    uint32 Byte;
    int Offset;
    void** Original = nullptr;
    void* Detour;
    UClass* Class;
};

class UKismetHookingLibrary
{
public:
    static void PatchBytes(uint64 addr, const std::vector<uint8_t>& Bytes) 
    {
        if (!addr)
            return;

        UHook* a1 = new UHook();
        for (int i = 0; i < Bytes.size(); i++)
        {
            a1->Address = addr + i;
            a1->Byte = Bytes.at(i);
            Hook(a1, EHook::Byte);
        }
    }
    
    static uint8_t* AllocateNearbyPage(void* targetAddr)
    {
        SYSTEM_INFO SysInfo;
        GetSystemInfo(&SysInfo);

        const uint64_t PageSize = SysInfo.dwPageSize;
        const uint64_t StartAddr = (uint64_t(targetAddr) & ~(PageSize - 1));
        const uint64_t MinAddr = min(StartAddr - 0x7FFFFF00, (uint64_t)SysInfo.lpMinimumApplicationAddress);
        const uint64_t MaxAddr = max(StartAddr + 0x7FFFFF00, (uint64_t)SysInfo.lpMaximumApplicationAddress);
        const uint64_t StartPage = (StartAddr - (StartAddr % PageSize));

        for (uint64_t PageOffset = 1; PageOffset; PageOffset++)
        {
            uint64_t ByteOffset = PageOffset * PageSize;
            uint64_t HighAddr = StartPage + ByteOffset;
            uint64_t LowAddr = (StartPage > ByteOffset) ? StartPage - ByteOffset : 0;

            bool NeedsExit = HighAddr > MaxAddr && LowAddr < MinAddr;

            if (HighAddr < MaxAddr)
            {
                if (void* OutAddr = VirtualAlloc((void*)HighAddr, PageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))
                {
                    return (uint8_t*)OutAddr;
                }
            }

            if (LowAddr > MinAddr)
            {
                if (void* OutAddr = VirtualAlloc((void*)LowAddr, PageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))
                {
                    return (uint8_t*)OutAddr;
                }
            }

            if (NeedsExit)
            {
                break;
            }
        }

        return nullptr;
    }
    
    static void Rel32Swap(void** Target, void* Detour)
    {
        auto Impl = (uint8*)(*Target);
        auto NearPage = AllocateNearbyPage(Impl);

        if (!NearPage)
        {
            return;
        }

        uint8_t Shellcode[] =
        {
            0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, // jmp [$+6]
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        memcpy(Shellcode + 6, &Detour, 8);
        memcpy(NearPage, Shellcode, sizeof(Shellcode));

        auto Offset = static_cast<int>(NearPage - (Impl + 5));

        memcpy(Impl + 1, &Offset, sizeof(int));
    }
    
    static void Hook(UHook* Hook, EHook Type = EHook::Address);

    static void NullCall(uint64_t Offset)
    {
        static UHook* a1 = new UHook();
        for (int i = 0; i < 5; i++)
        {
            a1->Address = ImageBase + Offset + i;
            a1->Byte = 0x90;
            Hook(a1, EHook::Byte);
        }
    }
};