// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <memcury.h>

#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"
#include "Engine/Source/Runtime/Engine/Classes/Engine/NetDriver.h"
#include "Engine/Source/Runtime/Engine/Classes/Engine/World.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/FortGameModeAthena.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Inventory/FortInventory.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Player/FortPlayerController.h"
#include "Engine/Source/Runtime/GameplayAbilities/Public/AbilitySystemComponent.h"

static void (*SendRequestNowOG)(void* Arg1, void* MCPData, int);
static void SendRequestNow(void* Arg1, void* MCPData, int)
{
    return SendRequestNowOG(Arg1, MCPData, 3);
}

namespace SDK
{
    UObject *InternalObjectGet(FSoftObjectPtr *Ptr, UClass *Class)
    {
        if (!Ptr)
            return nullptr;
        auto Ret = Ptr->Get();
        if (!Ret && Ptr->ObjectID.AssetPathName.ComparisonIndex > 0) [[unlikely]]
        {
            Ret = StaticLoadObjectNoType(Ptr->ObjectID.AssetPathName.GetRawString().c_str(), Class);
            if (Ret)
            {
                Ptr->WeakPtr.ObjectIndex = Ret->Index;
                Ptr->WeakPtr.ObjectSerialNumber = UObject::GObjects->GetItemByIndex(Ret->Index)->SerialNumber;
            }
        }
        return Ret;
    }
}

DWORD WINAPI Startup(LPVOID)
{
    AllocConsole();
    FILE* OutPtr = nullptr;
    freopen_s(&OutPtr, "CONIN$", "r", stdin);
    freopen_s(&OutPtr, "CONOUT$", "w", stdout);
    freopen_s(&OutPtr, "CONOUT$", "w", stderr);
    SetConsoleTitleA("Cubed | Loading...");

    MH_Initialize();
    Sleep(2500);
    
    FortGameModeAthena::Setup();
    World::Setup();
    NetDriver::Setup();
    FortInventory::Setup();
    FortPlayerController::Setup();
    AbilitySystemComponent::Setup();
    
    UHook* Hook = new UHook();

    {
        auto RequestExit = Memcury::Scanner::FindPattern("40 53 48 83 EC ? 80 3D ? ? ? ? ? 0F B6 D9 72 ? 48 8D 05 ? ? ? ? 89 5C 24 ? 41 B9 ? ? ? ? 48 89 44 24 ? 4C 8D 05 ? ? ? ? 33 D2 33 C9 E8 ? ? ? ? 48 8D 0D").Get();
        if (!RequestExit)
            RequestExit = Memcury::Scanner::FindPattern("88 4C 24 ? 53 48 83 EC ? 80 3D ? ? ? ? ? 8A D9").Get();
        if (!RequestExit)
            RequestExit = Memcury::Scanner::FindPattern("40 53 48 83 EC ? 41 B9 ? ? ? ? 0F B6 D9").Get();

        if (RequestExit)
        {
            Hook->Address = RequestExit;
            Hook->Byte = 0xC3;
            UKismetHookingLibrary::Hook(Hook, Byte);
        }
    }
    
    for (auto& Addr : Runtime::Offsets::NullFuncs)
    {
        Hook->Address = ImageBase + Addr;
        Hook->Byte = 0xC3;
        UKismetHookingLibrary::Hook(Hook, Byte);
    }

    for (auto& Addr : Runtime::Offsets::RetTrueFuncs)
    {
        Hook->Address = ImageBase + Addr;
        UKismetHookingLibrary::Hook(Hook, RTrue);
    }

    Hook->Address = ImageBase + 0x1023D3C;
    Hook->Original = (void**)&SendRequestNowOG;
    Hook->Detour = SendRequestNow;
    UKismetHookingLibrary::Hook(Hook, Address);
    
    *(bool *)(ImageBase + Runtime::Offsets::GIsClient) = false;
    *(bool *)(ImageBase + Runtime::Offsets::GIsClient + 1) = true;

    MH_EnableHook(MH_ALL_HOOKS);
    UWorld::GetWorld()->OwningGameInstance->LocalPlayers.Remove(0);
    UGameplayStatics::OpenLevel(UWorld::GetWorld(), UKismetStringLibrary::Conv_StringToName(L"Apollo_Terrain"), true, FString());
    
    return 0;
}

bool DllMain(HMODULE hModule, int rsn, void*)
{
    if (rsn == DLL_PROCESS_ATTACH)
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Startup, 0, 0, 0);
    
    return true;
}