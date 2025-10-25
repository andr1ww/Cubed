#include "pch.h"
#include "Engine/Source/Runtime/Engine/Classes/Engine/NetDriver.h"

#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"

void NetDriver::TickFlush(UNetDriver* NetDriver, float DeltaTime)
{
    if (NetDriver->ReplicationDriver)
        Runtime::Funcs::ServerReplicateActors(NetDriver->ReplicationDriver, DeltaTime);

    static bool bStartedBus = false;
    if (GetAsyncKeyState(VK_F5) & 0x1 && !bStartedBus) {
        bStartedBus = true;
        UKismetSystemLibrary::ExecuteConsoleCommand(NetDriver->World, L"startaircraft", nullptr);
    }
    
    return TickFlushOG(NetDriver, DeltaTime);
}

void NetDriver::Setup()
{
    UHook* Hook = new UHook();
    
    Hook->Address = ImageBase + Runtime::Offsets::TickFlush;
    Hook->Original = (void**)&TickFlushOG;
    Hook->Detour = TickFlush;
    UKismetHookingLibrary::Hook(Hook, Address);

    Hook->Address = ImageBase + Runtime::Offsets::GetMaxTickRate;
    Hook->Detour = GetMaxTickRate;
    UKismetHookingLibrary::Hook(Hook, Address);
    
    Hook->Address = ImageBase + 0xe97620;
    Hook->Detour = GetMaxTickRate;
    UKismetHookingLibrary::Hook(Hook, Address);

    free(Hook);
}