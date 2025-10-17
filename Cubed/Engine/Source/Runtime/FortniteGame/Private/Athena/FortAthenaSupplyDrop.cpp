#include "pch.h"

#include "Engine/Source/Runtime/FortniteGame/Public/Athena/FortAthenaSupplyDrop.h"

#include "Globals.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Kismet/FortKismetLibrary.h"

AFortPickup* FortAthenaSupplyDrop::SpawnPickup(AFortAthenaSupplyDrop* Context, FFrame& Stack, AFortPickup** Ret)
{
    UFortWorldItemDefinition* ItemDefinition;
    int32 NumberToSpawn;
    AFortPlayerPawn* TriggeringPawn;
    FVector Position;
    FVector Direction;
    Stack.StepCompiledIn(&ItemDefinition);
    Stack.StepCompiledIn(&NumberToSpawn);
    Stack.StepCompiledIn(&TriggeringPawn);
    Stack.StepCompiledIn(&Position);
    Stack.StepCompiledIn(&Direction);
    Stack.IncrementCode();

    int32 ItemCount = NumberToSpawn;
    int32 ClipSize = ItemDefinition->IsA(UFortWeaponItemDefinition::StaticClass()) 
        ? ((UFortWeaponItemDefinition*)ItemDefinition)->GetStats()->ClipSize 
        : 0;
    
    return *Ret = FortKismetLibrary::SpawnPickup(
        Position,
        FortKismetLibrary::ConstructItemEntry(ItemDefinition, ItemCount, ClipSize), 
        EFortPickupSourceTypeFlag::Other, 
        EFortPickupSpawnSource::SupplyDrop,
        TriggeringPawn,      
        -1,
        true,       
        true,         
        false          
    );
}

void FortAthenaSupplyDrop::Setup()
{
    UHook* Hook = new UHook();

    Hook->Path = "/Script/FortniteGame.FortAthenaSupplyDrop.SpawnPickup";
    Hook->Detour = SpawnPickup;
    UKismetHookingLibrary::Hook(Hook, Exec);
}
